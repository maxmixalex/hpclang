#include <hpclang/ui.hpp>
#include <cassert>
#include <cstdlib>
#include <type_traits>

static_assert(!std::is_copy_constructible_v<hpclang::ui::scope>);
static_assert(!std::is_move_constructible_v<hpclang::ui::scope>);
static_assert(!std::is_copy_constructible_v<hpclang::ui::declaration>);
static_assert(!std::is_move_constructible_v<hpclang::ui::declaration>);

static int component(hpclang_ui *ui, uint32_t key)
{
    hpclang::ui::scope group{ui, key, HPCLANG_UI_COMPONENT};
    if (!group)
        return -1;
    hpclang::ui::scope row{ui, 1, HPCLANG_UI_ROW};
    if (!row)
        return -1;
    return hpclang_ui_text(ui, 1, 0, "value");
}

static void abandoned(hpclang_ui *ui)
{
    hpclang::ui::declaration candidate{ui};
    assert(candidate);
    assert(component(ui, 99) == 0);
    // Deliberate early return: the old committed tree must survive.
}

int main()
{
    const hpclang_ui_config config{8, 64, 0};
    const auto bytes = hpclang_ui_storage_size(config);
    auto *storage = std::malloc(bytes);
    assert(storage);
    auto *ui = hpclang_ui_init(storage, bytes, config);
    assert(ui);
    {
        hpclang::ui::declaration frame{ui};
        assert(frame);
        {
            hpclang::ui::declaration duplicate{ui};
            assert(!duplicate && errno == EBUSY);
        }
        assert(component(ui, 10) == 0);
        assert(component(ui, 20) == 0);
        assert(frame.commit() == 0);
        assert(!frame);
        assert(frame.commit() == -1 && errno == EINVAL);
    }
    assert(hpclang_ui_node_count(ui) == 6);
    const auto first = hpclang_ui_node_at(ui, 0)->id;
    abandoned(ui);
    assert(hpclang_ui_node_count(ui) == 6 && hpclang_ui_node_at(ui, 0)->id == first);
    {
        hpclang::ui::declaration frame{ui};
        assert(frame);
        assert(component(ui, 20) == 0);
        assert(component(ui, 10) == 0);
        assert(frame.commit() == 0);
    }
    assert(hpclang_ui_node_at(ui, 3)->id == first);
    {
        hpclang::ui::declaration frame{ui};
        assert(component(ui, 10) == 0);
        assert(component(ui, 20) == 0);
        assert(component(ui, 30) == -1 && errno == ENOSPC);
        assert(frame.commit() == -1 && errno == ENOSPC);
    }
    assert(hpclang_ui_node_count(ui) == 6 && hpclang_ui_node_at(ui, 3)->id == first);
    assert(hpclang_ui_shutdown(ui) == 0);
    std::free(storage);
}
