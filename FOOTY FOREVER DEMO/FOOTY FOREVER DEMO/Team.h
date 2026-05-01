#pragma once

enum class Team { Home, Away, None };

enum class MatchPhase { Attacking, Defending, Neutral };
enum class TacticalSubState { Normal, Transition, KeepPossession, AllOut, TimeWasting };

struct TeamState {
    MatchPhase phase;
    TacticalSubState subState;
};