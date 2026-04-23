#pragma once
#include <SFML/Graphics.hpp>
#include <vector>

class Player;
struct Pitch;

class AimAssist {
public:
    // ==========================================
    // --- PASSING ASSIST ---
    // ==========================================
    // Calculates where the receiver will be when the ball arrives, and bends the aim/power toward it.
    // If 'isNPC' is true, it perfectly calculates the power. If false, it blends with the User's charged power.
    static void applyPassAssist(Player& passer, Player* receiver, sf::Vector2f& aimDir, float& kickPower, bool isHighPass, bool isNPC, const Pitch& pitch);

    // Helps the User controller find which teammate they are currently pointing their mouse at
    static Player* getTargetLock(const sf::Vector2f& playerPos, const sf::Vector2f& aimDir, const std::vector<Player*>& teammates);

    // ==========================================
    // --- SHOOTING ASSIST (THE AIMBOT) ---
    // ==========================================
    // Snaps the shot toward the top or bottom corner of the net, and calculates the perfect dip (Vz)
    static void applyShotAssist(Player& shooter, sf::Vector2f& aimDir, float& vzPower, float& kickPower, const Pitch& pitch);
};