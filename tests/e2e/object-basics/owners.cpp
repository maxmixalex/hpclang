#include <hpclang/abi.h>
#include <hpclang/device.h>
#include <hpclang/object-store.hpp>
#include <cstdint>
#include <utility>

static int digest(hpclang_object_store_iterator *iterator, std::uint32_t &hash)
{
    hash = 2166136261u;
    for (;;) {
        hpclang_object_store_entry entry{};
        entry.size = sizeof entry;
        entry.version = HPCLANG_OBJECT_STORE_ENTRY_VERSION;
        int result = hpclang_object_store_iterator_read(iterator, &entry);
        if (result <= 0)
            return result;
        hash = (hash ^ entry.kind) * 16777619u;
        hash = (hash ^ entry.flags) * 16777619u;
        hash = (hash ^ entry.name_length) * 16777619u;
        for (uint32_t i = 0; i < entry.name_length; ++i)
            hash = (hash ^ entry.name[i]) * 16777619u;
    }
}

extern "C" int hpclang_e2e_object_store_owners()
{
    const auto *boot = hpclang_boot_info();
    const bool live =
        boot && boot->saturn_state &&
        (boot->model == HPCLANG_MODEL_HP48GII || boot->model == HPCLANG_MODEL_HP49GPLUS ||
         boot->model == HPCLANG_MODEL_HP50G);
    hpclang::owned_object_store home{hpclang_object_store_home()};
    if (!live)
        return !home && errno == ENOSYS ? 0 : 1;
    if (!home)
        return 2;
    hpclang::owned_object_store_iterator first{hpclang_object_store_iterator_open(home.get())};
    hpclang::owned_object_store_iterator second{hpclang_object_store_iterator_open(home.get())};
    if (!first || !second)
        return 3;
    std::uint32_t expected, actual;
    if (digest(first.get(), expected))
        return 4;
    home.reset();
    first = std::move(second);
    if (second || digest(first.get(), actual) || actual != expected)
        return 5;
    auto *released = first.release();
    if (first)
        return 6;
    errno = EIO;
    {
        hpclang::owned_object_store_iterator adopted{released};
    }
    return errno == EIO ? 0 : 7;
}
