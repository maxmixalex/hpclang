"""Original PrRUN/helper, bounded stage0 entry or rejection, and return checks."""

import argparse
import ctypes as c
import subprocess
import hashlib
import importlib
import json
from pathlib import Path
import struct
import sys
from unicorn import (
    Uc,
    UC_ARCH_ARM,
    UC_MODE_ARM,
    UC_HOOK_CODE,
    UC_HOOK_INTR,
    UC_HOOK_MEM_READ,
    UC_HOOK_MEM_WRITE,
    UC_MEM_READ,
)
from unicorn.arm_const import (
    UC_ARM_REG_R0,
    UC_ARM_REG_R1,
    UC_ARM_REG_R2,
    UC_ARM_REG_R3,
    UC_ARM_REG_R4,
    UC_ARM_REG_R5,
    UC_ARM_REG_R7,
    UC_ARM_REG_SP,
    UC_ARM_REG_LR,
    UC_ARM_REG_PC,
    UC_ARM_REG_CPSR,
    UC_ARM_REG_SPSR,
)

if not __debug__:
    raise RuntimeError("Assertions required")
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--model", choices=("hp50g", "hp49gplus"), required=True)
parser.add_argument("--state", type=Path, required=True)
parser.add_argument("--launcher", type=Path, required=True)
parser.add_argument("--output", type=Path, required=True)
parser.add_argument("--relocate", action="store_true")
parser.add_argument(
    "--execute-helper",
    action="store_true",
    help="Execute the ROM allocation helper and real SVC handler up to stage0",
)
parser.add_argument("--cc", default="cc")
parser.add_argument(
    "--reject-stage0-pool",
    type=int,
    choices=(0, 1, 257),
    help="Execute stage0 rejection/free/return with an injected invalid page count",
)
parser.add_argument(
    "--stage0-entry",
    type=Path,
    metavar="STAGE1_BIN",
    help="Execute stage0 to stage1 entry with modeled page aliases and verify the supplied image",
)
args = parser.parse_args()
if args.stage0_entry:
    if args.relocate or args.reject_stage0_pool is not None:
        parser.error("Stage0 entry cannot be combined with relocation or rejection")
    args.execute_helper = True
if args.reject_stage0_pool is not None:
    if args.relocate:
        parser.error("Stage0 rejection cannot perform an application-side relocation")
    args.execute_helper = True
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / "tests/e2e/port-recovery"))
native_words = importlib.import_module("verify").native_words
out = args.output.resolve()
out.mkdir(parents=True, exist_ok=True)
report = out / "prrun.json"
report.unlink(missing_ok=True)
flash = (args.state / "flash").read_bytes()
ram = (args.state / "sram").read_bytes()
if len(flash) != 0x200000 or len(ram) != 0x80000:
    raise ValueError("Expected complete SRAM/Flash")
vector_entry = None
if args.execute_helper:
    if hashlib.sha256(flash[0x1596FC:0x159894]).hexdigest() != (
        "5b86773a66d1ea9b69b9662eed37f47d181b0283c22d79d263af3e2a582d0256"
    ):
        raise ValueError("Allocation helper differs from reviewed ROM code")
    vector = int.from_bytes(flash[8:12], "little")
    assert vector >> 24 == 0xEA
    offset = vector & 0xFFFFFF
    if offset & 0x800000:
        offset -= 0x1000000
    vector_entry = 16 + 4 * offset
    assert 0 <= vector_entry < len(flash)
    assert int.from_bytes(ram[8:12], "little") == 0x3F568
sources = {}
if args.relocate:
    base = root / "runtime/core"
    stage = root / "platform/armtoolbox/stage-stack.c"
    shim = Path(__file__).with_name("threaded-return.c")
    files = [
        stage,
        shim,
        *[
            base / name
            for name in (
                "rpl-mutation.c",
                "rpl-object-store.c",
                "rpl-stack-view.c",
                "object-snapshot.c",
                "rpl-object-span.c",
            )
        ],
    ]
    sources = {str(path): hashlib.sha256(path.read_bytes()).hexdigest() for path in files}
    subprocess.run(
        [
            args.cc,
            "-shared",
            "-fPIC",
            "-std=c11",
            "-Wall",
            "-Wextra",
            "-Werror",
            "-I" + str(base),
            "-I" + str(stage.parent),
            "-I" + str(root / "include"),
            *map(str, files),
            "-o",
            str(out / "geometry.so"),
        ],
        check=True,
    )
    lib = c.CDLL(str(out / "geometry.so"))
    lib.bank_catalog_grow.argtypes = [
        c.POINTER(c.c_uint8),
        c.POINTER(c.c_uint32),
        c.POINTER(c.c_uint32),
        c.c_uint32,
        c.c_uint,
        c.c_uint,
        c.POINTER(c.c_uint32),
    ]
launcher = args.launcher.read_bytes()
if (
    len(launcher) < 32
    or launcher[:8] != b"HPHP49-C"
    or int.from_bytes(launcher[-16:-12], "little") != 0x1C00000
    or int.from_bytes(launcher[-4:], "little") != 0x3176B34C
):
    raise ValueError("Expected reviewed MMU launcher")
stage1_image = args.stage0_entry.read_bytes() if args.stage0_entry else None
if stage1_image is not None and (not stage1_image or launcher.count(stage1_image) != 1):
    raise ValueError("Expected one independently supplied stage1 image in launcher")
mapping = native_words(ram, flash)
cache = bytes(mapping(0x877D1 + i, 1) for i in range(0x88D6C - 0x877D1))
if (
    hashlib.sha256(cache).hexdigest()
    != "4b064bc5841a06b3c883e57ef85a3253e74899dc98d4808da8665af9ceb129fa"
):
    raise ValueError("PrRUN/cache layout differs from reviewed input")
assert 0x90000 + (len(launcher) - 8) * 2 < mapping(0x806F8) - 5
cpu = 0x08000000 + (mapping.table - 128) * 4
rows = []
for cursor in range(8):
    uc = Uc(UC_ARCH_ARM, UC_MODE_ARM)
    if args.execute_helper:
        uc.reg_write(UC_ARM_REG_CPSR, 0xD0)  # User mode, IRQ/FIQ initially masked.
    uc.mem_map(0, len(flash))
    uc.mem_write(0, flash)
    if args.stage0_entry:
        ram_storage = (c.c_ubyte * len(ram)).from_buffer_copy(ram)
        uc.mem_map_ptr(0x08000000, len(ram), 7, c.addressof(ram_storage))
    else:
        uc.mem_map(0x08000000, len(ram))
        uc.mem_write(0x08000000, ram)
    uc.mem_map(0x300000, 0x10000)
    syscalls = []
    release_calls = []
    rejection_active = False
    stage0_active = False
    stage0_svcs = []

    def interrupt(_, number, user):
        # Model ARM SVC exception entry only; execute the original vector,
        # kernel dispatch and allocator. Never substitute an allocation result.
        assert args.execute_helper and number == 2
        cpsr = uc.reg_read(UC_ARM_REG_CPSR)
        pc = uc.reg_read(UC_ARM_REG_PC)
        assert cpsr & 0x1F == 0x10
        if rejection_active:
            assert stage0 <= pc - 4 < stage0 + len(launcher) - 20
            assert uc.reg_read(UC_ARM_REG_R0) == 101
            assert uc.reg_read(UC_ARM_REG_R1) == allocation
            release_calls.append(dict(instruction=hex(pc - 4), allocation=hex(allocation)))
        elif stage0_active:
            handler = int.from_bytes(uc.mem_read(0x08000008, 4), "little")
            assert stage0 <= handler < stage0 + launcher.index(stage1_image) - 20
            stage0_svcs.append(dict(instruction=hex(pc - 4), handler=hex(handler), cpsr=cpsr))
        else:
            assert pc == 0x159888
        syscalls.append(uc.reg_read(UC_ARM_REG_R0))
        uc.reg_write(UC_ARM_REG_CPSR, (cpsr & ~0x3F) | 0x93)
        uc.reg_write(UC_ARM_REG_SPSR, cpsr)
        uc.reg_write(UC_ARM_REG_SP, 0x307000)  # Isolated exception scratch stack.
        uc.reg_write(UC_ARM_REG_LR, pc)
        uc.reg_write(UC_ARM_REG_PC, 8)

    if args.execute_helper:
        uc.hook_add(UC_HOOK_INTR, interrupt)

    def put(at, value):
        for i in range(5):
            p = mapping.physical(at + i)
            shift = 4 * ((at + i) & 1)
            old = uc.mem_read(p, 1)[0]
            uc.mem_write(p, bytes([(old & ~(15 << shift)) | ((value >> (4 * i) & 15) << shift)]))

    def word(at):
        return sum(
            ((uc.mem_read(mapping.physical(at + i), 1)[0] >> (4 * ((at + i) & 1))) & 15) << (4 * i)
            for i in range(5)
        )

    put(0x8076B, 0x88D67)
    for off, value in [(0x95C, 0), (0x970, 0), (0xC24, 0), (0x998, cursor)]:
        uc.mem_write(cpu + off, struct.pack("<I", value))
    for i in range(9):
        uc.mem_write(cpu + 0x90C + 8 * i, struct.pack("<Q", 0x123456789AB00000 + i))
    for i in range(8):
        uc.mem_write(cpu + 0x978 + 4 * i, struct.pack("<I", 0xA1000 + 0x101 * i))
    uc.reg_write(UC_ARM_REG_R4, cpu)
    uc.reg_write(UC_ARM_REG_R5, 0xFFFFF)
    uc.reg_write(UC_ARM_REG_R7, 0x2D9D)
    uc.reg_write(UC_ARM_REG_SP, 0x308000)
    for i, v in enumerate(launcher[8:]):
        uc.mem_write(mapping.physical(0x90000 + 2 * i), bytes([v]))
    stack = word(0x806F8) - 5
    put(0x806F8, stack)
    put(0x80E9B, word(0x80E9B) - 1)
    put(stack, 0x90000)
    uc.mem_write(cpu + 0x90C, struct.pack("<Q", 0x877D1))
    uc.mem_write(cpu + 0x958, struct.pack("<I", stack))
    uc.mem_write(cpu + 0x954, struct.pack("<I", 0x88D67))
    uc.mem_write(cpu + 0x924, struct.pack("<Q", word(0x80E9B)))
    uc.mem_write(cpu + 0x914, struct.pack("<Q", word(0x806F3)))
    trace = []
    accesses = []
    depth = 0
    peak = 0
    minimum = 0

    def hook(_, at, size, user):
        if at == 0x28898:
            pc = uc.reg_read(UC_ARM_REG_R7)
            trace.append(pc)
            if pc == 0x266DB or word(pc) == 0xEFB08 or len(trace) > 30000:
                uc.emu_stop()

    def access(_, kind, at, size, value, user):
        global depth, peak, minimum
        if kind != UC_MEM_READ and at == cpu + 0x998:
            step = (value - int.from_bytes(uc.mem_read(at, 4), "little")) & 7
            assert step in (1, 7), step
            depth += 1 if step == 1 else -1
            peak = max(peak, depth)
            minimum = min(minimum, depth)
        if at < cpu + 0x954 and at + size > cpu + 0x92C:
            accesses.append((kind, at - cpu, size))

    entry_hook = uc.hook_add(UC_HOOK_CODE, hook)
    uc.hook_add(UC_HOOK_MEM_READ | UC_HOOK_MEM_WRITE, access)
    uc.emu_start(0x28828, 0x400000, count=3000000)
    assert word(trace[-1]) == 0xEFB08
    entry_trace = trace.copy()
    # The first ARMSYS is a ROM auxiliary call, not the application entry.
    # Prove the actual ARM dispatch target before simulating this helper's return.
    helper = int.from_bytes(uc.mem_read(cpu + 0x91C, 4), "little")
    assert helper == 0x1596FC, hex(helper)
    uc.hook_del(entry_hook)
    dispatch = []

    def dispatch_hook(_, at, size, user):
        dispatch.append(at)
        if at == helper:
            uc.emu_stop()

    dispatch_handle = uc.hook_add(UC_HOOK_CODE, dispatch_hook)
    uc.emu_start(0x28898, 0x400000, count=1000)
    assert dispatch[-1] == helper and 0x30214 in dispatch
    uc.hook_del(dispatch_handle)
    helper_path = []
    pool = None
    if args.execute_helper:
        stage0 = mapping.physical(0x90018)
        assert int.from_bytes(uc.mem_read(cpu + 0x910, 4), "little") == stage0
        # This launcher uses the helper's direct tail-call branch. Its MMU
        # branch and the stage0/application body are outside this check.
        assert int.from_bytes(uc.mem_read(cpu + 0x90C, 4), "little") == 0x1C00000
        old_sp = uc.reg_read(UC_ARM_REG_SP)
        old_lr = uc.reg_read(UC_ARM_REG_LR)
        old_r0 = uc.reg_read(UC_ARM_REG_R0)
        initial_bytes = int.from_bytes(uc.mem_read(cpu + 0x914, 4), "little")
        assert initial_bytes % 4 == 0 and initial_bytes < 1024
        initial_table = bytes(uc.mem_read(0x08021000, initial_bytes))

        def helper_hook(_, at, size, user):
            helper_path.append(at)
            if at == stage0:
                uc.emu_stop()

        handle = uc.hook_add(UC_HOOK_CODE, helper_hook)
        uc.emu_start(helper, 0x400000, count=200000)
        uc.hook_del(handle)
        assert helper_path[-1] == stage0
        assert all(at in helper_path for at in (8, vector_entry, 0x3F568, 0x1597B0, 0x159738))
        assert syscalls == [0xC9, 0xC9, 0xC9, 0xC9, 0x65, 0xC9]
        assert uc.reg_read(UC_ARM_REG_SP) == old_sp
        assert uc.reg_read(UC_ARM_REG_LR) == old_lr == 0x2A928
        assert uc.reg_read(UC_ARM_REG_R0) == old_r0 == trace[-1] + 5
        assert uc.reg_read(UC_ARM_REG_CPSR) & 0x1F == 0x10
        pages, allocation, _, table = struct.unpack("<4I", uc.mem_read(cpu + 0x914, 16))
        assert 0 < pages <= 256 and allocation and table == 0x08021000
        descriptors = list(struct.unpack("<" + "I" * pages, uc.mem_read(table, 4 * pages)))
        assert all(value & 0xFFF == 0xFFE for value in descriptors)
        assert bytes(uc.mem_read(table, initial_bytes)) == initial_table
        added = descriptors[initial_bytes // 4 :]
        assert added and added[0] == ((allocation + 4095) & ~4095) | 0xFFE
        assert all(b == a + 4096 for a, b in zip(added, added[1:]))
        assert len(set(descriptors)) == len(descriptors)
        assert int.from_bytes(uc.mem_read(table + 4 * pages, 4), "little") == 0
        pool = dict(
            pages=pages,
            initial_pages=initial_bytes // 4,
            allocation=hex(allocation),
            table=hex(table),
            descriptors=[hex(value) for value in descriptors],
        )
    if args.stage0_entry:
        # Unicorn's address aliases model translation only. Actual stage0
        # instructions must publish the expected descriptor, copy/clear the
        # image and establish the ABI before this check can succeed.
        stage0_active = True
        for index, descriptor in enumerate(descriptors):
            physical = descriptor & ~0xFFF
            assert 0x08000000 <= physical <= 0x08080000 - 4096
            uc.mem_map_ptr(
                0x01300000 + index * 4096,
                4096,
                7,
                c.addressof(ram_storage) + physical - 0x08000000,
            )
        uc.mem_map(0x07F00000, 0x4000)
        vector_before = bytes(uc.mem_read(0x08000008, 4))
        incoming_r0 = uc.reg_read(UC_ARM_REG_R0)
        stage_trace = []
        stage_written = bytearray(pages * 4096)

        def stage_write(_, access, address, size, value, user):
            first = max(address, 0x01300000)
            end = min(address + size, 0x01300000 + len(stage_written))
            for at in range(first, end):
                stage_written[at - 0x01300000] = 1

        uc.hook_add(UC_HOOK_MEM_WRITE, stage_write)

        def stage_hook(_, at, size, user):
            stage_trace.append(at)
            if at == 0x01300400:
                uc.emu_stop()

        uc.hook_add(UC_HOOK_CODE, stage_hook)
        uc.emu_start(stage0, 0x400000, count=2000000)
        assert stage_trace[-1] == 0x01300400
        assert len(stage0_svcs) == 3
        assert bytes(uc.mem_read(0x08000008, 4)) == vector_before
        assert int.from_bytes(uc.mem_read(0x07F0004C, 4), "little") == table | 0x11
        assert uc.reg_read(UC_ARM_REG_CPSR) & 0xDF == 0xD0
        assert uc.reg_read(UC_ARM_REG_R0) == cpu
        assert uc.reg_read(UC_ARM_REG_R3) == {"hp49gplus": 5, "hp50g": 6}[args.model]
        extent = uc.reg_read(UC_ARM_REG_R2)
        assert len(stage1_image) <= extent <= pages * 4096 - 0x400 - 4096
        assert bytes(uc.mem_read(0x01300400, len(stage1_image))) == stage1_image
        assert all(stage_written[0x400 : 0x400 + extent]), "Stage1 copy/clear writes omitted"
        tail = extent - len(stage1_image)
        assert not tail or bytes(uc.mem_read(0x01300400 + len(stage1_image), tail)) == bytes(tail)
        stack = uc.reg_read(UC_ARM_REG_SP)
        assert stack == 0x01300000 + pages * 4096 - 40
        parameters = struct.unpack("<7I", uc.mem_read(stack, 28))
        assert parameters[:6] == (
            incoming_r0,
            old_lr,
            stage0_svcs[0]["cpsr"],
            old_sp - 56,
            stage0,
            launcher.index(stage1_image) - 20,
        )
        hpc = uc.reg_read(UC_ARM_REG_R1)
        offset = hpc - stage0 + 20
        assert 20 <= offset < len(launcher) and 0 < parameters[6] <= len(launcher) - offset
        assert bytes(uc.mem_read(hpc, parameters[6])) == launcher[offset : offset + parameters[6]]
        rows.append(
            dict(
                cursor=cursor,
                boundary="stage1-before-body",
                helper_body_executed=True,
                stage0_body_executed=True,
                application_executed=False,
                return_executed=False,
                return_simulated=False,
                translation_mode="modeled-page-aliases",
                allocation_pool=pool,
                stage0_svc_calls=stage0_svcs,
                stage0_instruction_count=len(stage_trace),
                stage1_memory_bytes=extent,
                stage1_written_bytes=sum(stage_written[0x400 : 0x400 + extent]),
                stage1_image_sha256=hashlib.sha256(stage1_image).hexdigest(),
            )
        )
        continue
    uc.hook_add(UC_HOOK_CODE, hook)
    resume = trace[-1] + 5
    writes = c.c_uint32()
    original_resume = resume
    if args.relocate:
        current_cursor = int.from_bytes(uc.mem_read(cpu + 0x998, 4), "little")
        caller_at = cpu + 0x978 + 4 * ((current_cursor + 7) & 7)
        original_caller = int.from_bytes(uc.mem_read(caller_at, 4), "little")
        caller = c.c_uint32(original_caller)
        pc = c.c_uint32(resume)
        original_ip = word(0x8076B)
        nibbles = bytearray(
            n
            for page in range(128, 256)
            for value in uc.mem_read(mapping.physical(page << 12), 2048)
            for n in (value & 15, value >> 4)
        )
        array = (c.c_uint8 * len(nibbles)).from_buffer(nibbles)
        status = lib.bank_catalog_grow(
            array,
            c.byref(caller),
            c.byref(pc),
            0x86500,
            0,
            current_cursor,
            c.byref(writes),
        )
        assert status == 0, (status, writes.value)
        assert (
            writes.value
            and caller.value == original_caller + 16
            and pc.value == original_resume + 16
        )
        for page in range(128, 256):
            first = (page - 128) * 4096
            uc.mem_write(
                mapping.physical(page << 12),
                bytes(nibbles[i] | nibbles[i + 1] << 4 for i in range(first, first + 4096, 2)),
            )
        uc.mem_write(caller_at, struct.pack("<I", caller.value))
        assert word(0x8076B) == original_ip + 16
        resume = pc.value
    saved = bytes(uc.mem_read(cpu + 0x92C, 40))
    trace.clear()
    accesses.clear()

    def stop(_, at, size, user):
        if at == 0x28898 and uc.reg_read(UC_ARM_REG_R7) == 0x3130:
            uc.emu_stop()

    uc.hook_add(UC_HOOK_CODE, stop)
    rejection_trace = []
    if args.reject_stage0_pool is not None:
        rejection_active = True
        uc.mem_write(cpu + 0x914, struct.pack("<I", args.reject_stage0_pool))
        pool_before_rejection = bytes(uc.mem_read(table, (pages + 1) * 4))
        vector_before_rejection = bytes(uc.mem_read(0x08000008, 4))

        def rejection_hook(_, at, size, user):
            rejection_trace.append(at)

        uc.hook_add(UC_HOOK_CODE, rejection_hook)
        # Continue the real stage0, ROM free handler and interpreter return.
        # Neither the return PC nor the allocation result is manufactured.
        uc.emu_start(stage0, 0x400000, count=300000)
        assert len(release_calls) == 1 and syscalls[-1] == 101
        assert all(at in rejection_trace for at in (8, vector_entry, 0x3F568, 0x2A928))
        assert bytes(uc.mem_read(table, (pages + 1) * 4)) == pool_before_rejection
        assert bytes(uc.mem_read(0x08000008, 4)) == vector_before_rejection
        assert uc.reg_read(UC_ARM_REG_SP) == old_sp
        assert uc.reg_read(UC_ARM_REG_CPSR) & 0xDF == 0xD0
    else:
        uc.reg_write(UC_ARM_REG_R7, resume)
        uc.emu_start(0x28828, 0x400000, count=300000)
    assert trace[-1] == 0x3130, trace[-12:]
    assert not accesses and bytes(uc.mem_read(cpu + 0x92C, 40)) == saved
    assert (depth, peak, minimum) == (0, 4, 0)
    assert word(0x806F8) <= word(0x806FD) and (word(0x806FD) - word(0x806F8)) % 5 == 0
    used = {(cursor + i) & 7 for i in range(4)}
    for i in range(8):
        assert int.from_bytes(uc.mem_read(cpu + 0x978 + 4 * i, 4), "little") == (
            0 if i in used else 0xA1000 + 0x101 * i
        )
    rows.append(
        dict(
            cursor=cursor,
            boundary="stage0-before-body"
            if args.execute_helper
            else "first-rom-helper-before-body",
            helper_target=hex(helper),
            arm_dispatch=[hex(at) for at in dispatch],
            helper_body_executed=args.execute_helper,
            helper_instruction_count=len(helper_path),
            svc_requests=syscalls,
            svc_vector_entry=hex(vector_entry) if vector_entry is not None else None,
            allocation_pool=pool,
            rejected_pool_pages=args.reject_stage0_pool,
            stage0_rejection_executed=args.reject_stage0_pool is not None,
            kernel_release_calls=release_calls,
            rejection_instruction_count=len(rejection_trace),
            return_simulated=args.reject_stage0_pool is None,
            application_executed=False,
            original_resume=hex(original_resume),
            resume=hex(resume),
            relocation_writes=writes.value,
            stack_peak=peak,
            stack_final=depth,
            trace=[hex(x) for x in trace],
            entry_trace=[hex(x) for x in entry_trace],
        )
    )
assert all(
    hashlib.sha256(Path(path).read_bytes()).hexdigest() == digest
    for path, digest in sources.items()
)
report.write_text(
    json.dumps(
        dict(
            model=args.model,
            sources=sources,
            inputs={
                "sram": hashlib.sha256(ram).hexdigest(),
                "flash": hashlib.sha256(flash).hexdigest(),
                "launcher": hashlib.sha256(launcher).hexdigest(),
                "driver": hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
                "stage1": hashlib.sha256(stage1_image).hexdigest() if stage1_image else None,
            },
            rows=rows,
        ),
        indent=2,
    )
    + "\n"
)
print(
    args.model,
    "PASS eight PrRUN cursors; real allocation helper"
    if args.execute_helper
    else "PASS eight PrRUN prefix cursors; helper skipped",
    "; stage0 executed to stage1 entry (modeled translation)"
    if args.stage0_entry
    else "; stage0 rejection/free/return executed"
    if args.reject_stage0_pool is not None
    else "; application return simulated",
)
