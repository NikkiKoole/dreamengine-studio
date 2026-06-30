/* de:meta
{
  "title": "rampkit",
  "status": "active",
  "created": "2026-06-16",
  "kind": [
    "tech-demo"
  ],
  "teaches": [
    "algorithm-visualization"
  ],
  "lineage": "Proof-of-concept for the interchange-DSL geometry layer (docs/design/interchange-dsl.md); superseded by roadlab which replaces the Bezier ramps with arc-splines and adds lane-accurate ports.",
  "description": "A geometry SANDBOX for the interchange-DSL idea (docs/design/interchange-dsl.md): a road ramp is built from TWO PORTS (an attach point + a tangent direction) plus a TYPE — DIRECT / FLYOVER / LOOP all fall out of ONE closed-form constructor, with no per-shape coordinates. Ports are drawn as arrows; the orange ramp re-solves to whichever pair + type you pick. Proves 'describe junctions declaratively, not by hand-placed geometry' in isolation before porting back to interchange.c. Controls: LEFT/RIGHT cycle port A, UP/DOWN cycle port B, SPACE cycles the ramp type, F flips the DRIVE side (the loop mirrors). Press N for the NEST-SOLVE demo: two non-concentric loops where a relaxation solver finds the inner radius so the two nest at a target clearance (LEFT/RIGHT adjust the gap). See docs/design/interchange-dsl.md and docs/design/road-geometry-refs.md.",
  "todo": [
    "ui-audit?: the bottom control-hint line runs past the right edge (clipped) — low-confidence, may be intentional; see action-plan \"control-hint overflow\"."
  ]
}
de:meta */
#include "studio.h"
#include <stdio.h>

// rampkit — sandbox for the interchange-DSL GEOMETRY layer (docs/design/interchange-dsl.md).
// STEPS 1-2: a PORT is an attach point + a tangent direction; a ramp is built from TWO PORTS + a
// TYPE, with NO per-shape coordinates — DIRECT / FLYOVER / LOOP all fall out of the same constructor
// (Tier-1, closed-form). This proves the abstraction in isolation before porting back to interchange.c.
//   ←/→ cycle port A   ↑/↓ cycle port B   SPACE cycle ramp type   F flip DRIVE side
// Next (step 3-4): DRIVE-derived handedness everywhere, then a relation/relaxation solver (nest/clear).

#define MAXPTS 32
static int drive = 1;            // +1 = drive on the right, -1 = left — the LOOP curls to this side (DSL DRIVE seed)

static float ux(float d){ return cos_deg(d); }
static float uy(float d){ return sin_deg(d); }

// stroke a polyline as a road ribbon (casing + centre) via polyfill quads + joint discs
static void ribbon(const float *xs, const float *ys, int n, int hw, int cas, int cen){
    for (int pass=0; pass<2; pass++){
        if (pass==0 && cas<0) continue;
        int w = (pass==0)? hw+1 : hw;
        int col = (pass==0)? cas : cen;
        for (int i=0;i<n-1;i++){
            float a = angle_to((int)xs[i],(int)ys[i],(int)xs[i+1],(int)ys[i+1]);
            float px = ux(a+90)*w, py = uy(a+90)*w;
            int xy[8] = { (int)(xs[i]-px),(int)(ys[i]-py),   (int)(xs[i]+px),(int)(ys[i]+py),
                          (int)(xs[i+1]+px),(int)(ys[i+1]+py),(int)(xs[i+1]-px),(int)(ys[i+1]-py) };
            polyfill(xy, 4, col);
            circfill((int)xs[i],(int)ys[i], w, col);
        }
        circfill((int)xs[n-1],(int)ys[n-1], w, col);
    }
}

static void bez(float ax,float ay,float c1x,float c1y,float c2x,float c2y,float bx,float by,
                float t,float*ox,float*oy){
    float u=1-t, w0=u*u*u, w1=3*u*u*t, w2=3*u*t*t, w3=t*t*t;
    *ox = w0*ax+w1*c1x+w2*c2x+w3*bx;  *oy = w0*ay+w1*c1y+w2*c2y+w3*by;
}

// sample a loop arc into xs/ys; returns the count
static int gen_loop(float cx,float cy,float r,float a0,float sweep,float*xs,float*ys){
    int n=0; for (int i=0;i<=28;i++){ float a=a0+sweep*(float)i/28; xs[n]=cx+ux(a)*r; ys[n]=cy+uy(a)*r; n++; } return n;
}
// closest approach between two sampled curves (point-to-point; dense enough for this)
static float min_gap(const float*ax,const float*ay,int an,const float*bx,const float*by,int bn){
    float m=1e9f;
    for (int i=0;i<an;i++) for (int j=0;j<bn;j++){
        float d=distance((int)ax[i],(int)ay[i],(int)bx[j],(int)by[j]); if (d<m) m=d; }
    return m;
}

// ── PORT: where a ramp attaches — a point + the tangent direction (deg) traffic flows there ──
typedef struct { float x,y,dir; const char*name; } Port;

static void draw_port(Port p, int col){
    circfill((int)p.x,(int)p.y, 2, col);
    float tx = p.x+ux(p.dir)*13, ty = p.y+uy(p.dir)*13;        // tangent arrow
    line((int)p.x,(int)p.y,(int)tx,(int)ty, col);
    line((int)tx,(int)ty,(int)(tx+ux(p.dir+150)*5),(int)(ty+uy(p.dir+150)*5), col);
    line((int)tx,(int)ty,(int)(tx+ux(p.dir-150)*5),(int)(ty+uy(p.dir-150)*5), col);
}

// ── THE RAMP CONSTRUCTOR (Tier-1 closed-form): a curve A→B honoring both tangents, by TYPE ──
enum { R_DIRECT, R_FLYOVER, R_LOOP, R_COUNT };
static const char *RNAME[R_COUNT] = { "DIRECT", "FLYOVER", "LOOP" };

static void ramp(Port a, Port b, int type, int cas, int cen){
    float xs[96], ys[96]; int n=0;
    if (type == R_LOOP){
        // 270° loop tangent to A's direction, curling to the DRIVE side, then a bezier into B
        float d = distance((int)a.x,(int)a.y,(int)b.x,(int)b.y);
        float r = d*0.30f; if (r<14) r=14;
        float cx = a.x + ux(a.dir+90*drive)*r, cy = a.y + uy(a.dir+90*drive)*r;   // centre, DRIVE side
        float a0 = angle_to((int)cx,(int)cy,(int)a.x,(int)a.y);                    // A on the rim
        float sweep = 270.f*drive;
        for (int i=0;i<=24;i++){ float ang=a0+sweep*(float)i/24; xs[n]=cx+ux(ang)*r; ys[n]=cy+uy(ang)*r; n++; }
        float ex=xs[n-1], ey=ys[n-1];
        float edir = a0+sweep + ((sweep>0)?90.f:-90.f);                           // travel tangent at loop end
        float h = distance((int)ex,(int)ey,(int)b.x,(int)b.y)*0.40f;
        float c1x=ex+ux(edir)*h,  c1y=ey+uy(edir)*h;
        float c2x=b.x-ux(b.dir)*h, c2y=b.y-uy(b.dir)*h;
        for (int i=1;i<=20;i++){ float t=(float)i/20,ox,oy; bez(ex,ey,c1x,c1y,c2x,c2y,b.x,b.y,t,&ox,&oy); xs[n]=ox; ys[n]=oy; n++; }
    } else {
        // DIRECT = short handles; FLYOVER = long handles (a gentle semi-direct sweep). Same bezier.
        float d = distance((int)a.x,(int)a.y,(int)b.x,(int)b.y);
        float h = (type==R_FLYOVER? 0.65f : 0.40f) * d;
        float c1x=a.x+ux(a.dir)*h, c1y=a.y+uy(a.dir)*h;
        float c2x=b.x-ux(b.dir)*h, c2y=b.y-uy(b.dir)*h;
        for (int i=0;i<=30;i++){ float t=(float)i/30,ox,oy; bez(a.x,a.y,c1x,c1y,c2x,c2y,b.x,b.y,t,&ox,&oy); xs[n]=ox; ys[n]=oy; n++; }
    }
    ribbon(xs,ys,n, 4, cas, cen);
}

// ── state ──
static Port ports[MAXPTS]; static int nport=0;
static int selA=0, selB=2, ptype=R_LOOP;
static int   mode=0;        // 0 = ports/ramp demo, 1 = nested-loops SOLVE  (N toggles)
static float nestgap=7.f;   // target clearance between the two nested loops (px)

static void setup(void){
    if (nport) return;
    int CX=SCREEN_W/2, CY=SCREEN_H/2;
    ports[nport++] = (Port){ CX+8.f,  CY-14.f, 270.f, "trunk-N" };   // top of the vertical stub, heading up
    ports[nport++] = (Port){ CX-8.f,  CY+34.f,  90.f, "trunk-S" };   // vertical stub, heading down
    ports[nport++] = (Port){ 40.f,    CY-6.f,  180.f, "hw-W"    };    // horizontal road far lane, heading west
    ports[nport++] = (Port){ SCREEN_W-40.f, CY-6.f, 0.f, "hw-E"  };   // horizontal road far lane, heading east
}

void update(void){
    setup();
    if (keyp('N')) mode = !mode;
    if (mode==0){
        if (keyp(KEY_LEFT))  selA=(selA+nport-1)%nport;
        if (keyp(KEY_RIGHT)) selA=(selA+1)%nport;
        if (keyp(KEY_DOWN))  selB=(selB+nport-1)%nport;
        if (keyp(KEY_UP))    selB=(selB+1)%nport;
        if (keyp(KEY_SPACE)) ptype=(ptype+1)%R_COUNT;
        if (keyp('F'))       drive=-drive;
    } else {
        if (keyp(KEY_LEFT)||keyp(KEY_DOWN))  nestgap-=1;
        if (keyp(KEY_RIGHT)||keyp(KEY_UP))   nestgap+=1;
        if (nestgap<1) nestgap=1;
    }
}

// nested-loops SOLVE: two loops with DIFFERENT centres (like the real trumpet pair, so it's NOT the
// trivial concentric r_in = r_out - gap case). Relax the inner loop's radius until its closest approach
// to the outer loop equals the target gap — a real 1-parameter relaxation. Shows measured gap + iters.
static void draw_nest(void){
    cls(CLR_DARK_GREEN);
    int CX=SCREEN_W/2, CY=SCREEN_H/2;
    rectfill(0, CY-10, SCREEN_W, 20, CLR_DARKER_GREY);          // a road for context
    float Cox=CX-22, Coy=CY-34, Ro=46, a0=55, sweep=300;       // OUTER loop (fixed)
    float Cix=CX+4,  Ciy=CY-16;                                 // INNER loop centre — offset toward the trunk
    float ox[40],oy[40],ix[40],iy[40];
    int on=gen_loop(Cox,Coy,Ro,a0,sweep,ox,oy);
    float Ri=16; int it=0; float g=0; int in=gen_loop(Cix,Ciy,Ri,a0,sweep,ix,iy);
    for (; it<40; it++){                                        // ← the solver
        in=gen_loop(Cix,Ciy,Ri,a0,sweep,ix,iy);
        g=min_gap(ox,oy,on,ix,iy,in);
        float d=g-nestgap, ad=d<0?-d:d;
        if (ad<0.4f) break;
        Ri += 0.5f*d;                                          // gap too big → grow inner toward outer
        if (Ri<6) Ri=6;  if (Ri>Ro-2) Ri=Ro-2;
    }
    ribbon(ox,oy,on, 4, CLR_BROWNISH_BLACK, CLR_ORANGE);       // outer
    ribbon(ix,iy,in, 4, CLR_BROWNISH_BLACK, CLR_BLUE);         // inner (solved)
    char b[80];
    snprintf(b,sizeof b,"NEST SOLVE  target gap:%d  measured:%.1f  inner R:%.0f  iters:%d",
             (int)nestgap, g, Ri, it);
    print(b,4,4,CLR_WHITE);
    print("</> or ^v adjust gap     N: back to ports", 4, SCREEN_H-9, CLR_LIGHT_GREY);
}

void draw(void){
    setup();
    if (mode==1){ draw_nest(); return; }
    cls(CLR_DARK_GREEN);
    int CX=SCREEN_W/2, CY=SCREEN_H/2;
    // context roads (abstract): a horizontal road + a vertical stub up from the centre
    rectfill(0, CY-10, SCREEN_W, 20, CLR_DARKER_GREY);
    rectfill(CX-10, CY-10, 20, SCREEN_H-CY+10, CLR_DARKER_GREY);
    for (int i=0;i<nport;i++) if (i!=selA && i!=selB) draw_port(ports[i], CLR_MEDIUM_GREY);
    if (selA != selB) ramp(ports[selA], ports[selB], ptype, CLR_BROWNISH_BLACK, CLR_ORANGE);
    draw_port(ports[selA], CLR_GREEN);     // A = green
    draw_port(ports[selB], CLR_RED);       // B = red
    char buf[72];
    snprintf(buf,sizeof buf,"A:%s  ->  B:%s   [%s]   drive:%s",
             ports[selA].name, ports[selB].name, RNAME[ptype], drive>0?"R":"L");
    print(buf, 4, 4, CLR_WHITE);
    print("</> portA   ^v portB   SPACE type   F drive", 4, SCREEN_H-9, CLR_LIGHT_GREY);
}
