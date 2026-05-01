#pragma once
#include <vector>

// Forward declarations
class Ball;
struct Pitch;
class Goal;
class SoundManager;
class MatchStatistics;
class MatchInfo;
class MatchReferee;
class SpatialGrid;
class Player;

struct MatchEnvironment {
    Ball* ball = nullptr;
    const Pitch* pitch = nullptr;
    Goal* homeGoal = nullptr;
    Goal* awayGoal = nullptr;
    SoundManager* sound = nullptr;
    MatchStatistics* stats = nullptr;
    MatchInfo* info = nullptr;
    MatchReferee* referee = nullptr;
    SpatialGrid* grid = nullptr;

    // Player Groupings
    const std::vector<Player*>* allPlayers = nullptr;
    const std::vector<Player*>* teammates = nullptr;
    const std::vector<Player*>* opposition = nullptr;
};