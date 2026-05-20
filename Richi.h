#pragma once
// =============================================================================
//  Richi.h  — Public API for Member 2 (Features 21–40)
//  "A Nostalgic School Day Memory" — Scene 2: Classroom
// =============================================================================

namespace Richi {

// ── Lifecycle ────────────────────────────────────────────────────────────────
void init();                          // register timer; call once before glutMainLoop
void handleKey(unsigned char key, int x, int y);  // wire to glutKeyboardFunc

// ── Master entry (called every frame) ────────────────────────────────────────
void drawFeatures();

// ── Individual features (all called inside drawFeatures) ─────────────────────
void drawClassroomInterior();         // Feature 21
void animateTeacherEntry();           // Feature 22
void drawStudentsOnBenches();         // Feature 23
void animateTeacherGreeting();        // Feature 24
void drawBlackboard();                // Feature 25
void animateWritingOnBlackboard();    // Feature 26
void animateChalkMovement();          // Feature 27
void animateStudentsRaiseHands();     // Feature 28
void animateStudentAnswering();       // Feature 29
void animateTeacherExplaining();      // Feature 30
void drawClockOnWall();               // Feature 31
void animateClockMoving();            // Feature 32
void animateCeilingFan();             // Feature 33
void drawWindow();                    // Feature 34
void animateSunlightThroughWindow();  // Feature 35
void animateStudentsTalking();        // Feature 36
void drawBooks();                     // Feature 37
void animatePageFlipping();           // Feature 38
void animateBellRinging();            // Feature 39
void animateClassEnding();            // Feature 40

} // namespace Richi

