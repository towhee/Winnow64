"""
Oracle for tests/unit/tst_lensprofile.cpp: liblensfun's OWN answers for a few shots,
written as tests/unit/lensprofile_oracle.h (the body between the #define and #endif).

    python3 -m venv venv && venv/bin/pip install lensfunpy numpy
    mkdir dbv1 && for f in Develop/LensDb/db/*.xml; do
        sed -e 's/version="2"/version="1"/' -E -e 's/ real-focal="[^"]*"//g' $f > dbv1/$(basename $f)
    done
    venv/bin/python lensfun_oracle.py dbv1 > body.inc

lensfunpy bundles lensfun 0.3.4, which rejects the version-2 header and the real-focal
attribute; neither changes rectilinear correction. Keep the oracle run OUTSIDE the repo.
"""
import sys, glob, json, numpy as np, lensfunpy
db=lensfunpy.Database(paths=sorted(glob.glob(sys.argv[1]+'/*.xml')), load_common=False, load_bundled=False)
cases=[
 ("Nikon Corporation","Nikon D810","Nikon","AF-S NIKKOR 24-70mm f/2.8G ED",31.0,5.0,1000.0,600,400),
 ("Nikon Corporation","Nikon D7200","Nikon","AF-S NIKKOR 24-70mm f/2.8G ED",50.0,4.0,1000.0,600,400),
 ("Sony","ILCE-6000","Sony","E 50mm f/1.8 OSS",50.0,2.8,1000.0,400,600),
 ("Sony","ILCE-7RM3","Sony","FE 70-300mm f/4.5-5.6 G OSS",135.0,8.0,10.0,600,400),
 ("Canon","Canon PowerShot G12","Canon","Canon PowerShot G12 & compatibles (Standard)",10.0,4.0,1000.0,600,450),
]
pts=[(0,0),(599,0),(300,200),(150,100),(599,399),(10,390),(450,30),(299,399)]
out=[]
for mk,md,lmk,lmd,f,a,d,W,H in cases:
    cam=db.find_cameras(mk,md)[0]
    lens=db.find_lenses(cam,lmk,lmd)[0]
    c={"cameraMake":mk,"cameraModel":md,"lens":lmd,"focal":f,"aperture":a,"distance":d,"W":W,"H":H,
       "cameraCrop":cam.crop_factor,"lensModel":lens.model,"lensCrop":lens.crop_factor}
    dist=lens.interpolate_distortion(f); c["dist"]=[float(x) for x in dist.terms[:3]] if dist else None
    tca=lens.interpolate_tca(f); c["tca"]=[float(x) for x in tca.terms[:6]] if tca else None
    vig=lens.interpolate_vignetting(f,a,d); c["vig"]=[float(x) for x in vig.terms[:3]] if vig else None
    for scale,key in ((1.0,"geom1"),(0.0,"geomAuto")):
        m=lensfunpy.Modifier(lens,cam.crop_factor,W,H)
        m.initialize(f,a,d,scale=scale,flags=lensfunpy.ModifyFlags.DISTORTION|lensfunpy.ModifyFlags.TCA)
        g=m.apply_subpixel_geometry_distortion()
        c[key]=[[x,y,[[float(g[y,x,ch,0]),float(g[y,x,ch,1])] for ch in range(3)]] for x,y in pts if x<W and y<H]
    m=lensfunpy.Modifier(lens,cam.crop_factor,W,H)
    m.initialize(f,a,d,scale=1.0,pixel_format=np.float32,flags=lensfunpy.ModifyFlags.VIGNETTING)
    img=np.ones((H,W,3),np.float32)
    m.apply_color_modification(img)
    c["vigGain"]=[[x,y,float(img[y,x,1])] for x,y in pts if x<W and y<H]
    out.append(c)

cs=out
def f(v):
    t="%.9g"%v
    return t if ("." in t or "e" in t or "n" in t) else t+".0"
def arr(a,n,fill): 
    a=(a or [])+[fill]*n; return "{"+", ".join(f(x) for x in a[:n])+"}"
print("struct OracleGeom { int x, y; double s[3][2]; };")
print("struct OracleVig  { int x, y; double gain; };")
print("struct OracleCase {\n    const char *make, *model, *lens; float focal, aperture, distance; int W, H;\n    float cameraCrop; const char *lensModel; bool hasDist, hasTca, hasVig;\n    float dist[3]; float tca[6]; float vig[3];\n    OracleGeom geom[8]; int nGeom; OracleVig gain[8]; int nGain;\n};")
print("static const OracleCase kOracle[] = {")
for c in cs:
    g=c["geom1"]; v=c["vigGain"]
    gs=",\n      ".join("{%d, %d, {{%s, %s}, {%s, %s}, {%s, %s}}}"%(x,y,f(s[0][0]),f(s[0][1]),f(s[1][0]),f(s[1][1]),f(s[2][0]),f(s[2][1])) for x,y,s in g)
    vs=",\n      ".join("{%d, %d, %s}"%(x,y,f(gn)) for x,y,gn in v)
    print('    {"%s", "%s", "%s", %sf, %sf, %sf, %d, %d,\n     %sf, "%s", %s, %s, %s,\n     %s, %s, %s,\n     {%s}, %d,\n     {%s}, %d},'%(
      c["cameraMake"],c["cameraModel"],c["lens"].replace('"','\\"'),f(c["focal"]),f(c["aperture"]),f(c["distance"]),c["W"],c["H"],
      f(c["cameraCrop"]),c["lensModel"],str(c["dist"] is not None).lower(),str(c["tca"] is not None).lower(),str(c["vig"] is not None).lower(),
      arr(c["dist"],3,0),arr(c["tca"],6,0),arr(c["vig"],3,0),gs,len(g),vs,len(v)))
print("};")
