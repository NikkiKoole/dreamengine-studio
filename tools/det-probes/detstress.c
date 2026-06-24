// Probe 0 (stress) — exercise the rasterizer float path across MANY slopes/positions,
// including deliberate near-tie cases, to flush out any rounding divergence (esp. FMA).
#include <math.h>
#include <stdio.h>
#include <stdint.h>
#define W 320
#define H 200
static unsigned char BUF[W*H];
static void pset(int x,int y,int c){ if((unsigned)x<(unsigned)W&&(unsigned)y<(unsigned)H) BUF[y*W+x]=(unsigned char)c; }

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
int main(void){
    for(int i=0;i<W*H;i++) BUF[i]=0;
    // 1. dense line fan: every endpoint pair in a grid -> thousands of distinct slopes,
    //    many landing on exact-.5 ties (the rounding-sensitive case).
    for(int x1=2;x1<W;x1+=3) for(int y1=2;y1<H;y1+=5)
        sline(0,0, x1,y1, 1+((x1*y1)&7));   // colour index only; deterministic int math
    // 2. concentric rotating polygons -> sfill crossing math across all orientations.
    for(int k=3;k<=12;k++){ int xy[64]; int n=k; float R=8.f+k*7.f, cx=160, cy=100;
        for(int i=0;i<n;i++){ float a=6.2831853f*i/n + k*0.37f;
            xy[i*2]=(int)(cx+R*cosf(a)); xy[i*2+1]=(int)(cy+R*sinf(a)); }
        sfill(xy,n,1+(k&31)); }
    uint64_t h=1469598103934665603ULL; int set=0;
    for(int i=0;i<W*H;i++){ h=(h^BUF[i])*1099511628211ULL; if(BUF[i])set++; }
    printf("hash=%016llx set=%d\n",(unsigned long long)h,set);
    return 0;
}
