#include <stdlib.h>
#include <string.h>

#include "storage-namespace-lease.h"

struct fake_context {
    char log[32];
    unsigned used;
    int idle;
    int acquire_result;
    int mount_result;
    int unmount_result;
    int release_result;
    int reinit_result;
};

static void mark(struct fake_context *ctx, char value)
{
    if (ctx->used + 1u < sizeof(ctx->log)) {
        ctx->log[ctx->used++] = value;
        ctx->log[ctx->used] = '\0';
    }
}

static int stock_idle(void *opaque)
{
    struct fake_context *ctx = opaque;
    mark(ctx, 'I');
    return ctx->idle;
}

static int direct_acquire(void *opaque)
{
    struct fake_context *ctx = opaque;
    mark(ctx, 'A');
    return ctx->acquire_result;
}

static int direct_mount(void *opaque)
{
    struct fake_context *ctx = opaque;
    mark(ctx, 'M');
    return ctx->mount_result;
}

static int direct_unmount(void *opaque)
{
    struct fake_context *ctx = opaque;
    mark(ctx, 'U');
    return ctx->unmount_result;
}

static int direct_release(void *opaque)
{
    struct fake_context *ctx = opaque;
    mark(ctx, 'R');
    return ctx->release_result;
}

static int stock_reinit(void *opaque)
{
    struct fake_context *ctx = opaque;
    mark(ctx, 'S');
    return ctx->reinit_result;
}

static const struct hpclang_namespace_lease_ops ops = {
    .stock_idle = stock_idle,
    .direct_acquire = direct_acquire,
    .direct_mount = direct_mount,
    .direct_unmount = direct_unmount,
    .direct_release = direct_release,
    .stock_reinit = stock_reinit,
};

static int expect_log(const struct fake_context *ctx, const char *expected)
{
    return strcmp(ctx->log, expected) == 0;
}

static int success_path(void)
{
    struct fake_context ctx = {.idle = 1};
    struct hpclang_namespace_lease lease;
    hpclang_namespace_lease_init(&lease, &ops, &ctx);

    if (hpclang_namespace_lease_acquire(&lease) !=
        HPCLANG_NAMESPACE_LEASE_OK)
        return 0;
    if (lease.state != HPCLANG_NAMESPACE_DIRECT ||
        !expect_log(&ctx, "IAM"))
        return 0;

    if (hpclang_namespace_lease_acquire(&lease) !=
        HPCLANG_NAMESPACE_LEASE_BUSY ||
        !expect_log(&ctx, "IAM"))
        return 0;

    if (hpclang_namespace_lease_release(&lease) !=
        HPCLANG_NAMESPACE_LEASE_OK)
        return 0;
    return lease.state == HPCLANG_NAMESPACE_STOCK &&
           expect_log(&ctx, "IAMURS");
}

static int busy_preflight(void)
{
    struct fake_context ctx = {.idle = 0};
    struct hpclang_namespace_lease lease;
    hpclang_namespace_lease_init(&lease, &ops, &ctx);

    return hpclang_namespace_lease_acquire(&lease) ==
               HPCLANG_NAMESPACE_LEASE_BUSY &&
           lease.state == HPCLANG_NAMESPACE_STOCK &&
           expect_log(&ctx, "I");
}

static int mount_failure_restores_stock(void)
{
    struct fake_context ctx = {.idle = 1, .mount_result = -1};
    struct hpclang_namespace_lease lease;
    hpclang_namespace_lease_init(&lease, &ops, &ctx);

    return hpclang_namespace_lease_acquire(&lease) ==
               HPCLANG_NAMESPACE_LEASE_MOUNT_FAILED &&
           lease.state == HPCLANG_NAMESPACE_STOCK &&
           expect_log(&ctx, "IAMRS");
}

static int failed_cleanup_breaks_lease(void)
{
    struct fake_context ctx = {
        .idle = 1,
        .mount_result = -1,
        .release_result = -1,
    };
    struct hpclang_namespace_lease lease;
    hpclang_namespace_lease_init(&lease, &ops, &ctx);

    return hpclang_namespace_lease_acquire(&lease) ==
               HPCLANG_NAMESPACE_LEASE_RELEASE_FAILED &&
           lease.state == HPCLANG_NAMESPACE_BROKEN &&
           expect_log(&ctx, "IAMRS") &&
           hpclang_namespace_lease_acquire(&lease) ==
               HPCLANG_NAMESPACE_LEASE_BAD_STATE;
}

static int release_is_best_effort(void)
{
    struct fake_context ctx = {.idle = 1, .unmount_result = -1};
    struct hpclang_namespace_lease lease;
    hpclang_namespace_lease_init(&lease, &ops, &ctx);

    if (hpclang_namespace_lease_acquire(&lease) !=
        HPCLANG_NAMESPACE_LEASE_OK)
        return 0;

    return hpclang_namespace_lease_release(&lease) ==
               HPCLANG_NAMESPACE_LEASE_UNMOUNT_FAILED &&
           lease.state == HPCLANG_NAMESPACE_BROKEN &&
           expect_log(&ctx, "IAMURS");
}

static int reinit_failure_breaks_lease(void)
{
    struct fake_context ctx = {.idle = 1, .reinit_result = -1};
    struct hpclang_namespace_lease lease;
    hpclang_namespace_lease_init(&lease, &ops, &ctx);

    if (hpclang_namespace_lease_acquire(&lease) !=
        HPCLANG_NAMESPACE_LEASE_OK)
        return 0;

    return hpclang_namespace_lease_release(&lease) ==
               HPCLANG_NAMESPACE_LEASE_STOCK_REINIT_FAILED &&
           lease.state == HPCLANG_NAMESPACE_BROKEN &&
           expect_log(&ctx, "IAMURS");
}

int main(void)
{
    if (!success_path() ||
        !busy_preflight() ||
        !mount_failure_restores_stock() ||
        !failed_cleanup_breaks_lease() ||
        !release_is_best_effort() ||
        !reinit_failure_breaks_lease())
        return EXIT_FAILURE;

    return EXIT_SUCCESS;
}
