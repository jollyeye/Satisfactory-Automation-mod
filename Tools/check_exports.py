# Checks that every function the mod's DLL imports from the game exists in the game's DLL.
# A function the starter project's headers declare differently from the real game (e.g. an extra
# parameter) links fine but makes the game fail to load the mod: "module could not be loaded",
# GetLastError=127 in FactoryGame.log.
#   python check_exports.py <mod dll> <game dll>
#   e.g. Mods/AutomatedMachines/Binaries/Win64/FactoryGameEGS-AutomatedMachines-Win64-Shipping.dll
#        D:/games/Satisfactory/FactoryGame/Binaries/Win64/FactoryGameEGS-FactoryGame-Win64-Shipping.dll
# Plain Python, no packages.
import struct, sys
def load(path):
    d=open(path,'rb').read()
    pe=struct.unpack_from('<I',d,0x3c)[0]
    nsec=struct.unpack_from('<H',d,pe+6)[0]; optsz=struct.unpack_from('<H',d,pe+20)[0]
    opt=pe+24; magic=struct.unpack_from('<H',d,opt)[0]
    ddoff=opt+(112 if magic==0x20b else 96)
    secs=[]
    so=opt+optsz
    for i in range(nsec):
        name,vsz,va,rsz,raw=struct.unpack_from('<8sIIII',d,so+i*40)
        secs.append((va,max(vsz,rsz),raw))
    def r2o(rva):
        for va,sz,raw in secs:
            if va<=rva<va+sz: return rva-va+raw
        return None
    return d,ddoff,r2o
def cstr(d,o):
    e=d.index(b'\0',o); return d[o:e].decode('latin1')
def imports(path):
    d,dd,r2o=load(path)
    irva,isz=struct.unpack_from('<II',d,dd+8)
    o=r2o(irva); res={}
    while True:
        oft,ts,fc,name,ft=struct.unpack_from('<IIIII',d,o)
        if name==0: break
        dll=cstr(d,r2o(name)); lst=[]
        t=r2o(oft or ft)
        while True:
            v=struct.unpack_from('<Q',d,t)[0]
            if v==0: break
            if not v>>63: lst.append(cstr(d,r2o(v&0x7fffffff)+2))
            t+=8
        res[dll]=lst; o+=20
    return res
def exports(path):
    d,dd,r2o=load(path)
    erva,esz=struct.unpack_from('<II',d,dd)
    o=r2o(erva)
    n=struct.unpack_from('<I',d,o+24)[0]; names=struct.unpack_from('<I',d,o+32)[0]
    no=r2o(names); return set(cstr(d,r2o(struct.unpack_from('<I',d,no+4*i)[0])) for i in range(n))
if __name__=='__main__':
    mod_imports=imports(sys.argv[1])
    game=sys.argv[2]
    game_name=game.replace('\\','/').split('/')[-1]
    wanted=mod_imports.get(game_name, [])
    have=exports(game)
    missing=[n for n in wanted if n not in have]
    print('%d functions imported from %s, %d missing' % (len(wanted), game_name, len(missing)))
    for n in missing: print('MISSING', n)
