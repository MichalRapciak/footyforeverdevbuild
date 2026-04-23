#pragma once
#include <SFML/Graphics.hpp>
#include <vector>
#include "NPCPlayer.h"
#include "UserPlayer.h"
#include "MatchContext.h"
#include "Ball.h"
#include "Pitch.h"
#include "TeamAI.h"
#include "SoundManager.h"

class MatchStatistics;

class PossessionAI {
public:
    static sf::Vector2f handlePossession(NPCPlayer& npc, Ball& ball, const std::vector<Player*>& teammates, const std::vector<Player*>& opposition, UserPlayer* user, const Pitch& pitch, float dt, MatchState matchstate, const TeamAI& teamAI, SoundManager& soundManager, MatchStatistics& stats);

    static Player* findBestPassOption(NPCPlayer& npc, const std::vector<Player*>& team, const std::vector<Player*>& opposition, UserPlayer* user, const TeamAI& teamAI, const Pitch& pitch);
    static sf::Vector2f calculateDribbleDirection(NPCPlayer& npc, sf::Vector2f goalPos, const std::vector<Player*>& opposition, const Pitch& pitch, const TeamAI& teamAI);

    static void executePass(NPCPlayer& npc, Ball& ball, Player* target, const std::vector<Player*>& opposition, const Pitch& pitch, SoundManager& soundManager, MatchStatistics& stats);
    static void executeShot(NPCPlayer& npc, Ball& ball, sf::Vector2f goalPos, const std::vector<Player*>& opposition, const Pitch& pitch, float dt, SoundManager& soundManager, MatchStatistics& stats);
    static void executeThrowIn(NPCPlayer& npc, Ball& ball, const std::vector<Player*>& teammates);

    static void handleNPCJumpLogic(NPCPlayer& npc, Ball& ball);
    static bool tryNPCAerialStrike(NPCPlayer& npc, Ball& ball, sf::Vector2f aimDir, bool isShot, SoundManager& soundManager);
};