// Probe (Fork 2) — does INVERSE mapping rotate a filled shape gap-free at every angle, and is it
// device-stable? Inverse = visit each DEST pixel, rotate it back to shape-local coords, test inside
// (Mode-7/SNES trick). Forward = rotate each SOURCE pixel to dest + round (what staircases into
// holes). Sweep 0..359deg; report inverse area-stability + connectivity, forward's holes, and a
// cross-arch hash. Build/run via tools/det-probes/run.sh (added there), or:
//   clang -O2 rotfill.c -o rf && ./rf   |   emcc -O2 rotfill.c -o rf.js && node rf.js
#include <math.h>
#include <stdio.h>
#include <stdint.h>
#define W 200
#define H 200
static unsigned char inv[W*H], fwd[W*H];

static int components(unsigned char *g){
    static int st[W*H]; static unsigned char seen[W*H]; int comps=0;
    for(int i=0;i<W*H;i++) seen[i]=0;
    for(int i=0;i<W*H;i++){
        if(!g[i]||seen[i]) continue; comps++; int sp=0; st[sp++]=i; seen[i]=1;
        while(sp){ int p=st[--sp],x=p%W,y=p/W;
            for(int dy=-1;dy<=1;dy++)for(int dx=-1;dx<=1;dx++){ if(!dx&&!dy)continue;
                int nx=x+dx,ny=y+dy; if((unsigned)nx<W&&(unsigned)ny<H){int q=ny*W+nx;
                    if(g[q]&&!seen[q]){seen[q]=1;st[sp++]=q;} } } }
    }
    return comps;
}

int main(void){
    float cx=100,cy=100, hw=55,hh=28;     // rectangle half-extents
    int worst_holes=0, min_area=1<<30, max_area=0, worst_comps=1;
    uint64_t h=1469598103934665603ULL;
    for(int deg=0; deg<360; deg++){
        for(int i=0;i<W*H;i++){ inv[i]=0; fwd[i]=0; }
        // quantize the rotation matrix to 1/4096 — libm cosf/sinf differ ~1 ULP across arm64/x86/wasm,
        // which flips per-pixel floor/compare and breaks determinism (see textrot.c). Round c/s to fix.
        float a=deg*3.14159265f/180.f;
        float c=roundf(cosf(a)*4096.f)/4096.f, s=roundf(sinf(a)*4096.f)/4096.f;
        // INVERSE: each dest pixel -> rotate back -> inside test (gap-free by construction).
        for(int py=0;py<H;py++) for(int px=0;px<W;px++){
            float dx=px+0.5f-cx, dy=py+0.5f-cy;
            float lx= c*dx + s*dy, ly= -s*dx + c*dy;          // rotate by -a
            if(fabsf(lx)<=hw && fabsf(ly)<=hh) inv[py*W+px]=1;
        }
        // FORWARD: each source pixel -> rotate to dest -> round (leaves holes when rotated).
        for(int sy=(int)-hh; sy<=(int)hh; sy++) for(int sx=(int)-hw; sx<=(int)hw; sx++){
            float px=cx + c*sx - s*sy, py=cy + s*sx + c*sy;
            int ix=(int)lroundf(px), iy=(int)lroundf(py);
            if((unsigned)ix<W&&(unsigned)iy<H) fwd[iy*W+ix]=1;
        }
        int area=0, holes=0;
        for(int i=0;i<W*H;i++){ if(inv[i]){ area++; if(!fwd[i]) holes++; } }
        int comps=components(inv);
        if(holes>worst_holes) worst_holes=holes;
        if(area<min_area) min_area=area;
        if(area>max_area) max_area=area;
        if(comps>worst_comps) worst_comps=comps;
        for(int i=0;i<W*H;i++) h=(h^inv[i])*1099511628211ULL;
    }
    printf("hash=%016llx | INVERSE: area %d..%d comps<=%d | FORWARD worst_holes=%d\n",
           (unsigned long long)h, min_area, max_area, worst_comps, worst_holes);
    return 0;
}
