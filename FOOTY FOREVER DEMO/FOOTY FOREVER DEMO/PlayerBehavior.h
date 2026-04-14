#pragma once
/// <summary>
/// dribbleBias, passRiskBias, shootBias, crossBias, runFrequency, tackleAggression
/// </summary>
struct PlayerBehavior {
    // --- ON THE BALL (Possession Biases) ---

    // 0.0 = One-touch passing/Tiki-Taka, 1.0 = Hold the ball and dribble defenders
    // Trickster = 0.9, Orchestrator = 0.1
    float dribbleBias = 0.5f;

    // 0.0 = Safe sideways/backward passes, 1.0 = Risky killer through-balls / long switches
    // Playmaker = 0.9, Defensive FB = 0.2
    float passRiskBias = 0.5f;

    // 0.0 = Looks for the assist, 1.0 = Selfish, shoots on sight if within range
    // Finisher = 0.9, False 9 = 0.3
    float shootBias = 0.5f;

    // 0.0 = Cuts inside for a pass/shot, 1.0 = Whips a cross into the box
    // The Crosser = 0.9, Trickster = 0.3
    float crossBias = 0.5f;


    // --- OFF THE BALL (Movement & Defending) ---

    // 0.0 = Static/Stationary, 1.0 = Constantly making penetrating forward runs
    // Box-to-Box / Up and Down = 0.9, Sweeper = 0.1
    float runFrequency = 0.5f;

    // 0.0 = Stays on feet, jockeys patiently. 1.0 = Aggressive, slides in constantly.
    // The Killer / Backline Brawler = 0.9, Calm and Collected = 0.1
    float tackleAggression = 0.5f;
};