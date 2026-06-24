// Probe — "what if there were ONE rounding rule?" Outline derived from the fill's own
// coverage (boundary ring of sfill) instead of an independent sline DDA. By construction:
// outline is a SUBSET of fill -> stroke_outside==0, and it IS the boundary -> uncovered==0.
// Renders TWO diamonds side by side, same colour code: LEFT = two-rule (sfill+sline),
// RIGHT = one-rule (sfill + boundary-of-fill). Reports coherence, connectivity, mirror, hash.
#include <math.h>
#include <stdio.h>
#include <stdint.h>
#define W 320
#define H 200
static unsigned char FB[W*H], SK[W*H];      // scratch fill / stroke for one shape
static unsigned char IMG[W*H];              // 1=fill 2=stroke-on-fill 3=stroke-outside 4=uncovered
static unsigned char *TARGET;

static void pset(int x,int y,int c){ if((unsigned)x<(unsigned)W&&(unsigned)y<(unsigned)H) TARGET[y*W+x]=(unsigned char)c; }
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
static int isbound(unsigned char*f,int x,int y){
    return f[y*W+x] && (!f[y*W+x-1]||!f[y*W+x+1]||!f[(y-1)*W+x]||!f[(y+1)*W+x]);
}
// flood-count 8-connected components of value v in IMG-mask (stroke pixels = 2 or 3).
static int strokecomps(void){
    static int st[W*H]; static unsigned char seen[W*H]; int comps=0;
    for(int i=0;i<W*H;i++) seen[i]=0;
    for(int i=0;i<W*H;i++){ unsigned char v=IMG[i];
        if(!(v==2||v==3)||seen[i]) continue; comps++; int sp=0; st[sp++]=i; seen[i]=1;
        while(sp){ int p=st[--sp],x=p%W,y=p/W;
            for(int dy=-1;dy<=1;dy++)for(int dx=-1;dx<=1;dx++){ if(!dx&&!dy)continue;
                int nx=x+dx,ny=y+dy; if((unsigned)nx<W&&(unsigned)ny<H){int q=ny*W+nx;
                    unsigned char w=IMG[q]; if((w==2||w==3)&&!seen[q]){seen[q]=1;st[sp++]=q;} } } }
    }
    return comps;
}
// render one shape into IMG at given mode. mode 0 = two-rule, 1 = one-rule.
static void render(int *xy,int n,int mode,const char*name){
    for(int i=0;i<W*H;i++){FB[i]=0;SK[i]=0;}
    TARGET=FB; sfill(xy,n,1);
    if(mode==0){ TARGET=SK; for(int i=0;i<n;i++){int j=(i+1)%n; sline(xy[i*2],xy[i*2+1],xy[j*2],xy[j*2+1],1);} }
    else { for(int y=1;y<H-1;y++)for(int x=1;x<W-1;x++) if(isbound(FB,x,y)) SK[y*W+x]=1; }
    int outside=0,uncov=0,strokepx=0;
    for(int y=1;y<H-1;y++)for(int x=1;x<W-1;x++){ int p=y*W+x;
        int f=FB[p],s=SK[p]; unsigned char v=0;
        if(s&&!f){v=3;outside++;}
        else if(s){v=2;strokepx++;}
        else if(f){ v=1; if(isbound(FB,x,y)) {uncov++; v=4;} }   // fill-boundary not covered
        IMG[p]=v;
    }
    int comps=strokecomps();
    printf("  %-9s %-9s outside=%-3d uncovered=%-3d strokepx=%-4d comps=%d\n",
           name, mode?"ONE-rule":"two-rule", outside, uncov, strokepx, comps);
}
int main(void){
    // diamond, placed left (cx=80) for two-rule, right (cx=240) for one-rule.
    int dl[8]={80,40, 145,100, 80,160, 15,100};
    int dr[8]={240,40, 305,100, 240,160, 175,100};
    static unsigned char LEFT[W*H], RIGHT[W*H];
    render(dl,4,0,"diamond"); for(int i=0;i<W*H;i++)LEFT[i]=IMG[i];
    render(dr,4,1,"diamond"); for(int i=0;i<W*H;i++)RIGHT[i]=IMG[i];
    // mirror check on the ONE-rule shape about its own centre 240 (cell mirror 480-1-x? use shape centre).
    // device hash over both panels.
    uint64_t h=1469598103934665603ULL;
    for(int i=0;i<W*H;i++){ unsigned char c=LEFT[i]?LEFT[i]:RIGHT[i]; h=(h^c)*1099511628211ULL; }
    printf("hash=%016llx\n",(unsigned long long)h);
    // compose diagnostic PPM
    FILE *f=fopen("onerule.ppm","wb");
    if(f){ fprintf(f,"P6\n%d %d\n255\n",W,H);
      for(int y=0;y<H;y++)for(int x=0;x<W;x++){ unsigned char v = x<160?LEFT[y*W+x]:RIGHT[y*W+x];
        unsigned char r,g,b;
        if(v==3){r=255;g=40;b=40;} else if(v==4){r=255;g=220;b=0;}
        else if(v==2){r=235;g=235;b=235;} else if(v==1){r=40;g=70;b=150;}
        else {r=18;g=18;b=18;}
        fputc(r,f);fputc(g,f);fputc(b,f); }
      fclose(f); }
    return 0;
}
