#include "studio.h"
#include <stdio.h>
#include <math.h>

// roadlab — the FOUNDATION cart for the interchange geometry DSL, built on the research findings
// (docs/design/road-geometry-refs.md): LANE-ACCURATE roads (drive-on-right via DRIVE) + ports anchored
// to REAL lanes (a port = a lane + its travel direction) + ARC-SPLINE ramps (LINE→ARC→LINE; the
// OpenDRIVE model minus XML). Clothoid joints + nesting are later milestones. Supersedes rampkit
// (which proved the abstraction on Béziers); kept separate so rampkit stays as the proof reference.
//   MILESTONE 1: roads w/ lanes+arrows, ports on lanes, one arc-spline ramp between two ports.
//   MILESTONE 2: clothoid joints (LINE→CLOTHOID→ARC→CLOTHOID→LINE) — κ ramps 0→1/R→0 so steering is
//                continuous (G2); the §2 forward-integration loop, no Fresnel. C toggles it for A/B.
//   MILESTONE 3: MULTI-LANE ramps via lateral offsets of the one reference line (OpenDRIVE s/t shift).
//                Lanes = concentric offset polylines ⇒ constant gap ⇒ they NEST FOR FREE on arc ref-lines
//                — the empirical test of "do arcs nest without a solver?" (answer: yes). 1-4 set lanes.
//   ←/→ port A   ↑/↓ port B   [ / ] arc radius   ,/. clothoid length   C clothoid   1-4 lane count
// Next: lane add/drop tapers (a lane peeling off at a diverge — OpenDRIVE width cubic); then port into roadnet2.

#define DRIVE  1     // +1 = drive on the right — the single source of truth for handedness
#define LANEW  7     // one lane width (px)

static float ux(float d){ return cos_deg(d); }
static float uy(float d){ return sin_deg(d); }

// ── LANES = lateral offsets of ONE reference line (OpenDRIVE's "same s, shifted in t", §1/§5). We never
//    offset per-curve; we shift each sample ⊥ to the local tangent. On ARC ref-lines the offsets are
//    concentric arcs ⇒ constant gap ⇒ they NEST for free (the whole reason we chose arcs over Béziers). ──
// offset a polyline by d px along the local LEFT normal (averaged tangent; exact gap for dense samples)
static void offset_poly(const float *xs,const float *ys,int n,float d,float *ox,float *oy){
    for (int i=0;i<n;i++){
        float ax,ay;
        if      (i==0)   { ax=xs[1]-xs[0];       ay=ys[1]-ys[0]; }
        else if (i==n-1) { ax=xs[n-1]-xs[n-2];   ay=ys[n-1]-ys[n-2]; }
        else             { ax=xs[i+1]-xs[i-1];   ay=ys[i+1]-ys[i-1]; }   // averaged tangent at the vertex
        float L=sqrtf(ax*ax+ay*ay); if(L<0.001f)L=1;
        float nx=-ay/L, ny=ax/L;                                         // ⊥ (left) unit normal
        ox[i]=xs[i]+nx*d; oy[i]=ys[i]+ny*d;
    }
}
static void fill_strip(const float*lx,const float*ly,const float*rx,const float*ry,int n,int col){
    for (int i=0;i<n-1;i++){
        int xy[8]={(int)lx[i],(int)ly[i],(int)rx[i],(int)ry[i],(int)rx[i+1],(int)ry[i+1],(int)lx[i+1],(int)ly[i+1]};
        polyfill(xy,4,col);
    }
}
static void stroke_poly(const float*xs,const float*ys,int n,int col){
    for (int i=0;i<n-1;i++) line((int)xs[i],(int)ys[i],(int)xs[i+1],(int)ys[i+1],col);
}
// draw an nl-lane ramp from a reference centreline: casing + asphalt fill + white interior dividers.
// The dividers staying parallel at constant gap IS the nesting proof — concentric offsets, no solver.
static void draw_multilane(const float*xs,const float*ys,int n,int nl){
    float HW=nl*LANEW*0.5f, lx[128],ly[128],rx[128],ry[128];
    offset_poly(xs,ys,n,+(HW+1),lx,ly); offset_poly(xs,ys,n,-(HW+1),rx,ry);
    fill_strip(lx,ly,rx,ry,n,CLR_BROWNISH_BLACK);                         // casing
    offset_poly(xs,ys,n,+HW,lx,ly);     offset_poly(xs,ys,n,-HW,rx,ry);
    fill_strip(lx,ly,rx,ry,n,CLR_DARK_GREY);                             // asphalt surface
    for (int b=1;b<nl;b++){ float off=-HW+b*LANEW; offset_poly(xs,ys,n,off,lx,ly); stroke_poly(lx,ly,n,CLR_WHITE); }
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

#define R2D 57.29578f                                  // radians → degrees
static float tan_deg(float d){ return sin_deg(d)/cos_deg(d); }

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

// ── CLOTHOID-JOINTED ramp (M2): LINE → CLOTHOID → ARC → CLOTHOID → LINE. The spirals ramp curvature
//    0 → 1/R → 0 (κ linear in arc length) so steering rate is continuous (G2) — "drives easily".
//    Construction: the standard spiral SHIFT — measure the spiral's lateral shift p and back-distance k
//    by integrating ONE local clothoid arm (the §2 forward-integration loop; exact, no Fresnel), shift
//    the equivalent circle inward by p, take tangent dist Ts=(R+p)·tan(Δ/2)+k, then integrate the real
//    curve forward. Reduces EXACTLY to arc_spline as Ls→0. ──
static int clothoid_spline(Port a, Port b, float R, float Ls, float *xs, float *ys){
    if (Ls < 0.5f) return arc_spline(a,b,R,xs,ys);          // Ls→0 IS the plain arc (avoids 1/(R·Ls) blow-up)
    float uax=ux(a.dir),uay=uy(a.dir), ubx=ux(b.dir),uby=uy(b.dir);
    float den=uax*uby-uay*ubx;
    float dA=b.dir-a.dir; while(dA>180)dA-=360; while(dA<-180)dA+=360;
    float fa=dA<0?-dA:dA, turn=dA<0?-1.f:1.f;               // |Δ| and turn sign
    int n=0;
    if (fa<1.f || (den<0.02f&&den>-0.02f)){ xs[n]=a.x;ys[n++]=a.y; xs[n]=b.x;ys[n++]=b.y; return n; }
    float P_s=((b.x-a.x)*uby-(b.y-a.y)*ubx)/den;            // dist along A's line to the apex P
    float Px=a.x+uax*P_s, Py=a.y+uay*P_s;
    // spiral angle θs = Ls/2R; clamp so two spirals fit the deflection (spiral-spiral limit → arc=0)
    float thS=(Ls/(2*R))*R2D;                               // spiral angle (deg)
    if (thS > fa*0.5f){ thS=fa*0.5f; Ls=2*R*(thS/R2D); }
    int NC=10; float ds=Ls/NC, sig=1.f/(R*Ls);              // dκ/ds; integrate one arm with +curvature
    float lx=0,ly=0,th=0,k=0;                               // local frame: x along tangent, y lateral
    for(int i=0;i<NC;i++){ float tm=th+(k*ds*0.5f)*R2D; lx+=ux(tm)*ds; ly+=uy(tm)*ds; th+=(k+sig*ds*0.5f)*ds*R2D; k+=sig*ds; }
    float p =ly - R*(1-cos_deg(thS));                       // lateral shift of the equivalent circle
    float kk=lx - R*sin_deg(thS);                           // back-distance (TS → equiv-circle PC)
    float Ts=(R+p)*tan_deg(fa*0.5f) + kk;                   // tangent dist from apex P to the spiral start
    float TSx=Px-uax*Ts, TSy=Py-uay*Ts;                     // start of entry spiral (after the lead-in line)
    // emit: LINE a → TS, then forward-integrate spiral / arc / spiral; append exact port b for the run-out
    xs[n]=a.x; ys[n++]=a.y; xs[n]=TSx; ys[n++]=TSy;
    float x=TSx,y=TSy,hd=a.dir,kc=0,sg=turn/(R*Ls);         // entry CLOTHOID: κ 0 → turn/R
    for(int i=0;i<NC;i++){ float tm=hd+(kc*ds*0.5f)*R2D; x+=ux(tm)*ds; y+=uy(tm)*ds; hd+=(kc+sg*ds*0.5f)*ds*R2D; kc+=sg*ds; xs[n]=x;ys[n++]=y; }
    float arcDeg=fa-2*thS;                                  // ARC: constant κ over the residual deflection
    if (arcDeg>0.5f){ int NA=(int)(arcDeg/6)+1; float ads=(R*arcDeg/R2D)/NA;
        for(int i=0;i<NA;i++){ float tm=hd+(kc*ads*0.5f)*R2D; x+=ux(tm)*ads; y+=uy(tm)*ads; hd+=kc*ads*R2D; xs[n]=x;ys[n++]=y; } }
    sg=-turn/(R*Ls);                                        // exit CLOTHOID: κ turn/R → 0
    for(int i=0;i<NC;i++){ float tm=hd+(kc*ds*0.5f)*R2D; x+=ux(tm)*ds; y+=uy(tm)*ds; hd+=(kc+sg*ds*0.5f)*ds*R2D; kc+=sg*ds; xs[n]=x;ys[n++]=y; }
    xs[n]=b.x; ys[n++]=b.y;                                 // LINE (run-out) → b
    return n;
}

// ── state ──
static int selA=2, selB=0; static float radius=28.f, spiral=14.f; static int use_cloth=1, nlanes=2;

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
    if (keyp(','))       spiral-=2;
    if (keyp('.'))       spiral+=2;
    if (spiral<0) spiral=0;
    if (keyp('c')||keyp('C')) use_cloth=!use_cloth;
    if (keyp('1')) nlanes=1; if (keyp('2')) nlanes=2; if (keyp('3')) nlanes=3; if (keyp('4')) nlanes=4;
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
    // the ramp between the two selected ports — arc-spline (M1) + clothoid joints (M2), drawn as an
    // nl-lane ribbon via lateral offsets of the one reference line (M3: lanes/nesting via concentric arcs)
    if (selA!=selB){
        float xs[128],ys[128];
        int n = use_cloth ? clothoid_spline(ports[selA],ports[selB],radius,spiral,xs,ys)
                          : arc_spline(ports[selA],ports[selB],radius,xs,ys);
        draw_multilane(xs,ys,n,nlanes);
    }
    for (int i=0;i<nport;i++) if (i!=selA&&i!=selB) draw_port(ports[i], CLR_MEDIUM_GREY);
    draw_port(ports[selA], CLR_GREEN);  draw_port(ports[selB], CLR_RED);
    char b[80];
    snprintf(b,sizeof b,"A:%s -> B:%s   R:%d   lanes:%d", ports[selA].name, ports[selB].name, (int)radius, nlanes);
    print(b,4,4,CLR_WHITE);
    if (use_cloth){ snprintf(b,sizeof b,"clothoid Ls:%d  (C off)  [LINE>CLOTH>ARC>CLOTH>LINE]", (int)spiral); print(b,4,13,CLR_ORANGE); }
    else print("clothoid OFF  (C on)  [LINE>ARC>LINE]", 4,13, CLR_MEDIUM_GREY);
    print("</> A  ^v B  [ ] radius  ,/. spiral  C clothoid  1-4 lanes", 4, SCREEN_H-9, CLR_LIGHT_GREY);
}
