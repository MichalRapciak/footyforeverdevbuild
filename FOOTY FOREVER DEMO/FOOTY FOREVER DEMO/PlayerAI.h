#pragma once
#include <SFML/Graphics.hpp>
#include <vector>
#include "MatchContext.h"
#include "Direction.h"
#include "TacticalZone.h"

class Player;
class NPCPlayer;

class PlayerAI {
public:
    static Player* findBestThreat(const sf::Vector2f& npcPos, const std::vector<Player*>& opponents, const TacticalZone& zone, MatchState matchstate);
    static Player* findNearestOpponent(const sf::Vector2f& npcPos, const std::vector<Player*>& opponents);

    // Helper math
    static float dist(sf::Vector2f p1, sf::Vector2f p2);
    static sf::Vector2f normalize(sf::Vector2f source);
    static float length(sf::Vector2f v) {
        return std::sqrt(v.x * v.x + v.y * v.y);
    }
    static float dot(sf::Vector2f v1, sf::Vector2f v2) {
        return (v1.x * v2.x) + (v1.y * v2.y);
    }
    static sf::Vector2f getFacingVec(Direction dir) {
        switch (dir) {
        case Direction::Up:        return { 1.f, 0.f };
        case Direction::Down:      return { -1.f, 0.f };
        case Direction::Left:      return { 0.f, -1.f };
        case Direction::Right:     return { 0.f, 1.f };
        case Direction::UpLeft:    return { 0.707f, -0.707f };
        case Direction::UpRight:   return { 0.707f, 0.707f };
        case Direction::DownLeft:  return { -0.707f, -0.707f };
        case Direction::DownRight: return { -0.707f, 0.707f };
        default:                   return { 1.f, 0.f };
        }
    }

private:

};