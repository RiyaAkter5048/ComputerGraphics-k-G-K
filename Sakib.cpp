//  Sakib.cpp  —  "A Nostalgic School Day Memory" | Member 1 (Features 1–20)
//  Scene 1 : Morning Arrival
//
//  Keyboard controls (active in Scene 1):
//    SPACE       – launch bus toward the gate
//    G           – open / close gate manually
//    C           – toggle cloud movement
//    B           – toggle bird flight
//    W           – toggle wind on trees
//    +/-         – control sun/moon speed only
//    1/2         – control cloud speed
//    3/4         – control bird speed
//    5/6         – control vehicle speed
//    7/8         – control river tide speed
//    P           – pause / resume sun/moon movement only
//    Arrow LEFT  – move boats backward
//    Arrow RIGHT – move boats forward
//    N           – go to Scene 2 (pass control to Richi)
//
//  Day/night cycle: sun rises → day → sun sets → moon rises → night → moon sets → sun rises
//  All animation driven by glutTimerFunc; no globals leak into other TUs.

#include <GL/glut.h>
#include <cmath>
#include <cstring>
#include <cstdio>
#include "Sakib.h"

static const float PI = 3.14159265f;
static inline float lerp(float a, float b, float t){ return a+(b-a)*t; }
static inline float clamp01(float t){ return t<0?0:t>1?1:t; }

//  State block  (all private to this translation unit)
namespace {

float  gTime        = 0.0f;

float  sunScale     = 1.0f;   // +/- keys
float  cloudScale   = 1.0f;   // 1/2 keys
float  birdScale    = 1.0f;   // 3/4 keys
float  vehicleScale = 1.0f;   // 5/6 keys
float  tideScale    = 1.0f;   // 7/8 keys

bool   sunPaused    = false;  // P key

// cycleAngle goes 0 → 360 continuously.
// 0–180: daytime arc (sun)
// 180–360: nighttime arc (moon)
// We map sunAngle -30..70 for sun during 0..180,
// and moonAngle -30..70 for moon during 180..360
float  cycleAngle   = 0.0f;   // 0..360 degrees, drives the full day/night cycle

// Derived helpers:
inline float getSunAngle()  { return (cycleAngle <= 180.0f)
    ? lerp(-30.0f, 70.0f, cycleAngle / 180.0f) : -999.0f; }
inline float getMoonAngle() { return (cycleAngle > 180.0f)
    ? lerp(-30.0f, 70.0f, (cycleAngle - 180.0f) / 180.0f) : -999.0f; }
inline bool  isDaytime()    { return cycleAngle <= 180.0f; }

struct Star { float x, y, twinkle; };
static Star stars[80];
static bool starsInited = false;

struct Cloud { float x, y, baseSpeed; };
static Cloud clouds[5] = {
    {-300, 130, 18},
    { -80, 155, 12},
    { 100, 140, 15},
    { 250, 125, 10},
    {-180, 145, 14},
};
bool cloudsMoving = true;

struct Bird { float x, y, wingPhase, baseSpeed; };
static Bird birds[6] = {
    {-350,170, 0.0f, 38},
    {-320,185, 1.0f, 42},
    {-380,160, 2.1f, 35},
    {-290,175, 0.7f, 45},
    {-360,190, 1.5f, 40},
    {-400,162, 0.3f, 36},
};
bool birdsFlying = true;

// Road layout (Y coordinates):
//   -20  : road top / pavement bottom
//   -60  : lane divider (yellow dashed)
//   -100 : road bottom / bank top
// Upper lane (school side): y centre ≈ -40  → other vehicles travel here, L→R
// Lower lane (river side):  y centre ≈ -80  → bus + vehicles R→L or L→R
//
// Bus wheel radius = 11. Bus body bottom sits on lower lane surface.
// Lower lane top surface = -67 (lane divider).
// Bus bottom of body (wheels top) at y = -67.
// Wheel centre: -67 - 11 = -78. So busY = -67 (body starts here).
static const float UPPER_LANE_Y = -8.0f;  // upper lane centre (L→R, bus lane)
static const float LOWER_LANE_Y = -100.0f; // lower lane centre (R→L)  // body bottom for bus on lower lane

float  busX          = -520.0f;
float  busTargetX    = -170.0f;  // stops near left side of building (lower lane)
bool   busMoving     = false;
bool   busArrived    = false;
float  busSpeed      = 80.0f;

float  studentOffT   = 0.0f;

struct Student { float x, y; bool active; };
static Student students[5];
bool  studentsWalking = true;

float  gateOpenT   =  0.0f;
bool   gateOpening = false;
bool   gateOpen    = false;
bool   gateClosing = false;

float  guardArmAngle = 0.0f;

float  flagWave = 0.0f;

float  windPhase  = 0.0f;
bool   windOn     = true;

// Upper lane (school side, y≈-40):  bus + vehicles moving L→R
// Lower lane (river side, y≈-80):   vehicles moving R→L
struct Vehicle {
    float x;
    float laneY;
    int   type;
    float baseSpeed;  // px/s positive=L→R, negative=R→L
    int   spawnIdx;
};
// Upper lane: vehicles move L→R (positive speed). laneY = UPPER_LANE_Y = -20
// Lower lane: other vehicles move R→L (negative speed). laneY = LOWER_LANE_Y = -67
static Vehicle vehicles[4] = {
    {-440.0f, -54.0f, 0,  45.0f, 1},
    { 440.0f, LOWER_LANE_Y, 2, -48.0f, 3},
    {-200.0f, -54.0f, 1,  35.0f, 2},
    { 340.0f, LOWER_LANE_Y, 3, -52.0f, 0},
};
static int slot0Types[] = {0, 1, 3, 0};
static int slot1Types[] = {2, 1, 3};
static int slot2Types[] = {1, 0, 2};
static int slot3Types[] = {3, 0, 1};

// River band well below road: y ≈ -155 to -200
static float riverPts[4][2] = {
    {-400,-172}, {-150,-188}, {150,-188}, {400,-172}
};
float  boatT[2]    = {0.10f, 0.60f};
bool   boatMoving  = true;

float  tidePhase   = 0.0f;

bool   showNextMsg = false;

int    lastMs = 0;

} // anonymous namespace

//  Forward declarations
namespace Sakib {
static void tick(int);
static void skyColor(float angleDeg, bool isDay, float* r, float* g, float* b);
static void drawString(float x, float y, const char* s);
static float bezierVal(float t, float p0,float p1,float p2,float p3);
static float bezierX(float t);
static float bezierY(float t);
static void drawBackgroundCity();
static void drawTree(float x, float y, float scale, float lean);
static void drawDatePalm(float x, float y, float scale, float lean);
static void drawCoconutTree(float x, float y, float scale, float lean);
static void drawCloud(float cx, float cy);
static void drawBirdFigure(float x, float y, float phase);
static void drawStudentFigure(float x, float y, float r, float g, float b);
static void drawRiver();
static void drawBoat(float t, float yOff);
static void drawVehicle(float cx, float cy, int type, bool facingLeft);
static void drawBusStopSign(float x, float y);
static void drawHUD();
static void initStars();
static void drawStars(float nightFactor);
static void drawMoon(float moonAngle);
static void drawSun(float sunAngle);
}

//  Utility helpers
namespace Sakib {

static void drawString(float x, float y, const char* s){
    glRasterPos2f(x,y);
    while(*s) glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12, *s++);
}

// Sky colour based on the active body (sun or moon) and its angle.
// isDayPhase=true → sun arc; isDayPhase=false → moon/night arc.
static void skyColor(float angle, bool isDayPhase, float* r, float* g, float* b){
    if(!isDayPhase){
        // Night: deep blue/indigo during full night, slight pre-dawn as moon sets
        float moonUp = clamp01((angle + 10.0f) / 80.0f); // 0=deep night, 1=late night
        *r = lerp(0.02f, 0.06f, moonUp);
        *g = lerp(0.02f, 0.04f, moonUp);
        *b = lerp(0.14f, 0.22f, moonUp);
        return;
    }
    // Daytime sun arc
    if(angle < -10.0f){
        float u = clamp01((angle + 30.0f) / 20.0f);
        *r = lerp(0.03f, 0.15f, u);
        *g = lerp(0.03f, 0.10f, u);
        *b = lerp(0.18f, 0.30f, u);
    } else if(angle < 0.0f){
        float u = clamp01((angle + 10.0f) / 10.0f);
        *r = lerp(0.15f, 0.90f, u);
        *g = lerp(0.10f, 0.48f, u);
        *b = lerp(0.30f, 0.28f, u);
    } else if(angle < 15.0f){
        float u = clamp01(angle / 15.0f);
        *r = lerp(0.90f, 0.55f, u);
        *g = lerp(0.48f, 0.72f, u);
        *b = lerp(0.28f, 0.92f, u);
    } else if(angle < 60.0f){
        float u = clamp01((angle - 15.0f) / 45.0f);
        *r = lerp(0.55f, 0.38f, u);
        *g = lerp(0.72f, 0.65f, u);
        *b = lerp(0.92f, 1.00f, u);
    } else {
        // Sunset: angle 60..70 → orange-red dusk
        float u = clamp01((angle - 60.0f) / 10.0f);
        *r = lerp(0.38f, 0.88f, u);
        *g = lerp(0.65f, 0.35f, u);
        *b = lerp(1.00f, 0.18f, u);
    }
}

static void initStars(){
    unsigned int s = 98765;
    for(int i=0;i<80;i++){
        s = s*1664525u + 1013904223u;
        stars[i].x = -390.0f + (s & 0xFFFF) / 65535.0f * 780.0f;
        s = s*1664525u + 1013904223u;
        stars[i].y = 10.0f + (s & 0xFFFF) / 65535.0f * 185.0f;
        s = s*1664525u + 1013904223u;
        stars[i].twinkle = (s & 0xFFFF) / 65535.0f * 2.0f * PI;
    }
    starsInited = true;
}

// nightFactor: 0=full day, 1=full night
static void drawStars(float nightFactor){
    if(!starsInited) initStars();
    if(nightFactor <= 0.0f) return;
    glPointSize(2.0f);
    glBegin(GL_POINTS);
    for(int i=0;i<80;i++){
        float tw = 0.60f + 0.40f * sinf(stars[i].twinkle + gTime * 2.5f);
        float bright = nightFactor * tw;
        glColor3f(bright, bright, bright * 0.88f + 0.12f);
        glVertex2f(stars[i].x, stars[i].y);
    }
    glEnd();
    glPointSize(1.0f);
}

// Draw the moon as a crescent or full disc
static void drawMoon(float moonAngle){
    if(moonAngle < -28.0f || moonAngle > 72.0f) return;

    float rad = (moonAngle + 90.0f) * PI / 180.0f;
    float mx  = 220.0f * cosf(rad) + 30.0f;
    float my  = 160.0f * sinf(rad) - 40.0f;
    if(my < -90.0f) return;

    float moonR = 18.0f;

    // Soft glow
    int segs = 40;
    glBegin(GL_TRIANGLE_FAN);
        glColor3f(0.90f, 0.90f, 0.70f);
        glVertex2f(mx, my);
        glColor3f(0.10f, 0.10f, 0.14f);
        for(int i=0;i<=segs;i++){
            float a = 2*PI*i/segs;
            glVertex2f(mx + cosf(a)*moonR*2.2f, my + sinf(a)*moonR*2.2f);
        }
    glEnd();

    // Moon disc
    glColor3f(0.96f, 0.96f, 0.80f);
    glBegin(GL_TRIANGLE_FAN);
        glVertex2f(mx,my);
        for(int i=0;i<=segs;i++){
            float a=2*PI*i/segs;
            glVertex2f(mx+cosf(a)*moonR, my+sinf(a)*moonR);
        }
    glEnd();

    // Crescent shadow to make it look like a crescent moon
    glColor3f(0.04f, 0.04f, 0.18f);
    glBegin(GL_TRIANGLE_FAN);
        glVertex2f(mx+5, my);
        for(int i=0;i<=segs;i++){
            float a=2*PI*i/segs;
            glVertex2f(mx+5+cosf(a)*moonR*0.88f, my+sinf(a)*moonR);
        }
    glEnd();

    // Small craters
    glColor3f(0.82f,0.82f,0.66f);
    glBegin(GL_TRIANGLE_FAN);
        glVertex2f(mx-5,my+4);
        for(int i=0;i<=10;i++){
            float a=2*PI*i/10;
            glVertex2f(mx-5+cosf(a)*3, my+4+sinf(a)*3);
        }
    glEnd();
    glBegin(GL_TRIANGLE_FAN);
        glVertex2f(mx-9,my-5);
        for(int i=0;i<=10;i++){
            float a=2*PI*i/10;
            glVertex2f(mx-9+cosf(a)*2, my-5+sinf(a)*2);
        }
    glEnd();
}

} // namespace Sakib

//  Feature 17 — animateSkyColorChange  (background + gradient)
namespace Sakib {
void animateSkyColorChange(){
    float sunAng  = getSunAngle();
    float moonAng = getMoonAngle();
    bool  day     = isDaytime();

    float r,g,b;
    float activeAngle = day ? sunAng : moonAng;
    skyColor(activeAngle, day, &r, &g, &b);

    glClearColor(r, g, b, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    // Horizon gradient band (y = -60..30)
    float hr, hg, hb;
    if(day){
        if(sunAng < 0.0f){
            float u = clamp01((sunAng + 30.0f) / 30.0f);
            hr = lerp(0.05f, 1.00f, u);
            hg = lerp(0.05f, 0.55f, u);
            hb = lerp(0.22f, 0.22f, u);
        } else if(sunAng < 60.0f){
            float u = clamp01(sunAng / 60.0f);
            hr = lerp(1.00f, 0.72f, u);
            hg = lerp(0.55f, 0.78f, u);
            hb = lerp(0.22f, 1.00f, u);
        } else {
            // sunset dusk
            float u = clamp01((sunAng - 60.0f) / 10.0f);
            hr = lerp(0.72f, 1.00f, u);
            hg = lerp(0.78f, 0.30f, u);
            hb = lerp(1.00f, 0.10f, u);
        }
    } else {
        hr = lerp(r, 0.08f, 0.4f);
        hg = lerp(g, 0.08f, 0.4f);
        hb = lerp(b, 0.25f, 0.4f);
    }
    glBegin(GL_QUADS);
        glColor3f(hr,hg,hb); glVertex2f(-400,-60); glVertex2f(400,-60);
        glColor3f(r,  g,  b); glVertex2f( 400, 30); glVertex2f(-400, 30);
    glEnd();

    // Stars — visible during night phase
    float nightFactor = 0.0f;
    if(!day){
        nightFactor = clamp01((moonAng + 30.0f) / 30.0f); // fade in as moon rises
    } else {
        // briefly visible just before sun rises
        if(sunAng < -5.0f)
            nightFactor = clamp01((-sunAng - 5.0f) / 25.0f);
    }
    drawStars(nightFactor);
}
} // namespace Sakib

//  Feature 14 — animateSunRising  (clean sun, no outer black ring)
namespace Sakib {
static void drawSun(float sunAng){
    float rad = (sunAng + 90.0f) * PI / 180.0f;
    float sx  = 260.0f * cosf(rad) - 30.0f;
    float sy  = 200.0f * sinf(rad) - 50.0f;
    if(sy < -88.0f) return;

    int segs = 48;

    // Sun colour: deep orange at dawn → bright yellow at noon → orange-red at dusk
    float t;
    float sunR, sunG, sunB;
    if(sunAng < 60.0f){
        t = clamp01((sunAng + 30.0f) / 90.0f);
        sunR = 1.00f;
        sunG = lerp(0.38f, 1.00f, t);
        sunB = lerp(0.00f, 0.15f, t);
    } else {
        // sunset: transition to deep orange-red
        t = clamp01((sunAng - 60.0f) / 10.0f);
        sunR = 1.00f;
        sunG = lerp(1.00f, 0.38f, t);
        sunB = lerp(0.15f, 0.00f, t);
    }

    // Soft glow halo removed — only inner yellow circle remains

    // Sun disc — solid, clean
    glBegin(GL_TRIANGLE_FAN);
        glColor3f(sunR, sunG, sunB);
        glVertex2f(sx,sy);
        for(int i=0;i<=segs;i++){
            float a = 2*PI*i/segs;
            glVertex2f(sx + cosf(a)*15.0f, sy + sinf(a)*15.0f);
        }
    glEnd();

    // Rays only near horizon
    if(sunAng < 25.0f){
        float rayFade = clamp01(1.0f - sunAng/25.0f);
        glColor3f(sunR, sunG*0.85f, 0.3f);
        glLineWidth(1.5f);
        glBegin(GL_LINES);
        for(int i=0;i<12;i++){
            float ra = 2*PI*i/12;
            float r1 = 17.0f, r2 = 27.0f + (i%2)*8.0f;
            (void)rayFade;
            glVertex2f(sx+cosf(ra)*r1, sy+sinf(ra)*r1);
            glVertex2f(sx+cosf(ra)*r2, sy+sinf(ra)*r2);
        }
        glEnd();
        glLineWidth(1.0f);
    }
}

void animateSunRising(){
    float sunAng  = getSunAngle();
    float moonAng = getMoonAngle();

    if(isDaytime()) drawSun(sunAng);
    else            drawMoon(moonAng);
}
} // namespace Sakib

//  Feature 15 — animateCloudsMoving
namespace Sakib {
static void drawCloud(float cx, float cy){
    float bumps[5][3] = {
        {  0,  0, 22}, {-26, -7, 16}, {26, -7, 16}, {-12, 9, 13}, {12, 9, 13}
    };
    glColor3f(0.85f,0.85f,0.88f);
    for(auto& b: bumps){
        float bx=b[0], by=b[1], br=b[2];
        glBegin(GL_TRIANGLE_FAN);
            glVertex2f(cx+bx+2,cy+by-2);
            for(int i=0;i<=20;i++){
                float a=2*PI*i/20;
                glVertex2f(cx+bx+2+cosf(a)*br,cy+by-2+sinf(a)*br);
            }
        glEnd();
    }
    glColor3f(1,1,1);
    for(auto& b: bumps){
        float bx=b[0], by=b[1], br=b[2];
        glBegin(GL_TRIANGLE_FAN);
            glVertex2f(cx+bx,cy+by);
            for(int i=0;i<=20;i++){
                float a=2*PI*i/20;
                glVertex2f(cx+bx+cosf(a)*br,cy+by+sinf(a)*br);
            }
        glEnd();
    }
}

void animateCloudsMoving(){
    for(auto& c: clouds) drawCloud(c.x, c.y);
}
} // namespace Sakib

//  Feature 16 — animateBirdsFlying  (NO dot in the centre)
namespace Sakib {
// Clean V-shape bird with smooth wing articulation — no body dot
static void drawBirdFigure(float x, float y, float phase){
    float flapUp = sinf(phase);
    float inner  = flapUp *  8.0f;
    float outer  = flapUp * 13.0f;

    glColor3f(0.06f, 0.06f, 0.06f);
    glLineWidth(1.6f);
    glBegin(GL_LINE_STRIP);
        glVertex2f(x - 17.0f, y + outer - 3.0f);
        glVertex2f(x -  7.0f, y + inner);
        glVertex2f(x,         y);            // just the wing junction, NO filled dot
        glVertex2f(x +  7.0f, y + inner);
        glVertex2f(x + 17.0f, y + outer - 3.0f);
    glEnd();
    glLineWidth(1.0f);
}

void animateBirdsFlying(){
    for(auto& b: birds)
        drawBirdFigure(b.x, b.y, b.wingPhase);
}
} // namespace Sakib

//  Feature 18 — drawRoad
//  Layer order (back→front in Y):
//    y < -145       : river
//    -145..-115     : river bank (sandy)
//    -115..-20      : ROAD  (two lanes divided at y=-67)
//    -20..0         : pavement/kerb
//  Upper lane (school side): y -20..-67  — bus + L→R vehicles
//  Lower lane (river side):  y -67..-115 — R→L vehicles
namespace Sakib {
void drawRoad(){
    // River bank
    glColor3f(0.72f, 0.64f, 0.45f);
    glBegin(GL_QUADS);
        glVertex2f(-400,-145); glVertex2f(400,-145);
        glVertex2f( 400,-115); glVertex2f(-400,-115);
    glEnd();
    // bank texture dots
    glColor3f(0.62f,0.55f,0.38f);
    glPointSize(2.5f);
    glBegin(GL_POINTS);
    for(int i=-38; i<40; i++){
        glVertex2f(i*10.5f, -140.0f);
        glVertex2f(i*10.5f + 5.0f, -130.0f);
        glVertex2f(i*10.5f + 2.0f, -120.0f);
    }
    glEnd();
    glPointSize(1.0f);

    // Road surface
    glColor3f(0.26f,0.26f,0.26f);
    glBegin(GL_QUADS);
        glVertex2f(-400,-115); glVertex2f(400,-115);
        glVertex2f( 400, -20); glVertex2f(-400, -20);
    glEnd();

    // Road edge lines
    glColor3f(0.92f,0.92f,0.92f);
    glLineWidth(2.0f);
    glBegin(GL_LINES);
        glVertex2f(-400,-22); glVertex2f(400,-22);   // top edge
        glVertex2f(-400,-113);glVertex2f(400,-113);  // bottom edge
    glEnd();

    // Lane divider (yellow dashed) at y=-67
    glColor3f(0.95f,0.88f,0.05f);
    glBegin(GL_LINES);
    for(int i=-380; i<400; i+=42){
        glVertex2f((float)i,      -67.0f);
        glVertex2f((float)i+22.0f,-67.0f);
    }
    glEnd();
    glLineWidth(1.0f);

    // Pavement/kerb strip
    glColor3f(0.58f,0.55f,0.50f);
    glBegin(GL_QUADS);
        glVertex2f(-400,-20); glVertex2f(400,-20);
        glVertex2f( 400,-14); glVertex2f(-400,-14);
    glEnd();
}
} // namespace Sakib

//  Feature 19 — animateCarPassing
namespace Sakib {
static void drawVehicle(float cx, float cy, int type, bool facingLeft){
    float flip = facingLeft ? -1.0f : 1.0f;

    if(type == 0){
        // Red car
        glColor3f(0.80f, 0.12f, 0.12f);
        glBegin(GL_QUADS);
            glVertex2f(cx-30*flip,cy+4); glVertex2f(cx+30*flip,cy+4);
            glVertex2f(cx+30*flip,cy+18);glVertex2f(cx-30*flip,cy+18);
        glEnd();
        glColor3f(0.68f, 0.08f, 0.08f);
        glBegin(GL_QUADS);
            glVertex2f(cx-14*flip,cy+18);glVertex2f(cx+14*flip,cy+18);
            glVertex2f(cx+11*flip,cy+28);glVertex2f(cx-16*flip,cy+28);
        glEnd();
        glColor3f(0.6f,0.88f,1.0f);
        glBegin(GL_QUADS);
            glVertex2f(cx-13*flip,cy+19);glVertex2f(cx-2*flip,cy+19);
            glVertex2f(cx-2*flip, cy+26);glVertex2f(cx-13*flip,cy+26);
            glVertex2f(cx+ 1*flip,cy+19);glVertex2f(cx+10*flip,cy+19);
            glVertex2f(cx+10*flip,cy+26);glVertex2f(cx+ 1*flip,cy+26);
        glEnd();
        glColor3f(1.0f,0.95f,0.6f);
        glBegin(GL_QUADS);
            glVertex2f(cx+27*flip,cy+6); glVertex2f(cx+30*flip,cy+6);
            glVertex2f(cx+30*flip,cy+12);glVertex2f(cx+27*flip,cy+12);
        glEnd();
    }
    else if(type == 1){
        // Blue car
        glColor3f(0.15f, 0.30f, 0.80f);
        glBegin(GL_QUADS);
            glVertex2f(cx-32*flip,cy+4);glVertex2f(cx+32*flip,cy+4);
            glVertex2f(cx+32*flip,cy+20);glVertex2f(cx-32*flip,cy+20);
        glEnd();
        glColor3f(0.10f,0.22f,0.68f);
        glBegin(GL_QUADS);
            glVertex2f(cx-16*flip,cy+20);glVertex2f(cx+16*flip,cy+20);
            glVertex2f(cx+13*flip,cy+30);glVertex2f(cx-18*flip,cy+30);
        glEnd();
        glColor3f(0.6f,0.88f,1.0f);
        glBegin(GL_QUADS);
            glVertex2f(cx-15*flip,cy+21);glVertex2f(cx-3*flip,cy+21);
            glVertex2f(cx-3*flip, cy+28);glVertex2f(cx-15*flip,cy+28);
            glVertex2f(cx+ 2*flip,cy+21);glVertex2f(cx+12*flip,cy+21);
            glVertex2f(cx+12*flip,cy+28);glVertex2f(cx+ 2*flip,cy+28);
        glEnd();
        glColor3f(0.55f,0.70f,1.0f);
        glLineWidth(2.0f);
        glBegin(GL_LINES);
            glVertex2f(cx-30*flip,cy+14); glVertex2f(cx+30*flip,cy+14);
        glEnd();
        glLineWidth(1.0f);
    }
    else if(type == 2){
        // Green auto-rickshaw
        glColor3f(0.10f,0.55f,0.20f);
        glBegin(GL_QUADS);
            glVertex2f(cx-22*flip,cy+4); glVertex2f(cx+22*flip,cy+4);
            glVertex2f(cx+22*flip,cy+22);glVertex2f(cx-22*flip,cy+22);
        glEnd();
        glBegin(GL_TRIANGLE_FAN);
            glVertex2f(cx, cy+30);
            for(int i=0;i<=18;i++){
                float a=PI*i/18;
                glVertex2f(cx+cosf(a)*22*flip, cy+22+sinf(a)*10);
            }
        glEnd();
        glColor3f(0.05f,0.35f,0.12f);
        glBegin(GL_QUADS);
            glVertex2f(cx-20*flip,cy+6);glVertex2f(cx+2*flip,cy+6);
            glVertex2f(cx+2*flip,cy+20);glVertex2f(cx-20*flip,cy+20);
        glEnd();
        glColor3f(0.82f,0.65f,0.45f);
        glBegin(GL_TRIANGLE_FAN);
            glVertex2f(cx-12*flip,cy+20);
            for(int i=0;i<=10;i++){
                float a=2*PI*i/10;
                glVertex2f(cx-12*flip+cosf(a)*4, cy+20+sinf(a)*4);
            }
        glEnd();
    }
    else {
        // Yellow truck
        glColor3f(0.90f,0.75f,0.05f);
        glBegin(GL_QUADS);
            glVertex2f(cx-40*flip,cy+4); glVertex2f(cx+10*flip,cy+4);
            glVertex2f(cx+10*flip,cy+26);glVertex2f(cx-40*flip,cy+26);
        glEnd();
        glColor3f(0.78f,0.62f,0.03f);
        glBegin(GL_QUADS);
            glVertex2f(cx+10*flip,cy+4); glVertex2f(cx+40*flip,cy+4);
            glVertex2f(cx+40*flip,cy+30);glVertex2f(cx+10*flip,cy+30);
        glEnd();
        glColor3f(0.55f,0.82f,0.98f);
        glBegin(GL_QUADS);
            glVertex2f(cx+12*flip,cy+10);glVertex2f(cx+38*flip,cy+10);
            glVertex2f(cx+38*flip,cy+28);glVertex2f(cx+12*flip,cy+28);
        glEnd();
        glColor3f(0.68f,0.55f,0.02f);
        glLineWidth(1.5f);
        glBegin(GL_LINES);
            for(int i=0;i<4;i++){
                float lx=cx+(-35+i*12)*flip;
                glVertex2f(lx,cy+5);glVertex2f(lx,cy+25);
            }
        glEnd();
        glLineWidth(1.0f);
    }

    // Wheels
    float wOffsets[2];
    if(type==3){ wOffsets[0]=-28*flip; wOffsets[1]=28*flip; }
    else        { wOffsets[0]=-20*flip; wOffsets[1]=20*flip; }
    for(int w=0;w<2;w++){
        float wx = cx + wOffsets[w];
        glColor3f(0.12f,0.12f,0.12f);
        glBegin(GL_TRIANGLE_FAN);
            glVertex2f(wx,cy+4);
            for(int i=0;i<=20;i++){
                float a=2*PI*i/20;
                glVertex2f(wx+cosf(a)*8, cy+4+sinf(a)*8);
            }
        glEnd();
        glColor3f(0.58f,0.58f,0.62f);
        glBegin(GL_TRIANGLE_FAN);
            glVertex2f(wx,cy+4);
            for(int i=0;i<=12;i++){
                float a=2*PI*i/12;
                glVertex2f(wx+cosf(a)*4, cy+4+sinf(a)*4);
            }
        glEnd();
    }
}

void animateCarPassing(){
    static int* slotTypes[4] = {slot0Types, slot1Types, slot2Types, slot3Types};
    static int  slotCounts[4] = {4, 3, 3, 3};

    // Clip vehicle drawing to road y range so they never overlap the building
    // Road: y = -115 to -20.  Building sits at x = -160..160, y = -100..165.
    // We draw vehicles at their laneY; as long as laneY is within road range, fine.
    // Additional x-clipping: skip drawing if cx is within building x AND above -100
    for(int i=0;i<4;i++){
        Vehicle& v = vehicles[i];
        bool fl = (v.baseSpeed < 0.0f);
        // Don't draw over the building footprint
        bool inBuildingX = (v.x > -165.0f && v.x < 165.0f);
        bool inBuildingY = (v.laneY > -105.0f);
        // Building front face is at y=-100; upper lane (UPPER_LANE_Y=-20) is above it
        // so we skip rendering upper-lane vehicles when they're in the building x-zone.
        // Lower lane (LOWER_LANE_Y=-80) also partially overlaps building bottom.
        // We just don't skip — the road is drawn AFTER the building so it occludes.
        // Actually the correct fix is to draw road (and vehicles) AFTER the building.
        // The drawFeatures ordering handles this. Just draw normally.
        (void)inBuildingX; (void)inBuildingY;
        drawVehicle(v.x, v.laneY, v.type, fl);
    }
}
} // namespace Sakib

//  Bus stop sign helper
namespace Sakib {
static void drawBusStopSign(float x, float y){
    glColor3f(0.55f,0.55f,0.55f);
    glLineWidth(3.0f);
    glBegin(GL_LINES);
        glVertex2f(x, y); glVertex2f(x, y+40);
    glEnd();
    glLineWidth(1.0f);
    glColor3f(0.90f, 0.20f, 0.10f);
    glBegin(GL_QUADS);
        glVertex2f(x-18,y+40);glVertex2f(x+18,y+40);
        glVertex2f(x+18,y+58);glVertex2f(x-18,y+58);
    glEnd();
    glColor3f(1,1,1);
    drawString(x-15, y+51, "BUS");
    drawString(x-16, y+43, "STOP");
    glColor3f(0.45f,0.45f,0.45f);
    glBegin(GL_QUADS);
        glVertex2f(x-5,y-4);glVertex2f(x+5,y-4);
        glVertex2f(x+5,y+2);glVertex2f(x-5,y+2);
    glEnd();
}
} // namespace Sakib

//  Bezier river helpers
namespace Sakib {
static float bezierVal(float t,float p0,float p1,float p2,float p3){
    float u=1-t;
    return u*u*u*p0+3*u*u*t*p1+3*u*t*t*p2+t*t*t*p3;
}
static float bezierX(float t){
    return bezierVal(t,riverPts[0][0],riverPts[1][0],riverPts[2][0],riverPts[3][0]);
}
static float bezierY(float t){
    return bezierVal(t,riverPts[0][1],riverPts[1][1],riverPts[2][1],riverPts[3][1]);
}

static void drawRiver(){
    float tideAmp  = 7.0f;
    float tideFreq = 1.0f;  // multiplied with tidePhase
    int   segs     = 60;

    glColor3f(0.18f,0.42f,0.72f);
    glBegin(GL_TRIANGLE_STRIP);
    for(int i=0;i<=segs;i++){
        float t  = i/(float)segs;
        float x  = bezierX(t);
        float y  = bezierY(t);
        float tw = 24.0f + tideAmp * sinf(tidePhase * tideFreq + t*6.0f);
        glVertex2f(x, y - tw);
        glVertex2f(x, y + tw);
    }
    glEnd();

    glColor3f(0.28f,0.58f,0.88f);
    glBegin(GL_TRIANGLE_STRIP);
    for(int i=0;i<=segs;i++){
        float t  = i/(float)segs;
        float x  = bezierX(t);
        float y  = bezierY(t);
        float off= tideAmp * sinf(tidePhase * tideFreq + t*6.0f);
        glVertex2f(x, y + off - 6.0f);
        glVertex2f(x, y + off + 6.0f);
    }
    glEnd();

    glColor3f(0.65f,0.85f,1.0f);
    glLineWidth(1.2f);
    glBegin(GL_LINE_STRIP);
    for(int i=0;i<=segs;i++){
        float t = i/(float)segs;
        float wave = tideAmp * sinf(tidePhase * tideFreq + t*6.0f);
        glVertex2f(bezierX(t), bezierY(t) + wave);
    }
    glEnd();
    glLineWidth(1.0f);
}

static void drawBoat(float t, float yOff){
    float bx = bezierX(t);
    float tideAmp = 7.0f, tideFreq = 1.0f;
    float by = bezierY(t) + tideAmp * sinf(tidePhase * tideFreq + t*6.0f) + yOff;

    glColor3f(0.50f,0.30f,0.12f);
    glBegin(GL_QUADS);
        glVertex2f(bx-14, by); glVertex2f(bx+14, by);
        glVertex2f(bx+10, by-7); glVertex2f(bx-10, by-7);
    glEnd();
    glColor3f(0.30f,0.18f,0.06f);
    glLineWidth(1.2f);
    glBegin(GL_LINE_LOOP);
        glVertex2f(bx-14,by); glVertex2f(bx+14,by);
        glVertex2f(bx+10,by-7); glVertex2f(bx-10,by-7);
    glEnd();
    glLineWidth(1.0f);

    glColor3f(0.45f,0.28f,0.10f);
    glLineWidth(2.0f);
    glBegin(GL_LINES);
        glVertex2f(bx-1, by); glVertex2f(bx-1, by+22);
    glEnd();
    glLineWidth(1.0f);

    float sag = 3.0f * sinf(gTime * 0.8f + t * 3.0f);
    glColor3f(0.96f, 0.93f, 0.85f);
    glBegin(GL_TRIANGLES);
        glVertex2f(bx-1, by+22); glVertex2f(bx-1, by+2);
        glVertex2f(bx+13+sag, by+10);
    glEnd();
    glColor3f(0.75f,0.70f,0.60f);
    glBegin(GL_LINE_LOOP);
        glVertex2f(bx-1,by+22);glVertex2f(bx-1,by+2);glVertex2f(bx+13+sag,by+10);
    glEnd();
    glColor3f(0.85f,0.15f,0.15f);
    glBegin(GL_TRIANGLES);
        glVertex2f(bx-1,by+22); glVertex2f(bx+6,by+20); glVertex2f(bx-1,by+18);
    glEnd();
}
} // namespace Sakib

//  Feature 12 — drawTrees  |  Feature 13 — animateTreeWind
//  + Date Palm and Coconut Tree helpers
namespace Sakib {
static void drawTree(float x, float y, float scale, float lean){
    glColor3f(0.38f,0.24f,0.09f);
    glBegin(GL_QUADS);
        glVertex2f(x-4*scale, y);
        glVertex2f(x+4*scale, y);
        glVertex2f(x+4*scale+lean, y+28*scale);
        glVertex2f(x-4*scale+lean, y+28*scale);
    glEnd();
    float gy = y + 28*scale;
    for(int l=0;l<3;l++){
        float lw = (3-l)*17*scale;
        float lh = 20*scale;
        glColor3f(0.08f+l*0.05f, 0.48f+l*0.07f, 0.12f);
        glBegin(GL_TRIANGLES);
            glVertex2f(x+lean*0.5f,     gy+lh);
            glVertex2f(x-lw+lean*0.25f, gy);
            glVertex2f(x+lw+lean*0.25f, gy);
        glEnd();
        gy += lh*0.50f;
    }
}

// Date palm: tall thin trunk, feathery fronds at top
static void drawDatePalm(float x, float y, float scale, float lean){
    // Trunk (tapers slightly)
    int segs = 10;
    glColor3f(0.55f,0.38f,0.18f);
    glBegin(GL_QUADS);
    for(int i=0; i<segs; i++){
        float t0 = (float)i/segs, t1 = (float)(i+1)/segs;
        float y0 = y + t0*75*scale, y1 = y + t1*75*scale;
        float w0 = (4.0f - t0*1.5f)*scale, w1 = (4.0f - t1*1.5f)*scale;
        float lx = x + lean*t0;
        float lx1 = x + lean*t1;
        glVertex2f(lx-w0,y0);glVertex2f(lx+w0,y0);
        glVertex2f(lx1+w1,y1);glVertex2f(lx1-w1,y1);
    }
    glEnd();
    // Trunk rings
    glColor3f(0.42f,0.28f,0.12f);
    glLineWidth(1.0f);
    glBegin(GL_LINES);
    for(int i=1; i<segs; i++){
        float ty = y + (float)i/segs*75*scale;
        float lx = x + lean*(float)i/segs;
        glVertex2f(lx-3.5f*scale,ty); glVertex2f(lx+3.5f*scale,ty);
    }
    glEnd();
    // Fronds — 6 arching leaves at top
    float tx = x + lean, ty = y + 75*scale;
    float frondAngles[] = {-150,-120,-80,-40,0,40,80,120,150};
    for(int f=0;f<9;f++){
        float fa = frondAngles[f] * PI / 180.0f;
        float len = 32*scale;
        // draw frond as a curved strip
        glColor3f(0.15f,0.52f,0.15f);
        glBegin(GL_LINE_STRIP);
        for(int s=0;s<=8;s++){
            float u = (float)s/8;
            float droop = -u*u*18*scale; // droop down
            glVertex2f(tx + cosf(fa)*len*u, ty + sinf(fa)*len*u + droop);
        }
        glEnd();
        // thicker frond with triangle strip
        glBegin(GL_TRIANGLE_STRIP);
        for(int s=0;s<=8;s++){
            float u = (float)s/8;
            float droop = -u*u*18*scale;
            float fx = tx + cosf(fa)*len*u;
            float fy = ty + sinf(fa)*len*u + droop;
            float perp = (PI/2 - fa);
            float hw = (1.5f - u*1.0f)*scale;
            glColor3f(0.12f+u*0.08f, 0.50f+u*0.05f, 0.10f);
            glVertex2f(fx+cosf(perp)*hw, fy+sinf(perp)*hw);
            glVertex2f(fx-cosf(perp)*hw, fy-sinf(perp)*hw);
        }
        glEnd();
    }
    // Dates cluster
    glColor3f(0.75f,0.42f,0.08f);
    for(int d=0;d<5;d++){
        float da = (d-2)*0.25f;
        glBegin(GL_TRIANGLE_FAN);
            glVertex2f(tx+cosf(da)*8*scale, ty-6*scale);
            for(int i=0;i<=8;i++){
                float a=2*PI*i/8;
                glVertex2f(tx+cosf(da)*8*scale+cosf(a)*3*scale,
                           ty-6*scale+sinf(a)*3*scale);
            }
        glEnd();
    }
}

// Coconut tree: curved trunk, fan of long fronds, coconuts
static void drawCoconutTree(float x, float y, float scale, float lean){
    // Curved trunk
    int segs = 12;
    glColor3f(0.52f,0.36f,0.16f);
    glBegin(GL_TRIANGLE_STRIP);
    for(int i=0;i<=segs;i++){
        float u = (float)i/segs;
        float cu = sinf(u*PI*0.5f)*12*scale; // curve
        float tx2 = x + cu + lean*u;
        float ty2 = y + u*85*scale;
        float w = (4.5f - u*2.0f)*scale;
        glVertex2f(tx2-w, ty2);
        glVertex2f(tx2+w, ty2);
    }
    glEnd();
    // Trunk rings
    glColor3f(0.38f,0.26f,0.10f);
    glBegin(GL_LINES);
    for(int i=2;i<segs;i+=2){
        float u=(float)i/segs;
        float cu=sinf(u*PI*0.5f)*12*scale;
        float tx2=x+cu+lean*u, ty2=y+u*85*scale;
        glVertex2f(tx2-4*scale,ty2); glVertex2f(tx2+4*scale,ty2);
    }
    glEnd();

    float topX = x + sinf(PI*0.5f)*12*scale + lean, topY = y + 85*scale;
    // Long fronds
    float frondA[] = {-160,-130,-100,-60,-20,20,60,100,130,160};
    glColor3f(0.14f,0.50f,0.14f);
    for(int f=0;f<10;f++){
        float fa = frondA[f]*PI/180.0f;
        float len = 38*scale;
        glBegin(GL_TRIANGLE_STRIP);
        for(int s=0;s<=10;s++){
            float u=(float)s/10;
            float droop = -u*u*22*scale;
            float fx=topX+cosf(fa)*len*u, fy=topY+sinf(fa)*len*u+droop;
            float perp=fa+PI/2;
            float hw=(2.0f-u*1.5f)*scale;
            glColor3f(0.10f+u*0.08f,0.48f+u*0.04f,0.08f);
            glVertex2f(fx+cosf(perp)*hw,fy+sinf(perp)*hw);
            glVertex2f(fx-cosf(perp)*hw,fy-sinf(perp)*hw);
        }
        glEnd();
    }
    // Coconuts
    glColor3f(0.48f,0.28f,0.08f);
    for(int c=0;c<3;c++){
        float ca = (c-1)*0.3f;
        glBegin(GL_TRIANGLE_FAN);
            glVertex2f(topX+cosf(ca)*6*scale, topY-8*scale);
            for(int i=0;i<=10;i++){
                float a=2*PI*i/10;
                glVertex2f(topX+cosf(ca)*6*scale+cosf(a)*4*scale,
                           topY-8*scale+sinf(a)*4*scale);
            }
        glEnd();
    }
}

void drawTrees(){
    float lean = windOn ? sinf(windPhase)*5.5f : 0.0f;
    float ls   = windOn ? sinf(windPhase+0.5f)*3.5f : 0.0f;
    // Trees sit on the pavement/ground level at y=-20
    // Regular trees (foreground left & right)
    drawTree(-330,-20, 1.00f, lean);
    drawTree(-300,-20, 0.85f, ls);
    drawTree(-262,-20, 0.76f, lean*0.7f);
    drawTree( 210,-20, 0.90f, ls);
    drawTree( 244,-20, 1.00f, lean);
    drawTree( 272,-20, 0.80f, ls*0.8f);
    drawTree(-185,-20, 0.70f, lean*0.45f);
    drawTree( 185,-20, 0.70f, lean*0.45f);

    // Date palms (mid-distance, smaller)
    drawDatePalm(-360,-20, 0.70f, lean*0.6f);
    drawDatePalm( 320,-20, 0.72f, ls*0.5f);
    drawDatePalm(-220,-20, 0.60f, lean*0.4f);
    drawDatePalm( 370,-20, 0.65f, lean*0.3f);

    // Coconut trees (mid-distance)
    drawCoconutTree(-390,-20, 0.65f, lean*0.5f);
    drawCoconutTree( 350,-20, 0.68f, ls*0.4f);
    drawCoconutTree(-240,-20, 0.58f, lean*0.3f);
}

void animateTreeWind(){ /* state updated in tick() */ }
} // namespace Sakib

//  Feature 1 — drawSchoolBuilding
namespace Sakib {
void drawSchoolBuilding(){
    // Shadow
    glColor3f(0.55f,0.52f,0.48f);
    glBegin(GL_QUADS);
        glVertex2f(-155,-115);glVertex2f(165,-115);
        glVertex2f( 168,-110);glVertex2f(-158,-110);
    glEnd();

    // Main block
    glColor3f(0.93f,0.89f,0.81f);
    glBegin(GL_QUADS);
        glVertex2f(-160,-115); glVertex2f(160,-115);
        glVertex2f( 160, 118); glVertex2f(-160, 118);
    glEnd();

    // Facade columns
    glColor3f(0.85f,0.80f,0.72f);
    for(int c=0;c<5;c++){
        float cx=-120.0f+c*60.0f;
        glBegin(GL_QUADS);
            glVertex2f(cx-5,-115);glVertex2f(cx+5,-115);
            glVertex2f(cx+5, 118);glVertex2f(cx-5, 118);
        glEnd();
    }

    // Red triangular roof
    glColor3f(0.74f,0.20f,0.18f);
    glBegin(GL_TRIANGLES);
        glVertex2f(-172,118); glVertex2f(172,118); glVertex2f(0,165);
    glEnd();
    glColor3f(0.58f,0.14f,0.12f);
    glLineWidth(2.0f);
    glBegin(GL_LINES);
        glVertex2f(-172,118);glVertex2f(0,165);
        glVertex2f(0,165);   glVertex2f(172,118);
    glEnd();
    glLineWidth(1.0f);

    // Signboard
    glColor3f(0.12f,0.28f,0.68f);
    glBegin(GL_QUADS);
        glVertex2f(-110,100);glVertex2f(110,100);
        glVertex2f( 110,116);glVertex2f(-110,116);
    glEnd();
    glColor3f(0.95f,0.80f,0.10f);
    glLineWidth(2.0f);
    glBegin(GL_LINE_LOOP);
        glVertex2f(-110,100);glVertex2f(110,100);
        glVertex2f( 110,116);glVertex2f(-110,116);
    glEnd();
    glLineWidth(1.0f);
    glColor3f(1.0f,1.0f,1.0f);
    drawString(-74, 105, "SUNRISE PUBLIC SCHOOL");

    // Door
    glColor3f(0.32f,0.20f,0.08f);
    glBegin(GL_QUADS);
        glVertex2f(-22,-115);glVertex2f(22,-115);
        glVertex2f( 22, -34);glVertex2f(-22,-34);
    glEnd();
    glColor3f(0.38f,0.24f,0.10f);
    glBegin(GL_TRIANGLE_FAN);
        glVertex2f(0,-34);
        for(int i=0;i<=16;i++){
            float a=PI*i/16;
            glVertex2f(cosf(a)*22, -34+sinf(a)*22);
        }
    glEnd();
    glColor3f(0.55f,0.78f,0.95f);
    glBegin(GL_QUADS);
        glVertex2f(-18,-113);glVertex2f(-4,-113);
        glVertex2f( -4, -38);glVertex2f(-18,-38);
        glVertex2f(  4,-113);glVertex2f(18,-113);
        glVertex2f( 18, -38);glVertex2f(  4,-38);
    glEnd();

    // Windows (3 rows x 4 cols)
    for(int row=0;row<3;row++)
    for(int col=0;col<4;col++){
        float wx=-128.0f + col*86.0f;
        float wy= -65.0f + row*58.0f;
        if(wx > -32 && wx < 18 && wy < -20) continue;
        glColor3f(0.32f,0.20f,0.08f);
        glBegin(GL_QUADS);
            glVertex2f(wx-1,wy-1);glVertex2f(wx+27,wy-1);
            glVertex2f(wx+27,wy+36);glVertex2f(wx-1,wy+36);
        glEnd();
        float sunAng = getSunAngle();
        float brightness = 0.55f + clamp01((sunAng+20.0f)/80.0f)*0.30f;
        if(!isDaytime()) brightness = 0.72f; // lit windows at night
        glColor3f(brightness*0.70f, brightness*0.88f, brightness);
        glBegin(GL_QUADS);
            glVertex2f(wx,wy);glVertex2f(wx+26,wy);
            glVertex2f(wx+26,wy+35);glVertex2f(wx,wy+35);
        glEnd();
        glColor3f(0.32f,0.20f,0.08f);
        glBegin(GL_LINES);
            glVertex2f(wx+13,wy);glVertex2f(wx+13,wy+35);
            glVertex2f(wx,wy+17);glVertex2f(wx+26,wy+17);
        glEnd();
    }

    // Entrance steps
    glColor3f(0.78f,0.74f,0.67f);
    for(int s=0;s<3;s++){
        float sw=42.0f-s*8.0f;
        glBegin(GL_QUADS);
            glVertex2f(-sw, -115-s*5);  glVertex2f(sw,  -115-s*5);
            glVertex2f( sw, -115-s*5+5);glVertex2f(-sw, -115-s*5+5);
        glEnd();
    }
}
} // namespace Sakib

//  Feature 20 — drawSchoolSignboard
namespace Sakib {
void drawSchoolSignboard(){
    // Bus stop sign is placed at the building's left side where the bus stops
    drawBusStopSign(-175.0f, -20.0f);
}
} // namespace Sakib

//  Feature 2 — drawSchoolGate  |  Feature 3 — animateGateOpening
namespace Sakib {
void drawSchoolGate(){
    // Gate is centered on the building, at the 1st floor entrance level.
    // Building x: -160..160. Entrance centered at x=0.
    // Gate sits just in front of the building (y=-115 to -22), between pillars.
    // Pillars at x = ±38 (flanking the entrance), height from y=-115 to y=-22.

    float gateX = 0.0f;   // centre of gate
    float pillarW = 8.0f;
    float pillarL = -38.0f, pillarR = 38.0f; // inner edges of pillars

    // Left pillar
    glColor3f(0.80f,0.76f,0.70f);
    glBegin(GL_QUADS);
        glVertex2f(pillarL - pillarW, -115); glVertex2f(pillarL, -115);
        glVertex2f(pillarL,           -22);  glVertex2f(pillarL - pillarW, -22);
    glEnd();
    // Right pillar
    glBegin(GL_QUADS);
        glVertex2f(pillarR,           -115); glVertex2f(pillarR + pillarW, -115);
        glVertex2f(pillarR + pillarW, -22);  glVertex2f(pillarR,           -22);
    glEnd();

    // Pillar caps
    glColor3f(0.65f,0.60f,0.55f);
    glBegin(GL_QUADS);
        glVertex2f(pillarL - pillarW - 2, -24); glVertex2f(pillarL + 2, -24);
        glVertex2f(pillarL + 2, -20);           glVertex2f(pillarL - pillarW - 2, -20);
    glEnd();
    glBegin(GL_QUADS);
        glVertex2f(pillarR - 2, -24); glVertex2f(pillarR + pillarW + 2, -24);
        glVertex2f(pillarR + pillarW + 2, -20); glVertex2f(pillarR - 2, -20);
    glEnd();

    // Arch over gate
    glColor3f(0.70f,0.18f,0.14f);
    glBegin(GL_QUADS);
        glVertex2f(pillarL - pillarW, -24); glVertex2f(pillarR + pillarW, -24);
        glVertex2f(pillarR + pillarW, -16); glVertex2f(pillarL - pillarW, -16);
    glEnd();
    // Arch top curve
    glBegin(GL_TRIANGLE_FAN);
        glVertex2f(gateX, -6);
        for(int i=0;i<=20;i++){
            float a=PI*i/20;
            glVertex2f(gateX + cosf(a)*34, -16+sinf(a)*14);
        }
    glEnd();

    // Gate leaves — slide open outward from centre
    float slide = gateOpenT * 34.0f;

    // Left leaf slides left
    float lx = -slide;  // left edge of left leaf
    glColor3f(0.32f,0.20f,0.07f);
    glBegin(GL_QUADS);
        glVertex2f(lx - 32, -111); glVertex2f(lx, -111);
        glVertex2f(lx,      -24);  glVertex2f(lx - 32, -24);
    glEnd();
    // Left leaf bars
    glColor3f(0.20f,0.13f,0.05f);
    for(int b=0;b<3;b++){
        float bx=lx-30+b*10;
        glBegin(GL_QUADS);
            glVertex2f(bx,-111);glVertex2f(bx+3,-111);
            glVertex2f(bx+3,-24);glVertex2f(bx,-24);
        glEnd();
    }
    // Left leaf spikes
    glColor3f(0.55f,0.40f,0.12f);
    for(int s=0;s<3;s++){
        float sx=lx-29+s*10;
        glBegin(GL_TRIANGLES);
            glVertex2f(sx,-24);glVertex2f(sx+3,-24);glVertex2f(sx+1.5f,-18);
        glEnd();
    }

    // Right leaf slides right
    float rx = slide;  // right edge of right leaf
    glColor3f(0.32f,0.20f,0.07f);
    glBegin(GL_QUADS);
        glVertex2f(rx,      -111); glVertex2f(rx + 32, -111);
        glVertex2f(rx + 32, -24);  glVertex2f(rx,      -24);
    glEnd();
    // Right leaf bars
    glColor3f(0.20f,0.13f,0.05f);
    for(int b=0;b<3;b++){
        float bx=rx+4+b*10;
        glBegin(GL_QUADS);
            glVertex2f(bx,-111);glVertex2f(bx+3,-111);
            glVertex2f(bx+3,-24);glVertex2f(bx,-24);
        glEnd();
    }
    // Right leaf spikes
    glColor3f(0.55f,0.40f,0.12f);
    for(int s=0;s<3;s++){
        float sx=rx+5+s*10;
        glBegin(GL_TRIANGLES);
            glVertex2f(sx,-24);glVertex2f(sx+3,-24);glVertex2f(sx+1.5f,-18);
        glEnd();
    }
}

void animateGateOpening(){ /* state handled in tick() */ }
} // namespace Sakib

//  Feature 9 — drawSecurityGuard
namespace Sakib {
void drawSecurityGuard(){
    float gx=-98.0f, gy=-115.0f;
    glColor3f(0.12f,0.12f,0.35f);
    glBegin(GL_QUADS);
        glVertex2f(gx-7,gy);    glVertex2f(gx-1,gy);
        glVertex2f(gx-1,gy+16); glVertex2f(gx-7,gy+16);
        glVertex2f(gx+1,gy);    glVertex2f(gx+7,gy);
        glVertex2f(gx+7,gy+16); glVertex2f(gx+1,gy+16);
    glEnd();
    glColor3f(0.40f,0.30f,0.10f);
    glBegin(GL_QUADS);
        glVertex2f(gx-8,gy+15);glVertex2f(gx+8,gy+15);
        glVertex2f(gx+8,gy+18);glVertex2f(gx-8,gy+18);
    glEnd();
    glColor3f(0.10f,0.32f,0.12f);
    glBegin(GL_QUADS);
        glVertex2f(gx-8,gy+18); glVertex2f(gx+8,gy+18);
        glVertex2f(gx+8,gy+40); glVertex2f(gx-8,gy+40);
    glEnd();
    glColor3f(0.80f,0.70f,0.10f);
    glBegin(GL_QUADS);
        glVertex2f(gx-10,gy+35);glVertex2f(gx-6,gy+35);
        glVertex2f(gx-6, gy+40);glVertex2f(gx-10,gy+40);
        glVertex2f(gx+6, gy+35);glVertex2f(gx+10,gy+35);
        glVertex2f(gx+10,gy+40);glVertex2f(gx+6, gy+40);
    glEnd();
    glColor3f(0.80f,0.62f,0.44f);
    glBegin(GL_TRIANGLE_FAN);
        glVertex2f(gx,gy+46);
        for(int i=0;i<=16;i++){
            float a=2*PI*i/16;
            glVertex2f(gx+cosf(a)*7.5f,gy+40+sinf(a)*7.5f);
        }
    glEnd();
    glColor3f(0.08f,0.25f,0.10f);
    glBegin(GL_QUADS);
        glVertex2f(gx-9,gy+45);glVertex2f(gx+9,gy+45);
        glVertex2f(gx+9,gy+52);glVertex2f(gx-9,gy+52);
    glEnd();
    glColor3f(0.06f,0.18f,0.08f);
    glBegin(GL_QUADS);
        glVertex2f(gx-11,gy+45);glVertex2f(gx+11,gy+45);
        glVertex2f(gx+11,gy+47);glVertex2f(gx-11,gy+47);
    glEnd();
    float armA = (40.0f + sinf(guardArmAngle)*8.0f) * PI/180.0f;
    glColor3f(0.10f,0.32f,0.12f);
    glLineWidth(4.5f);
    glBegin(GL_LINES);
        glVertex2f(gx+8,gy+32);
        glVertex2f(gx+8+cosf(armA)*20, gy+32+sinf(armA)*20);
    glEnd();
    glLineWidth(1.0f);
    glColor3f(0.70f,0.60f,0.10f);
    glBegin(GL_QUADS);
        glVertex2f(gx-1,gy+36);glVertex2f(gx+5,gy+36);
        glVertex2f(gx+5,gy+38);glVertex2f(gx-1,gy+38);
    glEnd();
}
} // namespace Sakib

//  Feature 10 — drawFlag  |  Feature 11 — animateFlagWaving
namespace Sakib {
void drawFlag(){
    float px=142.0f, py=-20.0f;
    glColor3f(0.62f,0.62f,0.65f);
    glLineWidth(3.0f);
    glBegin(GL_LINES);
        glVertex2f(px,py); glVertex2f(px,py+102);
    glEnd();
    glLineWidth(1.0f);
    glColor3f(0.85f,0.75f,0.10f);
    glBegin(GL_TRIANGLE_FAN);
        glVertex2f(px,py+104);
        for(int i=0;i<=14;i++){
            float a=2*PI*i/14;
            glVertex2f(px+cosf(a)*4,py+100+sinf(a)*4);
        }
    glEnd();

    int segs=24;
    float fw=52.0f, fh=32.0f, amp=5.0f;
    float topY=py+98, botY=py+98-fh;
    glBegin(GL_TRIANGLE_STRIP);
    for(int i=0;i<=segs;i++){
        float u=i/(float)segs;
        float wave = sinf(flagWave + u*5.0f) * amp * (0.2f+0.8f*u);
        float fx=px+u*fw;
        glColor3f(0.0f,0.55f,0.25f);
        glVertex2f(fx, topY+wave);
        glVertex2f(fx, botY+wave);
    }
    glEnd();

    float discU=0.38f;
    float discWave = sinf(flagWave + discU*5.0f)*amp*(0.2f+0.8f*discU);
    float dcx=px+fw*discU, dcy=py+98-fh/2.0f+discWave;
    glColor3f(0.88f,0.10f,0.10f);
    glBegin(GL_TRIANGLE_FAN);
        glVertex2f(dcx,dcy);
        for(int i=0;i<=24;i++){
            float a=2*PI*i/24;
            glVertex2f(dcx+cosf(a)*11, dcy+sinf(a)*11);
        }
    glEnd();
}

void animateFlagWaving(){ /* state in tick() */ }
} // namespace Sakib

//  Feature 5 — drawSchoolBusArrival  (aligned to upper lane)
namespace Sakib {
void drawSchoolBusArrival(){
    // Upper lane: road top = -20 (pavement bottom).
    // Wheel radius = 11. Wheel centre = UPPER_LANE_Y - 11 = -31.
    // Bus body bottom = UPPER_LANE_Y = -20. Body goes up from there.
    float bx = busX;
    float by = UPPER_LANE_Y;  // body base (road surface of upper lane)

    // Body
    glColor3f(0.95f,0.80f,0.05f);
    glBegin(GL_QUADS);
        glVertex2f(bx-56,by);   glVertex2f(bx+56,by);
        glVertex2f(bx+56,by+32);glVertex2f(bx-56,by+32);
    glEnd();

    // Roof
    glColor3f(0.85f,0.70f,0.03f);
    glBegin(GL_QUADS);
        glVertex2f(bx-56,by+32);glVertex2f(bx+56,by+32);
        glVertex2f(bx+50,by+42);glVertex2f(bx-50,by+42);
    glEnd();

    // Front cabin
    glColor3f(0.78f,0.66f,0.04f);
    glBegin(GL_QUADS);
        glVertex2f(bx+42,by);   glVertex2f(bx+56,by);
        glVertex2f(bx+56,by+32);glVertex2f(bx+42,by+32);
    glEnd();

    // Windshield
    glColor3f(0.55f,0.82f,0.98f);
    glBegin(GL_QUADS);
        glVertex2f(bx+43,by+8); glVertex2f(bx+55,by+8);
        glVertex2f(bx+55,by+30);glVertex2f(bx+43,by+30);
    glEnd();

    // Windows
    glColor3f(0.55f,0.82f,0.98f);
    for(int w=0;w<4;w++){
        float wx=bx-50+w*22;
        glBegin(GL_QUADS);
            glVertex2f(wx,    by+12);glVertex2f(wx+16, by+12);
            glVertex2f(wx+16, by+28);glVertex2f(wx,    by+28);
        glEnd();
    }

    // "SCHOOL BUS" text
    glColor3f(0.08f,0.08f,0.08f);
    drawString(bx-38, by+34, "SCHOOL BUS");

    // Wheels — centre at by - 11, top of tyre at by
    float wcy = by - 11.0f;
    float wxArr[2] = {bx-36.0f, bx+36.0f};
    for(int wi=0; wi<2; wi++){
        float wx = wxArr[wi];
        glColor3f(0.12f,0.12f,0.12f);
        glBegin(GL_TRIANGLE_FAN);
            glVertex2f(wx,wcy);
            for(int i=0;i<=20;i++){
                float a=2*PI*i/20;
                glVertex2f(wx+cosf(a)*11, wcy+sinf(a)*11);
            }
        glEnd();
        glColor3f(0.58f,0.58f,0.62f);
        glBegin(GL_TRIANGLE_FAN);
            glVertex2f(wx,wcy);
            for(int i=0;i<=12;i++){
                float a=2*PI*i/12;
                glVertex2f(wx+cosf(a)*5, wcy+sinf(a)*5);
            }
        glEnd();
        glColor3f(0.08f,0.08f,0.08f);
        glLineWidth(1.5f);
        glBegin(GL_LINE_LOOP);
            for(int i=0;i<=20;i++){
                float a=2*PI*i/20;
                glVertex2f(wx+cosf(a)*11, wcy+sinf(a)*11);
            }
        glEnd();
        glLineWidth(1.0f);
    }

    // Open door when arrived
    if(busArrived){
        glColor3f(0.78f,0.63f,0.03f);
        glBegin(GL_QUADS);
            glVertex2f(bx-56,by+2); glVertex2f(bx-43,by+2);
            glVertex2f(bx-43,by+26);glVertex2f(bx-56,by+26);
        glEnd();
        glColor3f(0.12f,0.08f,0.05f);
        glBegin(GL_LINE_LOOP);
            glVertex2f(bx-56,by+2); glVertex2f(bx-43,by+2);
            glVertex2f(bx-43,by+26);glVertex2f(bx-56,by+26);
        glEnd();
    }
}

void animateBusStopping(){ /* state handled in tick() */ }
} // namespace Sakib

//  Features 7, 8, 4 — Students
namespace Sakib {
static void drawStudentFigure(float x, float y, float r, float g, float b){
    float walkBob = sinf(gTime * 6.0f + x*0.1f) * 1.2f;
    float fy = y + walkBob;
    glColor3f(r*0.55f, g*0.55f, b*0.55f);
    glLineWidth(2.5f);
    float legSwing = sinf(gTime * 6.0f + x*0.1f) * 5.0f;
    glBegin(GL_LINES);
        glVertex2f(x-3,fy);    glVertex2f(x-4-legSwing*0.3f, fy-12);
        glVertex2f(x+3,fy);    glVertex2f(x+4+legSwing*0.3f, fy-12);
    glEnd();
    glLineWidth(1.0f);
    glColor3f(r,g,b);
    glBegin(GL_QUADS);
        glVertex2f(x-5,fy);   glVertex2f(x+5,fy);
        glVertex2f(x+5,fy+17);glVertex2f(x-5,fy+17);
    glEnd();
    glColor3f(0.28f,0.18f,0.62f);
    glBegin(GL_QUADS);
        glVertex2f(x+4,fy+4);  glVertex2f(x+9,fy+4);
        glVertex2f(x+9,fy+14); glVertex2f(x+4,fy+14);
    glEnd();
    glColor3f(0.80f,0.62f,0.44f);
    glBegin(GL_TRIANGLE_FAN);
        glVertex2f(x,fy+22);
        for(int i=0;i<=12;i++){
            float a=2*PI*i/12;
            glVertex2f(x+cosf(a)*6, fy+17+sinf(a)*6);
        }
    glEnd();
    glColor3f(0.10f,0.06f,0.04f);
    glBegin(GL_TRIANGLE_FAN);
        glVertex2f(x,fy+24);
        for(int i=0;i<=10;i++){
            float a=PI*i/10;
            glVertex2f(x+cosf(a)*6.5f, fy+17+sinf(a)*7.0f);
        }
    glEnd();
}

void drawStudentsGettingOffBus(){
    if(!busArrived) return;
    float descY = (LOWER_LANE_Y + 4.0f) - 10.0f*(1.0f - clamp01(studentOffT));
    drawStudentFigure(busX - 58.0f, descY, 0.15f, 0.40f, 0.72f);
}

void animateWalkingStudents(){ /* state updated in tick() */ }

void drawStudentsEntering(){
    for(auto& s: students){
        if(!s.active) continue;
        float colors[6][3] = {
            {0.15f,0.40f,0.72f},{0.72f,0.15f,0.20f},{0.15f,0.60f,0.30f},
            {0.60f,0.40f,0.10f},{0.45f,0.15f,0.60f},{0.20f,0.50f,0.55f}
        };
        int ci = (int)(&s - students) % 6;
        drawStudentFigure(s.x, s.y, colors[ci][0], colors[ci][1], colors[ci][2]);
    }
}
} // namespace Sakib

//  Background city (more buildings, some distant trees)
namespace Sakib {
static void drawBackgroundCity(){
    struct Bldg{ float x,y,w,h; float r,g,b; };
    // More buildings — spread further, varied heights
    static Bldg bldgs[]={
        // far left
        {-395,-115,30, 85,0.38f,0.40f,0.48f},
        {-360,-115,44,110,0.42f,0.44f,0.52f},
        {-312,-115,36, 90,0.40f,0.42f,0.50f},
        {-272,-115,50,130,0.44f,0.46f,0.54f},
        // far right
        { 222,-115,48,118,0.44f,0.46f,0.54f},
        { 274,-115,38, 95,0.41f,0.43f,0.51f},
        { 316,-115,54,138,0.43f,0.45f,0.53f},
        { 374,-115,28, 78,0.40f,0.42f,0.50f},
        // extra distant (shorter, greyer)
        {-395,-115,20, 58,0.35f,0.36f,0.44f},
        { 375,-115,22, 62,0.35f,0.37f,0.44f},
    };
    float sunAng = isDaytime() ? getSunAngle() : -30.0f;
    float moonAng = !isDaytime() ? getMoonAngle() : -30.0f;
    float activeAng = isDaytime() ? sunAng : moonAng;
    float dim = lerp(0.30f, 1.0f, clamp01((activeAng + 30.0f)/100.0f));
    if(!isDaytime()) dim = lerp(0.25f, 0.45f, clamp01((moonAng+30.0f)/100.0f));

    for(auto& b: bldgs){
        glColor3f(b.r*dim,b.g*dim,b.b*dim);
        glBegin(GL_QUADS);
            glVertex2f(b.x,b.y);      glVertex2f(b.x+b.w,b.y);
            glVertex2f(b.x+b.w,b.y+b.h);glVertex2f(b.x,b.y+b.h);
        glEnd();
        // Lit windows (bright at night/pre-dawn)
        bool nightWin = (!isDaytime()) || sunAng < 10.0f || sunAng > 60.0f;
        float wbright = nightWin ? 0.90f : 0.42f*dim;
        float wcolR = nightWin ? 0.95f : 0.95f*dim;
        float wcolG = nightWin ? 0.85f : 0.80f*dim;
        float wcolB = nightWin ? 0.50f : 0.40f*dim;
        for(int wr=0;wr<(int)(b.h/20);wr++)
        for(int wc=0;wc<(int)(b.w/14);wc++){
            float wx=b.x+4+wc*14, wy=b.y+5+wr*20;
            // Randomise whether window is lit (simple deterministic pattern)
            if((wr+wc)%3==0 && !nightWin) continue;
            glColor3f(wcolR*wbright, wcolG*wbright, wcolB*wbright);
            glBegin(GL_QUADS);
                glVertex2f(wx,wy);    glVertex2f(wx+7,wy);
                glVertex2f(wx+7,wy+8);glVertex2f(wx,wy+8);
            glEnd();
        }
    }

    // A few small distant palm silhouettes between buildings
    float dim2 = dim * 0.7f;
    glColor3f(0.08f*dim2, 0.22f*dim2, 0.08f*dim2);
    // left side
    glBegin(GL_TRIANGLES);
        glVertex2f(-348,-115);glVertex2f(-342,-115);glVertex2f(-345,-85);
        glVertex2f(-308,-115);glVertex2f(-302,-115);glVertex2f(-305,-92);
    glEnd();
    // right side
    glBegin(GL_TRIANGLES);
        glVertex2f( 256,-115);glVertex2f( 262,-115);glVertex2f( 259,-82);
        glVertex2f( 298,-115);glVertex2f( 304,-115);glVertex2f( 301,-90);
    glEnd();
}
} // namespace Sakib

//  HUD overlay
namespace Sakib {
static void drawHUD(){
    glColor3f(0.0f,0.0f,0.0f);
    drawString(-395,188,"Scene 1: Morning Arrival");

    glColor3f(0.95f,0.95f,0.95f);
    if(!busMoving && !busArrived)
        drawString(-395,176,"SPACE:bus | C:clouds | B:birds | W:wind | +/-:sun | 1/2:cloud | 3/4:bird | 5/6:vehicle | 7/8:tide | P:pause sun");
    else if(busArrived && !gateOpen)
        drawString(-395,176,"G: open gate  |  Arrow keys: move boats");

    if(showNextMsg){
        glColor3f(0.1f,0.8f,0.2f);
        drawString(-100,-185,"Press N for Classroom scene");
    }
    if(sunPaused){
        glColor3f(0.9f,0.2f,0.1f);
        drawString(-30,-175,"[SUN/MOON PAUSED]");
    }

    // Time-of-day label
    float sunAng  = getSunAngle();
    float moonAng = getMoonAngle();
    const char* tod;
    if(!isDaytime()){
        tod = moonAng < 0.0f ? "Night" : "Late Night";
    } else {
        tod = sunAng < -10 ? "Pre-dawn" :
              sunAng <   0 ? "Twilight" :
              sunAng <  15 ? "Sunrise"  :
              sunAng <  60 ? "Morning"  : "Dusk";
    }
    glColor3f(0.1f,0.1f,0.1f);
    char buf[64];
    snprintf(buf,sizeof(buf),"Time: %s", tod);
    drawString(290,188,buf);

    // Speed indicators
    glColor3f(0.8f,0.8f,0.2f);
    snprintf(buf,sizeof(buf),"Sun:x%.2f Cld:x%.2f Brd:x%.2f Veh:x%.2f Tide:x%.2f",
             sunScale, cloudScale, birdScale, vehicleScale, tideScale);
    drawString(-395,164, buf);
}
} // namespace Sakib

//  Master draw entry — drawFeatures()
//  Draw order (back → front):
//   sky → sun/moon → distant city → river+boats → bank → building → gate →
//   flag → guard → bus stop sign → trees → road (over building bottom) →
//   vehicles on road → bus → students → clouds → birds → HUD
namespace Sakib {
void drawFeatures(){
    // 1. Sky background + sun/moon
    animateSkyColorChange();    // Feature 17 — calls glClear internally
    animateSunRising();         // Feature 14 + moon

    // 2. Distant city (behind everything)
    drawBackgroundCity();

    // 3. River and boats (below bank)
    drawRiver();
    drawBoat(boatT[0], 0.0f);
    drawBoat(boatT[1], 3.0f);

    // 4. School building (drawn before road so road overlaps bottom of building)
    drawSchoolBuilding();       // Feature 1

    // 5. Gate (attached to building)
    drawSchoolGate();           // Feature 2

    // 6. Flag
    drawFlag();                 // Feature 10

    // 7. Security guard
    drawSecurityGuard();        // Feature 9

    // 8. Trees (behind road pavement, in front of building sides)
    drawTrees();                // Feature 12

    // 9. Road — drawn AFTER building so it covers the building bottom edge.
    //    This ensures vehicles on the road are never seen "through" the building.
    drawRoad();                 // Feature 18

    // 10. Bus stop sign (on the pavement)
    drawSchoolSignboard();      // Feature 20

    // 11. Vehicles on road (drawn after road, so they're on top of road surface)
    animateCarPassing();        // Feature 19

    // 13. Students
    drawStudentsGettingOffBus();// Feature 7
    drawStudentsEntering();     // Feature 4

    // 12. Bus (upper lane, on top of road)
    drawSchoolBusArrival();     // Feature 5


    // 14. Clouds (near foreground sky layer)
    animateCloudsMoving();      // Feature 15

    // 15. Birds
    animateBirdsFlying();       // Feature 16

    // 16. HUD (always on top)
    drawHUD();
}
} // namespace Sakib

//  Timer tick — drives ALL animation at 60 fps
namespace Sakib {
static void tick(int){
    int now = glutGet(GLUT_ELAPSED_TIME);
    float dt = (now - lastMs) / 1000.0f;
    if(dt > 0.05f) dt = 0.05f;
    lastMs = now;

    gTime += dt;

        if(!sunPaused){
        // Full 360° cycle. At scale=1, full cycle ≈ 90s.
        cycleAngle += dt * sunScale * 4.0f;
        if(cycleAngle >= 360.0f) cycleAngle -= 360.0f;
    }

        if(cloudsMoving){
        for(auto& c: clouds){
            c.x += c.baseSpeed * cloudScale * dt;
            if(c.x > 430) c.x = -430;
        }
    }

        if(birdsFlying){
        for(auto& b: birds){
            b.x += b.baseSpeed * birdScale * dt;
            b.wingPhase += 5.5f * birdScale * dt;
            if(b.x > 440) b.x = -440;
        }
    }

        flagWave += 2.8f * dt;

        guardArmAngle += 1.6f * dt;

        if(windOn) windPhase += 1.9f * dt;

        tidePhase += 2.5f * tideScale * dt;

        if(busMoving && !busArrived){
        busX += busSpeed * dt;
        if(busX >= busTargetX){
            busX      = busTargetX;
            busMoving = false;
            busArrived= true;
            glutTimerFunc(700, [](int){ gateOpening=true; }, 0);
            studentOffT = 0.0f;
        }
    }

        if(gateOpening && gateOpenT < 1.0f){
        gateOpenT += dt * 0.85f;
        if(gateOpenT >= 1.0f){
            gateOpenT  = 1.0f;
            gateOpening= false;
            gateOpen   = true;
            studentsWalking = true;
            // Spawn all 5 students together
            for(int i=0;i<6;i++){
                students[i].x = busX - 35.0f + i*16.0f;
                students[i].y = -4.0f;
                students[i].active = true;
            }
        }
    }

        if(busArrived && studentOffT < 1.0f)
        studentOffT = clamp01(studentOffT + dt * 0.65f);

        if(studentsWalking){
        bool anyActive=false;
        for(auto& s: students){
            if(!s.active) continue;
            s.x += 26.0f * dt;
            if(s.x > -38.0f) s.active=false;  // enter through gate opening
            else anyActive=true;
        }
        if(!anyActive){
            studentsWalking=false;
            showNextMsg=true;
            // Auto-close the gate after 1 second
            glutTimerFunc(1000, [](int){ gateClosing=true; gateOpen=false; }, 0);
        }
    }

        if(gateClosing && gateOpenT > 0.0f){
        gateOpenT -= dt * 0.85f;
        if(gateOpenT <= 0.0f){
            gateOpenT  = 0.0f;
            gateClosing= false;
        }
    }

        static int* slotTypes[4] = {slot0Types, slot1Types, slot2Types, slot3Types};
    static int  slotCounts[4] = {4, 3, 3, 3};
    for(int i=0;i<4;i++){
        Vehicle& v = vehicles[i];
        v.x += v.baseSpeed * vehicleScale * dt;
        bool rl = (v.baseSpeed < 0.0f);
        if(rl && v.x < -490.0f){
            v.x = 490.0f;
            v.type = slotTypes[i][v.spawnIdx % slotCounts[i]];
            v.spawnIdx++;
        } else if(!rl && v.x > 490.0f){
            v.x = -490.0f;
            v.type = slotTypes[i][v.spawnIdx % slotCounts[i]];
            v.spawnIdx++;
        }
    }

        if(boatMoving){
        for(auto& bt: boatT){
            bt += 0.018f * dt;
            if(bt > 1.0f) bt -= 1.0f;
        }
    }

    glutPostRedisplay();
    glutTimerFunc(16, tick, 0);
}
} // namespace Sakib

//  Keyboard handler — individual speed controls
namespace Sakib {
void handleKey(unsigned char key, int /*x*/, int /*y*/){
    switch(key){
        case ' ':
            if(!busMoving && !busArrived) busMoving = true;
            break;
        case 'g': case 'G':
            if(!gateOpening && !gateClosing){
                if(!gateOpen){ gateOpening=true; }
                else { gateClosing=true; gateOpen=false; }
            }
            break;
        case 'c': case 'C': cloudsMoving = !cloudsMoving; break;
        case 'b': case 'B': birdsFlying  = !birdsFlying;  break;
        case 'w': case 'W': windOn       = !windOn;       break;
        case 'p': case 'P': sunPaused    = !sunPaused;    break;

        // Sun/moon speed
        case '+': case '=':
            sunScale = (sunScale < 8.0f) ? sunScale + 0.25f : sunScale;
            break;
        case '-': case '_':
            sunScale = (sunScale > 0.25f) ? sunScale - 0.25f : sunScale;
            break;

        // Cloud speed
        case '1':
            cloudScale = (cloudScale > 0.25f) ? cloudScale - 0.25f : cloudScale;
            break;
        case '2':
            cloudScale = (cloudScale < 6.0f) ? cloudScale + 0.25f : cloudScale;
            break;

        // Bird speed
        case '3':
            birdScale = (birdScale > 0.25f) ? birdScale - 0.25f : birdScale;
            break;
        case '4':
            birdScale = (birdScale < 6.0f) ? birdScale + 0.25f : birdScale;
            break;

        // Vehicle speed
        case '5':
            vehicleScale = (vehicleScale > 0.25f) ? vehicleScale - 0.25f : vehicleScale;
            break;
        case '6':
            vehicleScale = (vehicleScale < 6.0f) ? vehicleScale + 0.25f : vehicleScale;
            break;

        // Tide speed
        case '7':
            tideScale = (tideScale > 0.25f) ? tideScale - 0.25f : tideScale;
            break;
        case '8':
            tideScale = (tideScale < 6.0f) ? tideScale + 0.25f : tideScale;
            break;
    }
}

void handleSpecialKey(int key, int /*x*/, int /*y*/){
    float step = 0.04f;
    switch(key){
        case GLUT_KEY_RIGHT:
            for(auto& bt: boatT){ bt += step; if(bt>1.0f) bt-=1.0f; }
            boatMoving = false;
            break;
        case GLUT_KEY_LEFT:
            for(auto& bt: boatT){ bt -= step; if(bt<0.0f) bt+=1.0f; }
            boatMoving = false;
            break;
        case GLUT_KEY_UP:   boatMoving = true;  break;
        case GLUT_KEY_DOWN: boatMoving = false; break;
    }
    glutPostRedisplay();
}
} // namespace Sakib

//  Init
namespace Sakib {
void init(){
    initStars();
    // Start at pre-dawn (sun just below horizon, about to rise)
    cycleAngle = 10.0f;  // early in the day arc
    lastMs = glutGet(GLUT_ELAPSED_TIME);
    glutTimerFunc(16, tick, 0);
}
} // namespace Sakib
