#include <hpclang/object.hpp>
#include <hpclang/object-store.hpp>
#include <hpclang/port.hpp>
#include <cstdint>

int main()
{
    hpclang::owned_object value;
    if (value.receive([](auto **out) { return hpclang_object_create_integer_i64(42, out); }) != 0)
        return 1;

    // The previous snapshot remains alive while the producer reads it.
    if (value.receive([&](auto **out) {
            std::int64_t previous = 0;
            if (hpclang_object_integer_i64(value.get(), &previous) != 0)
                return -1;
            return hpclang_object_create_integer_i64(previous + 1, out);
        }) != 0)
        return 2;

    std::int64_t result = 0;
    if (hpclang_object_integer_i64(value.get(), &result) != 0 || result != 43)
        return 3;

    // Port 0 metadata is also optional; the owner closes its independent snapshot.
    hpclang::owned_port_iterator port{hpclang_port_iterator_open(0)};
    if (!port && errno != ENOSYS)
        return 7;
    while (port) {
        hpclang_port_entry entry{};
        entry.size = sizeof entry;
        entry.version = HPCLANG_PORT_ENTRY_VERSION;
        int next = hpclang_port_iterator_read(port.get(), &entry);
        if (next < 0)
            return 8;
        if (!next)
            break;
    }
    port.reset();

    // Native HOME is optional: HP39 and non-Stock contexts report ENOSYS.
    hpclang::owned_object_store home{hpclang_object_store_home()};
    if (!home)
        return errno == ENOSYS ? 0 : 4;
    hpclang::owned_object_store_iterator entries{hpclang_object_store_iterator_open(home.get())};
    if (!entries)
        return 5;
    home.reset(); // The independent metadata snapshot remains valid.
    for (;;) {
        hpclang_object_store_entry entry{};
        entry.size = sizeof entry;
        entry.version = HPCLANG_OBJECT_STORE_ENTRY_VERSION;
        int next = hpclang_object_store_iterator_read(entries.get(), &entry);
        if (next < 0)
            return 6;
        if (!next)
            return 0;
    }
}
