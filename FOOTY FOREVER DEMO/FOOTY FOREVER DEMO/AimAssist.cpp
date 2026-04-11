#include "AimAssist.h"
#include "Player.h"
#include "Pitch.h"
#include <cmath>
#include <algorithm>

// Helper math function
static float dist(sf::Vector2f p1, sf::Vector2f p2) {
    float dx = p1.x - p2.x;
    float dy = p1.y - p2.y;
    return std::sqrt(dx * dx + dy * dy);
}

static sf::Vector2f normalize(sf::Vector2f source) {
    float length = std::sqrt(source.x * source.x + source.y * source.y);
    if (length != 0) return source / length;
    return source;
}

// ==========================================
// --- PASSING ASSIST ---
// ==========================================

void AimAssist::applyPassAssist(Player& passer, Player* receiver, sf::Vector2f& aimDir, float& kickPower, bool isHighPass, bool isNPC) {
    if (!receiver) return;

    sf::Vector2f playerPos = passer.getPosition();
    sf::Vector2f targetPos = receiver->getPosition();
    sf::Vector2f targetVel = receiver->getVelocity();

    float rawDist = dist(playerPos, targetPos);
    float stat = isHighPass ? passer.getLongPassing() : ((rawDist < 1500.f) ? passer.getShortPassing() : passer.getLongPassing());
    float passingNorm = stat / 100.f;

    // 1. Physics Prediction (Lead the target)
    float arrivalSpeed = 500.f - (std::clamp(rawDist / 4000.f, 0.f, 1.f) * 300.f);
    float v0_est = std::sqrt((arrivalSpeed * arrivalSpeed) + (2.f * 800.f * rawDist));
    float travelTime = (rawDist > 1200.f) ? (rawDist / v0_est) + 0.3f : rawDist / ((v0_est + arrivalSpeed) * 0.5f);

    sf::Vector2f predictedPos = targetPos + (targetVel * travelTime);
    sf::Vector2f dirToPredicted = normalize(predictedPos - playerPos);

    // Elite players lead the pass further into space
    float leadAmount = (passingNorm > 0.8f) ? 250.f : 80.f;
    sf::Vector2f aimSpot = predictedPos + (dirToPredicted * leadAmount);

    sf::Vector2f perfectPassDir = normalize(aimSpot - playerPos);
    float perfectDist = dist(playerPos, aimSpot);

    // 2. Aim Magnetism
    float aimDot = (aimDir.x * perfectPassDir.x) + (aimDir.y * perfectPassDir.y);

    // If the player/NPC is looking generally in the right direction, snap the pass
    if (aimDot > 0.5f) {
        float magnetism = 0.4f + (passingNorm * 0.5f); // 90 stat = 85% snap, 50 stat = 65% snap
        aimDir = normalize((aimDir * (1.0f - magnetism)) + (perfectPassDir * magnetism));
    }

    // 3. Power Assistance
    float idealPowerWorld = std::sqrt((arrivalSpeed * arrivalSpeed) + (2.f * 800.f * perfectDist));
    float idealPowerAssisted = idealPowerWorld / 52.0f;
    idealPowerAssisted *= isHighPass ? 0.55f : 1.1f;

    if (isNPC) {
        // NPCs just use the ideal power, with a slight stat-based error
        float weightErrorFactor = (1.0f - passingNorm) * 0.12f;
        float randomWeight = 1.0f + (((rand() % 200) - 100) / 100.f) * weightErrorFactor;
        kickPower = std::min(idealPowerAssisted * randomWeight, passer.getKickPower());
    }
    else {
        // Users blend their charged bar with the ideal power
        float powerMagnetism = 0.3f + (passingNorm * 0.6f);
        kickPower = (kickPower * (1.0f - powerMagnetism)) + (idealPowerAssisted * powerMagnetism);
        kickPower = std::min(kickPower, passer.getKickPower());
    }
}

Player* AimAssist::getTargetLock(const sf::Vector2f& playerPos, const sf::Vector2f& aimDir, const std::vector<Player*>& teammates) {
    Player* currentTarget = nullptr;
    float bestMatch = 0.96f; // Threshold for "locking on"

    for (Player* teammate : teammates) {
        sf::Vector2f toTeammate = teammate->getPosition() - playerPos;
        float d = dist(playerPos, teammate->getPosition());

        if (d < 10.f || d > 4500.f) continue;

        sf::Vector2f dirToTeammate = toTeammate / d;
        float alignment = (aimDir.x * dirToTeammate.x + aimDir.y * dirToTeammate.y);

        if (alignment > bestMatch) {
            bestMatch = alignment;
            currentTarget = teammate;
        }
    }
    return currentTarget;
}

// ==========================================
// --- SHOOTING ASSIST (AIMBOT) ---
// ==========================================

void AimAssist::applyShotAssist(Player& shooter, sf::Vector2f& aimDir, float& vzPower, float& kickPower, const Pitch& pitch) {
    sf::Vector2f playerPos = shooter.getPosition();
    bool isHome = (shooter.getTeam() == Team::Home);
    float goalX = isHome ? pitch.totalWidth - pitch.margin : pitch.margin;

    // Are they aiming generally at the goal half?
    bool aimingAtGoal = (isHome && aimDir.x > 0) || (!isHome && aimDir.x < 0);
    if (!aimingAtGoal) return;

    float distToGoalX = std::abs(goalX - playerPos.x);
    float intersectY = playerPos.y + (aimDir.y / std::abs(aimDir.x)) * distToGoalX;

    // Standard goal coordinates
    float topPostY = 3500.f - 366.f;
    float bottomPostY = 3500.f + 366.f;

    // If the raw aim is within 200px of the goalposts, trigger the aimbot!
    if (intersectY > topPostY - 200.f && intersectY < bottomPostY + 200.f) {
        // Snap to the inside of the nearest post
        float targetY = (intersectY < 3500.f) ? topPostY + 40.f : bottomPostY - 40.f;
        sf::Vector2f perfectDir = normalize(sf::Vector2f(goalX, targetY) - playerPos);

        // Finishing stat determines how strongly the aimbot pulls the shot into the corner
        float magnetism = (shooter.getFinishing() / 100.f) * 0.75f;
        aimDir = normalize((aimDir * (1.0f - magnetism)) + (perfectDir * magnetism));

        // 3D VZ Dip Calculation (Dipping shots)
        float horizontalSpeed = std::max(kickPower * 52.0f, 1.f);
        float timeToGoal = distToGoalX / horizontalSpeed;

        float perfectVz = (180.f + (0.5f * 980.f * timeToGoal * timeToGoal)) / timeToGoal;
        vzPower = (vzPower * (1.0f - magnetism)) + (perfectVz * magnetism);
    }
}