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
    void prepareRestart(MatchState state, Ball& ball, const Pitch& pitch, const std::vector<Player*>& players);

    Player* getSetPieceTaker() const { return m_setPieceTaker; }
    bool isWhistleBlown() const { return m_whistleTimer <= 0.f; }

    void updateMatchContexts();
    TacticalContext getTacticalContext(Team team, bool isTaker) const;
    PositioningMask getPositioningMask(Team team, PositionRole role, const Pitch& pitch) const;
    int getHomeScore() { return m_homeScore; }
    int getAwayScore() { return m_awayScore; }


    MatchState getMatchState() const { return m_matchState; }
    void setMatchState(MatchState newState) { m_matchState = newState; }
    void checkBoundaries(Ball& ball, const Pitch& pitch);
    bool checkGoalScored(Ball& ball, const Pitch& pitch);
    void awardFoul(FoulEvent foul, const Pitch& pitch, Ball& ball, const std::vector<Player*>& players, Player* victim);
    float getMatchMinute() const { return m_matchMinute; }
    int getHalf() const { return m_half; }

private:
    MatchState m_matchState = MatchState::InPlay;
    Team m_awardedTo;
    sf::Vector2f m_restartPos;
    Player* m_setPieceTaker;
    Player* m_fouledPlayer;

    float m_matchMinute = 0.0f;
    float m_timeScale = 10.0f; // 1 real second = 20 in-game seconds (adjust to your liking!)
    int m_half = 1;

    float m_whistleTimer = 0.0f;
    int m_homeScore = 0;
    int m_awayScore = 0;

    TacticalContext m_attackCtx;
    TacticalContext m_defendCtx;

    PositioningMask m_attackMask;
    PositioningMask m_defendMask;
};