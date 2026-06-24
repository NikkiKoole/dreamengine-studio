// Probe (Fork 2, rotated sprites) — three ways to rotate a pixel-art sprite, head to head:
//   NEAREST      : 1 inverse-mapped sample per dest pixel (== what the GPU does today, point filter)
//   SUPERSAMPLE  : 4x4 subsamples of the RAW source, majority vote (a naive "quality" attempt)
//   ROTSPRITE    : EPX/Scale2x x3 -> 8x edge-aware upscale, then rotate+mode-downscale (the real route)
// Question: does RotSprite keep BOTH thin lines (the 1px frame) AND lone pixels (the dot), where
// nearest fragments lines and supersample erases the dot? Sweeps 360° for metrics + 3-panel PPM.
//   clang -O2 rotspr.c -o rs && ./rs   |   emcc -O2 rotspr.c -o rs.js && node rs.js
#include <math.h>
#include <stdio.h>
#include <stdint.h>
#define SS 16
#define DW 26
#define DH 26
#define US 128                 // upscaled source = SS*8
static unsigned char src[SS*SS];
static unsigned char up[US*US];                 // EPX x3 upscale of src
static unsigned char dN[DW*DH], dS[DW*DH], dR[DW*DH];
static unsigned char hit[SS*SS];

static void build_src(void){
    for(int y=0;y<SS;y++)for(int x=0;x<SS;x++){
        unsigned char c=0;
        if(x==0||y==0||x==SS-1||y==SS-1) c=1;     // 1px frame
        if(x==y) c=2;                              // diagonal
        if(x==11&&y==4) c=3;                       // lone dot (the canary)
        src[y*SS+x]=c;
    }
}
// EPX / Scale2x: edge-aware 2x upscale (in w*h -> out 2w*2h).
static void epx2x(const unsigned char*in,int w,int h,unsigned char*out){
    int ow=2*w;
    for(int y=0;y<h;y++)for(int x=0;x<w;x++){
        unsigned char P=in[y*w+x];
        unsigned char A=y>0?in[(y-1)*w+x]:P, B=x<w-1?in[y*w+x+1]:P;
        unsigned char C=x>0?in[y*w+x-1]:P,   D=y<h-1?in[(y+1)*w+x]:P;
        unsigned char e1=P,e2=P,e3=P,e4=P;
        if(C==A&&C!=D&&A!=B) e1=A;
        if(A==B&&A!=C&&B!=D) e2=B;
        if(D==C&&D!=B&&C!=A) e3=C;
        if(B==D&&B!=A&&D!=C) e4=D;
        out[(2*y)*ow+2*x]=e1; out[(2*y)*ow+2*x+1]=e2;
        out[(2*y+1)*ow+2*x]=e3; out[(2*y+1)*ow+2*x+1]=e4;
    }
}
static void build_up(void){
    static unsigned char t1[32*32], t2[64*64];
    epx2x(src,16,16,t1); epx2x(t1,32,32,t2); epx2x(t2,64,64,up);   // 16->32->64->128
}
static unsigned char samp_src(float sx,float sy,int markhit){
    int ix=(int)floorf(sx), iy=(int)floorf(sy);
    if((unsigned)ix>=SS||(unsigned)iy>=SS) return 0;
    if(markhit) hit[iy*SS+ix]=1;
    return src[iy*SS+ix];
}
static unsigned char samp_up(float sx,float sy){     // sample the 8x upscaled source
    int ux=(int)(sx*8.f), uy=(int)(sy*8.f);
    if((unsigned)ux>=US||(unsigned)uy>=US) return 0;
    return up[uy*US+ux];
}
static int components(unsigned char *g){
    static int st[DW*DH]; static unsigned char seen[DW*DH]; int comps=0;
    for(int i=0;i<DW*DH;i++) seen[i]=0;
    for(int i=0;i<DW*DH;i++){ if(!g[i]||seen[i]) continue; comps++; int sp=0; st[sp++]=i; seen[i]=1;
        while(sp){ int p=st[--sp],x=p%DW,y=p/DW;
            for(int dy=-1;dy<=1;dy++)for(int dx=-1;dx<=1;dx++){ if(!dx&&!dy)continue;
                int nx=x+dx,ny=y+dy; if((unsigned)nx<DW&&(unsigned)ny<DH){int q=ny*DW+nx;
                    if(g[q]&&!seen[q]){seen[q]=1;st[sp++]=q;} } } } }
    return comps;
}
static void render(float a){
    // quantize rotation matrix (see textrot.c): raw libm cosf/sinf diverge ~1 ULP across arches.
    float c=roundf(cosf(a)*4096.f)/4096.f, s=roundf(sinf(a)*4096.f)/4096.f;
    float cx=DW*0.5f,cy=DH*0.5f,ox=SS*0.5f,oy=SS*0.5f;
    for(int i=0;i<DW*DH;i++){ dN[i]=0; dS[i]=0; dR[i]=0; }
    for(int i=0;i<SS*SS;i++) hit[i]=0;
    for(int py=0;py<DH;py++) for(int px=0;px<DW;px++){
        float dx=px+0.5f-cx, dy=py+0.5f-cy;
        float lx=c*dx+s*dy, ly=-s*dx+c*dy;
        dN[py*DW+px]=samp_src(ox+lx, oy+ly, 1);                  // nearest
        int cs[4]={0,0,0,0}, cr[4]={0,0,0,0};
        for(int sj=0;sj<4;sj++)for(int si=0;si<4;si++){
            float ex=px+(si+0.5f)/4.f-cx, ey=py+(sj+0.5f)/4.f-cy;
            float qx=c*ex+s*ey, qy=-s*ex+c*ey;
            cs[ samp_src(ox+qx, oy+qy, 0) ]++;                   // supersample: raw source
            cr[ samp_up (ox+qx, oy+qy)    ]++;                   // rotsprite: EPX-upscaled source
        }
        int bs=0,br=0;                              // mode, ties broken toward OPAQUE (sprite rule)
        for(int k=1;k<4;k++){
            if(cs[k]>cs[bs] || (cs[k]==cs[bs]&&bs==0)) bs=k;
            if(cr[k]>cr[br] || (cr[k]==cr[br]&&br==0)) br=k;
        }
        dS[py*DW+px]=(unsigned char)bs;
        dR[py*DW+px]=(unsigned char)br;
    }
}
int main(void){
    build_src(); build_up();
    int worst_dropN=0, compsN=1, compsR=1, dotN=0, dotS=0, dotR=0; uint64_t h=1469598103934665603ULL;
    for(int deg=0; deg<360; deg++){
        render(deg*3.14159265f/180.f);
        int dropped=0; for(int i=0;i<SS*SS;i++) if(src[i]&&!hit[i]) dropped++;
        if(dropped>worst_dropN) worst_dropN=dropped;
        int cN=components(dN), cR=components(dR);
        if(cN>compsN) compsN=cN; if(cR>compsR) compsR=cR;
        int hN=0,hS=0,hR=0; for(int i=0;i<DW*DH;i++){ if(dN[i]==3)hN=1; if(dS[i]==3)hS=1; if(dR[i]==3)hR=1; }
        dotN+=hN; dotS+=hS; dotR+=hR;
        for(int i=0;i<DW*DH;i++){ h=(h^dN[i])*1099511628211ULL; h=(h^dS[i])*1099511628211ULL; h=(h^dR[i])*1099511628211ULL; }
    }
    printf("hash=%016llx | NEAREST drop=%d frame_comps<=%d dot=%d/360 | SUPER dot=%d/360 | ROTSPRITE frame_comps<=%d dot=%d/360\n",
           (unsigned long long)h, worst_dropN, compsN, dotN, dotS, compsR, dotR);

    // 3-panel visual at 32deg: nearest | supersample | rotsprite, magnified 8x.
    render(32.f*3.14159265f/180.f);
    int Z=7, GAP=8, PW=DW*Z*3+GAP*2, PH=DH*Z;
    FILE *f=fopen("rotspr.ppm","wb");
    if(f){ fprintf(f,"P6\n%d %d\n255\n",PW,PH);
        for(int y=0;y<PH;y++) for(int x=0;x<PW;x++){
            int panelw=DW*Z, slot=x/(panelw+GAP), inx=x-slot*(panelw+GAP);
            unsigned char v=0; int bg=0;
            if(inx>=panelw){ bg=1; }
            else { unsigned char *g = slot==0?dN : slot==1?dS : dR; v=g[(y/Z)*DW + inx/Z]; }
            unsigned char r,g_,b;
            if(bg){ r=10;g_=10;b=12; }
            else if(v==1){r=200;g_=200;b=210;}
            else if(v==2){r=90;g_=170;b=255;}
            else if(v==3){r=255;g_=80;b=80;}
            else {r=24;g_=24;b=28;}
            fputc(r,f);fputc(g_,f);fputc(b,f);
        }
        fclose(f);
    }
    return 0;
}
