#!/usr/bin/env python3
from pathlib import Path

path = Path("tests/e2e/harness/x50ng/stock-launcher-main.c")
source = path.read_text()


def replace_once(old: str, new: str) -> None:
    global source
    count = source.count(old)
    if count != 1:
        raise SystemExit(f"instrumentation anchor count {count}, expected 1:\n{old}")
    source = source.replace(old, new, 1)


replace_once(
    "#define PAYLOAD_KEY_FIRST_DELAY_US 70000LL",
    "#define PAYLOAD_KEY_FIRST_DELAY_US 20000LL",
)

replace_once(
    """static void key_cb(void *arg)\n{\n    const struct key_action *a = arg;\n    if (a->down) press_key(a->key);\n    else release_key(a->key);\n}\n\nstatic int tap(\n""",
    """static void key_cb(void *arg)\n{\n    const struct key_action *a = arg;\n    if (a->down) press_key(a->key);\n    else release_key(a->key);\n}\n\n/*\n * Stock sequence files pre-schedule sparse ENTER retries so slow serial-disk\n * transfers can still launch ARMToolbox. Once the HPC1 payload publishes KBRD,\n * those launcher retries must no longer inject key-down edges into the strict\n * 51-key payload test. Always allow key-up so a key already pressed before\n * KBRD can still be released.\n */\nstruct sequence_key_event {\n    struct state *state;\n    const struct key_action *action;\n};\n\nstatic void sequence_key_cb(void *arg)\n{\n    struct sequence_key_event *event = arg;\n    const struct key_action *action = event->action;\n    struct state *s = event->state;\n\n    if (!action->down || !s->payload_keyboard_scheduled)\n        key_cb((void *)action);\n    free(event);\n}\n\nstatic int sequence_key(\n    struct state *s,\n    const struct key_action *action,\n    int64_t us)\n{\n    struct sequence_key_event *event = calloc(1, sizeof(*event));\n    if (!event)\n        return 0;\n    event->state = s;\n    event->action = action;\n    if (!later(sequence_key_cb, event, us)) {\n        free(event);\n        return 0;\n    }\n    return 1;\n}\n\nstatic int sequence_tap(\n    struct state *s,\n    const struct key_action *down,\n    const struct key_action *up_action,\n    int64_t us)\n{\n    return sequence_key(s, down, us) &&\n           sequence_key(s, up_action, us + KEY_HOLD_US);\n}\n\nstatic int sequence_chord(\n    struct state *s,\n    const struct key_action *mod_down,\n    const struct key_action *key_down,\n    const struct key_action *key_up,\n    const struct key_action *mod_up,\n    int64_t us)\n{\n    return sequence_key(s, mod_down, us) &&\n           sequence_key(s, key_down, us + 30000) &&\n           sequence_key(s, key_up, us + 130000) &&\n           sequence_key(s, mod_up, us + 160000);\n}\n\nstatic int tap(\n""",
)

replace_once(
    """            if (!key || !tap(key->down, key->up, us))\n                goto invalid_action;\n""",
    """            if (!key || !sequence_tap(s, key->down, key->up, us))\n                goto invalid_action;\n""",
)

replace_once(
    """            if (!mod || !key || !chord(\n                    mod->down, key->down, key->up, mod->up, us))\n                goto invalid_action;\n""",
    """            if (!mod || !key || !sequence_chord(\n                    s, mod->down, key->down, key->up, mod->up, us))\n                goto invalid_action;\n""",
)

path.write_text(source)
print("instrumented Stock sequence retry guard; payload first delay restored to 20 ms")
