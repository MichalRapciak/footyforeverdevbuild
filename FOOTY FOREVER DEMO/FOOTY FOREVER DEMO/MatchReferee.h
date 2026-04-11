#pragma once
#include <SFML/Graphics.hpp>
#include "PlayerState.h"
#include "PositionRole.h"
#include "MatchContext.h"
#include "Team.h"

class Ball;
struct Pitch;
class Player;
struct Goal;
class SoundManager;

// 2. The Referee Class
class MatchReferee {
public:
    void update(Ball& ball, const Pitch& pitch, const std::vector<Player*>& players, float dt, const Goal& homeGoal, const Goal& awayGoal, SoundManager& soundManager);
    void prepareRestart(MatchState state, Ball& ball, const Pitch& pitch, const std::vector<Player*>& players, SoundManager& soundManager);

    Player* getSetPieceTaker() const { return m_setPieceTaker; }
    bool isWhistleBlown() const { return m_whistleTimer <= 0.f; }

    void updateMatchContexts();
    TacticalContext getTacticalContext(Team team, bool isTaker) const;
    PositioningMask getPositioningMask(const Player* p, const Pitch& pitch) const;
    int getHomeScore() { return m_homeScore; }
    int getAwayScore() { return m_awayScore; }


    MatchState getMatchState() const { return m_matchState; }
    void setMatchState(MatchState newState) { m_matchState = newState; }
    void checkBoundaries(Ball& ball, const Pitch& pitch, const Goal& homeGoal, const Goal& awayGoal, SoundManager& soundManager);
    bool checkGoalScored(Ball& ball, const Pitch& pitch, const Goal& homeGoal, const Goal& awayGoal, SoundManager& soundManager);
    void awardFoul(FoulEvent foul, const Pitch& pitch, Ball& ball, const std::vector<Player*>& players, Player* victim, SoundManager& soundManager);
    float getMatchMinute() const { return m_matchMinute; }
    int getHalf() const { return m_half; }
    std::vector<Player*> getFlaggedPlayers() { return m_offsideSnapshot.flaggedPlayers; }

    void applyForfeitScore(bool homeForfeited);
    // Teleports the players but holds the game state
    void setupReplayTeleports(Ball& ball, const Pitch& pitch, const std::vector<Player*>& players, SoundManager& soundManager);

    Team getAwardedTo() { return m_awardedTo; }

    // Releases the hold and officially starts the Set Piece
    void resumeFromReplay();
    void startMatch(Ball& ball, const Pitch& pitch, const std::vector<Player*>& players, SoundManager& soundManager);
    void checkOffsideLogic(Ball& ball, const std::vector<Player*>& players, float homeOffsideLine, float awayOffsideLine, const Pitch& pitch, SoundManager& soundManager);

private:

    struct OffsideSnapshot {
        Team attackingTeam;
        std::vector<Player*> flaggedPlayers; // Players past the line during the kick
        bool isActive = false;
    };

    OffsideSnapshot m_offsideSnapshot;
    Player* m_prevBallOwner = nullptr; // To detect the exact frame of a pass

    // Helper to award the specific offside restart
    void awardOffside(Player* offender, Ball& ball, const Pitch& pitch, SoundManager& soundManager);

    MatchState m_matchState = MatchState::KickOff;
    Team m_awardedTo;
    sf::Vector2f m_restartPos;
    Player* m_setPieceTaker;
    Player* m_fouledPlayer;

    float m_matchMinute = 0.0f;
    float m_timeScale = 20.0f; // 1 real second = 20 in-game seconds (adjust to your liking!)
    int m_half = 1;

    float m_whistleTimer = 0.0f;
    int m_homeScore = 0;
    int m_awayScore = 0;

    MatchState m_pendingState = MatchState::InPlay;
    float m_foulDelayTimer = 0.f;

    TacticalContext m_attackCtx;
    TacticalContext m_defendCtx;

    PositioningMask m_attackMask;
    PositioningMask m_defendMask;
};