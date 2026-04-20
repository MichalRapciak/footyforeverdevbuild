#include "PlayerAI.h"
#include "NPCPlayer.h"
#include <cmath>
#include <algorithm>

// ==========================================
// --- HELPER MATH ---
// ==========================================

float PlayerAI::dist(sf::Vector2f p1, sf::Vector2f p2) {
    float dx = p1.x - p2.x;
    float dy = p1.y - p2.y;
    return std::sqrt(dx * dx + dy * dy);
}

sf::Vector2f PlayerAI::normalize(sf::Vector2f source) {
    float length = std::sqrt(source.x * source.x + source.y * source.y);
    if (length != 0) return source / length;
    return source;
}

Player* PlayerAI::findNearestOpponent(const sf::Vector2f& npcPos, const std::vector<Player*>& opponents) {
    Player* nearest = nullptr;
    float minDistanceSq = std::numeric_limits<float>::max();

    for (auto& opp : opponents) {
        if (opp->getPositionRole() == PositionRole::Goalkeeper) continue;

        sf::Vector2f diff = npcPos - opp->getPosition();
        float distSq = (diff.x * diff.x) + (diff.y * diff.y);

        if (distSq < minDistanceSq) {
            minDistanceSq = distSq;
            nearest = opp;
        }
    }
    return nearest;
}

Player* PlayerAI::findBestThreat(const sf::Vector2f& npcPos, const std::vector<Player*>& opponents, const TacticalZone& zone, MatchState matchstate) {
    Player* bestThreat = nullptr;
    float minDist = zone.markingRange;

    for (auto& opp : opponents) {
        if (opp->getPositionRole() == PositionRole::Goalkeeper) continue;
        float d = dist(npcPos, opp->getPosition());

        // Priority 1: The guy with the ball
        if (opp->getBallPossession() && d < zone.markingRange * 1.5f && matchstate == MatchState::InPlay) {
            return opp;
        }
        // Priority 2: The closest guy in my zone
        if (d < minDist) {
            minDist = d;
            bestThreat = opp;
        }
    }
    return bestThreat;
}



