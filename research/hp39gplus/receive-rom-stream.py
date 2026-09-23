#!/usr/bin/env python3
"""Receive a one-launch HP39g+ 1 MiB physical ROM stream over Raw HID."""
from __future__ import annotations
import argparse, hashlib, json, struct, sys, time
from pathlib import Path

VID=0x03F0; PID=0x0121
VERSION=1; FLASH_BYTES=0x100000; BLOCK_BYTES=0x4000; DATA_BYTES=56
FNV_OFFSET=2166136261; FNV_PRIME=16777619


def u32(b: bytes|bytearray, off: int) -> int: return struct.unpack_from("<I",b,off)[0]
def put32(b: bytearray, off: int, v: int) -> None: struct.pack_into("<I",b,off,v)

def fnv1a(data: bytes) -> int:
    h=FNV_OFFSET
    for x in data: h=((h^x)*FNV_PRIME)&0xffffffff
    return h

def block_sum(data: bytes) -> int: return sum(data)&0xffffffff

def command(cmd: str, block: int=0, offset: int=0) -> bytes:
    b=bytearray(64); b[0]=ord(cmd); b[1]=VERSION; b[2:5]=b"H39"; b[5]=block&0xff; put32(b,8,offset); return bytes(b)

def write_report(dev, payload: bytes) -> None:
    if len(payload)!=64: raise ValueError("report must be 64 bytes")
    n=dev.write(b"\x00"+payload)
    if n not in (64,65): raise RuntimeError(f"short HID write: {n}")

def read_report(dev, timeout_ms: int=5000) -> bytes:
    data=bytes(dev.read(64,timeout_ms))
    if len(data)!=64: raise TimeoutError(f"expected 64-byte HID report, got {len(data)}")
    return data

def open_streamer(hid, timeout: float):
    deadline=time.monotonic()+timeout
    last=[]
    while time.monotonic()<deadline:
        for info in hid.enumerate(VID,PID):
            product=(info.get("product_string") or "")
            last.append(product)
            if "ROM Stream" not in product:
                continue
            d=hid.device(); d.open_path(info["path"]); d.set_nonblocking(False); return d, info
        time.sleep(0.25)
    raise RuntimeError("HP39g+ ROM Stream HID device not found. Close Conn3x, launch the aplet with USB unplugged, then plug the cable in.")

def main() -> None:
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument("output",type=Path,nargs="?",default=Path("hp39gplus-flash.bin"))
    ap.add_argument("--wait",type=float,default=45.0,help="seconds to wait for the streamer to enumerate")
    args=ap.parse_args()
    try: import hid
    except ImportError as e:
        raise SystemExit("Python package 'hidapi' is required: python -m pip install hidapi") from e

    print("Waiting for HP39g+ ROM Stream...",flush=True)
    dev,info=open_streamer(hid,args.wait)
    try:
        print(f"Connected: {info.get('product_string') or 'HP39g+ ROM Stream'}",flush=True)
        write_report(dev,command('?'))
        info_pkt=read_report(dev)
        if info_pkt[:8]!=b"I\x01H39ROM": raise RuntimeError(f"unexpected INFO: {info_pkt[:16]!r}")
        flash_bytes=u32(info_pkt,8); block_bytes=u32(info_pkt,12); data_bytes=u32(info_pkt,16)
        if (flash_bytes,block_bytes,data_bytes)!=(FLASH_BYTES,BLOCK_BYTES,DATA_BYTES):
            raise RuntimeError(f"protocol geometry mismatch: {flash_bytes:#x}/{block_bytes:#x}/{data_bytes}")

        image=bytearray(FLASH_BYTES); total_sum=0
        write_report(dev,command('S'))
        blocks=FLASH_BYTES//BLOCK_BYTES
        for block in range(blocks):
            base=block*BLOCK_BYTES; received=bytearray(); expected_seq=0; bad=False
            while True:
                p=read_report(dev)
                kind=p[0]
                if kind==ord('D'):
                    n=p[1]; seq=p[2]|(p[3]<<8); off=u32(p,4)
                    if not 1<=n<=DATA_BYTES:
                        bad=True
                        continue
                    if bad:
                        continue
                    if seq!=expected_seq or off!=base+len(received):
                        bad=True
                        print(f"Block {block:02X}: sequence gap; will retry at checkpoint",flush=True)
                        continue
                    received += p[8:8+n]; expected_seq+=1
                    if len(received)>BLOCK_BYTES:
                        bad=True
                    continue
                if kind==ord('C'):
                    cblock=p[2]; coff=u32(p,4); clen=u32(p,8); csum=u32(p,12); cfnv=u32(p,16)
                    ok=(not bad and cblock==block and coff==base and clen==BLOCK_BYTES and len(received)==BLOCK_BYTES and block_sum(received)==csum and fnv1a(received)==cfnv)
                    if not ok:
                        print(f"Block {block:02X}: validation failed; requesting automatic retry",flush=True)
                        write_report(dev,command('R',block,base)); received.clear(); expected_seq=0; bad=False
                        continue
                    image[base:base+BLOCK_BYTES]=received; total_sum=(total_sum+csum)&0xffffffff
                    write_report(dev,command('A',block,base))
                    print(f"Block {block+1:02d}/{blocks}: OK",flush=True)
                    break
                raise RuntimeError(f"unexpected report type 0x{kind:02x} in block {block}")

        end=read_report(dev)
        if end[:8]!=b"E\x01H39ROM": raise RuntimeError(f"unexpected END: {end[:16]!r}")
        if u32(end,8)!=FLASH_BYTES or u32(end,12)!=total_sum or u32(end,16)!=0x50415353:
            raise RuntimeError("END summary mismatch")

        digest=hashlib.sha256(image).hexdigest()
        args.output.write_bytes(image)
        manifest={
            "protocol":"H39ROM-HID/1","bytes":len(image),"sha256":digest,
            "byte_sum32":f"0x{total_sum:08x}","vid":"03f0","pid":"0121",
        }
        manifest_path=args.output.with_suffix(args.output.suffix+".json")
        manifest_path.write_text(json.dumps(manifest,indent=2)+"\n",encoding="utf-8")
        print(f"ROM saved: {args.output} ({len(image)} bytes)")
        print(f"SHA-256: {digest}")
        write_report(dev,command('F'))
        print("Dump complete. Unplug the calculator USB cable now; the aplet will then restore hardware state and return to the stock OS.",flush=True)
    except BaseException:
        try: write_report(dev,command('X'))
        except Exception: pass
        print("Receiver aborted. Unplug the calculator USB cable now so the aplet can restore hardware state and return to the stock OS.", file=sys.stderr, flush=True)
        raise
    finally:
        dev.close()

if __name__=="__main__": main()
