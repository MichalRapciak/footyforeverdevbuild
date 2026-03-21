#pragma once
#include <SFML/Graphics.hpp>
#include "PlayerState.h"
#include "PositionRole.h"
#include "MatchContext.h"
#include "Team.h"

class Ball;
struct Pitch;
class Player;

// 2. The Referee Class
class MatchReferee {
public:
    void update(Ball& ball, const Pitch& pitch, const std::vector<Player*>& players, float dt);
    void prepareRestart(MatchState state, Ball& ball, const std::vector<Player*>& players);

    Player* getSetPieceTaker() const { return m_setPieceTaker; }
    bool isWhistleBlown() const { return m_whistleTimer <= 0.f; }

    void updateMatchContexts();
    TacticalContext getTacticalContext(Team team, bool isTaker) const;
    PositioningMask getPositioningMask(Team team, PositionRole role) const;

    MatchState getMatchState() const { return m_matchState; }
    void setMatchState(MatchState newState) { m_matchState = newState; }
    void checkBoundaries(Ball& ball, const Pitch& pitch);
    bool checkGoalScored(Ball& ball, const Pitch& pitch);

private:
    MatchState m_matchState = MatchState::InPlay;
    Team m_awardedTo;
    sf::Vector2f m_restartPos;
    Player* m_setPieceTaker;

    float m_whistleTimer = 0.0f;
    int m_homeScore = 0;
    int m_awayScore = 0;

    TacticalContext m_attackCtx;
    TacticalContext m_defendCtx;

    PositioningMask m_attackMask;
    PositioningMask m_defendMask;
};