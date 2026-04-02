#pragma once
#include <SFML/Graphics.hpp>
#include <iostream>
#include "Player.h"
#include "PlayerStats.h"
#include "PlayerState.h"
#include <vector>
#include <string>

class NPCPlayer : public Player
{
public:
    NPCPlayer(const sf::Texture& texture);
    ~NPCPlayer();

    // Standard getters/setters (matching UserPlayer)
    sf::Sprite getSprite() override { return m_sprite; }
    void setRotation(float t_degrees) { m_sprite.setRotation(sf::degrees(t_degrees)); }
    //std::vector<std::pair<std::string, float>> actionLog;
    //void logAction(const std::string& action) {actionLog.push_back({ action, 3.0f });}

    sf::Vector2f getAimDirection() const override { return m_physicalHeading; }
    void move(sf::Vector2f t_move) override { m_position += t_move; }



    void update(float dt, AnimationServer& animServer) override;

    // NPC specific: needs to be told where to look since there's no mouse
    void setRotationToward(sf::Vector2f targetPos);
    void resetKickCooldown() { m_kickCooldown = 0.5f; } // Half second
    bool canPossess() { return m_kickCooldown <= 0.f; }
    void updateCooldown(float dt) { Player::updateCooldown(dt); if (m_kickCooldown > 0) m_kickCooldown -= dt; }
    void setKickCooldown() { m_kickCooldown = 0; }
    float getKickCooldown() { return m_kickCooldown; }
    sf::Vector2f getDribbleTargetDir() { return m_dribbleTargetDir; }
    void setDribbleTargetDir(sf::Vector2f t_dir) { m_dribbleTargetDir = t_dir; }

    float m_passTimer = 0.0f;

private:
    float m_kickCooldown = 0.f;
    sf::Vector2f m_physicalHeading = { 1.f, 0.f };
    sf::Vector2f m_dribbleTargetDir;
};