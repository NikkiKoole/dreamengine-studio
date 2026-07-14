#include <stdio.h>
#include "box2d/box2d.h"
int main(void) {
    b2WorldDef wd = b2DefaultWorldDef();
    wd.gravity = (b2Vec2){0.0f, -10.0f};
    b2WorldId world = b2CreateWorld(&wd);

    // static ground segment along y=0
    b2BodyDef gd = b2DefaultBodyDef();
    b2BodyId ground = b2CreateBody(world, &gd);
    b2Segment seg = {{-20.0f, 0.0f}, {20.0f, 0.0f}};
    b2ShapeDef sd = b2DefaultShapeDef();
    b2CreateSegmentShape(ground, &sd, &seg);

    // dynamic box dropped from y=8
    b2BodyDef bd = b2DefaultBodyDef();
    bd.type = b2_dynamicBody;
    bd.position = (b2Vec2){0.0f, 8.0f};
    b2BodyId box = b2CreateBody(world, &bd);
    b2Polygon poly = b2MakeBox(0.5f, 0.5f);
    b2ShapeDef bsd = b2DefaultShapeDef();
    b2CreatePolygonShape(box, &bsd, &poly);

    for (int i = 0; i <= 120; i++) {
        b2World_Step(world, 1.0f/60.0f, 4);
        if (i % 30 == 0) { b2Vec2 p = b2Body_GetPosition(box); printf("frame %3d: box y = %.4f\n", i, p.y); }
    }
    b2Vec2 f = b2Body_GetPosition(box);
    printf("settled y = %.4f (expect ~0.5, the box half-height resting on the ground)\n", f.y);
    b2DestroyWorld(world);
    return 0;
}
