#pragma once
#include <SFML/Graphics.hpp>
#include "PlayerState.h"
#include "PositionRole.h"
#include "MatchContext.h"
#include "Team.h"
#include "MatchStatistics.h"

class Ball;
struct Pitch;
class Player;
struct Goal;
class SoundManager;

// 2. The Referee Class
class MatchReferee {
public:
    void update(Ball& ball, const Pitch& pitch, const std::vector<Player*>& players, float dt, const Goal& homeGoal, const Goal& awayGoal, SoundManager& soundManager, MatchStatistics& stats);
    void prepareRestart(MatchState state, Ball& ball, const Pitch& pitch, const std::vector<Player*>& players, SoundManager& soundManager);

    Player* getSetPieceTaker() const { return m_setPieceTaker; }
    bool isWhistleBlown() const { return m_whistleTimer <= 0.f; }

    void updateMatchContexts();
    TacticalContext getTacticalContext(Team team, bool isTaker) const;
    PositioningMask getPositioningMask(const Player* p, const Pitch& pitch) const;
    int getHomeScore() const { return m_homeScore; }
    int getAwayScore() const { return m_awayScore; }
    void notifyPlayerSwap(Player* p1, Player* p2);

    MatchState getMatchState() const { return m_matchState; }
    void setMatchState(MatchState newState) { m_matchState = newState; }
    void checkBoundaries(Ball& ball, const Pitch& pitch, const Goal& homeGoal, const Goal& awayGoal, SoundManager& soundManager, MatchStatistics& stats);
    bool checkGoalScored(Ball& ball, const Pitch& pitch, const Goal& homeGoal, const Goal& awayGoal, SoundManager& soundManager, MatchStatistics& stats);
    void awardFoul(FoulEvent foul, const Pitch& pitch, Ball& ball, const std::vector<Player*>& players, Player* victim, SoundManager& soundManager, MatchStatistics& stats);
    float getMatchMinute() const { return m_matchMinute; }
    int getHalf() const { return m_half; }
    float getMatchTimeScale() { return m_timeScale; }


    FoulType getLastInfraction() const { return m_lastInfraction; }
    float getOffsideDefensiveLineX() const { return m_frozenDefensiveLineX; }
    sf::Vector2f getOffsideAttackerPos() const { return m_frozenAttackerPos; }

    void applyForfeitScore(bool homeForfeited);
    // Teleports the players but holds the game state
    void setupReplayTeleports(Ball& ball, const Pitch& pitch, const std::vector<Player*>& players, SoundManager& soundManager);
    float getTimeScale() { return m_timeScale; }

    Team getAwardedTo() { return m_awardedTo; }
    void clearLastInfraction() { m_lastInfraction = FoulType::None; } // Or whatever your default enum is

    // Releases the hold and officially starts the Set Piece
    void resumeFromReplay(Ball& ball, const Pitch& pitch, const std::vector<Player*>& players, SoundManager& soundManager);
    void startMatch(Ball& ball, const Pitch& pitch, const std::vector<Player*>& players, SoundManager& soundManager);
    void checkOffsideLogic(Ball& ball, const std::vector<Player*>& players, float homeOffsideLine, float awayOffsideLine, const Pitch& pitch, SoundManager& soundManager);

    std::string getLastGoalScorerName() const { return m_lastGoalScorerName; }
    std::string getLastGoalAssistName() const { return m_lastGoalAssistName; }
    bool getLastGoalWasOwnGoal() const { return m_lastGoalWasOwnGoal; }
    Team getLastGoalScoringTeam() const { return m_lastGoalScoringTeam; }
    Player* getAssistCandidate() const { return m_assistCandidate; }


private:

    struct FlaggedPlayer {
        Player* player;
        sf::Vector2f passMomentPos; // Store exactly where they were when the ball was kicked
    };

    struct OffsideSnapshot {
        Team attackingTeam;
        std::vector<FlaggedPlayer> flaggedPlayers;
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
    float m_timeScale = 10.0f; // 1 real second = 20 in-game seconds // 22.5:1 is 90mins = 4 mins
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

    Player* m_lastPossessor = nullptr;
    Player* m_assistCandidate = nullptr;

    std::string m_lastGoalScorerName = "";
    std::string m_lastGoalAssistName = "";
    bool m_lastGoalWasOwnGoal = false;
    Team m_lastGoalScoringTeam = Team::Home;

    FoulType m_lastInfraction = FoulType::None;
    float m_frozenDefensiveLineX = 0.0f;
    sf::Vector2f m_frozenAttackerPos;
    bool m_exemptNextPass = false;

};