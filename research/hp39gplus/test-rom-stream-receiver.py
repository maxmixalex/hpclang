#!/usr/bin/env python3
"""Offline protocol simulation for receive-rom-stream.py.

Feeds a deterministic 1 MiB fake device stream and deliberately corrupts one
packet on the first pass through block 3.  The receiver must request a retry and
produce the exact expected image.
"""
from __future__ import annotations

import hashlib
import os
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent
RECEIVER = ROOT / "receive-rom-stream.py"

FAKE_HID = r'''
import struct
VID=0x03f0; PID=0x0121; FLASH=0x100000; BLOCK=0x4000; DATA=56
FNV_OFFSET=2166136261; FNV_PRIME=16777619

def _u32(b,o): return struct.unpack_from('<I',b,o)[0]
def _p32(b,o,v): struct.pack_into('<I',b,o,v)
def _byte(off): return (off*17+3)&0xff
def _block_bytes(block):
    base=block*BLOCK
    return bytes(_byte(base+i) for i in range(BLOCK))
def _sum(d): return sum(d)&0xffffffff
def _fnv(d):
    h=FNV_OFFSET
    for x in d: h=((h^x)*FNV_PRIME)&0xffffffff
    return h

def enumerate(vid,pid):
    return [{"product_string":"HP39g+ ROM Stream","path":b"fake"}] if (vid,pid)==(VID,PID) else []

class device:
    def __init__(self):
        self.block=0; self.pos=0; self.mode='idle'; self.retry_done=False
    def open_path(self,path): pass
    def set_nonblocking(self,v): pass
    def close(self): pass
    def write(self,raw):
        p=bytes(raw[1:] if len(raw)==65 else raw); cmd=chr(p[0])
        if cmd=='?': self.mode='info'
        elif cmd=='S': self.block=0; self.pos=0; self.mode='data'
        elif cmd=='A':
            assert p[5]==self.block and _u32(p,8)==self.block*BLOCK
            self.block+=1; self.pos=0; self.mode='end' if self.block==FLASH//BLOCK else 'data'
        elif cmd=='R':
            assert p[5]==self.block
            self.pos=0; self.mode='data'; self.retry_done=True
        elif cmd=='F': self.mode='finished'
        elif cmd=='X': self.mode='aborted'
        return len(raw)
    def read(self,n,timeout_ms):
        b=bytearray(64)
        if self.mode=='info':
            b[:8]=b'I\x01H39ROM'; _p32(b,8,FLASH); _p32(b,12,BLOCK); _p32(b,16,DATA); self.mode='idle'; return list(b)
        if self.mode=='data':
            if self.pos>=BLOCK:
                self.mode='checkpoint'; return self.read(n,timeout_ms)
            count=min(DATA,BLOCK-self.pos); off=self.block*BLOCK+self.pos
            b[0]=ord('D'); b[1]=count; struct.pack_into('<H',b,2,self.pos//DATA); _p32(b,4,off)
            payload=bytearray(_byte(off+i) for i in range(count))
            if self.block==3 and not self.retry_done and self.pos==56*10:
                payload[0]^=0x55
            b[8:8+count]=payload; self.pos+=count
            if self.pos>=BLOCK: self.mode='checkpoint'
            return list(b)
        if self.mode=='checkpoint':
            d=_block_bytes(self.block); base=self.block*BLOCK
            b[0]=ord('C'); b[1]=1; b[2]=self.block; _p32(b,4,base); _p32(b,8,BLOCK); _p32(b,12,_sum(d)); _p32(b,16,_fnv(d))
            self.mode='await_ack'; return list(b)
        if self.mode=='end':
            b[:8]=b'E\x01H39ROM'; _p32(b,8,FLASH)
            total=sum(_sum(_block_bytes(i)) for i in range(FLASH//BLOCK))&0xffffffff
            _p32(b,12,total); _p32(b,16,0x50415353); self.mode='await_finish'; return list(b)
        raise RuntimeError(f'fake device read in state {self.mode}')
'''


def main() -> None:
    with tempfile.TemporaryDirectory() as td_s:
        td = Path(td_s)
        fake = td / "hid"
        fake.mkdir()
        (fake / "__init__.py").write_text(FAKE_HID, encoding="utf-8")
        out = td / "rom.bin"
        env = dict(os.environ)
        env["PYTHONPATH"] = str(td)
        result = subprocess.run(
            [sys.executable, str(RECEIVER), str(out)],
            env=env,
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            check=True,
        )
        expected = bytes(((i * 17 + 3) & 0xff) for i in range(0x100000))
        actual = out.read_bytes()
        if actual != expected:
            raise SystemExit("receiver produced the wrong 1 MiB image")
        if "validation failed; requesting automatic retry" not in result.stdout:
            raise SystemExit("receiver did not exercise the automatic retry path")
        print(
            "receiver simulation PASS: "
            f"bytes={len(actual)} sha256={hashlib.sha256(actual).hexdigest()} retry=PASS"
        )


if __name__ == "__main__":
    main()
