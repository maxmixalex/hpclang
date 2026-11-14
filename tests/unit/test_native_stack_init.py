import importlib.util
import json
from pathlib import Path
import struct
import unittest


PATH = Path(__file__).resolve().parents[1] / "e2e/harness/verify-native-stack-init.py"
SPEC = importlib.util.spec_from_file_location("native_stack_init", PATH)
CHECK = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(CHECK)


class NativeStackInitTest(unittest.TestCase):
    def setUp(self):
        # Synthetic decoder fixture, not evidence of a real ROM reservation.
        self.flash = bytearray(260000)
        self.rows = []
        for index, (mode, offset, flags, top) in enumerate(CHECK.INITIALIZERS):
            pc, literal = 258840 + offset, 259736 + index * 4
            for address, value in (
                (pc - 8, 0xE3801000 | flags),
                (pc - 4, 0xE12FF001),
                (pc, 0xE59FD000 | (literal - pc - 8)),
                (literal, top),
            ):
                struct.pack_into("<I", self.flash, address, value)
            self.rows.append(
                dict(
                    version=1,
                    mode=mode,
                    region=128,
                    writes=2,
                    minimum=top - 16,
                    maximum=top,
                    minimum_pc=pc + 100,
                    maximum_pc=pc,
                )
            )

    def check(self):
        return CHECK.verify(
            "hp50g",
            self.flash,
            "\n".join(json.dumps(row) for row in self.rows).encode(),
        )

    def test_reconciliation_is_not_reservation(self):
        result = self.check()
        self.assertEqual(len(result["initializers"]), 5)
        self.assertTrue(
            all(x["top_minus_observed_minimum"] == 16 for x in result["initializers"])
        )
        self.assertIn("not reserved capacities", result["scope"])

    def test_rom_mutations_reject(self):
        for address in (258832, 258836, 258840, 259736):
            with self.subTest(address=address):
                self.flash[address] ^= 1
                with self.assertRaises(ValueError):
                    self.check()
                self.flash[address] ^= 1
        self.flash = self.flash[:258840]
        with self.assertRaises(ValueError):
            self.check()

    def test_missing_duplicate_and_invalid_rows_reject(self):
        original = list(self.rows)
        for rows in (original[:-1], original + [original[0]], [None]):
            self.rows = rows
            with self.subTest(rows=rows), self.assertRaises(ValueError):
                self.check()
        self.rows = original
        for key, value in (
            ("writes", 0),
            ("version", True),
            ("region", 127),
            ("minimum", -1),
            ("maximum", 2**32),
            ("minimum_pc", 2**32),
        ):
            old = self.rows[0][key]
            self.rows[0][key] = value
            with self.subTest(key=key), self.assertRaises(ValueError):
                self.check()
            self.rows[0][key] = old

    def test_low_sram_excludes_borrowed_stack(self):
        for row in self.rows:
            row["low_sram"] = {
                k: row[k]
                for k in ("writes", "minimum", "maximum", "minimum_pc", "maximum_pc")
            }
            row["maximum"] = 0x08007000
            row["writes"] += 100
        result = self.check()
        self.assertTrue(
            all(x["low_sram"]["writes"] == 2 for x in result["initializers"])
        )
        original = self.rows[0]["low_sram"]
        for bad in (
            None,
            {},
            dict(original, minimum=0x07FFFFFF),
            dict(original, maximum=0x08001000),
            dict(original, writes=103),
            dict(original, writes=True),
            dict(original, minimum_pc=-1),
            dict(original, maximum=original["maximum"] - 1),
        ):
            self.rows[0]["low_sram"] = bad
            with self.subTest(bad=bad), self.assertRaises(ValueError):
                self.check()
        self.rows[0]["low_sram"] = original

    def test_other_regions_remain_visible(self):
        self.rows.append(
            dict(
                version=1,
                mode=18,
                region=19,
                writes=1,
                minimum=0x1300000,
                maximum=0x1300000,
                minimum_pc=123,
                maximum_pc=123,
            )
        )
        self.assertEqual(len(self.check()["observations"]), 6)


if __name__ == "__main__":
    unittest.main()
