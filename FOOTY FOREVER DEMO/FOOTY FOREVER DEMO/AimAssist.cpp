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

void AimAssist::applyPassAssist(Player& passer, Player* receiver, sf::Vector2f& aimDir, float& kickPower, bool isHighPass, bool isNPC, const Pitch& pitch) {
    if (!receiver) return;

    sf::Vector2f playerPos = passer.getPosition();
    sf::Vector2f targetPos = receiver->getPosition();
    sf::Vector2f targetVel = receiver->getVelocity();

    float rawDist = dist(playerPos, targetPos);
    float stat = isHighPass ? passer.getLongPassing() : ((rawDist < 1500.f) ? passer.getShortPassing() : passer.getLongPassing());
    float passingNorm = stat / 100.f;

    bool isHome = (passer.getTeam() == Team::Home);
    float forwardProgress = isHome ? (targetPos.x - playerPos.x) : (playerPos.x - targetPos.x);
    bool isBackpass = (forwardProgress < -300.f);
    if (isBackpass) isHighPass = false;

    // ==========================================
    // --- 1. THE UNIFIED LEAD PREDICTION ---
    // ==========================================
    float leadTime = 0.f;
    if (isHighPass) {
        float estVz = std::clamp(400.f + (rawDist * 0.15f), 550.f, 1050.f);
        float timeInAir = (2.f * estVz) / 980.f;
        leadTime = std::max(0.1f, timeInAir - 0.15f);
    }
    else {
        float estV0 = std::sqrt(2.f * 800.f * rawDist) + 500.f;
        leadTime = rawDist / (estV0 * 0.6f);
        leadTime = std::min(leadTime, 1.2f);
    }

    sf::Vector2f leadVec = targetVel * leadTime;
    float maxLeadDist = receiver->getTopSpeed() * 10.f * leadTime;
    float leadLen = std::sqrt(leadVec.x * leadVec.x + leadVec.y * leadVec.y);
    if (leadLen > maxLeadDist && leadLen > 0.001f) {
        leadVec = (leadVec / leadLen) * maxLeadDist;
    }

    sf::Vector2f aimSpot = targetPos + leadVec;

    aimSpot.x = std::clamp(aimSpot.x, pitch.margin + 50.f, pitch.totalWidth - pitch.margin - 50.f);
    aimSpot.y = std::clamp(aimSpot.y, pitch.margin + 50.f, pitch.totalHeight - pitch.margin - 50.f);

    // ==========================================
    // --- 2. THE UNIFIED POWER SOLVER ---
    // ==========================================
    float exactDist = dist(playerPos, aimSpot);
    float idealPowerWorld = 0.f;
    float ballFriction = 800.f;

    if (isHighPass) {
        float finalVz = std::clamp(400.f + (exactDist * 0.15f), 550.f, 1050.f);
        float timeInAir = (2.f * finalVz) / 980.f;
        float reqHorizSpeed = exactDist / timeInAir;
        float dragTax = 1.01f; // Reduced
        idealPowerWorld = reqHorizSpeed * dragTax;
    }
    else {
        float arrivalSpeed = std::clamp(exactDist * 1.0f, 400.f, 800.f);
        if (isBackpass) arrivalSpeed = 500.f;

        float requiredV0Sq = (arrivalSpeed * arrivalSpeed) + (2.f * ballFriction * exactDist);
        idealPowerWorld = std::sqrt(requiredV0Sq);
        idealPowerWorld = std::max(idealPowerWorld, 1200.f); // Base punch
    }

    float idealPowerAssisted = idealPowerWorld / 52.0f;
    sf::Vector2f perfectAim = normalize(aimSpot - playerPos);

    if (isNPC) {
        float magnetism = 0.6f + (passingNorm * 0.3f);
        aimDir = normalize((aimDir * (1.0f - magnetism)) + (perfectAim * magnetism));

        float powerMagnetism = 0.8f + (passingNorm * 0.2f);
        kickPower = (kickPower * (1.0f - powerMagnetism)) + (idealPowerAssisted * powerMagnetism);

        if (isHighPass) kickPower *= 0.95;
        else kickPower *= 1.2;
    }
    else {
        float aimDot = (aimDir.x * perfectAim.x) + (aimDir.y * perfectAim.y);
        if (aimDot > 0.4f) {
            float magnetism = 0.4f + (passingNorm * 0.5f);
            aimDir = normalize((aimDir * (1.0f - magnetism)) + (perfectAim * magnetism));
        }
        float powerMagnetism = 0.3f + (passingNorm * 0.6f);
        kickPower = (kickPower * (1.0f - powerMagnetism)) + (idealPowerAssisted * powerMagnetism);
    }

    kickPower = std::clamp(kickPower, 5.f, passer.getKickPower());
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
    float goalCenterY = 3500.f;
    float topPostY = goalCenterY - 366.f;
    float bottomPostY = goalCenterY + 366.f;

    // THE FIX 2: THE "PLACE IT" AIMBOT
    // Only trigger the aimbot if the ball is actually going to be on target!
    // (We allow a small 40px margin outside the post so elite strikers can curl it back in)
    if (intersectY > topPostY - 40.f && intersectY < bottomPostY + 40.f) {

        // Decide which post we are trying to place it inside
        float targetY = (intersectY < goalCenterY) ? topPostY + 60.f : bottomPostY - 60.f;
        sf::Vector2f perfectDir = normalize(sf::Vector2f(goalX, targetY) - playerPos);

        // THE FIX 3: SCALED MAGNETISM
        // 99 Finishing = 0.8 Magnetism (Pulls the shot beautifully into the side netting)
        // 50 Finishing = 0.2 Magnetism (Barely adjusts it, meaning they often shoot too central!)
        float finishingNorm = shooter.getFinishing() / 100.f;
        float magnetism = 0.1f + (finishingNorm * 0.7f);

        aimDir = normalize((aimDir * (1.0f - magnetism)) + (perfectDir * magnetism));

        // 3D VZ Dip Calculation (Dipping shots)
        float horizontalSpeed = std::max(kickPower * 52.0f, 1.f);
        float timeToGoal = distToGoalX / horizontalSpeed;

        float perfectVz = (180.f + (0.5f * 980.f * timeToGoal * timeToGoal)) / timeToGoal;
        vzPower = (vzPower * (1.0f - magnetism)) + (perfectVz * magnetism);
    }
}