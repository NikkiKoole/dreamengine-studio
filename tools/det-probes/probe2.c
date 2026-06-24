// Probe 2 — the hard one: do FILL and its bounding OUTLINE agree under ONE convention?
// Uses sline + sfill verbatim (the linesym.c convention candidates). For each polygon:
//   stroke_outside        = outline pixels that fall OUTSIDE the fill (the stroke pokes out)
//   fillbound_uncovered   = fill-boundary pixels the outline does NOT cover (gap ring)
//   mirror_mismatch       = is fill+outline reflection-symmetric (linesym's 320 vs 319 axes)?
// A coherent convention wants stroke_outside==0 (outline within fill) and a small/structured
// uncovered count — and mirror_mismatch==0 for the combined art.
#include <math.h>
#include <stdio.h>
#include <stdint.h>
#define W 320
#define H 200
static unsigned char FILLB[W*H], STRK[W*H], COMBO[W*H];
static unsigned char *TARGET;   // which buffer pset writes to

static void pset(int x,int y,int c){ if((unsigned)x<(unsigned)W&&(unsigned)y<(unsigned)H){ TARGET[y*W+x]=(unsigned char)c; if(TARGET!=COMBO) COMBO[y*W+x]=(unsigned char)c; } }

static void plot_minor(int maj,float v,float mid,int horiz,int c){
    float f=floorf(v); int fi=(int)f; float r=v-f; int m;
    if(r!=0.5f){ m=(r<0.5f)?fi:fi+1; }
    else if(v==mid){ if(horiz){pset(maj,fi,c);pset(maj,fi+1,c);}else{pset(fi,maj,c);pset(fi+1,maj,c);} return; }
    else m=(mid>v)?fi+1:fi;
    if(horiz) pset(maj,m,c); else pset(m,maj,c);
}
static void sline(int x0,int y0,int x1,int y1,int c){
    int dx=x1-x0,dy=y1-y0,adx=dx<0?-dx:dx,ady=dy<0?-dy:dy;
    if(adx==0&&ady==0){pset(x0,y0,c);return;}
    if(adx>=ady){ int lo=x0<x1?x0:x1,hi=x0<x1?x1:x0; float ymid=(y0+y1)*0.5f;
        for(int x=lo;x<=hi;x++) plot_minor(x, y0+(float)(x-x0)*dy/(float)dx, ymid,1,c);
    } else { int lo=y0<y1?y0:y1,hi=y0<y1?y1:y0; float xmid=(x0+x1)*0.5f;
        for(int y=lo;y<=hi;y++) plot_minor(y, x0+(float)(y-y0)*dx/(float)dy, xmid,0,c); }
}
static void sfill(int *xy,int n,int color){
    int y0=99999,y1=-99999;
    for(int i=0;i<n;i++){int y=xy[i*2+1]; if(y<y0)y0=y; if(y>y1)y1=y;}
    for(int y=y0;y<=y1;y++){ float yc=y+0.5f,cr[64]; int m=0;
        for(int i=0,j=n-1;i<n;j=i++){ float yi=xy[i*2+1],yj=xy[j*2+1];
            if((yi>yc)!=(yj>yc)){ float xi=xy[i*2],xj=xy[j*2]; cr[m++]=(xj-xi)*(yc-yi)/(yj-yi)+xi; } }
        for(int a=1;a<m;a++){float v=cr[a];int b=a-1;while(b>=0&&cr[b]>v){cr[b+1]=cr[b];b--;}cr[b+1]=v;}
        for(int t=0;t+1<m;t+=2){ int a=(int)ceilf(cr[t]-0.5f), b=(int)ceilf(cr[t+1]-0.5f)-1;
            for(int x=a;x<=b;x++) pset(x,y,color); }
    }
}

static void clearall(void){ for(int i=0;i<W*H;i++){FILLB[i]=STRK[i]=COMBO[i]=0;} }

// run one polygon: fill (into FILLB) + outline (into STRK), report coherence.
static void shape(const char*name, int *xy, int n){
    clearall();
    TARGET=FILLB; sfill(xy,n,5);
    TARGET=STRK;  for(int i=0;i<n;i++){int j=(i+1)%n; sline(xy[i*2],xy[i*2+1],xy[j*2],xy[j*2+1],7);}
    int stroke_outside=0, fillbound_uncovered=0;
    for(int y=1;y<H-1;y++) for(int x=1;x<W-1;x++){
        int f=FILLB[y*W+x], s=STRK[y*W+x];
        if(s && !f) stroke_outside++;
        if(f){ int boundary = !FILLB[y*W+x-1]||!FILLB[y*W+x+1]||!FILLB[(y-1)*W+x]||!FILLB[(y+1)*W+x];
            if(boundary && !s) fillbound_uncovered++; }
    }
    printf("  %-18s n=%2d  stroke_outside=%-4d fillbound_uncovered=%-4d\n",
           name, n, stroke_outside, fillbound_uncovered);
}

int main(void){
    // axis-aligned square
    int sq[8]={120,60, 200,60, 200,140, 120,140};
    shape("axis_square", sq, 4);
    // 45-degree diamond (rotated square) — the classic fill/stroke divergence case
    int di[8]={160,50, 230,110, 160,170, 90,110};
    shape("diamond_45", di, 4);
    // arbitrary triangle
    int tr[6]={70,40, 250,80, 120,180};
    shape("triangle", tr, 3);
    // hexagon
    int hx[12]; for(int i=0;i<6;i++){ float a=i*1.04719755f; hx[i*2]=(int)(160+60*cosf(a)); hx[i*2+1]=(int)(100+60*sinf(a)); }
    shape("hexagon", hx, 6);

    // mirror-coherence: diamond + its mirror, fill about 320, stroke about 319 (linesym rule),
    // all into COMBO; then check x <-> 319-x symmetry of the combined art.
    clearall();
    int dl[8], dr[8];
    for(int i=0;i<4;i++){ dl[i*2]=di[i*2]; dl[i*2+1]=di[i*2+1]; dr[i*2]=320-di[i*2]; dr[i*2+1]=di[i*2+1]; }
    TARGET=COMBO; sfill(dl,4,5); sfill(dr,4,5);
    for(int i=0;i<4;i++){int j=(i+1)%4;
        sline(dl[i*2],dl[i*2+1],dl[j*2],dl[j*2+1],7);
        sline(319-di[i*2],di[i*2+1],319-di[j*2],di[j*2+1],7); }
    int mm=0; for(int y=0;y<H;y++) for(int x=0;x<160;x++) if(COMBO[y*W+x]!=COMBO[y*W+(319-x)]) mm++;
    printf("  %-18s mirror_mismatch=%d (fill@320 / stroke@319, combined art)\n","diamond_mirror",mm);

    uint64_t h=1469598103934665603ULL;
    for(int i=0;i<W*H;i++){ h=(h^COMBO[i])*1099511628211ULL; }
    printf("hash(combo)=%016llx\n",(unsigned long long)h);

    // diagnostic PPM for the diamond: where do fill & outline disagree?
    clearall(); TARGET=FILLB; sfill(di,4,5);
    TARGET=STRK; for(int i=0;i<4;i++){int j=(i+1)%4; sline(di[i*2],di[i*2+1],di[j*2],di[j*2+1],7);}
    FILE *f=fopen("probe2.ppm","wb");
    if(f){ fprintf(f,"P6\n%d %d\n255\n",W,H);
      for(int y=0;y<H;y++) for(int x=0;x<W;x++){ int p=y*W+x; int fl=FILLB[p],st=STRK[p];
        int boundary = fl && (!FILLB[p-1]||!FILLB[p+1]||(y>0&&!FILLB[p-W])||(y<H-1&&!FILLB[p+W]));
        unsigned char r,g,b;
        if(st && !fl){ r=255;g=40;b=40; }              // RED: stroke pokes OUTSIDE fill
        else if(boundary && !st){ r=255;g=220;b=0; }   // YELLOW: fill edge NOT covered by stroke
        else if(st){ r=235;g=235;b=235; }              // white: stroke on/in fill
        else if(fl){ r=40;g=70;b=150; }                // blue: fill interior
        else { r=18;g=18;b=18; }
        fputc(r,f);fputc(g,f);fputc(b,f); }
      fclose(f); }
    return 0;
}
