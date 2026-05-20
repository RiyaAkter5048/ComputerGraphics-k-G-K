#pragma once
// =============================================================================
//  Sakib.h  —  Public API for Member 1 (Features 1–20)
//  "A Nostalgic School Day Memory" — Scene 1: Morning Arrival
// =============================================================================

namespace Sakib {

// ── Lifecycle ─────────────────────────────────────────────────────────────────
void init();
void handleKey(unsigned char key, int x, int y);   // wire to glutKeyboardFunc
void handleSpecialKey(int key, int x, int y);       // wire to glutSpecialFunc (arrow keys)

// ── Master entry (called every frame) ────────────────────────────────────────
void drawFeatures();

// ── Individual features (all called inside drawFeatures) ─────────────────────
void drawSchoolBuilding();            // Feature  1
void drawSchoolGate();                // Feature  2
void animateGateOpening();            // Feature  3
void drawStudentsEntering();          // Feature  4
void drawSchoolBusArrival();          // Feature  5
void animateBusStopping();            // Feature  6
void drawStudentsGettingOffBus();     // Feature  7
void animateWalkingStudents();        // Feature  8
void drawSecurityGuard();             // Feature  9
void drawFlag();                      // Feature 10
void animateFlagWaving();             // Feature 11
void drawTrees();                     // Feature 12
void animateTreeWind();               // Feature 13
void animateSunRising();              // Feature 14
void animateCloudsMoving();           // Feature 15
void animateBirdsFlying();            // Feature 16
void animateSkyColorChange();         // Feature 17
void drawRoad();                      // Feature 18
void animateCarPassing();             // Feature 19
void drawSchoolSignboard();           // Feature 20

} // namespace Sakib
