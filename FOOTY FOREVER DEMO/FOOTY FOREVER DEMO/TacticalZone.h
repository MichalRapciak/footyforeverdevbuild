#pragma once

/// <summary>
///  // forwardLeash, backwardLeash, lateralLeash, ballInfluence, markingRange, roamingFreedom, widthPreference, supportDepth, pressingTrigger
/// </summary>
struct TacticalZone {
    float forwardLeash;
    float backwardLeash;
    float lateralLeash;
    float ballInfluence;
    float markingRange; // New: Distance to trigger man-marking
    // forwardLeash, backwardLeash, lateralLeash, ballInfluence, markingRange, roamingFreedom, widthPreference, supportDepth, pressingTrigger
    // --- NEW: Playstyle Modifiers ---

    // 0.0 = Rigid (Stays strictly in position), 1.0 = Free Roam (Ignores leashes to find space)
    // TeamTactics "positionalFreedom" will scale this!
    float roamingFreedom = 0.0f;

    // -1.0 = Cut Inside (Half-spaces), 0.0 = Neutral, 1.0 = Hug the Touchline
    // Useful for distinguishing a "Wide Winger" from a "False Winger/Inverted Winger"
    float widthPreference = 0.0f;

    // -1.0 = Drops deep to receive, 0.0 = Neutral, 1.0 = Pushes up against the defensive line
    // Crucial for "False 9" vs "The Target" or "Playmaker" vs "Box to Box"
    float supportDepth = 0.0f;

    // Distance in pixels a player will break formation to press an opponent.
    // "Hardcore Press" or "The Killer" will have high values (e.g., 600.f). "Sweeper" is low (100.f).
    float pressingTrigger = 250.f;
};