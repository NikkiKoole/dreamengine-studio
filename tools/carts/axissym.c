// axissym — does sline stay reflection-symmetric about an ARBITRARY axis, not just screen centre?
// Draws a line fan + its CELL-mirror about x=AX (mirror = 2*AX-1-x, the same pairing mirror-diff
// uses for --cx AX). Run: node tools/mirror-diff.js axissym --cx <AX>  → expect 0 for any AX.
#include "studio.h"
#include <math.h>

#define AX 100                       // mirror axis (try 100, 64, 200, …; pass the same to --cx)
#define MIR(x) (2*AX - 1 - (x))

static void sl_plot(int maj, float v, float mid, int horiz, int c){
    float f=floorf(v); int fi=(int)f, m; float r=v-f;
    if (r!=0.5f) m=(r<0.5f)?fi:fi+1;
    else if (v==mid){ if(horiz){pset(maj,fi,c);pset(maj,fi+1,c);} else {pset(fi,maj,c);pset(fi+1,maj,c);} return; }
    else m=(mid>v)?fi+1:fi;
    if (horiz) pset(maj,m,c); else pset(m,maj,c);
}
static void sline(int x0,int y0,int x1,int y1,int c){
    int dx=x1-x0, dy=y1-y0, adx=dx<0?-dx:dx, ady=dy<0?-dy:dy;
    if (adx==0&&ady==0){ pset(x0,y0,c); return; }
    if (adx>=ady){ int lo=x0<x1?x0:x1, hi=x0<x1?x1:x0; float ym=(y0+y1)*0.5f;
        for (int x=lo;x<=hi;x++) sl_plot(x, y0+(float)(x-x0)*dy/(float)dx, ym, 1, c); }
    else { int lo=y0<y1?y0:y1, hi=y0<y1?y1:y0; float xm=(x0+x1)*0.5f;
        for (int y=lo;y<=hi;y++) sl_plot(y, x0+(float)(y-y0)*dx/(float)dy, xm, 0, c); }
}
static void seg(int x0,int y0,int x1,int y1,int c){
    sline(x0,y0,x1,y1,c);
    sline(MIR(x0),y0,MIR(x1),y1,c);
}
void draw(void){
    cls(CLR_BLACK);
    seg(20,40, 50,55,  CLR_WHITE);    // shallow x-major
    seg(15,80, 60,92,  CLR_YELLOW);
    seg(25,120,40,150, CLR_GREEN);    // steep y-major (the tie-break stressor)
    seg(30,160,70,168, CLR_BROWNISH_BLACK);
    seg(10,30, 68,34,  CLR_BROWN);
}
