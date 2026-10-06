"""Build a tiny import-free Windows x64 DLL or API probe using Apple's assembler.

The PE packaging is local; no downloaded DLL or system installation is used.
All instructions and data are position independent. A padding-only relocation
block permits loader relocation without fixing any absolute addresses.
"""
from pathlib import Path
import struct, subprocess, sys
root = Path(__file__).resolve().parent

def build(source, destination, dll=False):
    obj = root / (Path(source).stem + '.obj')
    subprocess.run(['/usr/bin/clang', '-target', 'x86_64-pc-windows-msvc', '-c', str(root/source), '-o', str(obj)], check=True)
    raw=obj.read_bytes()
    machine, count, _, symptr, nsym, optsize, _=struct.unpack_from('<HHIIIHH',raw)
    assert machine==0x8664
    sections=[]
    for i in range(count):
        o=20+optsize+40*i
        name=raw[o:o+8].rstrip(b'\0')
        _,_,size,ptr,_,_,nreloc,_,_=struct.unpack_from('<IIIIIIHHI',raw,o+8)
        if name==b'.text':
            assert nreloc==0, 'Unresolved assembler relocation'
            payload=raw[ptr:ptr+size]; section_index=i+1
    strings=raw[symptr+18*nsym:]
    labels={}; i=0
    while i<nsym:
        o=symptr+18*i; name8=raw[o:o+8]
        if name8[:4]==b'\0'*4:
            start=struct.unpack_from('<I',name8,4)[0]
            name=strings[start:strings.index(b'\0',start)].decode()
        else: name=name8.rstrip(b'\0').decode()
        value, sec, _, _, aux=struct.unpack_from('<IhHBB',raw,o+8)
        if sec==section_index: labels[name]=value
        i+=1+aux
    align=lambda n,a:(n+a-1)//a*a
    imagebase=0x180000000 if dll else 0x140000000
    header=bytearray(0x200); header[:2]=b'MZ'; struct.pack_into('<I',header,0x3c,0x80)
    header[0x80:0x84]=b'PE\0\0'
    struct.pack_into('<HHIIIHH',header,0x84,0x8664,1,0,0,0,0xf0,0x2022 if dll else 0x22)
    op=0x98
    struct.pack_into('<HBBIII',header,op,0x20b,0,0,align(len(payload),0x200),0,0)
    struct.pack_into('<IIQII',header,op+16,0 if dll else 0x1000+labels['entrypoint'],0x1000,imagebase,0x1000,0x200)
    struct.pack_into('<HHHHHHI',header,op+40,6,0,0,0,6,0,0)
    struct.pack_into('<IIIHH',header,op+56,align(0x1000+len(payload),0x1000),0x200,0,3,0x140)
    struct.pack_into('<QQQQII',header,op+72,0x100000,0x1000,0x100000,0x1000,0,16)
    for index,name,end in [(0,'exports','exports_end'),(1,'imports','imports_end'),(5,'relocations','relocations_end')]:
        if name in labels: struct.pack_into('<II',header,op+112+8*index,0x1000+labels[name],labels[end]-labels[name])
    sh=op+0xf0; header[sh:sh+8]=b'.text\0\0\0'
    struct.pack_into('<IIIIIIHHI',header,sh+8,len(payload),0x1000,align(len(payload),0x200),0x200,0,0,0,0,0x60000020)
    path=root/destination
    path.write_bytes(header+payload+b'\0'*(align(len(payload),0x200)-len(payload)))
    print(f'{path.name}: {len(path.read_bytes())} bytes; {len(labels)} symbols')

if __name__=='__main__': build(sys.argv[1],sys.argv[2],len(sys.argv)>3 and sys.argv[3]=='dll')
