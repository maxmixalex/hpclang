#!/usr/bin/env python3
from pathlib import Path

path = Path("tests/e2e/main.c")
source = path.read_text()


def replace_once(old: str, new: str) -> None:
    global source
    count = source.count(old)
    if count != 1:
        raise SystemExit(f"instrumentation anchor count {count}, expected 1:\n{old}")
    source = source.replace(old, new, 1)


replace_once(
    """static uint64_t hpclang_keyboard_passed_bits;\nstatic uint8_t hpclang_keyboard_failed_scan = 0xffu;\nstatic unsigned hpclang_keyboard_passed_count;\n""",
    """static uint64_t hpclang_keyboard_passed_bits;\nstatic uint8_t hpclang_keyboard_failed_scan = 0xffu;\nstatic unsigned hpclang_keyboard_passed_count;\n\n/* Diagnostic-only fields injected by the Stock hp39 witness workflow. */\nstatic uint8_t hpclang_keyboard_failed_phase;\nstatic uint8_t hpclang_keyboard_actual_valid;\nstatic uint8_t hpclang_keyboard_actual_type;\nstatic uint8_t hpclang_keyboard_actual_scan = 0xffu;\nstatic uint16_t hpclang_keyboard_expected_key;\nstatic uint16_t hpclang_keyboard_actual_key;\nstatic uint32_t hpclang_keyboard_expected_modifiers;\nstatic uint32_t hpclang_keyboard_actual_modifiers;\nstatic unsigned hpclang_keyboard_failed_index;\n""",
)

replace_once(
    """static void draw_keyboard_result(struct hpclang_surface *surface, int pass)\n{\n    hpclang_surface_clear(surface, 0u);\n""",
    """static void draw_keyboard_failure_witness(struct hpclang_surface *surface)\n{\n    char line[40];\n    const char *phase =\n        hpclang_keyboard_failed_phase == 1u ? \"DOWN\" :\n        hpclang_keyboard_failed_phase == 2u ? \"UP\" :\n        hpclang_keyboard_failed_phase == 3u ? \"TDN\" :\n        hpclang_keyboard_failed_phase == 4u ? \"TUP\" : \"UNK\";\n\n    hpclang_surface_clear(surface, 0u);\n    (void)snprintf(\n        line, sizeof(line), \"KBD %s I%02u\",\n        phase, hpclang_keyboard_failed_index);\n    text(surface, 2, 2, line);\n\n    (void)snprintf(\n        line, sizeof(line), \"E S%02X K%04X M%02X\",\n        hpclang_keyboard_failed_scan,\n        (unsigned)hpclang_keyboard_expected_key,\n        (unsigned)(hpclang_keyboard_expected_modifiers & 0xffu));\n    text(surface, 2, 16, line);\n\n    if (hpclang_keyboard_actual_valid) {\n        (void)snprintf(\n            line, sizeof(line), \"A T%u S%02X K%04X M%02X\",\n            (unsigned)hpclang_keyboard_actual_type,\n            (unsigned)hpclang_keyboard_actual_scan,\n            (unsigned)hpclang_keyboard_actual_key,\n            (unsigned)(hpclang_keyboard_actual_modifiers & 0xffu));\n    } else {\n        (void)snprintf(line, sizeof(line), \"A TIMEOUT\");\n    }\n    text(surface, 2, 30, line);\n}\n\nstatic void draw_keyboard_result(struct hpclang_surface *surface, int pass)\n{\n    if (!pass && hpclang_keyboard_failed_phase != 0u) {\n        draw_keyboard_failure_witness(surface);\n        return;\n    }\n\n    hpclang_surface_clear(surface, 0u);\n""",
)

replace_once(
    """    hpclang_keyboard_passed_bits = 0u;\n    hpclang_keyboard_failed_scan = 0xffu;\n    hpclang_keyboard_passed_count = 0u;\n""",
    """    hpclang_keyboard_passed_bits = 0u;\n    hpclang_keyboard_failed_scan = 0xffu;\n    hpclang_keyboard_passed_count = 0u;\n    hpclang_keyboard_failed_phase = 0u;\n    hpclang_keyboard_actual_valid = 0u;\n    hpclang_keyboard_actual_type = 0u;\n    hpclang_keyboard_actual_scan = 0xffu;\n    hpclang_keyboard_expected_key = 0u;\n    hpclang_keyboard_actual_key = 0u;\n    hpclang_keyboard_expected_modifiers = 0u;\n    hpclang_keyboard_actual_modifiers = 0u;\n    hpclang_keyboard_failed_index = 0u;\n""",
)

replace_once(
    """                hpclang_keyboard_failed_scan = want->scan;\n                return 0;\n            }\n            awaiting_up = 1;\n""",
    """                hpclang_keyboard_failed_scan = want->scan;\n                hpclang_keyboard_failed_phase = 1u;\n                hpclang_keyboard_failed_index = (unsigned)index;\n                hpclang_keyboard_expected_key = want->key;\n                hpclang_keyboard_expected_modifiers = want->down_modifiers;\n                hpclang_keyboard_actual_valid = 1u;\n                hpclang_keyboard_actual_type = event.type;\n                hpclang_keyboard_actual_scan = event.scan_code;\n                hpclang_keyboard_actual_key = event.key;\n                hpclang_keyboard_actual_modifiers = event.modifiers;\n                return 0;\n            }\n            awaiting_up = 1;\n""",
)

replace_once(
    """            hpclang_keyboard_failed_scan = want->scan;\n            return 0;\n        }\n\n        hpclang_keyboard_passed_bits |= UINT64_C(1) << want->scan;\n""",
    """            hpclang_keyboard_failed_scan = want->scan;\n            hpclang_keyboard_failed_phase = 2u;\n            hpclang_keyboard_failed_index = (unsigned)index;\n            hpclang_keyboard_expected_key = want->key;\n            hpclang_keyboard_expected_modifiers = 0u;\n            hpclang_keyboard_actual_valid = 1u;\n            hpclang_keyboard_actual_type = event.type;\n            hpclang_keyboard_actual_scan = event.scan_code;\n            hpclang_keyboard_actual_key = event.key;\n            hpclang_keyboard_actual_modifiers = event.modifiers;\n            return 0;\n        }\n\n        hpclang_keyboard_passed_bits |= UINT64_C(1) << want->scan;\n""",
)

replace_once(
    """    if (index != count || awaiting_up) {\n        if (index < count)\n            hpclang_keyboard_failed_scan = expected[index].scan;\n        return 0;\n    }\n""",
    """    if (index != count || awaiting_up) {\n        if (index < count) {\n            const struct expected_key_event *want = &expected[index];\n            hpclang_keyboard_failed_scan = want->scan;\n            hpclang_keyboard_failed_phase = awaiting_up ? 4u : 3u;\n            hpclang_keyboard_failed_index = (unsigned)index;\n            hpclang_keyboard_expected_key = want->key;\n            hpclang_keyboard_expected_modifiers =\n                awaiting_up ? 0u : want->down_modifiers;\n            hpclang_keyboard_actual_valid = 0u;\n        }\n        return 0;\n    }\n""",
)

path.write_text(source)
print("instrumented Stock keyboard expected-vs-actual witness")
