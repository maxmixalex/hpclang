#ifndef HPCLANG_STORAGE_NAMESPACE_LEASE_H
#define HPCLANG_STORAGE_NAMESPACE_LEASE_H

#ifdef __cplusplus
extern "C" {
#endif

enum hpclang_namespace_lease_state {
    HPCLANG_NAMESPACE_STOCK = 0,
    HPCLANG_NAMESPACE_DIRECT = 1,
    HPCLANG_NAMESPACE_BROKEN = 2
};

enum hpclang_namespace_lease_result {
    HPCLANG_NAMESPACE_LEASE_OK = 0,
    HPCLANG_NAMESPACE_LEASE_BUSY = -1,
    HPCLANG_NAMESPACE_LEASE_BAD_STATE = -2,
    HPCLANG_NAMESPACE_LEASE_PREFLIGHT_FAILED = -3,
    HPCLANG_NAMESPACE_LEASE_ACQUIRE_FAILED = -4,
    HPCLANG_NAMESPACE_LEASE_MOUNT_FAILED = -5,
    HPCLANG_NAMESPACE_LEASE_UNMOUNT_FAILED = -6,
    HPCLANG_NAMESPACE_LEASE_RELEASE_FAILED = -7,
    HPCLANG_NAMESPACE_LEASE_STOCK_REINIT_FAILED = -8
};

/*
 * Internal lifecycle operations for the M7.2c whole-process namespace lease.
 *
 * stock_idle:
 *   >0 when no Stock-KOS stream handle/state blocks acquisition;
 *    0 when Stock is still in use;
 *   <0 when the preflight check itself failed.
 *
 * direct_acquire must either acquire the media/controller completely or roll
 * back its own partial work before returning failure.
 *
 * direct_release is best-effort neutralization: even when it reports failure,
 * it must leave the implementation safe enough for stock_reinit to be
 * attempted. Any cleanup failure marks the lease BROKEN and prevents reuse.
 */
struct hpclang_namespace_lease_ops {
    int (*stock_idle)(void *context);
    int (*direct_acquire)(void *context);
    int (*direct_mount)(void *context);
    int (*direct_unmount)(void *context);
    int (*direct_release)(void *context);
    int (*stock_reinit)(void *context);
};

struct hpclang_namespace_lease {
    enum hpclang_namespace_lease_state state;
    const struct hpclang_namespace_lease_ops *ops;
    void *context;
};

void hpclang_namespace_lease_init(
    struct hpclang_namespace_lease *lease,
    const struct hpclang_namespace_lease_ops *ops,
    void *context);

int hpclang_namespace_lease_acquire(struct hpclang_namespace_lease *lease);
int hpclang_namespace_lease_release(struct hpclang_namespace_lease *lease);

#ifdef __cplusplus
}
#endif

#endif
