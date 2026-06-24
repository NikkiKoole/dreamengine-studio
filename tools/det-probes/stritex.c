// Probe 3 — software tritex (affine textured triangle), edge-function rasterizer with the
// top-left fill rule. Validates the three things goal-B needs from the hardest routine:
//   (a) correct textured output, (b) device-stable (arm64/x86/wasm hash match),
//   (c) ZERO gap / ZERO overlap on a shared edge (two tris -> a quad must tile perfectly).
// Build: clang -O2 stritex.c -o st && ./st   |   emcc -O2 stritex.c -o st.js && node st.js
#include <math.h>
#include <stdio.h>
#include <stdint.h>
#define W 320
#define H 200
static unsigned char BUF[W*H];     // rendered colour (texture sample)
static unsigned char CNT[W*H];     // how many triangles wrote each pixel (gap/overlap probe)

// checker texture: u,v in [0,1] -> index. Deterministic, no filtering.
static unsigned char tex(float u, float v){
    int tu=(int)(u*8.f), tv=(int)(v*8.f);
    return ((tu^tv)&1) ? 9 : 3;
}

// edge function: >0 if p is left of directed edge a->b (CCW positive).
static float edge(float ax,float ay,float bx,float by,float px,float py){
    return (bx-ax)*(py-ay) - (by-ay)*(px-ax);
}
// top-left test for an edge (a->b) in a CCW triangle: a pixel exactly ON the edge is
// covered iff the edge is a "top" edge (horizontal, pointing left) or a "left" edge.
static int top_left(float ax,float ay,float bx,float by){
    float dx=bx-ax, dy=by-ay;
    return (dy < 0.f) || (dy == 0.f && dx < 0.f);
}

static void stritex(float x0,float y0,float u0,float v0,
                    float x1,float y1,float u1,float v1,
                    float x2,float y2,float u2,float v2, int mark){
    // ensure CCW (positive area); if not, swap v1,v2 so winding is consistent.
    float area = edge(x0,y0,x1,y1,x2,y2);
    if (area == 0.f) return;
    if (area < 0.f){ float t;
        t=x1;x1=x2;x2=t; t=y1;y1=y2;y2=t; t=u1;u1=u2;u2=t; t=v1;v1=v2;v2=t;
        area = -area;
    }
    int minx=(int)floorf(fminf(x0,fminf(x1,x2))), maxx=(int)ceilf(fmaxf(x0,fmaxf(x1,x2)));
    int miny=(int)floorf(fminf(y0,fminf(y1,y2))), maxy=(int)ceilf(fmaxf(y0,fmaxf(y1,y2)));
    if(minx<0)minx=0; if(miny<0)miny=0; if(maxx>=W)maxx=W-1; if(maxy>=H)maxy=H-1;
    int tl0=top_left(x1,y1,x2,y2);   // edge opposite v0
    int tl1=top_left(x2,y2,x0,y0);   // edge opposite v1
    int tl2=top_left(x0,y0,x1,y1);   // edge opposite v2
    for(int py=miny;py<=maxy;py++){
        float fy=py+0.5f;
        for(int px=minx;px<=maxx;px++){
            float fx=px+0.5f;
            float w0=edge(x1,y1,x2,y2,fx,fy);
            float w1=edge(x2,y2,x0,y0,fx,fy);
            float w2=edge(x0,y0,x1,y1,fx,fy);
            int in0 = (w0>0.f) || (w0==0.f && tl0);
            int in1 = (w1>0.f) || (w1==0.f && tl1);
            int in2 = (w2>0.f) || (w2==0.f && tl2);
            if(in0&&in1&&in2){
                float l0=w0/area, l1=w1/area, l2=w2/area;
                float u=l0*u0+l1*u1+l2*u2, v=l0*v0+l1*v1+l2*v2;
                BUF[py*W+px]=tex(u,v);
                if(mark) CNT[py*W+px]++;
            }
        }
    }
}

int main(void){
    for(int i=0;i<W*H;i++){BUF[i]=0;CNT[i]=0;}
    // a quad split into two triangles sharing the A-C diagonal. CCW corners:
    float Ax=60,Ay=40, Bx=260,By=40, Cx=260,Cy=160, Dx=60,Dy=160;
    stritex(Ax,Ay,0,0, Bx,By,1,0, Cx,Cy,1,1, 1);   // tri 1: A,B,C
    stritex(Ax,Ay,0,0, Cx,Cy,1,1, Dx,Dy,0,1, 1);   // tri 2: A,C,D
    // also a rotated/odd triangle to exercise non-axis edges in the hash.
    stritex(30,30,0,0, 150,70,1,0, 80,180,0.5f,1, 0);

    // gap/overlap audit over the quad's interior rectangle (the tiling correctness bar).
    int once=0, twice=0, gap=0;
    for(int py=(int)Ay+1; py<(int)Cy; py++)
        for(int px=(int)Ax+1; px<(int)Bx; px++){
            int c=CNT[py*W+px];
            if(c==1) once++; else if(c>=2) twice++; else gap++;
        }
    uint64_t h=1469598103934665603ULL; int set=0;
    for(int i=0;i<W*H;i++){ h=(h^BUF[i])*1099511628211ULL; if(BUF[i])set++; }
    printf("hash=%016llx set=%d | tiling: once=%d twice(OVERLAP)=%d gap=%d\n",
           (unsigned long long)h, set, once, twice, gap);
    // PPM dump for a visual sanity check (native run only).
    FILE *f=fopen("stritex.ppm","wb");
    if(f){ fprintf(f,"P6\n%d %d\n255\n",W,H);
        for(int i=0;i<W*H;i++){ unsigned char c=BUF[i],r,g,b;
            if(c==0){r=g=b=16;} else if(c==3){r=40;g=50;b=90;} else if(c==9){r=120;g=200;b=255;}
            else {r=g=b=200;}
            fputc(r,f);fputc(g,f);fputc(b,f); }
        fclose(f); }
    return 0;
}
