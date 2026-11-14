"""Positive and corruption checks for an independent-entry snapshot."""

from pathlib import Path
import importlib.util
import json
import tempfile
import argparse

spec = importlib.util.spec_from_file_location("verify", Path(__file__).with_name("verify.py"))
m = importlib.util.module_from_spec(spec)
spec.loader.exec_module(m)
parser = argparse.ArgumentParser(description="Verify a real snapshot and reject corrupted copies.")
for name in ("state", "artifact", "baseline", "report"):
    parser.add_argument("--" + name, type=Path, required=True)
args = parser.parse_args()
state, artifact, baseline = args.state, args.artifact, args.baseline
result = m.verify(artifact, state, baseline)
meta = json.loads((artifact / "metadata.json").read_text())
start = result["payload_physical"] - 0x08000000
symbols = meta["symbols"]
cases = {
    "caller-control": ("sram", start + symbols["control"]),
    "c-control": ("sram", start + symbols["control"] + 4),
    "restored-control": ("sram", start + symbols["control"] + 8),
    "restored-vector": ("sram", start + symbols["control"] + 16),
    "workspace-value": ("sram", start + symbols["workspace"] + 24),
    "stack-pointer": ("sram", start + symbols["workspace"] + 12),
    "low-guard": ("sram", start + symbols["guard_low"]),
    "high-guard": ("sram", start + symbols["guard_high"]),
    "code": ("sram", start),
    "flash": ("flash", 0x180000),
}
rejected = {}
with tempfile.TemporaryDirectory(prefix="hpclang-entry-negative-") as temp:
    target = Path(temp)
    for name, (file, offset) in cases.items():
        for f in ("sram", "flash"):
            (target / f).write_bytes((state / f).read_bytes())
        data = bytearray((target / file).read_bytes())
        data[offset] ^= 1
        (target / file).write_bytes(data)
        try:
            m.verify(artifact, target, baseline)
        except ValueError as e:
            rejected[name] = str(e)
        else:
            raise RuntimeError("accepted " + name)
args.report.write_text(json.dumps(rejected, indent=2) + "\n")
print(json.dumps(rejected))
