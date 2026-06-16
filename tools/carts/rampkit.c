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
    if (keyp(KEY_LEFT))  selA=(selA+nport-1)%nport;
    if (keyp(KEY_RIGHT)) selA=(selA+1)%nport;
    if (keyp(KEY_DOWN))  selB=(selB+nport-1)%nport;
    if (keyp(KEY_UP))    selB=(selB+1)%nport;
    if (keyp(KEY_SPACE)) ptype=(ptype+1)%R_COUNT;
    if (keyp('F'))       drive=-drive;
}

void draw(void){
    setup();
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
