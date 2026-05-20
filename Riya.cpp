// =============================================================================
//  Riya.cpp  —  "A Nostalgic School Day Memory" | Member 3 (Features 41–60)
//  Scene 3 : Playground → Evening → Farewell
//
//  NEW / IMPROVED in this version:
//   • Sun moves LEFT → RIGHT across the sky (not just descends)
//   • Palm trees + coconut trees (replaces plain round trees)
//   • Moving clouds with gentle drift
//   • River in the distance with Bézier wave tide + two floating boats
//   • Bell auto-triggers when sun touches horizon (sunset event)
//   • Swing corrected: proper A-frame, seat hangs from TOP bar, pendulum physics
//   • Slide corrected: ladder on the LEFT, ramp goes down-right, kid slides down
//   • Gate slides open before students leave, closes after
//   • All keyboard controls:
//       SPACE – start football match
//       G     – force goal
//       S     – swing motion
//       L     – slide animation
//       R     – kids running
//       B     – bell ringing
//       E     – students leave / full exit sequence
//       W     – toggle wind on trees / clouds
//
//  glutTimerFunc drives all animation; nothing leaks to Sakib/Richi TUs.
// =============================================================================

#include <GL/glut.h>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>
#include "Riya.h"

// ─── math helpers ─────────────────────────────────────────────────────────────
static const float RPI = 3.14159265f;
static inline float rlerp(float a, float b, float t) { return a + (b - a) * t; }
static inline float rclamp01(float t) { return t < 0 ? 0 : t > 1 ? 1 : t; }
static inline float rabs(float v) { return v < 0 ? -v : v; }

// =============================================================================
//  State block  (all private to this TU)
// =============================================================================
namespace {

// ── timing ───────────────────────────────────────────────────────────────────
float  rTime   = 0.0f;
int    rLastMs = 0;

// ── sun ───────────────────────────────────────────────────────────────────────
// The sun travels along an arc: angle 0 = east horizon, 90 = noon, 180 = west horizon
// We map this to screen coords so it visually moves left → right
float  sunAngle   = 10.0f;    // starts near east horizon (left)
bool   sunMoving  = true;
bool   sunsetBellFired = false; // one-shot: bell rings at sunset

// ── sky / clouds ──────────────────────────────────────────────────────────────
struct Cloud { float x, y, speed, scale; };
static Cloud clouds[5] = {
    {-350, 130, 18.0f, 1.0f},
    {-150, 145, 12.0f, 0.75f},
    {  50, 120, 20.0f, 1.2f},
    { 200, 140, 15.0f, 0.9f},
    { 350, 125, 10.0f, 0.65f}
};

// ── river / boats ─────────────────────────────────────────────────────────────
float riverWaveT = 0.0f;   // drives Bézier wave animation
struct Boat { float x, y, speed; int dir; }; // dir: +1 right, -1 left
static Boat boats[2] = {
    {-300, -65, 20.0f, +1},
    { 200, -68, 16.0f, -1}
};

// ── football ──────────────────────────────────────────────────────────────────
bool  matchRunning = false;

struct Kid {
    float x, y, vx, vy;
    float r, g, b;
    float legPhase;
    bool  hasBall;
};
static Kid kids[6];

struct Ball { float x, y, vx, vy, radius, spinAngle; };
static Ball ball;

bool  goalScored     = false;
float goalCelebT     = 0.0f;
int   goalCount      = 0;
bool  ballInGoal     = false;

// ── swing ────────────────────────────────────────────────────────────────────
bool  swingActive   = false;
float swingAngle    = 0.0f;
float swingVelocity = 0.0f;
const float swingAmp = 32.0f;

// ── slide ────────────────────────────────────────────────────────────────────
bool  slideActive   = false;
float slideT        = 0.0f;
float slideCooldown = 0.0f;

// ── running kids ──────────────────────────────────────────────────────────────
bool  kidsRunning = false;
struct RunKid { float x, y, speed, legPhase; float r, g, b; };
static RunKid runKids[4];

// ── talking / playing bubbles ─────────────────────────────────────────────────
struct PlayBubble { float x, y, phase; bool active; };
static PlayBubble playBubbles[3];

// ── trees / wind ─────────────────────────────────────────────────────────────
float windPhase = 0.0f;
bool  windOn    = true;

// ── exit sequence ─────────────────────────────────────────────────────────────
bool  leavingPlayground = false;
float leavingT          = 0.0f;

bool  bellRinging  = false;
float bellSwing    = 0.0f;
float bellDecay    = 1.0f;

// Gate: opens (gateOpenT 0→1) before kids leave, then closes (gateCloseT 0→1)
float gateOpenT    = 0.0f;
bool  gateOpening  = false;
bool  gateFullOpen = false;
bool  gateClosing  = false;
float gateCloseT   = 0.0f;

struct ExitKid { float x, y; float r, g, b; bool done; };
static ExitKid exitKids[8];
bool  exitingGate  = false;

bool  fadeOut      = false;
float fadeAlpha    = 0.0f;
bool  showEndCard  = false;

} // anonymous namespace

// =============================================================================
//  Internal prototypes
// =============================================================================
namespace Riya {
    static void drawStr(float x, float y, const char* s,
                        void* font = GLUT_BITMAP_HELVETICA_12);
    static void skyColor(float angle, float* r, float* g, float* b);
    static void drawKidFigure(float x, float y, float r, float g, float b,
                               float legPh, bool mirrored = false);
    static void initKids();
    static void drawPalmTree(float x, float y, float scale, float lean);
    static void drawCoconutTree(float x, float y, float scale, float lean);
    static void drawCloud(float cx, float cy, float sc);
    static void tick(int);
}

// =============================================================================
//  Utility
// =============================================================================
namespace Riya {
static void drawStr(float x, float y, const char* s, void* font) {
    glRasterPos2f(x, y);
    while (*s) glutBitmapCharacter(font, *s++);
}
} // namespace Riya

// =============================================================================
//  Sky colour helper
//  angle: 0=east horizon, 90=noon, 180=west horizon, >180=dusk/night
// =============================================================================
namespace Riya {
static void skyColor(float angle, float* r, float* g, float* b) {
    // normalise to 0..1: 0=night, 1=noon
    float t = rclamp01((angle - 0.0f) / 120.0f); // peaks at 120°
    // clamp for dusk
    if (angle > 150.0f) {
        float du = rclamp01((angle - 150.0f) / 40.0f); // 0=still lit, 1=dark
        t = rlerp(t, 0.0f, du);
    }
    if (t > 0.55f) {
        float u = (t - 0.55f) / 0.45f;
        *r = rlerp(0.98f, 0.40f, u); *g = rlerp(0.60f, 0.65f, u); *b = rlerp(0.20f, 1.00f, u);
    } else if (t > 0.2f) {
        float u = (t - 0.2f) / 0.35f;
        *r = rlerp(0.40f, 0.98f, u); *g = rlerp(0.18f, 0.60f, u); *b = rlerp(0.35f, 0.20f, u);
    } else {
        float u = t / 0.2f;
        *r = rlerp(0.08f, 0.40f, u); *g = rlerp(0.05f, 0.18f, u); *b = rlerp(0.18f, 0.35f, u);
    }
}
} // namespace Riya

// =============================================================================
//  Feature 55 — animateSkyEveningColor
// =============================================================================
namespace Riya {
void animateSkyEveningColor() {
    float r, g, b;
    skyColor(sunAngle, &r, &g, &b);
    glClearColor(r, g, b, 1);
    glClear(GL_COLOR_BUFFER_BIT);

    // horizon glow strip
    float hr = rlerp(r, 1.0f, 0.35f), hg = rlerp(g, 0.55f, 0.20f), hb = rlerp(b, 0.10f, 0.3f);
    glBegin(GL_QUADS);
        glColor3f(hr, hg, hb); glVertex2f(-400, -80); glVertex2f(400, -80);
        glColor3f(r, g, b);    glVertex2f(400,  30); glVertex2f(-400, 30);
    glEnd();
}
} // namespace Riya

// =============================================================================
//  Cloud helper
// =============================================================================
namespace Riya {
static void drawCloud(float cx, float cy, float sc) {
    glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    // Build cloud from overlapping ellipses
    float parts[5][3] = {
        {0,0,22}, {-18,-5,16}, {18,-5,16}, {-9,10,18}, {9,10,18}
    };
    for (auto& p : parts) {
        float px = cx + p[0]*sc, py = cy + p[1]*sc, pr = p[2]*sc;
        glColor4f(1,1,1, 0.82f);
        glBegin(GL_TRIANGLE_FAN);
            glVertex2f(px, py);
            for (int i = 0; i <= 20; i++) {
                float a = 2*RPI*i/20;
                glVertex2f(px + cosf(a)*pr, py + sinf(a)*pr*0.62f);
            }
        glEnd();
    }
    glDisable(GL_BLEND);
}
} // namespace Riya

// =============================================================================
//  Feature 54 — animateSunsetMovement  (left → right arc)
// =============================================================================
namespace Riya {
void animateSunsetMovement() {
    // Map angle [0..180] to screen X [-360..360], Y via sine arc
    float rad    = sunAngle * RPI / 180.0f;
    float sx     = rlerp(-360.0f, 360.0f, sunAngle / 180.0f);
    float sy     = -60.0f + sinf(rad) * 210.0f;   // peaks at 150 above ground line

    if (sy < -100.0f) return; // below horizon, don't draw

    float t = rclamp01(sunAngle / 120.0f);
    // white noon → orange/red sunset
    float sr = 1.0f, sg = rlerp(0.30f, 0.90f, t), sb = rlerp(0.00f, 0.20f, t);

    // glow halo
    glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glBegin(GL_TRIANGLE_FAN);
        glColor4f(sr, sg * 0.55f, 0.0f, 0.0f);
        glVertex2f(sx, sy);
        glColor4f(sr, sg * 0.55f, 0.0f, 0.0f);
        for (int i = 0; i <= 40; i++) {
            float a = 2*RPI*i/40;
            glVertex2f(sx + cosf(a)*36, sy + sinf(a)*36);
        }
    glEnd();
    glDisable(GL_BLEND);

    // sun disc
    glColor3f(sr, sg, sb);
    glBegin(GL_TRIANGLE_FAN);
        glVertex2f(sx, sy);
        for (int i = 0; i <= 40; i++) {
            float a = 2*RPI*i/40;
            glVertex2f(sx + cosf(a)*18, sy + sinf(a)*18);
        }
    glEnd();

    // sun rays (only when well above horizon)
    if (sunAngle > 20.0f && sunAngle < 160.0f) {
        glColor3f(rlerp(sr, 1, 0.6f), rlerp(sg, 1, 0.4f), 0.0f);
        glLineWidth(1.5f);
        for (int i = 0; i < 12; i++) {
            float a = 2*RPI*i/12 + rTime*0.3f;
            glBegin(GL_LINES);
                glVertex2f(sx + cosf(a)*20, sy + sinf(a)*20);
                glVertex2f(sx + cosf(a)*30, sy + sinf(a)*30);
            glEnd();
        }
        glLineWidth(1.0f);
    }
}
} // namespace Riya

// =============================================================================
//  Clouds draw (called in drawFeatures)
// =============================================================================
namespace Riya {
static void drawClouds() {
    for (auto& c : clouds)
        drawCloud(c.x, c.y, c.scale);
}
} // namespace Riya

// =============================================================================
//  River + Bézier waves + boats (between sky and ground)
// =============================================================================
namespace Riya {
// Cubic Bézier point
static void bezierPoint(float t, float p0x, float p0y,
                         float p1x, float p1y,
                         float p2x, float p2y,
                         float p3x, float p3y,
                         float* ox, float* oy) {
    float u  = 1 - t;
    float uu = u*u, uuu = uu*u;
    float tt = t*t, ttt = tt*t;
    *ox = uuu*p0x + 3*uu*t*p1x + 3*u*tt*p2x + ttt*p3x;
    *oy = uuu*p0y + 3*uu*t*p1y + 3*u*tt*p2y + ttt*p3y;
}

static void drawRiver() {
    // River sits in a band at y = -68..-80 (distance, above grass)
    float top = -62.0f, bot = -78.0f;
    float wave = sinf(riverWaveT) * 4.0f; // tide amplitude

    // River body
    float rt, gt, bt;
    skyColor(sunAngle, &rt, &gt, &bt);
    // tint river bluer
    glColor3f(rt*0.4f + 0.15f, gt*0.5f + 0.20f, bt*0.8f + 0.35f);
    glBegin(GL_QUADS);
        glVertex2f(-400, bot); glVertex2f(400, bot);
        glVertex2f(400, top);  glVertex2f(-400, top);
    glEnd();

    // Bézier wave crests (3 overlapping waves)
    for (int w = 0; w < 3; w++) {
        float phase = riverWaveT + w * 2.1f;
        float wamp  = 3.0f + w * 1.5f;
        float wy    = top - 2.0f - w * 2.5f;

        glColor3f(1.0f, 1.0f, 1.0f);
        glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glColor4f(1, 1, 1, 0.35f - w * 0.08f);
        glBegin(GL_LINE_STRIP);
        for (int seg = 0; seg < 6; seg++) {
            float segW = 800.0f / 6;
            float x0   = -400.0f + seg * segW;
            float ox, oy;
            for (int step = 0; step <= 16; step++) {
                float t = step / 16.0f;
                float ph = phase + seg * 1.1f;
                bezierPoint(t,
                    x0,          wy,
                    x0 + segW*0.25f, wy + wamp*sinf(ph),
                    x0 + segW*0.75f, wy - wamp*sinf(ph + 1.0f),
                    x0 + segW,   wy,
                    &ox, &oy);
                glVertex2f(ox, oy);
            }
        }
        glEnd();
        glDisable(GL_BLEND);
    }

    // Reflective shimmer dots
    glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor4f(1, 1, 0.7f, 0.45f);
    glPointSize(2.0f);
    glBegin(GL_POINTS);
    for (int i = 0; i < 12; i++) {
        float rx = -380.0f + i * 65.0f + sinf(riverWaveT * 1.3f + i) * 8.0f;
        float ry = (top + bot) * 0.5f + sinf(riverWaveT + i * 0.9f) * 3.0f;
        glVertex2f(rx, ry);
    }
    glEnd();
    glPointSize(1.0f);
    glDisable(GL_BLEND);
}

static void drawBoats() {
    for (auto& b : boats) {
        float bx = b.x, by = b.y;
        // hull
        glColor3f(0.55f, 0.32f, 0.12f);
        glBegin(GL_TRIANGLE_FAN);
            glVertex2f(bx, by);
            glVertex2f(bx - 20*b.dir, by);
            glVertex2f(bx - 20*b.dir, by - 7);
            glVertex2f(bx + 5*b.dir,  by - 7);
            glVertex2f(bx + 22*b.dir, by - 3);
            glVertex2f(bx, by);
        glEnd();
        // mast
        glColor3f(0.70f, 0.55f, 0.30f);
        glLineWidth(2.0f);
        glBegin(GL_LINES);
            glVertex2f(bx - 2, by - 7);
            glVertex2f(bx - 2, by + 18);
        glEnd();
        glLineWidth(1.0f);
        // sail
        float sway = sinf(rTime * 0.9f + bx * 0.01f) * 2.0f;
        glColor3f(0.95f, 0.92f, 0.82f);
        glBegin(GL_TRIANGLES);
            glVertex2f(bx - 2,      by + 18);
            glVertex2f(bx - 2,      by - 5);
            glVertex2f(bx + 16*b.dir + sway, by + 8);
        glEnd();
    }
}
} // namespace Riya

// =============================================================================
//  Feature 41 — drawPlayground
// =============================================================================
namespace Riya {
void drawPlayground() {
    // sky already cleared by animateSkyEveningColor
    float t = rclamp01((sinf(sunAngle * RPI / 180.0f) + 0.1f) / 1.1f);

    // river band (in distance, above ground)
    drawRiver();
    drawBoats();

    // ── grass ─────────────────────────────────────────────────────────────────
    glColor3f(rlerp(0.10f, 0.22f, t), rlerp(0.28f, 0.55f, t), rlerp(0.08f, 0.18f, t));
    glBegin(GL_QUADS);
        glVertex2f(-400, -200); glVertex2f(400, -200);
        glVertex2f(400,  -78);  glVertex2f(-400, -78);
    glEnd();

    // grass tufts
    glColor3f(rlerp(0.08f, 0.18f, t), rlerp(0.25f, 0.50f, t), rlerp(0.06f, 0.14f, t));
    glLineWidth(1.0f);
    for (int i = -380; i < 400; i += 22) {
        for (int j = -180; j < -82; j += 18) {
            float wobble = sinf(i * 0.3f + j * 0.7f) * 3.0f;
            glBegin(GL_LINES);
                glVertex2f(i, j); glVertex2f(i + wobble, j + 5);
                glVertex2f(i+5, j); glVertex2f(i+5+wobble*0.5f, j+4);
            glEnd();
        }
    }

    // dirt path near gate (left side)
    glColor3f(0.55f, 0.42f, 0.28f);
    glBegin(GL_QUADS);
        glVertex2f(-400, -95); glVertex2f(-310, -95);
        glVertex2f(-310, -78); glVertex2f(-400, -78);
    glEnd();

    // low boundary wall
    glColor3f(0.72f, 0.68f, 0.62f);
    glBegin(GL_QUADS);
        glVertex2f(-400, -80); glVertex2f(400, -80);
        glVertex2f(400,  -76); glVertex2f(-400, -76);
    glEnd();
}
} // namespace Riya

// =============================================================================
//  Feature 42 — setupFootballGround
// =============================================================================
namespace Riya {
void setupFootballGround() {
    // pitch
    glColor3f(0.18f, 0.50f, 0.16f);
    glBegin(GL_QUADS);
        glVertex2f(-170, -175); glVertex2f(170, -175);
        glVertex2f(170,  -100); glVertex2f(-170, -100);
    glEnd();
    // alternating stripes
    glColor3f(0.16f, 0.46f, 0.14f);
    for (int s = 1; s < 5; s += 2) {
        float sx = -170.0f + s * 68.0f;
        glBegin(GL_QUADS);
            glVertex2f(sx, -175); glVertex2f(sx+68, -175);
            glVertex2f(sx+68, -100); glVertex2f(sx, -100);
        glEnd();
    }

    // markings
    glColor3f(0.95f, 0.95f, 0.95f); glLineWidth(1.5f);
    glBegin(GL_LINE_LOOP);
        glVertex2f(-168,-173); glVertex2f(168,-173);
        glVertex2f(168,-102); glVertex2f(-168,-102);
    glEnd();
    glBegin(GL_LINES);
        glVertex2f(0,-173); glVertex2f(0,-102);
    glEnd();
    glBegin(GL_LINE_LOOP);
    for (int i = 0; i < 32; i++) {
        float a = 2*RPI*i/32;
        glVertex2f(cosf(a)*20, -137.5f + sinf(a)*15);
    }
    glEnd();
    // penalty boxes
    glBegin(GL_LINE_LOOP);
        glVertex2f(-168,-128); glVertex2f(-148,-128);
        glVertex2f(-148,-148); glVertex2f(-168,-148);
    glEnd();
    glBegin(GL_LINE_LOOP);
        glVertex2f(168,-128); glVertex2f(148,-128);
        glVertex2f(148,-148); glVertex2f(168,-148);
    glEnd();
    glLineWidth(1.0f);

    // goalposts LEFT
    glColor3f(0.92f, 0.92f, 0.92f); glLineWidth(3.0f);
    glBegin(GL_LINE_STRIP);
        glVertex2f(-175,-125); glVertex2f(-168,-125);
        glVertex2f(-168,-150); glVertex2f(-175,-150);
    glEnd();
    // goalposts RIGHT
    glBegin(GL_LINE_STRIP);
        glVertex2f(175,-125); glVertex2f(168,-125);
        glVertex2f(168,-150); glVertex2f(175,-150);
    glEnd();
    // nets
    glColor3f(0.80f, 0.80f, 0.80f); glLineWidth(0.8f);
    for (int l = 0; l < 4; l++) {
        float ly = -125.0f - l * 6.0f;
        glBegin(GL_LINES);
            glVertex2f(-175, ly); glVertex2f(-168, ly);
            glVertex2f(175, ly);  glVertex2f(168, ly);
        glEnd();
    }
    glLineWidth(1.0f);
}
} // namespace Riya

// =============================================================================
//  Kid figure helper
// =============================================================================
namespace Riya {
static void drawKidFigure(float x, float y, float r, float g, float b,
                           float legPh, bool mirrored) {
    int dir = mirrored ? -1 : 1;
    // body
    glColor3f(r, g, b);
    glBegin(GL_QUADS);
        glVertex2f(x-5, y);    glVertex2f(x+5, y);
        glVertex2f(x+5, y+14); glVertex2f(x-5, y+14);
    glEnd();
    // head
    glColor3f(0.82f, 0.65f, 0.45f);
    glBegin(GL_TRIANGLE_FAN);
        glVertex2f(x, y+20);
        for (int i = 0; i <= 12; i++) {
            float a = 2*RPI*i/12;
            glVertex2f(x + cosf(a)*6, y+14 + sinf(a)*6);
        }
    glEnd();
    // legs
    float lSwing = sinf(legPh) * 12.0f;
    glColor3f(r*0.55f, g*0.55f, b*0.55f);
    glLineWidth(2.5f);
    glBegin(GL_LINES);
        glVertex2f(x-3, y);   glVertex2f(x-3 + dir*lSwing*0.35f, y-10);
        glVertex2f(x+3, y);   glVertex2f(x+3 - dir*lSwing*0.35f, y-10);
    glEnd();
    glLineWidth(1.0f);
    // arms
    glColor3f(r, g, b);
    glLineWidth(2.0f);
    glBegin(GL_LINES);
        glVertex2f(x-5, y+10); glVertex2f(x-5-dir*8, y+10-sinf(legPh)*5);
        glVertex2f(x+5, y+10); glVertex2f(x+5+dir*8, y+10+sinf(legPh)*5);
    glEnd();
    glLineWidth(1.0f);
}

static void initKids() {
    float colors[6][3] = {
        {0.15f,0.40f,0.75f},{0.75f,0.20f,0.15f},
        {0.15f,0.65f,0.25f},{0.65f,0.45f,0.10f},
        {0.60f,0.15f,0.60f},{0.20f,0.55f,0.65f}
    };
    float startX[6] = {-120,-60,0,30,80,120};
    for (int i = 0; i < 6; i++) {
        kids[i].x = startX[i]; kids[i].y = -155.0f;
        float spd = 30.0f + i*8.0f;
        kids[i].vx = (i%2==0 ? spd : -spd); kids[i].vy = 0;
        kids[i].r = colors[i][0]; kids[i].g = colors[i][1]; kids[i].b = colors[i][2];
        kids[i].legPhase = i * 1.0f;
        kids[i].hasBall  = (i == 0);
    }
    ball.x = kids[0].x; ball.y = kids[0].y - 10;
    ball.vx = 55.0f;    ball.vy = 0;
    ball.radius = 8.0f; ball.spinAngle = 0;
    ballInGoal = false; goalScored = false;
}
} // namespace Riya

// =============================================================================
//  Feature 43 — animateChildrenPlayingFootball
// =============================================================================
namespace Riya {
void animateChildrenPlayingFootball() {
    if (!matchRunning) return;
    for (int i = 0; i < 6; i++) {
        bool mir = (kids[i].vx < 0);
        drawKidFigure(kids[i].x, kids[i].y,
                      kids[i].r, kids[i].g, kids[i].b,
                      kids[i].legPhase, mir);
    }
}
} // namespace Riya

// =============================================================================
//  Feature 44 — animateBallMovement
// =============================================================================
namespace Riya {
void animateBallMovement() {
    if (!matchRunning) return;
    float bx = ball.x, by = ball.y, br = ball.radius;

    // ball shadow
    glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor4f(0, 0, 0, 0.18f);
    glBegin(GL_TRIANGLE_FAN);
        glVertex2f(bx, by-br);
        for (int i = 0; i <= 16; i++) {
            float a = 2*RPI*i/16;
            glVertex2f(bx + cosf(a)*br*0.9f, by-br + sinf(a)*2);
        }
    glEnd();
    glDisable(GL_BLEND);

    // ball body
    glColor3f(0.95f, 0.92f, 0.88f);
    glBegin(GL_TRIANGLE_FAN);
        glVertex2f(bx, by);
        for (int i = 0; i <= 24; i++) {
            float a = 2*RPI*i/24;
            glVertex2f(bx + cosf(a)*br, by + sinf(a)*br);
        }
    glEnd();

    // spin pattern (pentagon spokes)
    glColor3f(0.15f, 0.15f, 0.15f); glLineWidth(1.0f);
    for (int i = 0; i < 5; i++) {
        float a = ball.spinAngle*RPI/180.0f + 2*RPI*i/5;
        glBegin(GL_LINES);
            glVertex2f(bx, by);
            glVertex2f(bx + cosf(a)*br*0.7f, by + sinf(a)*br*0.7f);
        glEnd();
    }
    glBegin(GL_LINE_LOOP);
    for (int i = 0; i < 5; i++) {
        float a = ball.spinAngle*RPI/180.0f + 2*RPI*i/5;
        glVertex2f(bx + cosf(a)*br*0.65f, by + sinf(a)*br*0.65f);
    }
    glEnd();
}
} // namespace Riya

// =============================================================================
//  Feature 45 — animateGoalScoring
// =============================================================================
namespace Riya {
void animateGoalScoring() {
    if (!goalScored) return;
    float alpha = 0.6f + 0.4f*sinf(goalCelebT*8);
    glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor4f(1.0f, 0.9f, 0.1f, alpha*rclamp01(1.0f - goalCelebT/3.0f));
    glBegin(GL_QUADS);
        glVertex2f(-120,-110); glVertex2f(120,-110);
        glVertex2f(120, -90); glVertex2f(-120, -90);
    glEnd();
    glDisable(GL_BLEND);
    glColor3f(0.6f, 0.05f, 0.05f);
    char buf[32]; sprintf(buf, "GOAL! (%d)", goalCount);
    drawStr(-32, -104, buf, GLUT_BITMAP_HELVETICA_18);
    // celebration stars
    glColor3f(1, 0.85f, 0.1f); glPointSize(4.0f);
    glBegin(GL_POINTS);
    for (int s = 0; s < 10; s++) {
        float sa = 2*RPI*s/10;
        float sr = 50 + 30*sinf(goalCelebT*4 + s);
        glVertex2f(cosf(sa)*sr, -137.5f + sinf(sa)*sr*0.5f);
    }
    glEnd();
    glPointSize(1.0f);
}
} // namespace Riya

// =============================================================================
//  Feature 46 — setupSwing  (corrected: proper A-frame, seat below top bar)
// =============================================================================
namespace Riya {
void setupSwing() {
    // A-frame centred at x=220, ground at y=-80
    // The two legs form an inverted V; top bar is the apex
    float cx = 220.0f, gy = -80.0f;
    float topY  = gy - 60.0f;  // apex of the A-frame
    float spreadX = 40.0f;      // half-width at ground level

    glColor3f(0.50f, 0.32f, 0.12f);
    glLineWidth(4.0f);
    glBegin(GL_LINES);
        // left leg: from ground spread to apex
        glVertex2f(cx - spreadX, gy);
        glVertex2f(cx,           topY);
        // right leg: from ground spread to apex
        glVertex2f(cx + spreadX, gy);
        glVertex2f(cx,           topY);
        // cross brace (horizontal, mid-height)
        glVertex2f(cx - spreadX*0.55f, gy - 28);
        glVertex2f(cx + spreadX*0.55f, gy - 28);
    glEnd();
    glLineWidth(1.0f);

    // cap circle at apex
    glColor3f(0.60f, 0.42f, 0.18f);
    glBegin(GL_TRIANGLE_FAN);
        glVertex2f(cx, topY);
        for (int i = 0; i <= 12; i++) {
            float a = 2*RPI*i/12;
            glVertex2f(cx + cosf(a)*5, topY + sinf(a)*5);
        }
    glEnd();
}

// =============================================================================
//  Feature 47 — animateSwingMotion  (seat hangs from apex)
// =============================================================================
void animateSwingMotion() {
    float cx = 220.0f, gy = -80.0f;
    float topY    = gy - 60.0f;
    float chainLen = 42.0f;
    float swRad   = swingAngle * RPI / 180.0f;
    float seatX   = cx + sinf(swRad)*chainLen;
    float seatY   = topY - cosf(swRad)*chainLen;  // hangs downward

    // chains
    glColor3f(0.55f, 0.55f, 0.55f); glLineWidth(2.0f);
    glBegin(GL_LINES);
        glVertex2f(cx-4, topY); glVertex2f(seatX-4, seatY);
        glVertex2f(cx+4, topY); glVertex2f(seatX+4, seatY);
    glEnd();
    glLineWidth(1.0f);

    // seat plank
    glColor3f(0.50f, 0.32f, 0.12f);
    glPushMatrix();
        glTranslatef(seatX, seatY, 0);
        glRotatef(swingAngle, 0, 0, 1);
        glBegin(GL_QUADS);
            glVertex2f(-10,-2); glVertex2f(10,-2);
            glVertex2f(10,  2); glVertex2f(-10, 2);
        glEnd();
    glPopMatrix();

    // child sitting on seat
    if (swingActive || rabs(swingAngle) > 1.0f) {
        bool mirr = (swingAngle < 0);
        glPushMatrix();
            glTranslatef(seatX, seatY, 0);
            glRotatef(swingAngle * 0.4f, 0, 0, 1);
            // draw kid body relative to seat
            glColor3f(0.75f, 0.20f, 0.20f);
            glBegin(GL_QUADS);
                glVertex2f(-5, 0); glVertex2f(5, 0);
                glVertex2f(5, 12); glVertex2f(-5, 12);
            glEnd();
            // head
            glColor3f(0.82f, 0.65f, 0.45f);
            glBegin(GL_TRIANGLE_FAN);
                glVertex2f(0, 18);
                for (int i = 0; i <= 12; i++) {
                    float a = 2*RPI*i/12;
                    glVertex2f(cosf(a)*5, 12+sinf(a)*5);
                }
            glEnd();
            // dangling legs
            glColor3f(0.35f, 0.22f, 0.55f);
            glLineWidth(2.5f);
            float legSwing = sinf(swingAngle * 0.08f) * 8.0f;
            glBegin(GL_LINES);
                glVertex2f(-3, 0); glVertex2f(-3 + legSwing*0.5f, -10);
                glVertex2f(+3, 0); glVertex2f(+3 - legSwing*0.5f, -10);
            glEnd();
            glLineWidth(1.0f);
        glPopMatrix();
    }
}
} // namespace Riya

// =============================================================================
//  Feature 48 — setupSlide  (corrected: ladder LEFT, slide ramp goes right-down)
// =============================================================================
namespace Riya {
void setupSlide() {
    // Slide: ladder at x=-240, platform top at y=-130 (high point),
    //        slide ramp goes right and down to x=-160, y=-80 (ground level)
    float ladX = -240.0f;   // ladder x (left side)
    float topY  = -130.0f;  // platform height
    float gy    = -80.0f;   // ground level

    // ── Ladder (left, vertical) ───────────────────────────────────────────────
    glColor3f(0.60f, 0.60f, 0.62f);
    glLineWidth(3.0f);
    glBegin(GL_LINES);
        glVertex2f(ladX,    gy);   glVertex2f(ladX,    topY);
        glVertex2f(ladX+8,  gy);   glVertex2f(ladX+8,  topY);
    glEnd();
    // rungs
    for (int r = 0; r < 7; r++) {
        float ry = gy - 8 - r * (gy - topY - 8) / 7.0f;
        glBegin(GL_LINES);
            glVertex2f(ladX, ry); glVertex2f(ladX+8, ry);
        glEnd();
    }
    glLineWidth(1.0f);

    // ── Platform (top) ────────────────────────────────────────────────────────
    glColor3f(0.65f, 0.42f, 0.18f);
    glBegin(GL_QUADS);
        glVertex2f(ladX-2, topY);   glVertex2f(ladX+24, topY);
        glVertex2f(ladX+24, topY+6); glVertex2f(ladX-2, topY+6);
    glEnd();

    // ── Slide ramp (from platform top-right corner to ground right) ───────────
    float rampStartX = ladX + 20;
    float rampEndX   = ladX + 88;  // lands to the right
    glColor3f(0.78f, 0.35f, 0.12f);
    glBegin(GL_QUADS);
        glVertex2f(rampStartX,    topY+6);
        glVertex2f(rampStartX+10, topY+6);
        glVertex2f(rampEndX+10,   gy);
        glVertex2f(rampEndX,      gy);
    glEnd();
    // slide rails
    glColor3f(0.60f, 0.25f, 0.08f);
    glLineWidth(2.0f);
    glBegin(GL_LINES);
        glVertex2f(rampStartX,    topY+6); glVertex2f(rampEndX,    gy);
        glVertex2f(rampStartX+10, topY+6); glVertex2f(rampEndX+10, gy);
    glEnd();
    glLineWidth(1.0f);

    // ── Support leg under slide mid-point ─────────────────────────────────────
    float midX = (rampStartX + rampEndX) * 0.5f + 5;
    float midY = (topY + gy) * 0.5f;
    glColor3f(0.60f, 0.60f, 0.62f);
    glLineWidth(2.5f);
    glBegin(GL_LINES);
        glVertex2f(midX, midY); glVertex2f(midX, gy);
    glEnd();
    glLineWidth(1.0f);
}

// =============================================================================
//  Feature 49 — animateSliding  (kid travels along ramp, top-left → bottom-right)
// =============================================================================
void animateSliding() {
    if (!slideActive && slideT <= 0.0f) return;
    float ladX = -240.0f, topY = -130.0f, gy = -80.0f;
    float rampStartX = ladX + 20, rampEndX = ladX + 88;

    float t = slideT;
    float cx = rlerp(rampStartX + 5, rampEndX + 5, t);
    float cy = rlerp(topY + 6,       gy - 2,        t);

    glPushMatrix();
        glTranslatef(cx, cy, 0);
        // tilt kid to match ramp angle
        float angle = atan2f(gy - topY - 6, rampEndX - rampStartX) * 180.0f / RPI;
        glRotatef(angle * 0.5f, 0, 0, 1);
        drawKidFigure(0, 0, 0.15f, 0.65f, 0.85f, rTime*3.0f, false);
    glPopMatrix();
}
} // namespace Riya

// =============================================================================
//  Feature 50 — animateKidsRunning
// =============================================================================
namespace Riya {
void animateKidsRunning() {
    if (!kidsRunning) return;
    for (auto& k : runKids) {
        bool mir = (k.speed < 0);
        drawKidFigure(k.x, k.y, k.r, k.g, k.b, k.legPhase, mir);
    }
}
} // namespace Riya

// =============================================================================
//  Feature 51 — animateKidsPlaying
// =============================================================================
namespace Riya {
void animateKidsPlaying() {
    for (auto& pb : playBubbles) {
        if (!pb.active) continue;
        float sc = 0.75f + 0.25f*sinf(pb.phase);
        glColor3f(1, 1, 1);
        glBegin(GL_TRIANGLE_FAN);
            glVertex2f(pb.x, pb.y+8);
            for (int i = 0; i <= 16; i++) {
                float a = 2*RPI*i/16;
                glVertex2f(pb.x + cosf(a)*16*sc, pb.y+8 + sinf(a)*9*sc);
            }
        glEnd();
        glBegin(GL_TRIANGLES);
            glVertex2f(pb.x-4, pb.y+1); glVertex2f(pb.x+4, pb.y+1); glVertex2f(pb.x, pb.y-5);
        glEnd();
        glColor3f(0.3f, 0.3f, 0.3f); glPointSize(2.5f);
        glBegin(GL_POINTS);
            glVertex2f(pb.x-5, pb.y+9); glVertex2f(pb.x+5, pb.y+9);
        glEnd();
        glBegin(GL_LINE_STRIP);
        for (int i = 0; i <= 8; i++) {
            float a = RPI + RPI*i/8;
            glVertex2f(pb.x + cosf(a)*5, pb.y+6 + sinf(a)*3);
        }
        glEnd();
        glPointSize(1.0f);
    }
}
} // namespace Riya

// =============================================================================
//  Feature 52 — drawPlaygroundTrees  (palm + coconut trees)
// =============================================================================
namespace Riya {
static void drawPalmTree(float x, float y, float scale, float lean) {
    // Curved trunk using line strip segments
    glColor3f(0.45f, 0.30f, 0.12f);
    glLineWidth(5.0f * scale);
    glBegin(GL_LINE_STRIP);
    int segs = 10;
    for (int i = 0; i <= segs; i++) {
        float t = float(i)/segs;
        float tx = x + lean * t*t;            // curve increases toward top
        float ty = y + t * 55.0f * scale;
        glVertex2f(tx, ty);
    }
    glEnd();
    glLineWidth(1.0f);

    // trunk rings (horizontal marks)
    glColor3f(0.38f, 0.24f, 0.09f);
    glLineWidth(1.5f);
    for (int r = 1; r < 6; r++) {
        float t = float(r)/6;
        float rx = x + lean*t*t;
        float ry = y + t * 55.0f * scale;
        float rw = (5.0f - r*0.5f)*scale;
        glBegin(GL_LINES);
            glVertex2f(rx-rw, ry); glVertex2f(rx+rw, ry);
        glEnd();
    }
    glLineWidth(1.0f);

    // fronds (5 large arching leaves from top)
    float topX = x + lean;
    float topY = y + 55.0f * scale;
    float angles[5] = {-60, -30, 0, 30, 60};
    for (int f = 0; f < 5; f++) {
        float fa = (angles[f] + lean*1.5f + windPhase*8.0f*sinf(f*0.7f)) * RPI/180.0f;
        float frondLen = 30.0f * scale;
        glColor3f(0.10f, 0.50f + f*0.03f, 0.12f);
        glLineWidth(2.5f);
        glBegin(GL_LINE_STRIP);
        for (int s = 0; s <= 8; s++) {
            float st = float(s)/8;
            float fx = topX + sinf(fa)*frondLen*st + cosf(fa)*sinf(st*RPI)*5.0f*scale;
            float fy = topY + cosf(fa)*frondLen*st - sinf(fa)*sinf(st*RPI)*4.0f*scale;
            glVertex2f(fx, fy);
        }
        glEnd();
        // leaflets along frond
        glLineWidth(1.0f);
        glColor3f(0.12f, 0.48f, 0.14f);
        for (int s = 2; s <= 8; s += 2) {
            float st = float(s)/8;
            float fx = topX + sinf(fa)*frondLen*st;
            float fy = topY + cosf(fa)*frondLen*st;
            float lfa = fa + RPI*0.4f;
            float lfb = fa - RPI*0.4f;
            float llen = 8.0f*scale*(1.0f - st*0.4f);
            glBegin(GL_LINES);
                glVertex2f(fx, fy);
                glVertex2f(fx + sinf(lfa)*llen, fy + cosf(lfa)*llen);
                glVertex2f(fx, fy);
                glVertex2f(fx + sinf(lfb)*llen, fy + cosf(lfb)*llen);
            glEnd();
        }
    }
    glLineWidth(1.0f);
}

static void drawCoconutTree(float x, float y, float scale, float lean) {
    // Stouter, straighter trunk than palm
    glColor3f(0.48f, 0.33f, 0.14f);
    glBegin(GL_QUADS);
        glVertex2f(x-5*scale, y);
        glVertex2f(x+5*scale, y);
        glVertex2f(x+5*scale + lean*0.6f, y+50*scale);
        glVertex2f(x-5*scale + lean*0.6f, y+50*scale);
    glEnd();
    // trunk rings
    glColor3f(0.35f, 0.22f, 0.08f);
    for (int r = 1; r < 8; r++) {
        float ry = y + r*6.2f*scale;
        float rw = (5.0f - r*0.3f)*scale;
        glBegin(GL_LINES);
            glVertex2f(x-rw + lean*r*0.075f, ry);
            glVertex2f(x+rw + lean*r*0.075f, ry);
        glEnd();
    }

    float topX = x + lean*0.6f;
    float topY = y + 50.0f*scale;

    // coconuts (small circles huddled at base of fronds)
    glColor3f(0.45f, 0.30f, 0.10f);
    for (int c = 0; c < 3; c++) {
        float ca = (c - 1) * 0.4f;
        glBegin(GL_TRIANGLE_FAN);
            glVertex2f(topX + sinf(ca)*7*scale, topY - cosf(ca)*3*scale);
            for (int i = 0; i <= 10; i++) {
                float a2 = 2*RPI*i/10;
                glVertex2f(topX + sinf(ca)*7*scale + cosf(a2)*4*scale,
                           topY - cosf(ca)*3*scale + sinf(a2)*4*scale);
            }
        glEnd();
    }

    // fronds (denser, shorter)
    float anglesC[6] = {-80,-45,-15,15,45,80};
    for (int f = 0; f < 6; f++) {
        float fa = (anglesC[f] + lean*1.2f + windPhase*6.0f*sinf(f*0.9f)) * RPI/180.0f;
        float frondLen = 24.0f*scale;
        glColor3f(0.08f, 0.45f + f*0.02f, 0.10f);
        glLineWidth(2.0f);
        glBegin(GL_LINE_STRIP);
        for (int s = 0; s <= 8; s++) {
            float st = float(s)/8;
            float fx = topX + sinf(fa)*frondLen*st;
            float fy = topY + cosf(fa)*frondLen*st - sinf(st*RPI)*6.0f*scale;
            glVertex2f(fx, fy);
        }
        glEnd();
        glLineWidth(1.0f);
        // leaflets
        glColor3f(0.10f, 0.42f, 0.12f);
        for (int s = 2; s <= 7; s += 2) {
            float st = float(s)/8;
            float fx = topX + sinf(fa)*frondLen*st;
            float fy = topY + cosf(fa)*frondLen*st - sinf(st*RPI)*6.0f*scale;
            float lfa = fa + RPI*0.35f;
            float lfb = fa - RPI*0.35f;
            float llen = 7.0f*scale;
            glBegin(GL_LINES);
                glVertex2f(fx, fy); glVertex2f(fx+sinf(lfa)*llen, fy+cosf(lfa)*llen);
                glVertex2f(fx, fy); glVertex2f(fx+sinf(lfb)*llen, fy+cosf(lfb)*llen);
            glEnd();
        }
    }
    glLineWidth(1.0f);
}

void drawPlaygroundTrees() {
    float lean = windOn ? sinf(windPhase) * 5.5f : 0.0f;
    // left side: two palm trees
    drawPalmTree(-355, -80, 1.00f,  lean);
    drawPalmTree(-320, -80, 0.78f,  lean*0.65f);
    // right side: coconut trees
    drawCoconutTree(345, -80, 0.95f, -lean);
    drawCoconutTree(315, -80, 0.72f, -lean*0.7f);
    // behind field: small palms
    drawPalmTree(-200, -80, 0.60f,  lean*0.45f);
    drawCoconutTree(200, -80, 0.60f, -lean*0.4f);
}

void animatePlaygroundWind() { /* windPhase updated in tick */ }
} // namespace Riya

// =============================================================================
//  Feature 55 — animateSkyEveningColor   (defined above, body repeated here
//  to honour the header order; actual impl at top of file)
// =============================================================================
// (already defined above)

// =============================================================================
//  Feature 56 — animateStudentsLeavingPlayground
// =============================================================================
namespace Riya {
void animateStudentsLeavingPlayground() {
    if (!leavingPlayground) return;
    for (int i = 0; i < 8; i++) {
        if (exitKids[i].done) continue;
        drawKidFigure(exitKids[i].x, exitKids[i].y,
                      exitKids[i].r, exitKids[i].g, exitKids[i].b,
                      rTime*6.0f + i, true);
    }
}
} // namespace Riya

// =============================================================================
//  Feature 57 — animateEndOfDayBell
// =============================================================================
namespace Riya {
void animateEndOfDayBell() {
    float bx = 330.0f, by = 10.0f;
    // pole
    glColor3f(0.50f, 0.40f, 0.30f);
    glBegin(GL_QUADS);
        glVertex2f(bx-3, -80);     glVertex2f(bx+3, -80);
        glVertex2f(bx+3, by+38);   glVertex2f(bx-3, by+38);
    glEnd();

    float swing = bellRinging ? sinf(bellSwing)*22.0f*bellDecay : 0.0f;
    glPushMatrix();
        glTranslatef(bx, by+38, 0);
        glRotatef(swing, 0, 0, 1);
        glTranslatef(-bx, -(by+38), 0);

        // bell dome
        glColor3f(0.85f, 0.72f, 0.15f);
        glBegin(GL_TRIANGLE_FAN);
            glVertex2f(bx, by+36);
            for (int i = 0; i <= 20; i++) {
                float a = RPI + RPI*i/20;
                glVertex2f(bx + cosf(a)*18, by+36 + sinf(a)*18);
            }
        glEnd();
        // rim
        glColor3f(0.72f, 0.60f, 0.10f);
        glBegin(GL_QUADS);
            glVertex2f(bx-19, by+18); glVertex2f(bx+19, by+18);
            glVertex2f(bx+19, by+22); glVertex2f(bx-19, by+22);
        glEnd();
        // clapper
        float clapOff = bellRinging ? sinf(bellSwing*1.1f)*5*bellDecay : 0;
        glColor3f(0.55f, 0.45f, 0.10f);
        glBegin(GL_LINES); glVertex2f(bx, by+28); glVertex2f(bx+clapOff, by+20); glEnd();
        glBegin(GL_TRIANGLE_FAN);
            glVertex2f(bx+clapOff, by+20);
            for (int i = 0; i <= 10; i++) {
                float a = 2*RPI*i/10;
                glVertex2f(bx+clapOff+cosf(a)*3, by+20+sinf(a)*3);
            }
        glEnd();
    glPopMatrix();

    // DING DONG text
    if (bellRinging && bellDecay > 0.1f) {
        glColor3f(0.80f, 0.10f, 0.10f);
        float sc = 0.8f + 0.4f*sinf(bellSwing*3);
        glPushMatrix();
            glTranslatef(bx-55, by+24, 0);
            glScalef(sc, sc, 1);
            drawStr(0, 0, "DING DONG!");
        glPopMatrix();
    }
}
} // namespace Riya

// =============================================================================
//  Feature 58 — animateStudentsExitingGate
// =============================================================================
namespace Riya {
void animateStudentsExitingGate() {
    // students already walk to gate in leavingPlayground; nothing extra needed
}
} // namespace Riya

// =============================================================================
//  Feature 59 — animateGateClosing  (gate slides open first, then closes)
// =============================================================================
namespace Riya {
void animateGateClosing() {
    float gx = -360.0f, gy = -80.0f;
    float gw = 50.0f, gh = 42.0f;

    // pillars
    glColor3f(0.72f, 0.68f, 0.62f);
    glBegin(GL_QUADS);
        glVertex2f(gx-10, gy);      glVertex2f(gx,    gy);
        glVertex2f(gx,    gy+gh);   glVertex2f(gx-10, gy+gh);
        glVertex2f(gx+gw, gy);      glVertex2f(gx+gw+10, gy);
        glVertex2f(gx+gw+10, gy+gh); glVertex2f(gx+gw, gy+gh);
    glEnd();
    // arch
    glColor3f(0.60f, 0.18f, 0.15f);
    glBegin(GL_QUADS);
        glVertex2f(gx-10, gy+gh);    glVertex2f(gx+gw+10, gy+gh);
        glVertex2f(gx+gw+10, gy+gh+6); glVertex2f(gx-10, gy+gh+6);
    glEnd();

    // Gate leaves:
    //   When gateOpenT=0 (closed): left leaf at gx, right leaf at gx+gw/2
    //   When gateOpenT=1 (open):   both leaves pushed to sides (gap in middle)
    //   When gateCloseT>0: animate back to closed
    float openFrac  = rclamp01(gateOpenT  - gateCloseT);
    float halfW     = gw * 0.5f;

    // left door leaf: moves left when opening
    float leftLeafX  = gx + rlerp(0.0f, -halfW, openFrac);
    // right door leaf: moves right when opening
    float rightLeafX = gx + halfW + rlerp(0.0f, halfW, openFrac);

    glColor3f(0.32f, 0.20f, 0.08f);
    // left leaf
    glBegin(GL_QUADS);
        glVertex2f(leftLeafX,      gy+2); glVertex2f(leftLeafX+halfW, gy+2);
        glVertex2f(leftLeafX+halfW,gy+gh-2); glVertex2f(leftLeafX,   gy+gh-2);
    glEnd();
    // right leaf
    glBegin(GL_QUADS);
        glVertex2f(rightLeafX,     gy+2); glVertex2f(rightLeafX+halfW, gy+2);
        glVertex2f(rightLeafX+halfW,gy+gh-2); glVertex2f(rightLeafX,  gy+gh-2);
    glEnd();
    // bars on leaves
    glColor3f(0.22f, 0.14f, 0.06f);
    for (int b = 0; b < 3; b++) {
        float bxL = leftLeafX + 3 + b*7;
        float bxR = rightLeafX + 3 + b*7;
        for (float bxv : {bxL, bxR}) {
            glBegin(GL_QUADS);
                glVertex2f(bxv,   gy+2); glVertex2f(bxv+3, gy+2);
                glVertex2f(bxv+3, gy+gh-2); glVertex2f(bxv, gy+gh-2);
            glEnd();
        }
    }
}
} // namespace Riya

// =============================================================================
//  Feature 60 — animateFadeOutEnding
// =============================================================================
namespace Riya {
void animateFadeOutEnding() {
    if (!fadeOut && !showEndCard) return;
    glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor4f(0, 0, 0, fadeAlpha);
    glBegin(GL_QUADS);
        glVertex2f(-400,-200); glVertex2f(400,-200);
        glVertex2f(400,  200); glVertex2f(-400, 200);
    glEnd();
    glDisable(GL_BLEND);

    if (showEndCard) {
        glColor3f(0.95f, 0.90f, 0.70f);
        drawStr(-185, 30, "A Nostalgic School Day Memory", GLUT_BITMAP_HELVETICA_18);
        drawStr(-110,  5, "~ The End ~",                  GLUT_BITMAP_HELVETICA_18);
        glColor3f(0.70f, 0.70f, 0.70f);
        drawStr(-165,-25, "Thank you for watching our story.");
        drawStr(-120,-45, "Press N to replay from Scene 1.");
    }
}
} // namespace Riya

// =============================================================================
//  HUD
// =============================================================================
namespace Riya {
static void drawHUD() {
    glColor3f(0.12f, 0.12f, 0.20f);
    drawStr(-395, 185, "Scene 3: Playground  Riya's Features");
    glColor3f(0.20f, 0.18f, 0.14f);
    if (!leavingPlayground) {
        drawStr(-395, 172,
            "SPACE: play football | G: goal | S: swing | L: slide | R: run | B: bell | E: leave | W: wind");
    }
    // goal counter
    if (goalCount > 0) {
        char buf[32]; sprintf(buf, "Goals: %d", goalCount);
        drawStr(310, 172, buf);
    }
    if (!showEndCard && leavingPlayground && gateCloseT >= 1.0f && !fadeOut)
        drawStr(-110, -168, "Scene ending...");
}
} // namespace Riya

// =============================================================================
//  Master draw entry — drawFeatures()
// =============================================================================
namespace Riya {
void drawFeatures() {
    animateSkyEveningColor();          // F55: clear + sky gradient

    drawClouds();                      // Clouds (moving)
    animateSunsetMovement();           // F54: sun arc left→right

    drawPlayground();                  // F41: grass, river, boats, path, wall
    drawPlaygroundTrees();             // F52: palm + coconut trees
    animatePlaygroundWind();           // F53: wind state (no-op; tick updates)

    setupFootballGround();             // F42: pitch + goalposts
    animateChildrenPlayingFootball();  // F43: kids on pitch
    animateBallMovement();             // F44: ball
    animateGoalScoring();              // F45: goal celebration

    setupSwing();                      // F46: swing frame (A-frame)
    animateSwingMotion();              // F47: pendulum + kid

    setupSlide();                      // F48: slide structure (corrected)
    animateSliding();                  // F49: kid sliding down

    animateKidsRunning();              // F50: running kids
    animateKidsPlaying();              // F51: happy bubbles

    animateStudentsLeavingPlayground();// F56: exit walk
    animateEndOfDayBell();             // F57: bell on pole
    animateStudentsExitingGate();      // F58: through gate (no-op)
    animateGateClosing();              // F59: gate open/close

    animateFadeOutEnding();            // F60: black fade + end card

    drawHUD();
}
} // namespace Riya

// =============================================================================
//  Timer tick  (16 ms ≈ 60 fps)
// =============================================================================
namespace Riya {
static void tick(int) {
    int now = glutGet(GLUT_ELAPSED_TIME);
    float dt = (now - rLastMs) / 1000.0f;
    if (dt > 0.05f) dt = 0.05f;
    rLastMs = now; rTime += dt;

    // ── sun moves left → right across sky ────────────────────────────────────
    if (sunMoving && sunAngle < 185.0f)
        sunAngle += dt * 2.8f;  // full arc in about 65 seconds

    // ── auto-trigger bell at sunset (sunAngle ≈ 165°) ────────────────────────
    if (!sunsetBellFired && sunAngle >= 162.0f) {
        sunsetBellFired = true;
        if (!bellRinging) {
            bellRinging = true; bellSwing = 0; bellDecay = 1.0f;
        }
    }

    // ── clouds drift right (wrap around) ─────────────────────────────────────
    for (auto& c : clouds) {
        if (windOn) c.x += c.speed * dt;
        if (c.x > 430) c.x = -430;
    }

    // ── river wave ───────────────────────────────────────────────────────────
    riverWaveT += dt * 1.4f;

    // ── boats drift ──────────────────────────────────────────────────────────
    for (auto& b : boats) {
        b.x += b.speed * b.dir * dt;
        b.y = -67.0f + sinf(riverWaveT * 1.2f + b.x * 0.02f) * 1.8f;
        if (b.dir == +1 && b.x > 420) b.x = -420;
        if (b.dir == -1 && b.x < -420) b.x = 420;
    }

    // ── wind phase ───────────────────────────────────────────────────────────
    if (windOn) windPhase += 1.6f * dt;

    // ── football AI ──────────────────────────────────────────────────────────
    if (matchRunning && !ballInGoal) {
        ball.x += ball.vx*dt; ball.y += ball.vy*dt;
        ball.spinAngle += ball.vx*dt*2.0f;
        if (ball.x >  168) { ball.vx = -rabs(ball.vx)*0.85f; }
        if (ball.x < -168) { ball.vx =  rabs(ball.vx)*0.85f; }
        if (ball.y > -102) { ball.vy = -rabs(ball.vy)*0.85f; }
        if (ball.y < -173) { ball.vy =  rabs(ball.vy)*0.85f; }
        // friction
        ball.vx *= 0.999f; ball.vy *= 0.999f;

        // goal check (right goal)
        if (ball.x >= 165 && ball.y >= -152 && ball.y <= -123) {
            ballInGoal = true; goalScored = true; goalCount++;
            goalCelebT = 0;
            glutTimerFunc(3000, [](int){
                ballInGoal = false; goalScored = false;
                ball.x = 0; ball.y = -137.5f;
                ball.vx = 50; ball.vy = 20;
            }, 0);
        }

        // kid AI
        for (int i = 0; i < 6; i++) {
            float tx, ty;
            if (i < 3) { tx = ball.x + (i-1)*25; ty = ball.y + (i-1)*10; }
            else       { tx = -ball.x + (i-4)*30; ty = -137.5f; }
            float dx = tx-kids[i].x, dy = ty-kids[i].y;
            float dist = sqrtf(dx*dx+dy*dy);
            if (dist > 2) {
                float spd = 45.0f + i*5;
                kids[i].vx = dx/dist*spd; kids[i].vy = dy/dist*spd;
                kids[i].x += kids[i].vx*dt; kids[i].y += kids[i].vy*dt;
                kids[i].legPhase += 5.0f*dt;
            }
            if (kids[i].x < -165) kids[i].x=-165;
            if (kids[i].x >  165) kids[i].x= 165;
            if (kids[i].y < -173) kids[i].y=-173;
            if (kids[i].y > -103) kids[i].y=-103;
        }
    }
    if (goalScored) goalCelebT += dt;

    // ── swing pendulum ────────────────────────────────────────────────────────
    if (swingActive) {
        float g = 280.0f, L = 42.0f;
        float alpha = -(g/L)*sinf(swingAngle*RPI/180.0f);
        swingVelocity += alpha*dt*3.5f;
        swingAngle    += swingVelocity*dt*55.0f;
        swingVelocity *= 0.998f;
        if (rabs(swingAngle) < 0.5f && rabs(swingVelocity) < 0.5f)
            swingActive = false;
    }

    // ── slide ────────────────────────────────────────────────────────────────
    if (slideActive) {
        slideT += dt * 0.55f;
        if (slideT >= 1.0f) { slideT=0; slideActive=false; slideCooldown=1.5f; }
    }
    if (slideCooldown > 0) slideCooldown -= dt;

    // ── running kids ──────────────────────────────────────────────────────────
    if (kidsRunning) {
        for (auto& k : runKids) {
            k.x += k.speed*dt;
            k.legPhase += 6.0f*dt;
            if (k.x >  430) k.x=-430;
            if (k.x < -430) k.x= 430;
        }
    }

    // ── talk bubbles ──────────────────────────────────────────────────────────
    for (auto& pb : playBubbles) if (pb.active) pb.phase += 2.2f*dt;

    // ── bell decay ────────────────────────────────────────────────────────────
    if (bellRinging) {
        bellSwing += 10.0f*dt;
        bellDecay -= dt * 0.14f;
        if (bellDecay <= 0) {
            bellDecay=0; bellRinging=false;
            // auto-trigger exit if this was the sunset bell
            if (sunsetBellFired && !leavingPlayground) {
                glutTimerFunc(800, [](int){ leavingPlayground=true; }, 0);
            }
        }
    }

    // ── gate opens when students start leaving ────────────────────────────────
    if (leavingPlayground && !gateFullOpen) {
        gateOpenT += dt * 1.2f;
        if (gateOpenT >= 1.0f) { gateOpenT=1.0f; gateFullOpen=true; }
    }

    // ── students walk toward gate ─────────────────────────────────────────────
    if (leavingPlayground) {
        leavingT += dt * 0.35f;
        for (int i = 0; i < 8; i++) {
            if (exitKids[i].done) continue;
            float delay = i * 0.18f;
            float mt = rclamp01((leavingT - delay) / 1.2f);
            exitKids[i].x = rlerp(exitKids[i].x, -395.0f, mt*dt*1.8f);
            if (exitKids[i].x < -388) exitKids[i].done = true;
        }
        bool allDone = true;
        for (auto& e : exitKids) if (!e.done) { allDone=false; break; }
        if (allDone && !gateClosing) {
            gateClosing   = true;
            exitingGate   = true;
        }
    }

    // ── gate closes ───────────────────────────────────────────────────────────
    if (gateClosing && gateCloseT < 1.0f) {
        gateCloseT += dt * 0.7f;
        if (gateCloseT >= 1.0f) {
            gateCloseT = 1.0f;
            glutTimerFunc(800, [](int){ fadeOut=true; }, 0);
        }
    }

    // ── fade out ──────────────────────────────────────────────────────────────
    if (fadeOut && fadeAlpha < 1.0f) {
        fadeAlpha += dt * 0.4f;
        if (fadeAlpha >= 1.0f) { fadeAlpha=1.0f; fadeOut=false; showEndCard=true; }
    }

    glutPostRedisplay();
    glutTimerFunc(16, tick, 0);
}
} // namespace Riya

// =============================================================================
//  Keyboard handler
// =============================================================================
namespace Riya {
void handleKey(unsigned char key, int, int) {
    switch (key) {
        case ' ':
            if (!matchRunning && !leavingPlayground) {
                matchRunning = true;
                initKids();
                playBubbles[0] = {-80,-95,0.0f,true};
                playBubbles[1] = { 20,-95,1.1f,true};
                playBubbles[2] = {120,-95,2.2f,true};
            }
            break;
        case 'g': case 'G':
            if (matchRunning && !goalScored) {
                ball.x=166; ball.y=-137.5f;
                ball.vx=5;  ball.vy=0;
            }
            break;
        case 's': case 'S':
            if (!swingActive && !leavingPlayground) {
                swingActive=true; swingAngle=swingAmp; swingVelocity=0;
            }
            break;
        case 'l': case 'L':
            if (!slideActive && slideCooldown<=0 && !leavingPlayground) {
                slideActive=true; slideT=0;
            }
            break;
        case 'r': case 'R':
            if (!kidsRunning && !leavingPlayground) {
                kidsRunning=true;
                float rc[4][3]={{0.80f,0.25f,0.10f},{0.10f,0.45f,0.75f},
                                {0.15f,0.60f,0.25f},{0.65f,0.15f,0.60f}};
                float ry[4]={-88,-93,-88,-93};
                float spd[4]={62,-52,70,-58};
                for (int i = 0; i < 4; i++) {
                    runKids[i]={spd[i]>0?-430.0f:430.0f, ry[i],
                                spd[i], float(i), rc[i][0],rc[i][1],rc[i][2]};
                }
            }
            break;
        case 'b': case 'B':
            if (!bellRinging && !leavingPlayground) {
                bellRinging=true; bellSwing=0; bellDecay=1.0f;
            }
            break;
        case 'e': case 'E':
            if (!leavingPlayground) {
                float ec[8][3]={{0.15f,0.40f,0.75f},{0.75f,0.20f,0.15f},
                                {0.15f,0.65f,0.25f},{0.65f,0.45f,0.10f},
                                {0.60f,0.15f,0.60f},{0.20f,0.55f,0.65f},
                                {0.80f,0.30f,0.10f},{0.25f,0.60f,0.40f}};
                float eys[8]={-90,-95,-90,-95,-88,-93,-90,-95};
                float exs[8]={100,60,-20,140,180,-60,220,80};
                for (int i = 0; i < 8; i++) {
                    exitKids[i]={exs[i],eys[i],ec[i][0],ec[i][1],ec[i][2],false};
                }
                matchRunning=false; kidsRunning=false;
                leavingPlayground=true; leavingT=0;
            }
            break;
        case 'w': case 'W':
            windOn = !windOn;
            break;
    }
}
} // namespace Riya

// =============================================================================
//  Init
// =============================================================================
namespace Riya {
void init() {
    rLastMs = glutGet(GLUT_ELAPSED_TIME);
    glutTimerFunc(16, tick, 0);
}
} // namespace Riya
