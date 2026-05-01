#pragma once
#include <SFML/Graphics.hpp>
#include <vector>
#include "Player.h"
#include "Ball.h"
#include "Pitch.h"

struct GridCell {
    float homeInfluence = 0.f;
    float awayInfluence = 0.f;
    int homePlayerCount = 0;
    int awayPlayerCount = 0;
};

class SpatialGrid {
public:
    static const int COLS = 24;
    static const int ROWS = 24;

    SpatialGrid() = default;

    void update(const std::vector<Player*>& homeTeam, const std::vector<Player*>& awayTeam, const Ball& ball, const Pitch& pitch);

    // Core conversions
    sf::Vector2i worldToGrid(sf::Vector2f pos, const Pitch& pitch) const;
    sf::Vector2f gridToWorld(int col, int row, const Pitch& pitch) const;

    void drawDebug(sf::RenderWindow& window, const Pitch& pitch) const;
    sf::Vector2f findBestAttackingPocket(sf::Vector2f myPos, sf::Vector2f sectorCenter, float sectorRadius, Team myTeam, const Pitch& pitch) const;
    sf::Vector2f findBestSupportPocket(sf::Vector2f carrierPos, sf::Vector2f myPos, Team myTeam, const Pitch& pitch, TeamState state, float passLengthPref) const;

    // Tactical Queries
    float getTeamControl(int col, int row, Team team) const;
    bool isZoneVacant(int col, int row, Team team) const;

    // Finds the most dangerous empty space in a specific sector
    sf::Vector2f findBestCoverSpace(sf::Vector2f myPos, sf::Vector2f sectorCenter, float sectorRadius, Team myTeam, const Pitch& pitch) const;

private:
    GridCell m_cells[COLS][ROWS];
    float m_cellWidth = 0;
    float m_cellHeight = 0;

    void clearGrid();
    void addInfluence(sf::Vector2f pos, float radius, float strength, Team team, const Pitch& pitch);
};