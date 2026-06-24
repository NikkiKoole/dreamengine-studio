// Probe (Fork 2, rotated TEXT) — a glyph is just a tiny sprite, so rotated text reduces to
// rotated-sprite algorithms. Confirms it on a real 1px-stroke letter "E": rotate it at a small
// size three ways — NEAREST (== print_rot today, GPU point-filter), SUPERSAMPLE, and ROTSPRITE
// (EPX/Scale2x x3 -> rotate -> mode-downscale). Metric: connected components of the lit glyph
// (an intact E is 1; fragmentation = illegible). Sweeps 360° + dumps a 3-panel PPM.
//   clang -O2 textrot.c -o tr && ./tr   |   emcc -O2 textrot.c -o tr.js && node tr.js
#include <math.h>
#include <stdio.h>
#include <stdint.h>
#define SS 8
#define DW 14
#define DH 14
#define US 64                  // SS * 8
static unsigned char src[SS*SS], up[US*US];
static unsigned char dN[DW*DH], dS[DW*DH], dR[DW*DH];

static void build_src(void){            // 8x8 letter "E", 1px strokes (1 = ink)
    for(int i=0;i<SS*SS;i++) src[i]=0;
    for(int x=0;x<5;x++){ src[0*SS+x]=1; src[6*SS+x]=1; }   // top + bottom bars
    for(int x=0;x<4;x++)   src[3*SS+x]=1;                   // middle bar
    for(int y=0;y<7;y++)   src[y*SS+0]=1;                   // left stem
}
static void epx2x(const unsigned char*in,int w,int h,unsigned char*out){
    int ow=2*w;
    for(int y=0;y<h;y++)for(int x=0;x<w;x++){
        unsigned char P=in[y*w+x];
        unsigned char A=y>0?in[(y-1)*w+x]:P,B=x<w-1?in[y*w+x+1]:P;
        unsigned char C=x>0?in[y*w+x-1]:P,D=y<h-1?in[(y+1)*w+x]:P;
        unsigned char e1=P,e2=P,e3=P,e4=P;
        if(C==A&&C!=D&&A!=B) e1=A; if(A==B&&A!=C&&B!=D) e2=B;
        if(D==C&&D!=B&&C!=A) e3=C; if(B==D&&B!=A&&D!=C) e4=D;
        out[(2*y)*ow+2*x]=e1; out[(2*y)*ow+2*x+1]=e2; out[(2*y+1)*ow+2*x]=e3; out[(2*y+1)*ow+2*x+1]=e4;
    }
}
static void build_up(void){ static unsigned char t1[16*16],t2[32*32];
    epx2x(src,8,8,t1); epx2x(t1,16,16,t2); epx2x(t2,32,32,up); }
static unsigned char samp_src(float sx,float sy){ int ix=(int)floorf(sx),iy=(int)floorf(sy);
    return ((unsigned)ix<SS&&(unsigned)iy<SS)?src[iy*SS+ix]:0; }
static unsigned char samp_up(float sx,float sy){ int ux=(int)(sx*8.f),uy=(int)(sy*8.f);
    return ((unsigned)ux<US&&(unsigned)uy<US)?up[uy*US+ux]:0; }
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
    // QUANTIZE the rotation matrix to fixed-point: libm cosf/sinf differ ~1 ULP across toolchains
    // (arm64 vs x86 vs wasm), which flips floor()/sample at small scales -> non-deterministic. Rounding
    // c/s to 1/4096 erases the sub-ULP disagreement so rotation is bit-identical across devices.
    float c=roundf(cosf(a)*4096.f)/4096.f, s=roundf(sinf(a)*4096.f)/4096.f;
    float cx=DW*0.5f,cy=DH*0.5f,ox=SS*0.5f,oy=SS*0.5f;
    for(int i=0;i<DW*DH;i++){ dN[i]=0; dS[i]=0; dR[i]=0; }
    for(int py=0;py<DH;py++) for(int px=0;px<DW;px++){
        float dx=px+0.5f-cx,dy=py+0.5f-cy, lx=c*dx+s*dy, ly=-s*dx+c*dy;
        dN[py*DW+px]=samp_src(ox+lx,oy+ly);
        int cs0=0,cs1=0,cr0=0,cr1=0;
        for(int sj=0;sj<4;sj++)for(int si=0;si<4;si++){
            float ex=px+(si+0.5f)/4.f-cx,ey=py+(sj+0.5f)/4.f-cy, qx=c*ex+s*ey, qy=-s*ex+c*ey;
            if(samp_src(ox+qx,oy+qy)) cs1++; else cs0++;
            if(samp_up (ox+qx,oy+qy)) cr1++; else cr0++;
        }
        dS[py*DW+px]= (cs1>=cs0)?1:0;     // ink wins ties (sprite rule)
        dR[py*DW+px]= (cr1>=cr0)?1:0;
    }
}
int main(void){
    build_src(); build_up();
    int compsN=1,compsS=1,compsR=1; uint64_t h=1469598103934665603ULL;
    for(int deg=0; deg<360; deg++){
        render(deg*3.14159265f/180.f);
        int cN=components(dN),cS=components(dS),cR=components(dR);
        if(cN>compsN)compsN=cN; if(cS>compsS)compsS=cS; if(cR>compsR)compsR=cR;
        for(int i=0;i<DW*DH;i++){ h=(h^dN[i])*1099511628211ULL; h=(h^dS[i])*1099511628211ULL; h=(h^dR[i])*1099511628211ULL; }
    }
    // an intact "E" is 1 connected component; higher = more fragmented/illegible.
    printf("hash=%016llx | worst components (1=intact): NEAREST=%d SUPER=%d ROTSPRITE=%d\n",
           (unsigned long long)h, compsN, compsS, compsR);

    render(28.f*3.14159265f/180.f);
    int Z=10,GAP=8,PW=DW*Z*3+GAP*2,PH=DH*Z;
    FILE *f=fopen("textrot.ppm","wb");
    if(f){ fprintf(f,"P6\n%d %d\n255\n",PW,PH);
        for(int y=0;y<PH;y++)for(int x=0;x<PW;x++){
            int pw=DW*Z, slot=x/(pw+GAP), inx=x-slot*(pw+GAP); unsigned char v=0; int bg=(inx>=pw);
            if(!bg){ unsigned char*g=slot==0?dN:slot==1?dS:dR; v=g[(y/Z)*DW+inx/Z]; }
            unsigned char r,gg,b;
            if(bg){r=10;gg=10;b=12;} else if(v){r=235;gg=235;b=245;} else {r=26;gg=26;b=32;}
            fputc(r,f);fputc(gg,f);fputc(b,f);
        }
        fclose(f);
    }
    return 0;
}
