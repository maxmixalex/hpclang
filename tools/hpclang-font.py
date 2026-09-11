#!/usr/bin/env python3
from __future__ import annotations
import argparse, hashlib, io, json, os, re, struct, sys, zipfile
from dataclasses import dataclass
from pathlib import Path

@dataclass
class Glyph:
    codepoint:int; width:int; height:int; advance:int
    x_offset:int=0; y_offset:int=0; bitmap:bytes=b""; stride:int=0

@dataclass
class Font:
    source:str; glyphs:dict[int,Glyph]; ascent:int; descent:int
    line_height:int; nominal_advance:int; format:str; face:str=""

def _num(s:str)->int:
    s=s.strip()
    if len(s)>=2 and s[0]==s[-1]=='"': s=s[1:-1]
    return int(s,0)

class Package:
    def __init__(self,path):
        self.path=Path(path); self.z=zipfile.ZipFile(path)
        self.manifest=json.loads(self.z.read("fonts/manifest.json"))
        self.files={e["path"]:e for e in self.manifest["files"]}
    def data(self,path): return self.z.read("fonts/"+path)
    def fonts(self): return [e for e in self.manifest["files"] if e.get("is_font")]
    def verify(self):
        bad=[]
        for e in self.manifest["files"]:
            b=self.data(e["path"])
            if len(b)!=e["bytes"] or hashlib.sha256(b).hexdigest()!=e["sha256"]:
                bad.append(e["path"])
        return bad

def parse_bdf(data:bytes, source:str)->Font:
    lines=data.decode("latin-1").splitlines()
    ascent=descent=None; fbb=None; glyphs={}
    i=0
    while i<len(lines):
        line=lines[i].strip()
        if line.startswith("FONTBOUNDINGBOX "):
            p=line.split()
            try: fbb=tuple(_num(x) for x in p[1:5])
            except: pass
        elif line.startswith("FONT_ASCENT "):
            try: ascent=_num(line.split(None,1)[1])
            except: pass
        elif line.startswith("FONT_DESCENT "):
            try: descent=_num(line.split(None,1)[1])
            except: pass
        elif line.startswith("STARTCHAR"):
            enc=None; adv=None; bbx=None; bitmap=[]
            i+=1
            while i<len(lines):
                s=lines[i].strip()
                if s.startswith("ENCODING "):
                    try:
                        vals=s.split()
                        enc=int(vals[1])
                        if enc<0 and len(vals)>2: enc=int(vals[2])
                    except: enc=None
                elif s.startswith("DWIDTH "):
                    try: adv=int(s.split()[1])
                    except: pass
                elif s.startswith("BBX "):
                    try: bbx=tuple(int(x) for x in s.split()[1:5])
                    except: pass
                elif s=="BITMAP":
                    i+=1
                    while i<len(lines) and lines[i].strip()!="ENDCHAR":
                        bitmap.append(lines[i].strip()); i+=1
                    break
                elif s=="ENDCHAR": break
                i+=1
            if enc is not None and 0<=enc<=0x10ffff and bbx:
                w,h,xoff,yoff=bbx
                stride=(max(w,0)+7)//8
                rows=bytearray()
                for row in bitmap[:max(h,0)]:
                    try:
                        rb=bytes.fromhex(row)
                    except: rb=b""
                    rows += rb[:stride].ljust(stride,b"\0")
                rows += b"\0"*max(0,h-len(bitmap))*stride
                glyphs[enc]=Glyph(enc,w,h,adv if adv is not None else w,xoff,yoff,bytes(rows),stride)
        i+=1
    if fbb:
        _,fh,_,fy=fbb
        if ascent is None: ascent=max(0,fh+fy)
        if descent is None: descent=max(0,-fy)
    if ascent is None: ascent=max((g.height+g.y_offset for g in glyphs.values()),default=0)
    if descent is None: descent=max((max(0,-g.y_offset) for g in glyphs.values()),default=0)
    line=max(1,ascent+descent)
    nominal=round(sum(g.advance for g in glyphs.values())/len(glyphs)) if glyphs else 0
    return Font(source,glyphs,ascent,descent,line,nominal,"bdf")

def bmp8(data:bytes):
    if data[:2]!=b"BM": raise ValueError("not BMP")
    off=struct.unpack_from("<I",data,10)[0]
    dib=struct.unpack_from("<I",data,14)[0]
    w=struct.unpack_from("<i",data,18)[0]; hs=struct.unpack_from("<i",data,22)[0]
    planes,bpp=struct.unpack_from("<HH",data,26)
    comp=struct.unpack_from("<I",data,30)[0]
    if planes!=1 or bpp!=8 or comp!=0: raise ValueError("newRPL BMP must be uncompressed 8-bit")
    h=abs(hs); topdown=hs<0; stride=((w+3)//4)*4
    pal_start=14+dib
    palette=[struct.unpack_from("<I",data,pal_start+i*4)[0] for i in range(256)]
    rows=[]
    for logical_y in range(h):
        file_y=logical_y if topdown else h-1-logical_y
        start=off+file_y*stride
        rows.append(data[start:start+w])
    marker=rows[-1]
    bitmap_rows=rows[:-1]
    return w,h-1,palette,marker,bitmap_rows

def parse_map(txt:bytes):
    cp_to_index={}; index_codes=[]; used=0
    for raw in txt.decode("latin-1").splitlines():
        line=raw.split("/",1)[0].strip()
        if not line: continue
        vals=[]
        for tok in line.split(","):
            tok=tok.strip()
            if not tok: continue
            try: cp=int(tok,0)
            except: continue
            if 0<=cp<=0x10ffff: vals.append(cp)
        if vals:
            index_codes.append(vals)
            for cp in vals: cp_to_index[cp]=used
            used+=1
    return cp_to_index,index_codes

def parse_newrpl_bmp(pkg:Package,path:str)->Font:
    w,h,palette,marker,rows=bmp8(pkg.data(path))
    offsets=[]; widths=[]; prev=None
    for x,v in enumerate(marker):
        if x==0 or v!=prev:
            if offsets: widths.append(x-offsets[-1])
            offsets.append(x); prev=v
    if offsets: widths.append(w-offsets[-1])
    txtpath=os.path.splitext(path)[0]+".txt"
    cp_to_index,index_codes=parse_map(pkg.data(txtpath))
    if cp_to_index and max(cp_to_index.values())>=len(offsets):
        raise ValueError(f"{path}: mapping references glyph beyond {len(offsets)} bitmap runs")
    glyphs={}
    for cp,idx in cp_to_index.items():
        x0=offsets[idx]; gw=widths[idx]; stride=(gw+7)//8
        bits=bytearray(stride*h)
        for yy,row in enumerate(rows):
            for xx in range(gw):
                color=palette[row[x0+xx]]
                if color==0:
                    bits[yy*stride+(xx>>3)] |= 0x80>>(xx&7)
        glyphs[cp]=Glyph(cp,gw,h,gw,0,0,bytes(bits),stride)
    nominal=round(sum(g.advance for g in glyphs.values())/len(glyphs)) if glyphs else 0
    return Font(path,glyphs,h,0,h,nominal,"bmp")

def _pil_font(data:bytes,size:int,index:int=0):
    from PIL import ImageFont
    return ImageFont.truetype(io.BytesIO(data),size=size,index=index)

def _ttf_codepoints(data:bytes):
    from fontTools.ttLib import TTFont
    tt=TTFont(io.BytesIO(data),fontNumber=0,lazy=True)
    cps=set()
    for t in tt["cmap"].tables:
        if t.isUnicode(): cps.update(t.cmap.keys())
    return sorted(cp for cp in cps if 0<=cp<=0x10ffff)

def raster_pil(data:bytes, source:str, size:int=12, index:int=0, codepoints=None, fmt="ttf", face="")->Font:
    f=_pil_font(data,size,index)
    asc,desc=f.getmetrics()
    if codepoints is None:
        codepoints=_ttf_codepoints(data) if fmt=="ttf" else range(256)
    glyphs={}
    for cp in codepoints:
        ch=chr(cp)
        try:
            mask,offset=f.getmask2(ch,mode="1")
            bbox=f.getbbox(ch)
            adv=round(f.getlength(ch))
        except Exception:
            continue
        if bbox is None:
            glyphs[cp]=Glyph(cp,0,0,max(0,adv),0,0,b"",0); continue
        x0,y0,x1,y1=bbox; gw=max(0,x1-x0); gh=max(0,y1-y0)
        stride=(gw+7)//8
        raw=bytes(mask)
        bits=bytearray(stride*gh)
        if gw and gh and len(raw)>=gw*gh:
            for y in range(gh):
                for x in range(gw):
                    if raw[y*gw+x]:
                        bits[y*stride+(x>>3)] |= 0x80>>(x&7)
        glyphs[cp]=Glyph(cp,gw,gh,max(0,adv),x0,asc-y1,bytes(bits),stride)
    nominal=round(sum(g.advance for g in glyphs.values())/len(glyphs)) if glyphs else 0
    return Font(source,glyphs,asc,desc,asc+desc,nominal,fmt,face)

def fon_faces(data:bytes, source:str):
    out=[]
    for idx,size in enumerate((8,11,13)):
        out.append(raster_pil(data,source,size=size,index=idx,codepoints=range(256),fmt="fon",face=f"strike-{idx}"))
    return out

ALIASES={
    "@mini":"newrpl/bitmap-masters/Font6_mediumA.bmp",
    "@normal":"newrpl/bitmap-masters/Font8_StyleC.bmp",
    "@large":"newrpl/bitmap-masters/Font10_StyleA.bmp",
}

def split_selector(selector:str):
    selector=ALIASES.get(selector,selector)
    if "#" in selector:
        base,face=selector.rsplit("#",1)
        return base,face
    return selector,None

FON_FACE_SIZES=((0,8,"6x10"),(1,11,"7x13"),(2,13,"8x16"))

def load_selected(pkg:Package, selector:str, mode:str, requested:set[int]|None=None):
    path,face=split_selector(selector)
    ext=os.path.splitext(path)[1].lower()
    data=pkg.data(path)
    if ext in (".bdf",".bmp"):
        full=parse_bdf(data,path) if ext==".bdf" else parse_newrpl_bmp(pkg,path)
        if mode=="FULL":
            return full
        requested=requested or set()
        glyphs={cp:g for cp,g in full.glyphs.items() if cp in requested}
        nominal=round(sum(g.advance for g in glyphs.values())/len(glyphs)) if glyphs else full.nominal_advance
        return Font(path,glyphs,full.ascent,full.descent,full.line_height,nominal,full.format,full.face)
    if ext==".ttf":
        cps=_ttf_codepoints(data)
        chosen=cps if mode=="FULL" else [cp for cp in cps if cp in (requested or set())]
        return raster_pil(data,path,size=12,codepoints=chosen,fmt="ttf")
    if ext==".fon":
        if face is None:
            idx,size,label=FON_FACE_SIZES[0]
        else:
            match=None
            for item in FON_FACE_SIZES:
                if face in (str(item[0]),item[2]):
                    match=item; break
            if match is None: raise ValueError(f"{path}: unknown FON face {face!r}; use #0/#1/#2 or #6x10/#7x13/#8x16")
            idx,size,label=match
        chosen=range(256) if mode=="FULL" else sorted(cp for cp in (requested or set()) if 0<=cp<=255)
        return raster_pil(data,path,size=size,index=idx,codepoints=chosen,fmt="fon",face=label)
    raise ValueError(f"unsupported font resource: {path}")

def c_ident(name:str):
    ident=re.sub(r"[^A-Za-z0-9_]","_",name)
    if not ident or ident[0].isdigit(): ident="_"+ident
    return ident

def emit_c(font:Font, symbol:str, out):
    symbol=c_ident(symbol)
    glyphs=sorted(font.glyphs.values(), key=lambda g:g.codepoint)
    print('#include <stdint.h>',file=out)
    print('#include <hpclang/font-package.h>',file=out)
    print('',file=out)
    ptrs=[]
    for n,g in enumerate(glyphs):
        bn=f"{symbol}_bitmap_{n}"
        gn=f"{symbol}_glyph_{n}"
        data=", ".join(f"0x{b:02x}" for b in g.bitmap) or "0"
        print(f'static const uint8_t {bn}[] = {{ {data} }};',file=out)
        print(f'static const struct hpclang_bitmap_glyph {gn} = {{',file=out)
        print(f'    0x{g.codepoint:08x}u, {bn}, {g.x_offset}, {g.y_offset},',file=out)
        print(f'    {g.width}u, {g.height}u, {g.advance}u, {g.stride}u',file=out)
        print('};',file=out)
        ptrs.append("&"+gn)
    print('',file=out)
    print(f'static const struct hpclang_bitmap_glyph *const {symbol}_glyphs[] = {{',file=out)
    for p in ptrs: print(f'    {p},',file=out)
    print('};',file=out)
    fallback=0x3f if 0x3f in font.glyphs else (glyphs[0].codepoint if glyphs else 0)
    print(f'static const struct hpclang_bitmap_font_data {symbol}_data = {{',file=out)
    print(f'    {symbol}_glyphs, {len(glyphs)}u, 0x{fallback:08x}u,',file=out)
    print(f'    {font.ascent}, {font.descent}, {font.line_height}u, {font.nominal_advance}u',file=out)
    print('};',file=out)
    print(f'const struct hpclang_font {symbol} = HPCLANG_BITMAP_FONT({symbol}_data);',file=out)

def catalog(pkg:Package):
    rows=[]
    for e in pkg.fonts():
        p=e["path"]; ext=os.path.splitext(p)[1].lower()[1:]
        if ext=="fon":
            for idx,_,label in FON_FACE_SIZES:
                rows.append({"selector":f"{p}#{idx}","path":p,"format":ext,"face":label,"family":e.get("family")})
        else:
            rows.append({"selector":p,"path":p,"format":ext,"face":None,"family":e.get("family")})
    return rows

def main():
    ap=argparse.ArgumentParser(description="Compile hpclang bitmap fonts directly from the canonical source ZIP.")
    ap.add_argument("--package",default="sdk/fonts/fonts.zip")
    sub=ap.add_subparsers(dest="cmd",required=True)
    sub.add_parser("verify")
    sub.add_parser("audit")
    sub.add_parser("catalog")
    p=sub.add_parser("inspect"); p.add_argument("font")
    c=sub.add_parser("compile")
    c.add_argument("font")
    c.add_argument("--mode",choices=("STATIC","FULL"),default="STATIC")
    c.add_argument("--symbol",required=True)
    c.add_argument("--text",action="append",default=[])
    c.add_argument("--charset-file",action="append",default=[])
    c.add_argument("--output",required=True)
    a=ap.parse_args()
    pkg=Package(a.package)
    if a.cmd=="verify":
        bad=pkg.verify()
        if bad:
            print("\n".join(bad),file=sys.stderr); return 1
        print(f"PASS: {len(pkg.manifest['files'])} manifest files verified; {len(pkg.fonts())} font resources")
    elif a.cmd=="audit":
        failed=[]; faces=0
        for e in pkg.fonts():
            path=e["path"]; ext=os.path.splitext(path)[1].lower()
            try:
                data=pkg.data(path)
                if ext==".bdf":
                    f=parse_bdf(data,path); faces+=1
                    if not f.glyphs: raise ValueError("no glyphs")
                elif ext==".bmp":
                    f=parse_newrpl_bmp(pkg,path); faces+=1
                    if not f.glyphs: raise ValueError("no glyphs")
                elif ext==".ttf":
                    cps=_ttf_codepoints(data)
                    if not cps: raise ValueError("empty Unicode cmap")
                    cpset=set(cps)
                    sample=[cp for cp in (0x20,0x3f,0x41,0x61) if cp in cpset] or cps[:1]
                    f=raster_pil(data,path,size=12,codepoints=sample,fmt="ttf"); faces+=1
                    if not f.glyphs: raise ValueError("no sample glyphs")
                elif ext==".fon":
                    for idx,size,label in FON_FACE_SIZES:
                        f=raster_pil(data,path,size=size,index=idx,codepoints=(0x20,0x3f,0x41),fmt="fon",face=label)
                        if not f.glyphs: raise ValueError(f"strike {idx} has no sample glyphs")
                        faces+=1
                else:
                    raise ValueError("unsupported extension")
            except Exception as ex:
                failed.append((path,repr(ex)))
        for p,e in failed: print(f"FAIL {p}: {e}")
        print(f"resources={len(pkg.fonts())} faces={faces} passed={len(pkg.fonts())-len(failed)} failed={len(failed)}")
        return bool(failed)
    elif a.cmd=="catalog":
        json.dump({"resources":len(pkg.fonts()),"faces":catalog(pkg),"aliases":ALIASES},sys.stdout,ensure_ascii=False,indent=2)
        print()
    elif a.cmd=="inspect":
        path,face=split_selector(a.font)
        ext=os.path.splitext(path)[1].lower()
        if ext==".fon" and face is None:
            fonts=fon_faces(pkg.data(path),path)
        else:
            f=load_selected(pkg,a.font,"FULL")
            fonts=[f]
        for f in fonts:
            print(f"{f.source} {f.face} fmt={f.format} glyphs={len(f.glyphs)} metrics={f.ascent}/{f.descent}/{f.line_height} nominal={f.nominal_advance}")
    elif a.cmd=="compile":
        requested=set()
        for text in a.text:
            requested.update(ord(ch) for ch in text)
        for fp in a.charset_file:
            requested.update(ord(ch) for ch in Path(fp).read_text(encoding="utf-8"))
        if a.mode=="STATIC" and not requested:
            ap.error("STATIC compile requires --text or --charset-file")
        font=load_selected(pkg,a.font,a.mode,requested)
        if not font.glyphs:
            raise SystemExit(f"no requested glyphs are present in {a.font}")
        outp=Path(a.output); outp.parent.mkdir(parents=True,exist_ok=True)
        with outp.open("w",encoding="utf-8") as out:
            emit_c(font,a.symbol,out)
        print(f"{outp}: {a.font} {a.mode} -> {len(font.glyphs)} glyphs")
    return 0

if __name__=="__main__":
    raise SystemExit(main())
