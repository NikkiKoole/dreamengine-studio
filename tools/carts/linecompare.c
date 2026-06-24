// linecompare — three ways to draw the same line, magnified so you can see the pixels.
//
// A line on a grid must choose which cells to light. Three approaches, all here:
//   * DDA STACK (blue, view 1): walk the long axis, round the other coord — exactly 1 cell
//     per step. Thickness is an ADD-ON: stack N cells along the MINOR axis, so a thick DDA's
//     perpendicular width VARIES with angle (thinnest near 45°).
//   * COVERAGE (orange, view 2): light a cell if its CENTRE is within `radius` of the line —
//     the same rule fills/outlines use. Thickness is intrinsic (widen radius): uniform
//     perpendicular width at any angle, but raggeder edges.
//   * DDA PERP-OFFSET (view 3): draw N parallel 1px DDA lines offset along the PERPENDICULAR
//     — crisp like DDA AND uniform width like coverage (the best-of-both candidate; watch for
//     tiny seams between passes).
// Overlay views (0 = stack-DDA vs coverage, 4 = perp-DDA vs coverage) colour agreement WHITE,
// disagreement blue/orange — the gap between two methods at the same thickness.
//
// LEFT/RIGHT rotate · A auto-spin · B cycle view · UP/DOWN thickness.
#include "studio.h"
#include <math.h>

#define GW 24
#define GH 12
#define CELL 12
#define OX 16
#define OY 4

static unsigned char ddag[GW*GH];   // DDA, minor-axis stack thickening
static unsigned char pddag[GW*GH];  // DDA, perpendicular-offset thickening
static unsigned char covg[GW*GH];   // coverage (capsule)
static float angle = 0.6f;
static int   thick = 1;             // line thickness in cells (1 = 1px); all methods share it
static int   spin  = 1;
static int   view  = 0;             // 0 stackDDA|cov  1 stackDDA  2 cov  3 perpDDA  4 perpDDA|cov

static void mark(unsigned char *g, int i, int j){ if(i>=0&&i<GW&&j>=0&&j<GH) g[j*GW+i]=1; }

// round the minor coord, ties break toward the segment mid (faithful to sline).
static int round_minor(float v, float mid){
    float f=floorf(v); int fi=(int)f; float r=v-f;
    if(r!=0.5f) return (r<0.5f)?fi:fi+1;
    if(v==mid)  return fi;
    return (mid>v)?fi+1:fi;
}
// 1px DDA into grid g (one cell per major step).
static void dda1(unsigned char *g, int x0,int y0,int x1,int y1){
    int dx=x1-x0,dy=y1-y0,adx=dx<0?-dx:dx,ady=dy<0?-dy:dy;
    if(adx==0&&ady==0){ mark(g,x0,y0); return; }
    if(adx>=ady){ int lo=x0<x1?x0:x1,hi=x0<x1?x1:x0; float ymid=(y0+y1)*0.5f;
        for(int x=lo;x<=hi;x++) mark(g, x, round_minor(y0+(float)(x-x0)*dy/(float)dx, ymid));
    } else { int lo=y0<y1?y0:y1,hi=y0<y1?y1:y0; float xmid=(x0+x1)*0.5f;
        for(int y=lo;y<=hi;y++) mark(g, round_minor(x0+(float)(y-y0)*dx/(float)dy, xmid), y); }
}
// thick DDA #1: stack `thick` cells along the MINOR axis, centred (perp width varies w/ angle).
static void dda_stack(int x0,int y0,int x1,int y1){
    int dx=x1-x0,dy=y1-y0,adx=dx<0?-dx:dx,ady=dy<0?-dy:dy;
    int half=(thick-1)/2;
    if(adx==0&&ady==0){ for(int k=0;k<thick;k++) mark(ddag,x0,y0-half+k); return; }
    if(adx>=ady){ int lo=x0<x1?x0:x1,hi=x0<x1?x1:x0; float ymid=(y0+y1)*0.5f;
        for(int x=lo;x<=hi;x++){ int m=round_minor(y0+(float)(x-x0)*dy/(float)dx, ymid);
            for(int k=0;k<thick;k++) mark(ddag, x, m-half+k); }
    } else { int lo=y0<y1?y0:y1,hi=y0<y1?y1:y0; float xmid=(x0+x1)*0.5f;
        for(int y=lo;y<=hi;y++){ int m=round_minor(x0+(float)(y-y0)*dx/(float)dy, xmid);
            for(int k=0;k<thick;k++) mark(ddag, m-half+k, y); } }
}
// thick DDA #2: `thick` parallel 1px DDA lines offset along the PERPENDICULAR (uniform width).
static void dda_perp(int x0,int y0,int x1,int y1){
    float dx=x1-x0,dy=y1-y0,L=sqrtf(dx*dx+dy*dy);
    if(L<0.001f){ mark(pddag,x0,y0); return; }
    float px=-dy/L, py=dx/L; int half=(thick-1)/2;
    for(int o=-half;o<thick-half;o++){
        int ax=(int)lroundf(x0+o*px), ay=(int)lroundf(y0+o*py);
        int bx=(int)lroundf(x1+o*px), by=(int)lroundf(y1+o*py);
        dda1(pddag, ax,ay,bx,by);
    }
}
// coverage: distance from a cell centre to the segment <= radius.
static float dist_seg(float px,float py,float ax,float ay,float bx,float by){
    float dx=bx-ax,dy=by-ay,Lq=dx*dx+dy*dy;
    float t = Lq>0.f ? ((px-ax)*dx+(py-ay)*dy)/Lq : 0.f;
    if(t<0.f)t=0.f; if(t>1.f)t=1.f;
    float qx=ax+t*dx, qy=ay+t*dy;
    return sqrtf((px-qx)*(px-qx)+(py-qy)*(py-qy));
}

void update(void){
    if(btnp(0,BTN_A)) spin=!spin;
    if(btnp(0,BTN_B)) view=(view+1)%5;
    if(spin) angle += 0.012f;
    if(btn(0,BTN_LEFT))  { angle -= 0.03f; spin=0; }
    if(btn(0,BTN_RIGHT)) { angle += 0.03f; spin=0; }
    if(btnp(0,BTN_UP))   { if(thick<6) thick++; }
    if(btnp(0,BTN_DOWN)) { if(thick>1) thick--; }
}

void draw(void){
    cls(CLR_BLACK);
    for(int k=0;k<GW*GH;k++){ ddag[k]=0; pddag[k]=0; covg[k]=0; }

    float cx=GW*0.5f, cy=GH*0.5f, R=GH*0.42f;
    float ax=cx+R*cosf(angle), ay=cy+R*sinf(angle);
    float bx=cx-R*cosf(angle), by=cy-R*sinf(angle);
    int ix0=(int)lroundf(ax), iy0=(int)lroundf(ay), ix1=(int)lroundf(bx), iy1=(int)lroundf(by);

    dda_stack(ix0,iy0,ix1,iy1);
    dda_perp (ix0,iy0,ix1,iy1);
    float radius = thick*0.5f;
    for(int j=0;j<GH;j++) for(int i=0;i<GW;i++)
        if(dist_seg(i+0.5f,j+0.5f, ix0,iy0, ix1,iy1) <= radius) covg[j*GW+i]=1;

    // pick the grid(s) this view shows. gB==0 => single-method view.
    unsigned char *gA, *gB; const char *label;
    switch(view){
        case 1: gA=ddag;  gB=0;    label="VIEW: DDA stack (minor-axis)";   break;
        case 2: gA=covg;  gB=0;    label="VIEW: COVERAGE (capsule)";       break;
        case 3: gA=pddag; gB=0;    label="VIEW: DDA perp-offset";          break;
        case 4: gA=pddag; gB=covg; label="VIEW: perp-DDA  vs  COVERAGE";   break;
        default:gA=ddag;  gB=covg; label="VIEW: stack-DDA  vs  COVERAGE";  break;
    }

    int solo = (gA==covg) ? CLR_ORANGE : CLR_BLUE;   // single-view colour matches the method
    int differ=0, lit=0;
    for(int j=0;j<GH;j++) for(int i=0;i<GW;i++){
        int a=gA[j*GW+i], b=gB?gB[j*GW+i]:0, col;
        if(a) lit++;
        if(gB && (a!=b)) differ++;
        if(gB) col = (a&&b)?CLR_WHITE : a?CLR_BLUE : b?CLR_ORANGE : CLR_DARKER_GREY;
        else   col = a ? solo : CLR_DARKER_GREY;
        rectfill(OX+i*CELL, OY+j*CELL, CELL-1, CELL-1, col);
    }

    int sx0=OX+(int)(ax*CELL), sy0=OY+(int)(ay*CELL);
    int sx1=OX+(int)(bx*CELL), sy1=OY+(int)(by*CELL);
    line(sx0,sy0,sx1,sy1, CLR_YELLOW);
    rect(OX+ix0*CELL+2, OY+iy0*CELL+2, CELL-5, CELL-5, CLR_GREEN);
    rect(OX+ix1*CELL+2, OY+iy1*CELL+2, CELL-5, CELL-5, CLR_GREEN);

    int y=GH*CELL+OY+2;
    print(label, OX, y, CLR_LIGHT_GREY);
    if(gB){ int kx=OX;                                  // colour key only when overlaying
        rectfill(kx,y+9,7,7,CLR_WHITE);  print("agree", kx+10,y+9,CLR_WHITE);  kx+=10+5*8+8;
        rectfill(kx,y+9,7,7,CLR_BLUE);   print("dda",   kx+10,y+9,CLR_BLUE);   kx+=10+3*8+8;
        rectfill(kx,y+9,7,7,CLR_ORANGE); print("cover", kx+10,y+9,CLR_ORANGE);
    }
    print(str("ang %d", (int)(angle*57.3f)%360), OX, y+18, CLR_LIGHT_GREY);
    print(str("thick %d", thick), OX+62, y+18, CLR_LIGHT_GREY);
    if(gB) print(str("differ %d", differ), OX+128, y+18, differ?CLR_ORANGE:CLR_GREEN);
    else   print(str("lit %d", lit), OX+128, y+18, CLR_LIGHT_GREY);
    print(str("spin %s", spin?"ON":"off"), OX+210, y+18, spin?CLR_GREEN:CLR_MEDIUM_GREY);
    print("A spin  B view  </> turn  ^v thick", OX, y+27, CLR_MEDIUM_GREY);
}
