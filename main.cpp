// =============================================================================
//  main.cpp  —  "A Nostalgic School Day Memory"
//  Wires all three scenes together.
//
//  Scene 0 : Sakib   (Morning Arrival)
//  Scene 1 : Richi   (Classroom)
//  Scene 2 : Riya    (Playground)
//
//  Press N (or n) at any time to advance to the next scene.
// =============================================================================

#include <GL/glut.h>
#include "Sakib.h"
#include "Richi.h"
#include "Riya.h"

// ── active scene index ────────────────────────────────────────────────────────
static int currentScene = 0;  // 0=Sakib, 1=Richi, 2=Riya

// ── display callback ──────────────────────────────────────────────────────────
void display(){
    if      (currentScene == 0) Sakib::drawFeatures();
    else if (currentScene == 1) Richi::drawFeatures();
    else                        Riya::drawFeatures();
    glutSwapBuffers();
}

// ── keyboard callback ─────────────────────────────────────────────────────────
void keyboard(unsigned char key, int x, int y){
    // Scene transition — N advances through all three scenes cyclically
    if(key == 'n' || key == 'N'){
        currentScene = (currentScene + 1) % 3;
        glutPostRedisplay();   // force a repaint immediately on scene switch
        return;
    }

    // Route every other key to the currently active scene's handler
    if      (currentScene == 0) Sakib::handleKey(key, x, y);
    else if (currentScene == 1) Richi::handleKey(key, x, y);   // ← was missing
    else                        Riya::handleKey (key, x, y);   // ← was missing
}

// ── special-key callback (arrow keys — used by Sakib) ─────────────────────────
void specialKey(int key, int x, int y){
    if(currentScene == 0) Sakib::handleSpecialKey(key, x, y);
}

// ── main ──────────────────────────────────────────────────────────────────────
int main(int argc, char** argv){
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB);
    glutInitWindowSize(900, 600);
    glutCreateWindow("A Nostalgic School Day Memory");

    // Shared projection — all three scenes use the same coordinate space
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluOrtho2D(-400, 400, -200, 200);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    // Register GLUT callbacks
    glutDisplayFunc (display);
    glutKeyboardFunc(keyboard);
    glutSpecialFunc (specialKey);

    // ── Init ALL three scenes ────────────────────────────────────────────────
    // Each init() call registers that scene's own glutTimerFunc(16, tick, 0).
    // All three timers run in parallel so that switching scenes is instant
    // and animations resume exactly where they left off.
    Sakib::init();   // registers Sakib's 60 fps timer
    Richi::init();   // registers Richi's 60 fps timer  ← was commented out
    Riya::init();    // registers Riya's  60 fps timer  ← was commented out

    glutMainLoop();
    return 0;
}
