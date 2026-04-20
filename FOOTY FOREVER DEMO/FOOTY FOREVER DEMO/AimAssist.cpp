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

    float travelTime = 0.f;
    float idealPowerWorld = 0.f;
    sf::Vector2f aimSpot;

    if (isHighPass) {
        float maxLoft = std::clamp(500.f + (rawDist * 0.25f), 700.f, 1150.f);
        float vzPower = std::max(maxLoft - ((stat / 100.f) * 80.f), 300.f);

        float g = 980.f;
        float targetZ = 30.f;
        float discriminant = (vzPower * vzPower) - (2.f * g * targetZ);

        if (discriminant > 0.f) {
            travelTime = (vzPower + std::sqrt(discriminant)) / g;
        }
        else {
            travelTime = (2.0f * vzPower) / g;
        }

        // THE FIX 1: CAP RUN PREDICTION
        // Players don't sprint in a perfectly straight line for 3 seconds. 
        // We cap the prediction vector to 0.8 seconds max so the ball is played to their feet/immediate path!
        float predictionTime = std::min(travelTime, 0.8f);
        sf::Vector2f predictedPos = targetPos + (targetVel * predictionTime);

        float leadAmount = (passingNorm > 0.8f) ? 120.f : 40.f;
        sf::Vector2f dirToPredicted = normalize(predictedPos - playerPos);
        aimSpot = predictedPos + (dirToPredicted * leadAmount);

        float perfectDist = dist(playerPos, aimSpot);

        // THE FIX 2: INTENTIONAL UNDERHIT FOR LONG BALLS
        float requiredVelocity = perfectDist / travelTime;
        float dragTax = 1.0f + (travelTime * 0.05f); // Reduced drag tax

        // If the pass is over 20 meters (2000px), progressively underhit it 
        // so it drops out of the sky early and allows the receiver to run onto it
        float longPassUnderhit = 1.0f;
        if (rawDist > 2000.f) {
            longPassUnderhit = std::clamp(1.0f - ((rawDist - 2000.f) / 12000.f), 0.70f, 1.0f);
        }

        idealPowerWorld = requiredVelocity * dragTax * longPassUnderhit;
    }
    else {
        float ballFriction = 800.f;

        bool isHome = (passer.getTeam() == Team::Home);
        float forwardProgress = isHome ? (targetPos.x - playerPos.x) : (playerPos.x - targetPos.x);
        bool isBackpass = (forwardProgress < -150.f);

        float arrivalSpeed = 400.f - (std::clamp(rawDist / 3000.f, 0.f, 1.f) * 350.f);
        if (isBackpass) arrivalSpeed = 100.f;

        float v0_est = std::sqrt((arrivalSpeed * arrivalSpeed) + (2.f * ballFriction * rawDist));
        travelTime = (v0_est - arrivalSpeed) / ballFriction;

        // Ground passes use a smaller prediction cap
        float predictionTime = std::min(travelTime, 0.6f);
        sf::Vector2f predictedPos = targetPos + (targetVel * predictionTime);

        float leadAmount = (passingNorm > 0.8f) ? 200.f : 80.f;
        sf::Vector2f dirToPredicted = normalize(predictedPos - playerPos);
        aimSpot = predictedPos + (dirToPredicted * leadAmount);

        float perfectDist = dist(playerPos, aimSpot);
        idealPowerWorld = std::sqrt((arrivalSpeed * arrivalSpeed) + (2.f * ballFriction * perfectDist));
    }

    // --- 5. APPLY MAGNETISM TO AIM ---
    sf::Vector2f perfectPassDir = normalize(aimSpot - playerPos);
    float aimDot = (aimDir.x * perfectPassDir.x) + (aimDir.y * perfectPassDir.y);

    if (aimDot > 0.5f) {
        float magnetism = 0.4f + (passingNorm * 0.5f);
        aimDir = normalize((aimDir * (1.0f - magnetism)) + (perfectPassDir * magnetism));
    }

    // --- 6. APPLY PERFECT POWER ---
    float idealPowerAssisted = idealPowerWorld / 52.0f;

    if (isNPC) {
        float weightErrorFactor = (1.0f - passingNorm) * 0.05f; // Very tight error margin for NPCs
        float randomWeight = 1.0f + (((rand() % 200) - 100) / 100.f) * weightErrorFactor;
        kickPower = std::min(idealPowerAssisted * randomWeight, passer.getKickPower());
    }
    else {
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