/* de:meta
{
  "title": "tristress",
  "status": "active",
  "kind": [
    "tech-demo"
  ],
  "teaches": [],
  "description": "A deterministic torture test for trifill — which is poly_fill_cov with n=3, so it already inherits the polygon scanline span fill (one DrawRectangle per row). The measurement seam for a possible DEDICATED triangle rasterizer (sorted-vertex incremental edge walk, no per-row division): scene 0 BIG (key 1) = 8 large rotating triangles (per-row math over large area), scene 1 MANY (key 2) = a 26x16 grid of tiny triangles (per-CALL setup, where poly_clamp_scan's 4 matrix-inverts dominate). Pure function of the frame counter. Verdict so far: a tri-specific path isn't worth it (BIG 0.33ms / MANY 0.97ms, no real cart trifill-bound); the per-call overhead it would cut is shared by all software fills and better solved by a per-frame clamp-box cache. Kept as the rig + seam. See guides/engine-optimization.md."
}
de:meta */
// tristress — torture test for trifill (which is poly_fill_cov with n=3). Used to
// measure whether a DEDICATED triangle rasterizer (precomputed edge slopes + incremental
// x, no per-row division; cheap bbox clamp instead of poly_clamp_scan's 4 matrix-inverts)
// would beat routing through the general polygon span fill. Deterministic (frame counter).
//   keys: 1 = BIG (few large tris — per-row math)   2 = MANY (tiny tris — per-call overhead)

#include "studio.h"
#include <math.h>
#include <stdio.h>
#define TAU 6.28318530718f
static int F = 0, scene = 0;
void update(void){ F++; if (keyp('1')) scene=0; if (keyp('2')) scene=1; }

// big rotating triangles: per-row crossing math over large area
static int big(float ph){
    int fills=0;
    for (int i=0;i<8;i++){
        float a=ph+i*0.7f, cx=SCREEN_W*0.5f, cy=SCREEN_H*0.5f, R=90;
        int x1=(int)(cx+cosf(a)*R),       y1=(int)(cy+sinf(a)*R);
        int x2=(int)(cx+cosf(a+2.2f)*R),  y2=(int)(cy+sinf(a+2.2f)*R);
        int x3=(int)(cx+cosf(a+4.1f)*8),  y3=(int)(cy+sinf(a+4.1f)*8);
        trifill(x1,y1,x2,y2,x3,y3, 8+i%8); fills++;
    }
    return fills;
}
// dense grid of tiny triangles: per-CALL setup (bbox + poly_clamp_scan) dominates
static int many(float ph){
    int fills=0, GX=26, GY=16;
    for (int gy=0;gy<GY;gy++) for (int gx=0;gx<GX;gx++){
        int x=(int)((gx+0.5f)*SCREEN_W/GX), y=(int)((gy+0.5f)*SCREEN_H/GY);
        float a=ph+(gx+gy)*0.3f; int r=5;
        trifill(x+(int)(cosf(a)*r), y+(int)(sinf(a)*r),
                x+(int)(cosf(a+2.1f)*r), y+(int)(sinf(a+2.1f)*r),
                x+(int)(cosf(a+4.2f)*r), y+(int)(sinf(a+4.2f)*r), 16+((gx+gy)%16));
        fills++;
    }
    return fills;
}
void draw(void){
    cls(CLR_BLACK);
    float ph=F*0.04f;
    int fills = scene==0 ? big(ph) : many(ph);
    print(scene==0?"tristress BIG (1)":"tristress MANY (2)", 4, 4, CLR_WHITE);
    char b[40]; snprintf(b,sizeof b,"F=%d tris=%d",F,fills); print(b,4,12,CLR_LIGHT_GREY);
#ifdef DE_TRACE
    watch("tris","%d",fills);
#endif
}
