#pragma once
// =============================================================================
//  Riya.h  — Public API for Member 3 (Features 41–60)
//  "A Nostalgic School Day Memory" — Scene 3: Playground
// =============================================================================

namespace Riya {

// ── Lifecycle ────────────────────────────────────────────────────────────────
void init();                          // register timer; call once before glutMainLoop
void handleKey(unsigned char key, int x, int y);  // wire to glutKeyboardFunc

// ── Master entry (called every frame) ────────────────────────────────────────
void drawFeatures();

// ── Individual features (all called inside drawFeatures) ─────────────────────
    void drawPlayground();                 // Feature 41
    void setupFootballGround();
    void animateChildrenPlayingFootball();
    void animateBallMovement();
    void animateGoalScoring();
    void setupSwing();
    void animateSwingMotion();
    void setupSlide();
    void animateSliding();
    void animateKidsRunning();
    void animateKidsPlaying();
    void drawPlaygroundTrees();
    void animatePlaygroundWind();
    void animateSunsetMovement();
    void animateSkyEveningColor();
    void animateStudentsLeavingPlayground();
    void animateEndOfDayBell();
    void animateStudentsExitingGate();
    void animateGateClosing();
    void animateFadeOutEnding();           // Feature 60

} // namespace Riya


