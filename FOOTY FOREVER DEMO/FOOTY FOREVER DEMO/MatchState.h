#pragma once

enum class MatchState {
    KickOff,
    InPlay,
    BallOut,      // The brief moment the ball crosses the line
    ThrowIn,
    GoalKick,
    Corner,
    FreeKick,
    Penalty,
    GoalScored,   // Celebration time!
    HalfTime,
    FullTime
};