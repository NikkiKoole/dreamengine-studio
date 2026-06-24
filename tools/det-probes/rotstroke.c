// Probe 4 — rotated stroke (outline) via "rotate endpoints -> sline". The crisp-pixel
// failure mode is a GAP: the closed loop breaking into >1 connected component. Sweep a
// square outline through many angles; for each, assert the 4-edge loop is ONE 8-connected
// component (closed, no gap) and accumulate a device-stability hash.
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

// count 8-connected components of set pixels via flood fill (iterative stack).
static int components(void){
    static int stack[W*H]; int comps=0;
    static unsigned char seen[W*H];
    for(int i=0;i<W*H;i++) seen[i]=0;
    for(int i=0;i<W*H;i++){
        if(!BUF[i]||seen[i]) continue;
        comps++; int sp=0; stack[sp++]=i; seen[i]=1;
        while(sp){ int p=stack[--sp]; int x=p%W,y=p/W;
            for(int dy=-1;dy<=1;dy++)for(int dx=-1;dx<=1;dx++){
                if(!dx&&!dy)continue; int nx=x+dx,ny=y+dy;
                if((unsigned)nx<(unsigned)W&&(unsigned)ny<(unsigned)H){ int q=ny*W+nx;
                    if(BUF[q]&&!seen[q]){seen[q]=1;stack[sp++]=q;} } }
        }
    }
    return comps;
}

int main(void){
    int worst_comps=1, total_broken=0, angles=0;
    uint64_t h=1469598103934665603ULL;
    float cx=160,cy=100,R=70;
    for(int deg=0; deg<360; deg++){
        for(int i=0;i<W*H;i++) BUF[i]=0;
        float a=deg*3.14159265f/180.f;
        // 4 corners of a square rotated by `a`
        float px[4],py[4];
        for(int k=0;k<4;k++){ float ca=a+k*1.5707963f;
            px[k]=cx+R*cosf(ca); py[k]=cy+R*sinf(ca); }
        for(int k=0;k<4;k++){ int n=(k+1)&3;
            sline((int)lroundf(px[k]),(int)lroundf(py[k]),
                  (int)lroundf(px[n]),(int)lroundf(py[n]), 7); }
        int c=components();
        if(c>worst_comps) worst_comps=c;
        if(c>1) total_broken++;
        angles++;
        for(int i=0;i<W*H;i++){ h=(h^BUF[i])*1099511628211ULL; }
    }
    printf("hash=%016llx angles=%d worst_components=%d broken_angles=%d\n",
           (unsigned long long)h, angles, worst_comps, total_broken);
    return 0;
}
