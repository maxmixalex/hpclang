#include "storage-namespace-lease.h"

static int lease_ops_valid(const struct hpclang_namespace_lease_ops *ops)
{
    return ops != 0 &&
           ops->stock_idle != 0 &&
           ops->direct_acquire != 0 &&
           ops->direct_mount != 0 &&
           ops->direct_unmount != 0 &&
           ops->direct_release != 0 &&
           ops->stock_reinit != 0;
}

void hpclang_namespace_lease_init(
    struct hpclang_namespace_lease *lease,
    const struct hpclang_namespace_lease_ops *ops,
    void *context)
{
    if (lease == 0)
        return;

    lease->ops = ops;
    lease->context = context;
    lease->state = lease_ops_valid(ops)
        ? HPCLANG_NAMESPACE_STOCK
        : HPCLANG_NAMESPACE_BROKEN;
}

int hpclang_namespace_lease_acquire(struct hpclang_namespace_lease *lease)
{
    if (lease == 0 || !lease_ops_valid(lease->ops))
        return HPCLANG_NAMESPACE_LEASE_BAD_STATE;
    if (lease->state == HPCLANG_NAMESPACE_DIRECT)
        return HPCLANG_NAMESPACE_LEASE_BUSY;
    if (lease->state != HPCLANG_NAMESPACE_STOCK)
        return HPCLANG_NAMESPACE_LEASE_BAD_STATE;

    int idle = lease->ops->stock_idle(lease->context);
    if (idle < 0)
        return HPCLANG_NAMESPACE_LEASE_PREFLIGHT_FAILED;
    if (idle == 0)
        return HPCLANG_NAMESPACE_LEASE_BUSY;

    /*
     * direct_acquire owns its own partial-failure rollback. Once it succeeds,
     * every later path must pass through direct_release and Stock reinit.
     */
    if (lease->ops->direct_acquire(lease->context) != 0)
        return HPCLANG_NAMESPACE_LEASE_ACQUIRE_FAILED;

    if (lease->ops->direct_mount(lease->context) != 0) {
        int release_failed =
            lease->ops->direct_release(lease->context) != 0;
        int reinit_failed =
            lease->ops->stock_reinit(lease->context) != 0;

        if (release_failed || reinit_failed) {
            lease->state = HPCLANG_NAMESPACE_BROKEN;
            return release_failed
                ? HPCLANG_NAMESPACE_LEASE_RELEASE_FAILED
                : HPCLANG_NAMESPACE_LEASE_STOCK_REINIT_FAILED;
        }

        lease->state = HPCLANG_NAMESPACE_STOCK;
        return HPCLANG_NAMESPACE_LEASE_MOUNT_FAILED;
    }

    lease->state = HPCLANG_NAMESPACE_DIRECT;
    return HPCLANG_NAMESPACE_LEASE_OK;
}

int hpclang_namespace_lease_release(struct hpclang_namespace_lease *lease)
{
    if (lease == 0 || !lease_ops_valid(lease->ops))
        return HPCLANG_NAMESPACE_LEASE_BAD_STATE;
    if (lease->state != HPCLANG_NAMESPACE_DIRECT)
        return HPCLANG_NAMESPACE_LEASE_BAD_STATE;

    int result = HPCLANG_NAMESPACE_LEASE_OK;

    if (lease->ops->direct_unmount(lease->context) != 0)
        result = HPCLANG_NAMESPACE_LEASE_UNMOUNT_FAILED;

    if (lease->ops->direct_release(lease->context) != 0 &&
        result == HPCLANG_NAMESPACE_LEASE_OK)
        result = HPCLANG_NAMESPACE_LEASE_RELEASE_FAILED;

    if (lease->ops->stock_reinit(lease->context) != 0 &&
        result == HPCLANG_NAMESPACE_LEASE_OK)
        result = HPCLANG_NAMESPACE_LEASE_STOCK_REINIT_FAILED;

    lease->state = result == HPCLANG_NAMESPACE_LEASE_OK
        ? HPCLANG_NAMESPACE_STOCK
        : HPCLANG_NAMESPACE_BROKEN;

    return result;
}
