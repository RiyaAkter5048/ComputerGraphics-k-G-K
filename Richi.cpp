// =============================================================================
//  Richi.cpp  —  "A Nostalgic School Day Memory" | Member 2 (Features 21–40)
//  Scene 2 : Classroom
//
//  Keyboard controls (active in Scene 2):
//    T       – trigger teacher entry  (students stand up on arrival)
//    F       – cycle fan speed  (slow / fast)
//    B       – trigger book-flipping animation
//    R       – ring the bell → students exit right
//    N       – handled by main.cpp (go to Scene 3)
//
//  Layout contract (same coord space as Sakib: gluOrtho2D -400..400, -200..200):
//    Wall   :  y = -90  ..  175
//    Floor  :  y = -200 .. -90
//    Ceiling:  y =  155 ..  175   (thin strip above wall top)
//    Board  :  centred at x=0, spans x -120..120, y 10..130
//    Fan L  :  x = -170,  y = 165
//    Fan R  :  x =  170,  y = 165
//    Clock  :  x =  300,  y =  110
//    Bell   :  x = -340,  y =  100  (lowered vs original)
//    Win L  :  x = -260, left of board
//    Win R  :  x =  145, right of board
//    Desks  :  2 rows × 5 columns centred in lower half
// =============================================================================

#include <GL/glut.h>
#include <cmath>
#include <cstring>
#include <vector>
#include "Richi.h"

// ─── constants ───────────────────────────────────────────────────────────────
static const float RPI = 3.14159265f;
static inline float rlerp (float a,float b,float t){return a+(b-a)*t;}
static inline float rclamp(float t,float lo,float hi){return t<lo?lo:t>hi?hi:t;}
static inline float rclamp01(float t){return rclamp(t,0,1);}

// =============================================================================
//  Private state
// =============================================================================
namespace {

// timing
float  rTime  = 0.0f;
int    rLastMs= 0;

// ── fans ──────────────────────────────────────────────────────────────────────
float fanAngle = 0.0f;
float fanSpeed = 180.0f;   // deg/sec  (toggled by F)

// ── clock ────────────────────────────────────────────────────────────────────
float clockSec = 0.0f;     // degrees from 12 o'clock
float clockMin = 0.0f;
float clockHr  = 0.0f;

// ── bell ──────────────────────────────────────────────────────────────────────
float bellSwing  = 0.0f;
float bellDecay  = 0.0f;   // amplitude; 0 = idle
bool  bellRinging= false;

// ── teacher ───────────────────────────────────────────────────────────────────
float teacherX      = -460.0f;
float teacherTarget = -175.0f;   // stands left of board
bool  teacherMoving = false;
bool  teacherArrived= false;
float teacherArmPhase = 0.0f;   // drives arm wave / explain swing

// ── students standing-up ──────────────────────────────────────────────────────
float studentStandT = 0.0f;    // 0=seated, 1=standing (rises when teacher arrives)
bool  studentsStanding = false;

// ── chalk writing "Hello Students" ───────────────────────────────────────────
bool  chalkWriting = false;
float chalkProgress= 0.0f;   // 0→1 fraction of text revealed

// ── sunlight ─────────────────────────────────────────────────────────────────
float rayPhase = 0.0f;

// ── page flipping ────────────────────────────────────────────────────────────
bool  pageFlipping = false;
float pageFlipT    = 0.0f;
int   pageFlipNum  = 0;

// ── class ending ─────────────────────────────────────────────────────────────
bool  classEnding  = false;
float classEndT    = 0.0f;
bool  showNextMsg  = false;

// ── desk/student layout (pre-computed) ───────────────────────────────────────
//   Row 0 (back)  : y = -95,  Row 1 (front) : y = -140
//   5 desks per row, evenly spaced from x=-150 to x=150
static const int   ROWS = 2, COLS = 5;
static const float DESK_Y[ROWS]   = {-95.0f, -140.0f};
static const float DESK_X_START   = -150.0f;
static const float DESK_SPACING   =  75.0f;

float deskX(int col){ return DESK_X_START + col * DESK_SPACING; }

// per-student colours (10 students)
static const float SCOLORS[10][3] = {
    {0.15f,0.35f,0.70f}, {0.70f,0.20f,0.15f}, {0.15f,0.60f,0.25f},
    {0.60f,0.40f,0.10f}, {0.55f,0.15f,0.55f}, {0.20f,0.50f,0.60f},
    {0.65f,0.30f,0.10f}, {0.25f,0.55f,0.45f}, {0.45f,0.45f,0.10f},
    {0.30f,0.20f,0.65f},
};

} // anonymous namespace

// =============================================================================
//  Utility text helpers
// =============================================================================
namespace Richi {
static void drawStr(float x,float y,const char* s,void* font=GLUT_BITMAP_HELVETICA_12){
    glRasterPos2f(x,y);
    while(*s) glutBitmapCharacter(font,*s++);
}
} // namespace Richi

// =============================================================================
//  Feature 21 — drawClassroomInterior
// =============================================================================
namespace Richi {
void drawClassroomInterior(){
    // ── back wall ────────────────────────────────────────────────────────────
    glColor3f(0.94f,0.91f,0.83f);
    glBegin(GL_QUADS);
        glVertex2f(-400,-90); glVertex2f(400,-90);
        glVertex2f( 400,155); glVertex2f(-400,155);
    glEnd();

    // ── ceiling ───────────────────────────────────────────────────────────────
    glColor3f(0.98f,0.97f,0.94f);
    glBegin(GL_QUADS);
        glVertex2f(-400,155); glVertex2f(400,155);
        glVertex2f( 400,175); glVertex2f(-400,175);
    glEnd();
    // ceiling/wall join strip
    glColor3f(0.85f,0.82f,0.75f);
    glBegin(GL_QUADS);
        glVertex2f(-400,152); glVertex2f(400,152);
        glVertex2f( 400,157); glVertex2f(-400,157);
    glEnd();

    // ── floor ─────────────────────────────────────────────────────────────────
    glColor3f(0.76f,0.62f,0.42f);
    glBegin(GL_QUADS);
        glVertex2f(-400,-200); glVertex2f(400,-200);
        glVertex2f( 400,- 90); glVertex2f(-400,- 90);
    glEnd();
    // floor tiles
    glColor3f(0.68f,0.55f,0.36f);
    glLineWidth(1.0f);
    for(int i=-400;i<=400;i+=50){
        glBegin(GL_LINES);
            glVertex2f((float)i,-200); glVertex2f((float)i,-90);
        glEnd();
    }
    for(int j=-200;j<=-90;j+=30){
        glBegin(GL_LINES);
            glVertex2f(-400,(float)j); glVertex2f(400,(float)j);
        glEnd();
    }

    // ── skirting ─────────────────────────────────────────────────────────────
    glColor3f(0.80f,0.76f,0.68f);
    glBegin(GL_QUADS);
        glVertex2f(-400,-93); glVertex2f(400,-93);
        glVertex2f( 400,-88); glVertex2f(-400,-88);
    glEnd();
}
} // namespace Richi

// =============================================================================
//  Helper: drawFan (used twice)
// =============================================================================
namespace Richi {
static void drawFan(float fx, float fy){
    // down-rod
    glColor3f(0.60f,0.58f,0.55f);
    glBegin(GL_QUADS);
        glVertex2f(fx-2,fy); glVertex2f(fx+2,fy);
        glVertex2f(fx+2,fy+12);glVertex2f(fx-2,fy+12);
    glEnd();
    // motor housing
    glColor3f(0.65f,0.62f,0.58f);
    glBegin(GL_TRIANGLE_FAN);
        glVertex2f(fx,fy);
        for(int i=0;i<=20;i++){
            float a=2*RPI*i/20;
            glVertex2f(fx+cosf(a)*10,fy+sinf(a)*8);
        }
    glEnd();
    // 4 blades (rotate)
    glPushMatrix();
    glTranslatef(fx,fy,0);
    glRotatef(fanAngle,0,0,1);
    for(int b=0;b<4;b++){
        glPushMatrix();
        glRotatef(b*90.0f,0,0,1);
        glColor3f(0.55f,0.40f,0.20f);
        glBegin(GL_QUADS);
            glVertex2f( 8,-4); glVertex2f(55,-6);
            glVertex2f(55, 6); glVertex2f( 8, 4);
        glEnd();
        glColor3f(0.65f,0.50f,0.28f);
        glBegin(GL_LINES);
            glVertex2f(8,-4); glVertex2f(55,-6);
        glEnd();
        glPopMatrix();
    }
    glPopMatrix();
}
} // namespace Richi

// =============================================================================
//  Feature 33 — animateCeilingFan  (two fans)
// =============================================================================
namespace Richi {
void animateCeilingFan(){
    drawFan(-170.0f, 165.0f);   // left fan
    drawFan( 170.0f, 165.0f);   // right fan
}
} // namespace Richi

// =============================================================================
//  Feature 25 — drawBlackboard  (centred on wall)
// =============================================================================
namespace Richi {
void drawBlackboard(){
    // board frame — centred x=-120..120, y=10..130
    glColor3f(0.38f,0.24f,0.10f);
    glBegin(GL_QUADS);
        glVertex2f(-124, 6); glVertex2f(124, 6);
        glVertex2f( 124,134);glVertex2f(-124,134);
    glEnd();
    // board surface
    glColor3f(0.06f,0.26f,0.10f);
    glBegin(GL_QUADS);
        glVertex2f(-120,10); glVertex2f(120,10);
        glVertex2f( 120,130);glVertex2f(-120,130);
    glEnd();
    // chalk tray
    glColor3f(0.30f,0.20f,0.08f);
    glBegin(GL_QUADS);
        glVertex2f(-124,5); glVertex2f(124,5);
        glVertex2f( 124,10);glVertex2f(-124,10);
    glEnd();
    // chalk pieces in tray
    for(int c=0;c<3;c++){
        float cx = -110.0f + c*15.0f;
        glColor3f(c==0?0.95f:0.90f, c==1?0.60f:0.95f, c==2?0.50f:0.90f);
        glBegin(GL_QUADS);
            glVertex2f(cx,6); glVertex2f(cx+10,6);
            glVertex2f(cx+10,8);glVertex2f(cx,8);
        glEnd();
    }

    // ── "Hello Students" text (revealed progressively via chalkProgress) ─────
    const char* msg = "Hello Students";
    int len = (int)strlen(msg);
    int reveal = (int)(chalkProgress * len);
    char buf[64]; buf[0]='\0';
    if(reveal>0){
        strncpy(buf, msg, reveal);
        buf[reveal]='\0';
    }
    glColor3f(0.92f,0.90f,0.85f);
    if(buf[0]) drawStr(-95.0f, 68.0f, buf, GLUT_BITMAP_HELVETICA_18);

    // chalk tip indicator while writing
    if(chalkWriting && chalkProgress < 1.0f){
        float tipX = -95.0f + chalkProgress * 180.0f;
        glColor3f(0.95f,0.95f,0.90f);
        glPointSize(5.0f);
        glBegin(GL_POINTS);
            glVertex2f(tipX, 68.0f);
        glEnd();
        glPointSize(1.0f);
    }

    // "TODAY'S LESSON" header
    glColor3f(0.70f,0.90f,0.70f);
    drawStr(-90.0f, 118.0f, "TODAY'S LESSON", GLUT_BITMAP_HELVETICA_12);
}
} // namespace Richi

// =============================================================================
//  Feature 26/27 — stubs (logic lives in tick + drawBlackboard)
// =============================================================================
namespace Richi {
void animateWritingOnBlackboard(){ /* state drives chalkProgress in tick */ }
void animateChalkMovement()      { /* visual merged into drawBlackboard  */ }
} // namespace Richi

// =============================================================================
//  Feature 34 — drawWindow  |  Feature 35 — animateSunlightThroughWindow
//  Windows: left of board at x≈-210, right of board at x≈145
// =============================================================================
namespace Richi {
static void drawOneWindow(float wx, float wy){
    // sill/frame
    glColor3f(0.82f,0.78f,0.70f);
    glBegin(GL_QUADS);
        glVertex2f(wx-3,wy-3);  glVertex2f(wx+53,wy-3);
        glVertex2f(wx+53,wy+73);glVertex2f(wx-3, wy+73);
    glEnd();
    // sky pane
    glColor3f(0.58f,0.79f,0.95f);
    glBegin(GL_QUADS);
        glVertex2f(wx,wy);    glVertex2f(wx+50,wy);
        glVertex2f(wx+50,wy+70);glVertex2f(wx,wy+70);
    glEnd();
    // cross bars
    glColor3f(0.82f,0.78f,0.70f);
    glBegin(GL_QUADS);
        glVertex2f(wx,   wy+32);glVertex2f(wx+50,wy+32);
        glVertex2f(wx+50,wy+37);glVertex2f(wx,   wy+37);
    glEnd();
    glBegin(GL_QUADS);
        glVertex2f(wx+22,wy);   glVertex2f(wx+28,wy);
        glVertex2f(wx+28,wy+70);glVertex2f(wx+22,wy+70);
    glEnd();
}

void drawWindow(){
    drawOneWindow(-265.0f, 20.0f);   // left of board
    drawOneWindow( 148.0f, 20.0f);   // right of board
}

void animateSunlightThroughWindow(){
    float alpha = 0.10f + 0.05f*sinf(rayPhase);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    // rays from RIGHT window (148..198, y=20..90)
    glColor4f(1.0f,0.95f,0.70f,alpha);
    glBegin(GL_QUADS);
        glVertex2f(155,90); glVertex2f(195,90);
        glVertex2f( 50,-90);glVertex2f( 10,-90);
    glEnd();
    glColor4f(1.0f,0.95f,0.70f,alpha*0.55f);
    glBegin(GL_QUADS);
        glVertex2f(165,90); glVertex2f(198,90);
        glVertex2f( 90,-90);glVertex2f( 55,-90);
    glEnd();
    // dust motes
    glColor4f(1.0f,1.0f,0.85f,0.45f);
    glPointSize(2.0f);
    glBegin(GL_POINTS);
    for(int m=0;m<10;m++){
        float mt = fmodf(m/10.0f + rTime*0.06f, 1.0f);
        float mx = 175.0f + (30.0f - 175.0f)*mt;
        float my =  90.0f + (-90.0f -  90.0f)*mt;
        glVertex2f(mx + sinf(rTime+m)*5, my + cosf(rTime*0.6f+m)*3);
    }
    glEnd();
    glPointSize(1.0f);
    glDisable(GL_BLEND);
}
} // namespace Richi

// =============================================================================
//  Feature 31/32 — drawClockOnWall + animateClockMoving
// =============================================================================
namespace Richi {
void drawClockOnWall(){
    float cx=300.0f, cy=110.0f, cr=28.0f;
    // frame
    glColor3f(0.55f,0.35f,0.15f);
    glBegin(GL_TRIANGLE_FAN);
        glVertex2f(cx,cy);
        for(int i=0;i<=40;i++){
            float a=2*RPI*i/40;
            glVertex2f(cx+cosf(a)*(cr+4),cy+sinf(a)*(cr+4));
        }
    glEnd();
    // face
    glColor3f(0.97f,0.96f,0.90f);
    glBegin(GL_TRIANGLE_FAN);
        glVertex2f(cx,cy);
        for(int i=0;i<=40;i++){
            float a=2*RPI*i/40;
            glVertex2f(cx+cosf(a)*cr,cy+sinf(a)*cr);
        }
    glEnd();
    // hour marks
    glColor3f(0.25f,0.20f,0.18f);
    for(int h=0;h<12;h++){
        float a=2*RPI*h/12 - RPI/2;
        float len = (h%3==0) ? 5.0f : 3.0f;
        glLineWidth((h%3==0)?2.5f:1.2f);
        glBegin(GL_LINES);
            glVertex2f(cx+cosf(a)*(cr-len),cy+sinf(a)*(cr-len));
            glVertex2f(cx+cosf(a)*(cr-1),  cy+sinf(a)*(cr-1));
        glEnd();
    }
    glLineWidth(1.0f);
    // hour hand
    float hrRad  = (clockHr -90)*RPI/180.0f;
    float minRad = (clockMin-90)*RPI/180.0f;
    float secRad = (clockSec-90)*RPI/180.0f;
    glColor3f(0.15f,0.12f,0.10f);
    glLineWidth(3.0f);
    glBegin(GL_LINES);
        glVertex2f(cx,cy);
        glVertex2f(cx+cosf(hrRad)*(cr*0.55f),cy+sinf(hrRad)*(cr*0.55f));
    glEnd();
    // minute hand
    glLineWidth(2.0f);
    glBegin(GL_LINES);
        glVertex2f(cx,cy);
        glVertex2f(cx+cosf(minRad)*(cr*0.80f),cy+sinf(minRad)*(cr*0.80f));
    glEnd();
    // second hand
    glColor3f(0.80f,0.10f,0.10f);
    glLineWidth(1.2f);
    glBegin(GL_LINES);
        glVertex2f(cx,cy);
        glVertex2f(cx+cosf(secRad)*(cr*0.88f),cy+sinf(secRad)*(cr*0.88f));
    glEnd();
    // centre dot
    glColor3f(0.15f,0.12f,0.10f);
    glBegin(GL_TRIANGLE_FAN);
        glVertex2f(cx,cy);
        for(int i=0;i<=10;i++){
            float a=2*RPI*i/10;
            glVertex2f(cx+cosf(a)*3,cy+sinf(a)*3);
        }
    glEnd();
    glLineWidth(1.0f);
}
void animateClockMoving(){ /* updated in tick() */ }
} // namespace Richi

// =============================================================================
//  Feature 39 — animateBellRinging  (bell at top-left, slightly lower)
// =============================================================================
namespace Richi {
void animateBellRinging(){
    float bx=-340.0f, by=105.0f;   // lower than original 130

    // mount bracket
    glColor3f(0.45f,0.35f,0.25f);
    glBegin(GL_QUADS);
        glVertex2f(bx-4,by+22);glVertex2f(bx+4,by+22);
        glVertex2f(bx+4,by+32);glVertex2f(bx-4,by+32);
    glEnd();

    // vibrate left/right when ringing, else keep visible but still
    float swing = bellRinging ? sinf(bellSwing)*16.0f*bellDecay : 0.0f;
    glPushMatrix();
    glTranslatef(bx,by+22,0);
    glRotatef(swing,0,0,1);
    glTranslatef(-bx,-(by+22),0);

    // dome
    glColor3f(0.88f,0.74f,0.12f);
    glBegin(GL_TRIANGLE_FAN);
        glVertex2f(bx,by+20);
        for(int i=0;i<=20;i++){
            float a=RPI+RPI*i/20;
            glVertex2f(bx+cosf(a)*20,by+20+sinf(a)*20);
        }
    glEnd();
    // rim
    glColor3f(0.75f,0.62f,0.10f);
    glBegin(GL_QUADS);
        glVertex2f(bx-21,by);  glVertex2f(bx+21,by);
        glVertex2f(bx+21,by+4);glVertex2f(bx-21,by+4);
    glEnd();
    // clapper
    float clapOff = bellRinging ? sinf(bellSwing*1.1f)*6.0f*bellDecay : 0.0f;
    glColor3f(0.60f,0.50f,0.10f);
    glLineWidth(2.0f);
    glBegin(GL_LINES);
        glVertex2f(bx,by+14); glVertex2f(bx+clapOff,by+4);
    glEnd();
    glLineWidth(1.0f);
    glBegin(GL_TRIANGLE_FAN);
        glVertex2f(bx+clapOff,by+4);
        for(int i=0;i<=10;i++){
            float a=2*RPI*i/10;
            glVertex2f(bx+clapOff+cosf(a)*3,by+4+sinf(a)*3);
        }
    glEnd();

    glPopMatrix();

    // "RING!" label
    if(bellRinging && bellDecay>0.05f){
        glColor3f(0.80f,0.10f,0.10f);
        float sz = 0.8f + 0.4f*sinf(bellSwing*3);
        glPushMatrix();
            glTranslatef(bx+26,by+12,0);
            glScalef(sz,sz,1);
            drawStr(0,0,"RING!");
        glPopMatrix();
    }
}
} // namespace Richi

// =============================================================================
//  Desks: helper (one individual desk)
// =============================================================================
namespace Richi {
static void drawDesk(float dx, float dy){
    // desk top
    glColor3f(0.55f,0.35f,0.15f);
    glBegin(GL_QUADS);
        glVertex2f(dx-22,dy+14); glVertex2f(dx+22,dy+14);
        glVertex2f(dx+22,dy+20); glVertex2f(dx-22,dy+20);
    glEnd();
    // left leg
    glColor3f(0.45f,0.28f,0.10f);
    glBegin(GL_QUADS);
        glVertex2f(dx-18,dy);    glVertex2f(dx-14,dy);
        glVertex2f(dx-14,dy+14); glVertex2f(dx-18,dy+14);
    glEnd();
    // right leg
    glBegin(GL_QUADS);
        glVertex2f(dx+14,dy);    glVertex2f(dx+18,dy);
        glVertex2f(dx+18,dy+14); glVertex2f(dx+14,dy+14);
    glEnd();
    // chair seat (behind desk)
    glColor3f(0.50f,0.32f,0.12f);
    glBegin(GL_QUADS);
        glVertex2f(dx-16,dy-8); glVertex2f(dx+16,dy-8);
        glVertex2f(dx+16,dy-3); glVertex2f(dx-16,dy-3);
    glEnd();
}
} // namespace Richi

// =============================================================================
//  Feature 37 — drawBooks  |  Feature 38 — animatePageFlipping
// =============================================================================
namespace Richi {
void drawBooks(){
    static const float BCOLORS[5][3] = {
        {0.80f,0.18f,0.18f},{0.18f,0.48f,0.80f},{0.18f,0.62f,0.28f},
        {0.70f,0.55f,0.10f},{0.55f,0.15f,0.55f},
    };
    for(int row=0;row<ROWS;row++){
        for(int col=0;col<COLS;col++){
            float dx = deskX(col);
            float dy = DESK_Y[row];
            // book cover (clearly visible on desk top)
            float r=BCOLORS[col][0], g=BCOLORS[col][1], b=BCOLORS[col][2];
            glColor3f(r,g,b);
            glBegin(GL_QUADS);
                glVertex2f(dx-14,dy+20); glVertex2f(dx+14,dy+20);
                glVertex2f(dx+14,dy+30); glVertex2f(dx-14,dy+30);
            glEnd();
            // book spine
            glColor3f(r*0.7f,g*0.7f,b*0.7f);
            glBegin(GL_QUADS);
                glVertex2f(dx-14,dy+20); glVertex2f(dx-11,dy+20);
                glVertex2f(dx-11,dy+30); glVertex2f(dx-14,dy+30);
            glEnd();
            // page edges (white)
            glColor3f(0.95f,0.93f,0.88f);
            glBegin(GL_QUADS);
                glVertex2f(dx+11,dy+20); glVertex2f(dx+14,dy+20);
                glVertex2f(dx+14,dy+30); glVertex2f(dx+11,dy+30);
            glEnd();
        }
    }
}

void animatePageFlipping(){
    if(!pageFlipping) return;
    // animate on ALL desks simultaneously
    for(int row=0;row<ROWS;row++){
        for(int col=0;col<COLS;col++){
            float dx = deskX(col);
            float dy = DESK_Y[row];
            float t  = pageFlipT;
            // open book base
            glColor3f(0.96f,0.93f,0.86f);
            glBegin(GL_QUADS);
                glVertex2f(dx-14,dy+20); glVertex2f(dx+14,dy+20);
                glVertex2f(dx+14,dy+32); glVertex2f(dx-14,dy+32);
            glEnd();
            // spine divider
            glColor3f(0.50f,0.30f,0.10f);
            glBegin(GL_LINES);
                glVertex2f(dx,dy+20); glVertex2f(dx,dy+32);
            glEnd();
            // flipping page (right half, rotates using cosine foreshortening)
            float rad = t * RPI;
            float pw  = 14.0f * cosf(rad);
            float shade = 0.55f + 0.45f*fabsf(cosf(rad));
            glColor3f(shade, shade*0.94f, shade*0.84f);
            glBegin(GL_QUADS);
                glVertex2f(dx,        dy+20);
                glVertex2f(dx+pw,     dy+20);
                glVertex2f(dx+pw,     dy+32);
                glVertex2f(dx,        dy+32);
            glEnd();
        }
    }
}
} // namespace Richi

// =============================================================================
//  Feature 22 — animateTeacherEntry  |  Teacher draw helper
// =============================================================================
namespace Richi {
static void drawTeacher(float x, float y){
    // body (saree teal)
    glColor3f(0.10f,0.55f,0.52f);
    glBegin(GL_QUADS);
        glVertex2f(x-10,y);    glVertex2f(x+10,y);
        glVertex2f(x+10,y+30); glVertex2f(x-10,y+30);
    glEnd();
    // head
    glColor3f(0.82f,0.65f,0.45f);
    glBegin(GL_TRIANGLE_FAN);
        glVertex2f(x,y+38);
        for(int i=0;i<=16;i++){
            float a=2*RPI*i/16;
            glVertex2f(x+cosf(a)*9,y+30+sinf(a)*9);
        }
    glEnd();
    // hair bun
    glColor3f(0.12f,0.08f,0.05f);
    glBegin(GL_TRIANGLE_FAN);
        glVertex2f(x,y+47);
        for(int i=0;i<=12;i++){
            float a=2*RPI*i/12;
            glVertex2f(x+cosf(a)*5,y+41+sinf(a)*5);
        }
    glEnd();
    // arm (wave/explain)
    float armAngle = teacherArrived
                       ? 45.0f + sinf(teacherArmPhase)*35.0f
                       : 0.0f;
    glColor3f(0.10f,0.55f,0.52f);
    glLineWidth(4.0f);
    glBegin(GL_LINES);
        glVertex2f(x+10,y+22);
        glVertex2f(x+10+cosf(armAngle*RPI/180.0f)*20,
                   y+22+sinf(armAngle*RPI/180.0f)*20);
    glEnd();
    // holding-chalk left arm
    glBegin(GL_LINES);
        glVertex2f(x-10,y+22);
        glVertex2f(x-22,y+10);
    glEnd();
    glLineWidth(1.0f);
    // legs
    glColor3f(0.08f,0.44f,0.41f);
    glLineWidth(3.0f);
    glBegin(GL_LINES);
        glVertex2f(x-5,y);  glVertex2f(x-6,y-14);
        glVertex2f(x+5,y);  glVertex2f(x+6,y-14);
    glEnd();
    glLineWidth(1.0f);
}

void animateTeacherEntry(){
    if(classEnding){
        // teacher exits right during class ending
        float ex = teacherX + rclamp01((classEndT-0.3f)/0.5f)*350.0f;
        if(ex < 430.0f) drawTeacher(ex,-90.0f);
    } else {
        drawTeacher(teacherX,-90.0f);
    }
}
} // namespace Richi

// =============================================================================
//  Feature 23 — drawStudentsOnBenches  (2 rows × 5 cols)
// =============================================================================
namespace Richi {
static void drawStudentAt(float x, float y, int idx, float standT, bool flip){
    float r=SCOLORS[idx][0], g=SCOLORS[idx][1], b=SCOLORS[idx][2];
    float bodyY = y + standT*22.0f;
    // body
    glColor3f(r,g,b);
    glBegin(GL_QUADS);
        glVertex2f(x-7,bodyY);    glVertex2f(x+7,bodyY);
        glVertex2f(x+7,bodyY+18);glVertex2f(x-7,bodyY+18);
    glEnd();
    // head
    glColor3f(0.82f,0.65f,0.45f);
    glBegin(GL_TRIANGLE_FAN);
        glVertex2f(x,bodyY+25);
        for(int i=0;i<=12;i++){
            float a=2*RPI*i/12;
            glVertex2f(x+cosf(a)*7,bodyY+18+sinf(a)*7);
        }
    glEnd();
    // arms (down by default, slight reading lean when flip)
    glColor3f(r,g,b);
    glLineWidth(3.0f);
    if(flip){
        // leaning forward to read
        glBegin(GL_LINES);
            glVertex2f(x-7,bodyY+12); glVertex2f(x-16,bodyY+3);
            glVertex2f(x+7,bodyY+12); glVertex2f(x+16,bodyY+3);
        glEnd();
    } else {
        glBegin(GL_LINES);
            glVertex2f(x-7,bodyY+12); glVertex2f(x-12,bodyY+4);
            glVertex2f(x+7,bodyY+12); glVertex2f(x+12,bodyY+4);
        glEnd();
    }
    glLineWidth(1.0f);
    // bag
    glColor3f(0.30f,0.20f,0.55f);
    glBegin(GL_QUADS);
        glVertex2f(x-7, bodyY+4); glVertex2f(x-13,bodyY+4);
        glVertex2f(x-13,bodyY+14);glVertex2f(x-7, bodyY+14);
    glEnd();
}

void drawStudentsOnBenches(){
    for(int row=0;row<ROWS;row++){
        for(int col=0;col<COLS;col++){
            float dx = deskX(col);
            float dy = DESK_Y[row];
            // draw individual desk first
            drawDesk(dx, dy);
            // student above desk
            int idx = row*COLS + col;
            float standT = studentsStanding ? studentStandT : 0.0f;
            drawStudentAt(dx, dy+20.0f, idx, standT, pageFlipping);
        }
    }
}
} // namespace Richi

// =============================================================================
//  Feature 24, 28, 29, 30 — stubs (state managed in tick)
// =============================================================================
namespace Richi {
void animateTeacherGreeting() { /* arm via drawTeacher */ }
void animateStudentsRaiseHands() { /* merged into drawStudentAt */ }
void animateStudentAnswering() { /* merged into student stand logic */ }
void animateTeacherExplaining() { /* arm via drawTeacher */ }
void animateStudentsTalking() { /* removed — per spec no explicit talk bubbles needed */ }
} // namespace Richi

// =============================================================================
//  Feature 40 — animateClassEnding  (students exit to the RIGHT)
// =============================================================================
namespace Richi {
void animateClassEnding(){
    if(!classEnding) return;
    // draw exiting students (slide right off screen)
    for(int row=0;row<ROWS;row++){
        for(int col=0;col<COLS;col++){
            int idx = row*COLS + col;
            float delay = idx * 0.08f;
            float moveT = rclamp01((classEndT - delay) / 0.7f);
            float ex = deskX(col) + moveT * 600.0f;
            if(ex > 430.0f) continue;
            float dy = DESK_Y[row];
            drawStudentAt(ex, dy+20.0f, idx, 1.0f, false);
        }
    }
    if(classEndT >= 1.2f) showNextMsg = true;
}
} // namespace Richi

// =============================================================================
//  HUD
// =============================================================================
namespace Richi {
static void drawHUD(){
    glColor3f(0.15f,0.15f,0.30f);
    drawStr(-395, 185, "Scene 2: Classroom  Richi's Features");
    glColor3f(0.30f,0.30f,0.30f);
    if(!teacherArrived && !classEnding)
        drawStr(-395, 172, "T: teacher enters | F: fan speed | B: flip book | R: ring bell");
    else if(!classEnding)
        drawStr(-395, 172, "F: fan speed | B: flip book | R: ring bell");
    if(showNextMsg)
        drawStr(-130,-185,"Press N for next scene (Riya's playground)");
}
} // namespace Richi

// =============================================================================
//  Master draw entry — drawFeatures()
// =============================================================================
namespace Richi {
void drawFeatures(){
    glClearColor(0.94f,0.91f,0.83f,1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    drawClassroomInterior();         // F21
    drawWindow();                    // F34
    animateSunlightThroughWindow();  // F35  (must come after window draws)
    drawBlackboard();                // F25+F26+F27
    animateCeilingFan();             // F33

    if(!classEnding){
        drawStudentsOnBenches();     // F23 (includes desks)
        animatePageFlipping();       // F38
    } else {
        // still draw empty desks + books during exit
        for(int row=0;row<ROWS;row++)
            for(int col=0;col<COLS;col++)
                drawDesk(deskX(col), DESK_Y[row]);
        drawBooks();
        animateClassEnding();        // F40
    }

    // books stay visible when not flipping
    if(!classEnding && !pageFlipping) drawBooks();

    animateTeacherEntry();           // F22
    animateTeacherGreeting();        // F24
    animateTeacherExplaining();      // F30

    drawClockOnWall();               // F31+F32
    animateBellRinging();            // F39

    drawHUD();
}
} // namespace Richi

// =============================================================================
//  Timer tick
// =============================================================================
namespace Richi {
static void tick(int){
    int now = glutGet(GLUT_ELAPSED_TIME);
    float dt = (now - rLastMs) / 1000.0f;
    if(dt > 0.05f) dt = 0.05f;
    rLastMs = now;
    rTime  += dt;

    // clock (realistic speed: 6 deg/s for seconds, /60 for minutes, /720 for hours)
    clockSec += 6.0f   * dt;
    clockMin += 0.1f   * dt;
    clockHr  += (0.1f/12.0f) * dt;

    // fans
    fanAngle += fanSpeed * dt;
    if(fanAngle > 360.0f) fanAngle -= 360.0f;

    // sunlight pulse
    rayPhase += 0.8f * dt;

    // teacher entry
    if(teacherMoving && !teacherArrived){
        teacherX += 95.0f * dt;
        if(teacherX >= teacherTarget){
            teacherX = teacherTarget;
            teacherMoving  = false;
            teacherArrived = true;
            // students stand up in respect
            studentsStanding = true;
            // start writing 1.5s after arrival
            glutTimerFunc(1500,[](int){ chalkWriting=true; },0);
        }
    }

    // teacher arm animation (greeting/explaining)
    if(teacherArrived && !classEnding)
        teacherArmPhase += 1.2f * dt;

    // chalk writing
    if(chalkWriting && chalkProgress < 1.0f){
        chalkProgress += dt * 0.18f;   // takes ~5.5s to write full text
        if(chalkProgress > 1.0f) chalkProgress = 1.0f;
    }

    // students standing up smoothly
    if(studentsStanding && studentStandT < 1.0f)
        studentStandT = rclamp01(studentStandT + dt*1.5f);

    // page flipping
    if(pageFlipping){
        pageFlipT += dt * 1.2f;
        if(pageFlipT >= 1.0f){
            pageFlipT = 0.0f;
            pageFlipNum++;
            if(pageFlipNum >= 4){ pageFlipping=false; pageFlipNum=0; }
        }
    }

    // bell
    if(bellRinging){
        bellSwing += 14.0f * dt;
        bellDecay -= dt * 0.22f;
        if(bellDecay <= 0.0f){
            bellDecay   = 0.0f;
            bellRinging = false;
            // start class ending 0.6s after bell fades
            glutTimerFunc(600,[](int){ classEnding=true; },0);
        }
    }

    // class ending
    if(classEnding && classEndT < 1.5f)
        classEndT += dt * 0.45f;

    glutPostRedisplay();
    glutTimerFunc(16, tick, 0);
}
} // namespace Richi

// =============================================================================
//  Keyboard handler
// =============================================================================
namespace Richi {
void handleKey(unsigned char key, int /*x*/, int /*y*/){
    switch(key){
        case 't': case 'T':
            if(!teacherMoving && !teacherArrived)
                teacherMoving = true;
            break;
        case 'f': case 'F':
            fanSpeed = (fanSpeed < 300.0f) ? 420.0f : 180.0f;
            break;
        case 'b': case 'B':
            if(!pageFlipping && !classEnding){
                pageFlipping = true; pageFlipT = 0.0f; pageFlipNum = 0;
            }
            break;
        case 'r': case 'R':
            if(!bellRinging && !classEnding){
                bellRinging = true;
                bellSwing   = 0.0f;
                bellDecay   = 1.0f;
            }
            break;
    }
}
} // namespace Richi

// =============================================================================
//  Init
// =============================================================================
namespace Richi {
void init(){
    rLastMs = glutGet(GLUT_ELAPSED_TIME);
    glutTimerFunc(16, tick, 0);
}
} // namespace Richi
