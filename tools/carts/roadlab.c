#include "studio.h"
#include <stdio.h>

// roadlab — the FOUNDATION cart for the interchange geometry DSL, built on the research findings
// (docs/design/road-geometry-refs.md): LANE-ACCURATE roads (drive-on-right via DRIVE) + ports anchored
// to REAL lanes (a port = a lane + its travel direction) + ARC-SPLINE ramps (LINE→ARC→LINE; the
// OpenDRIVE model minus XML). Clothoid joints + nesting are later milestones. Supersedes rampkit
// (which proved the abstraction on Béziers); kept separate so rampkit stays as the proof reference.
//   MILESTONE 1: roads w/ lanes+arrows, ports on lanes, one arc-spline ramp between two ports.
//   ←/→ port A   ↑/↓ port B   [ / ] arc radius   F flip DRIVE
// Next: M2 clothoid joints (line↔arc, the §2 forward-integration loop) · M3 nesting (concentric arcs).

#define DRIVE  1     // +1 = drive on the right — the single source of truth for handedness
#define LANEW  7     // one lane width (px)

static float ux(float d){ return cos_deg(d); }
static float uy(float d){ return sin_deg(d); }

// stroke a polyline as a road ribbon (casing + centre) via polyfill quads + joint discs
static void ribbon(const float *xs,const float *ys,int n,int hw,int cas,int cen){
    for (int pass=0; pass<2; pass++){
        if (pass==0 && cas<0) continue;
        int w=(pass==0)?hw+1:hw, col=(pass==0)?cas:cen;
        for (int i=0;i<n-1;i++){
            float a=angle_to((int)xs[i],(int)ys[i],(int)xs[i+1],(int)ys[i+1]);
            float px=ux(a+90)*w, py=uy(a+90)*w;
            int xy[8]={ (int)(xs[i]-px),(int)(ys[i]-py),(int)(xs[i]+px),(int)(ys[i]+py),
                        (int)(xs[i+1]+px),(int)(ys[i+1]+py),(int)(xs[i+1]-px),(int)(ys[i+1]-py) };
            polyfill(xy,4,col); circfill((int)xs[i],(int)ys[i],w,col);
        }
        circfill((int)xs[n-1],(int)ys[n-1],w,col);
    }
}

static void arrow(float x,float y,float dir,int col){      // a small travel-direction chevron
    float tx=x+ux(dir)*4, ty=y+uy(dir)*4;
    line((int)x,(int)y,(int)tx,(int)ty,col);
    line((int)tx,(int)ty,(int)(tx+ux(dir+150)*3),(int)(ty+uy(dir+150)*3),col);
    line((int)tx,(int)ty,(int)(tx+ux(dir-150)*3),(int)(ty+uy(dir-150)*3),col);
}

// ── PORT = a point on a lane + the lane's travel direction (degrees) ──
typedef struct { float x,y,dir; const char*name; } Port;
static Port ports[16]; static int nport=0;
static void addport(float x,float y,float dir,const char*nm){ ports[nport++]=(Port){x,y,dir,nm}; }

static void draw_port(Port p,int col){
    circfill((int)p.x,(int)p.y,2,col);
    float tx=p.x+ux(p.dir)*12, ty=p.y+uy(p.dir)*12;
    line((int)p.x,(int)p.y,(int)tx,(int)ty,col);
    line((int)tx,(int)ty,(int)(tx+ux(p.dir+150)*5),(int)(ty+uy(p.dir+150)*5),col);
    line((int)tx,(int)ty,(int)(tx+ux(p.dir-150)*5),(int)(ty+uy(p.dir-150)*5),col);
}

// ── ARC-SPLINE ramp: LINE → ARC → LINE between two ports (a "simple curve": round the corner where
//    A's heading-line meets B's heading-line with an arc of radius R). Returns the polyline length. ──
static int arc_spline(Port a, Port b, float R, float *xs, float *ys){
    float uax=ux(a.dir),uay=uy(a.dir), ubx=ux(b.dir),uby=uy(b.dir);
    float den=uax*uby-uay*ubx;                              // cross(uA,uB); ~0 ⇒ parallel
    float dA=b.dir-a.dir; while(dA>180)dA-=360; while(dA<-180)dA+=360;
    float fa=dA<0?-dA:dA;
    int n=0;
    if (fa<1.f || (den<0.02f&&den>-0.02f)){ xs[n]=a.x;ys[n++]=a.y; xs[n]=b.x;ys[n++]=b.y; return n; }  // ~straight
    float s=((b.x-a.x)*uby-(b.y-a.y)*ubx)/den;              // distance along A's line to the corner P
    float Px=a.x+uax*s, Py=a.y+uay*s;
    float T=R*(sin_deg(fa*0.5f)/cos_deg(fa*0.5f));          // tangent length = R·tan(Δ/2)
    float t1x=Px-uax*T,t1y=Py-uay*T, t2x=Px+ubx*T,t2y=Py+uby*T;
    float nx=-uay,ny=uax;                                   // ⊥ to uA; pick the centre that's also R from T2
    float caX=t1x+nx*R,caY=t1y+ny*R, cbX=t1x-nx*R,cbY=t1y-ny*R, cx,cy;
    if (distance((int)caX,(int)caY,(int)t2x,(int)t2y) < distance((int)cbX,(int)cbY,(int)t2x,(int)t2y)){cx=caX;cy=caY;}
    else {cx=cbX;cy=cbY;}
    xs[n]=a.x; ys[n++]=a.y;                                 // LINE a → T1
    float a0=angle_to((int)cx,(int)cy,(int)t1x,(int)t1y);
    float a1=angle_to((int)cx,(int)cy,(int)t2x,(int)t2y);
    float sw=a1-a0; while(sw>180)sw-=360; while(sw<-180)sw+=360;
    for (int i=0;i<=16;i++){ float ang=a0+sw*(float)i/16; xs[n]=cx+ux(ang)*R; ys[n++]=cy+uy(ang)*R; }  // ARC
    xs[n]=b.x; ys[n++]=b.y;                                 // LINE T2 → b
    return n;
}

// ── state ──
static int selA=2, selB=0; static float radius=22.f;

static void setup(void){
    if (nport) return;
    int CX=SCREEN_W/2, CY=SCREEN_H/2;
    // drive-on-right lanes: highway N lane = westbound, S lane = eastbound; trunk E lane = northbound, W = southbound
    addport(CX-44, CY-LANEW/2.0f, 180, "hw-W");    // 0  on the westbound (north) lane, west of the junction
    addport(CX+44, CY+LANEW/2.0f,   0, "hw-E");    // 1  on the eastbound (south) lane, east of the junction
    addport(CX+LANEW/2.0f, CY-44, 270, "tr-N");    // 2  on the northbound (east) lane, north of the junction
    addport(CX-LANEW/2.0f, CY+44,  90, "tr-S");    // 3  on the southbound (west) lane, south of the junction
}

void update(void){
    setup();
    if (keyp(KEY_LEFT))  selA=(selA+nport-1)%nport;
    if (keyp(KEY_RIGHT)) selA=(selA+1)%nport;
    if (keyp(KEY_DOWN))  selB=(selB+nport-1)%nport;
    if (keyp(KEY_UP))    selB=(selB+1)%nport;
    if (keyp('['))       radius-=2;
    if (keyp(']'))       radius+=2;
    if (radius<6) radius=6;
}

void draw(void){
    setup();
    cls(CLR_DARK_GREEN);
    int CX=SCREEN_W/2, CY=SCREEN_H/2;
    // lane-accurate roads: slabs + yellow median + per-lane direction arrows
    rectfill(0, CY-LANEW, SCREEN_W, 2*LANEW, CLR_DARKER_GREY);     // highway (E-W)
    rectfill(CX-LANEW, 0, 2*LANEW, SCREEN_H, CLR_DARKER_GREY);     // trunk   (N-S)
    line(0,CY,SCREEN_W,CY,CLR_YELLOW);  line(CX,0,CX,SCREEN_H,CLR_YELLOW);
    for (int x=12;x<SCREEN_W;x+=40){ arrow(x, CY-LANEW/2.0f, 180, CLR_YELLOW); arrow(x, CY+LANEW/2.0f, 0, CLR_YELLOW); }
    for (int y=12;y<SCREEN_H;y+=40){ arrow(CX+LANEW/2.0f, y, 270, CLR_YELLOW); arrow(CX-LANEW/2.0f, y, 90, CLR_YELLOW); }
    // the arc-spline ramp between the two selected ports
    if (selA!=selB){
        float xs[64],ys[64]; int n=arc_spline(ports[selA],ports[selB],radius,xs,ys);
        ribbon(xs,ys,n, 4, CLR_BROWNISH_BLACK, CLR_ORANGE);
    }
    for (int i=0;i<nport;i++) if (i!=selA&&i!=selB) draw_port(ports[i], CLR_MEDIUM_GREY);
    draw_port(ports[selA], CLR_GREEN);  draw_port(ports[selB], CLR_RED);
    char b[72];
    snprintf(b,sizeof b,"A:%s -> B:%s   arc R:%d   [LINE->ARC->LINE]", ports[selA].name, ports[selB].name, (int)radius);
    print(b,4,4,CLR_WHITE);
    print("</> portA  ^v portB  [ ] radius", 4, SCREEN_H-9, CLR_LIGHT_GREY);
}
