#include "SpatialGrid.h"
#include <cmath>
#include <algorithm>
#include "PlayerAI.h"

void SpatialGrid::update(const std::vector<Player*>& homeTeam, const std::vector<Player*>& awayTeam, const Ball& ball, const Pitch& pitch) {
    m_cellWidth = (pitch.totalWidth - (pitch.margin * 2.f)) / COLS;
    m_cellHeight = (pitch.totalHeight - (pitch.margin * 2.f)) / ROWS;

    clearGrid();

    // Apply Home Influence
    for (Player* p : homeTeam) {
        if (p->isSentOff() || p->getState() == PlayerState::Injured) continue;

        // Midfielders control a larger radius (approx 4 cells). Defenders are tightly packed (3 cells).
        float radius = (p->getPositionRole() == PositionRole::CenterMid) ? (m_cellWidth * 5.5f) : (m_cellWidth * 4.5f);
        addInfluence(p->getPosition(), radius, 1.5f, Team::Home, pitch);

        sf::Vector2i gPos = worldToGrid(p->getPosition(), pitch);
        m_cells[gPos.x][gPos.y].homePlayerCount++;
    }

    // Apply Away Influence
    for (Player* p : awayTeam) {
        if (p->isSentOff() || p->getState() == PlayerState::Injured) continue;
        float radius = (p->getPositionRole() == PositionRole::CenterMid) ? (m_cellWidth * 5.5f) : (m_cellWidth * 4.5f);
        addInfluence(p->getPosition(), radius, 1.5f, Team::Away, pitch);

        sf::Vector2i gPos = worldToGrid(p->getPosition(), pitch);
        m_cells[gPos.x][gPos.y].awayPlayerCount++;
    }
}

void SpatialGrid::addInfluence(sf::Vector2f pos, float radius, float strength, Team team, const Pitch& pitch) {
    sf::Vector2i centerGrid = worldToGrid(pos, pitch);
    int cellRadius = static_cast<int>(std::ceil(radius / m_cellWidth));

    for (int x = -cellRadius; x <= cellRadius; ++x) {
        for (int y = -cellRadius; y <= cellRadius; ++y) {
            int checkX = centerGrid.x + x;
            int checkY = centerGrid.y + y;

            if (checkX >= 0 && checkX < COLS && checkY >= 0 && checkY < ROWS) {
                sf::Vector2f cellWorld = gridToWorld(checkX, checkY, pitch);
                float dist = std::sqrt(std::pow(cellWorld.x - pos.x, 2) + std::pow(cellWorld.y - pos.y, 2));

                if (dist < radius) {
                    // Influence decays the further you are from the center of the player
                    float inf = strength * (1.0f - (dist / radius));
                    if (team == Team::Home) m_cells[checkX][checkY].homeInfluence += inf;
                    else m_cells[checkX][checkY].awayInfluence += inf;
                }
            }
        }
    }
}

sf::Vector2i SpatialGrid::worldToGrid(sf::Vector2f pos, const Pitch& pitch) const {
    int x = static_cast<int>((pos.x - pitch.margin) / m_cellWidth);
    int y = static_cast<int>((pos.y - pitch.margin) / m_cellHeight);
    return sf::Vector2i(std::clamp(x, 0, COLS - 1), std::clamp(y, 0, ROWS - 1));
}

sf::Vector2f SpatialGrid::gridToWorld(int col, int row, const Pitch& pitch) const {
    float x = pitch.margin + (col * m_cellWidth) + (m_cellWidth * 0.5f);
    float y = pitch.margin + (row * m_cellHeight) + (m_cellHeight * 0.5f);
    return sf::Vector2f(x, y);
}

float SpatialGrid::getTeamControl(int col, int row, Team team) const {
    float myInf = (team == Team::Home) ? m_cells[col][row].homeInfluence : m_cells[col][row].awayInfluence;
    float oppInf = (team == Team::Home) ? m_cells[col][row].awayInfluence : m_cells[col][row].homeInfluence;

    // Returns 1.0 (Total Control), 0.0 (Neutral), or -1.0 (Enemy Controlled)
    float total = myInf + oppInf;
    if (total == 0.f) return 0.f;
    return (myInf - oppInf) / total;
}

bool SpatialGrid::isZoneVacant(int col, int row, Team team) const {
    // A zone is considered vacant if our pure influence in that box is less than 0.3
    float myInf = (team == Team::Home) ? m_cells[col][row].homeInfluence : m_cells[col][row].awayInfluence;
    return myInf < 0.3f;
}

sf::Vector2f SpatialGrid::findBestCoverSpace(sf::Vector2f myPos, sf::Vector2f sectorCenter, float sectorRadius, Team myTeam, const Pitch& pitch) const {
    sf::Vector2i centerGrid = worldToGrid(sectorCenter, pitch);
    int checkRadius = static_cast<int>(std::ceil(sectorRadius / m_cellWidth));

    sf::Vector2f bestSpot = myPos;
    float worstControl = 999.f; // We want to find the cell we control the LEAST

    for (int x = -checkRadius; x <= checkRadius; ++x) {
        for (int y = -checkRadius; y <= checkRadius; ++y) {
            int cx = centerGrid.x + x;
            int cy = centerGrid.y + y;

            if (cx >= 0 && cx < COLS && cy >= 0 && cy < ROWS) {
                if (isZoneVacant(cx, cy, myTeam)) {
                    float control = getTeamControl(cx, cy, myTeam);
                    // If the enemy is flooding this vacant space, it's highly dangerous!
                    if (control < worstControl) {
                        worstControl = control;
                        bestSpot = gridToWorld(cx, cy, pitch);
                    }
                }
            }
        }
    }
    return bestSpot;
}

void SpatialGrid::clearGrid() {
    for (int x = 0; x < COLS; ++x) {
        for (int y = 0; y < ROWS; ++y) {
            m_cells[x][y] = GridCell();
        }
    }
}

void SpatialGrid::drawDebug(sf::RenderWindow& window, const Pitch& pitch) const 
{
    // Create a reusable rectangle to save memory
    sf::RectangleShape cellShape(sf::Vector2f(m_cellWidth, m_cellHeight));

    // Draw faint white lines so you can clearly see the 24x24 grid structure
    cellShape.setOutlineThickness(1.0f);
    cellShape.setOutlineColor(sf::Color(255, 255, 255, 25));

    for (int x = 0; x < COLS; ++x) {
        for (int y = 0; y < ROWS; ++y) {
            float hInf = m_cells[x][y].homeInfluence;
            float aInf = m_cells[x][y].awayInfluence;
            float totalInf = hInf + aInf;

            // Only color the cell if someone is actually influencing it
            if (totalInf > 0.05f) {
                float hRatio = hInf / totalInf;
                float aRatio = aInf / totalInf;

                // Color Mixing: Home = Blue, Away = Red
                std::uint8_t r = static_cast<std::uint8_t>(255.0f * aRatio);
                std::uint8_t b = static_cast<std::uint8_t>(255.0f * hRatio);

                // Alpha scales with total influence. 
                // We cap it at 160 so it never becomes completely opaque (blocking the players/ball)
                std::uint8_t a = static_cast<std::uint8_t>(std::clamp(totalInf * 60.0f, 0.0f, 160.0f));

                cellShape.setFillColor(sf::Color(r, 0, b, a));
            }
            else {
                // Empty space gets no fill color
                cellShape.setFillColor(sf::Color::Transparent);
            }

            // Calculate exact screen coordinates for this cell
            float screenX = pitch.margin + (x * m_cellWidth);
            float screenY = pitch.margin + (y * m_cellHeight);

            cellShape.setPosition({ screenX, screenY });
            window.draw(cellShape);
        }
    }
}

sf::Vector2f SpatialGrid::findBestAttackingPocket(sf::Vector2f myPos, sf::Vector2f sectorCenter, float sectorRadius, Team myTeam, const Pitch& pitch) const {
    sf::Vector2i centerGrid = worldToGrid(sectorCenter, pitch);
    int checkRadius = static_cast<int>(std::ceil(sectorRadius / m_cellWidth));

    sf::Vector2f bestSpot = myPos;
    float bestScore = -999.f;

    for (int x = -checkRadius; x <= checkRadius; ++x) {
        for (int y = -checkRadius; y <= checkRadius; ++y) {
            int cx = centerGrid.x + x;
            int cy = centerGrid.y + y;

            if (cx >= 0 && cx < COLS && cy >= 0 && cy < ROWS) {
                float myInf = (myTeam == Team::Home) ? m_cells[cx][cy].homeInfluence : m_cells[cx][cy].awayInfluence;
                float oppInf = (myTeam == Team::Home) ? m_cells[cx][cy].awayInfluence : m_cells[cx][cy].homeInfluence;

                // We want to find a cell that has high team control, but SPECIFICALLY very low opponent influence
                float pocketScore = myInf - (oppInf * 2.5f); // Heavily penalize cells with enemies in them

                if (pocketScore > bestScore) {
                    bestScore = pocketScore;
                    bestSpot = gridToWorld(cx, cy, pitch);
                }
            }
        }
    }
    return bestSpot;
}

sf::Vector2f SpatialGrid::findBestSupportPocket(sf::Vector2f carrierPos, sf::Vector2f myPos, Team myTeam, const Pitch& pitch, TeamState state, float passLengthPref) const {
    sf::Vector2i carrierGrid = worldToGrid(carrierPos, pitch);
    sf::Vector2i myGrid = worldToGrid(myPos, pitch);

    // How marked am I currently?
    float myCurrentOppInf = (myTeam == Team::Home) ? m_cells[myGrid.x][myGrid.y].awayInfluence : m_cells[myGrid.x][myGrid.y].homeInfluence;
    bool amICurrentlyMarked = (myCurrentOppInf > 0.4f);

    // Tiki-Taka multiplier (0.0 to 1.0). High value means we love short passes and constant movement.
    float tikiTakaPref = 1.0f - passLengthPref;

    // Search up to ~30m (3000px) around the ball carrier to find lanes
    int checkRadius = static_cast<int>(std::ceil(3000.f / m_cellWidth));

    sf::Vector2f bestSpot = myPos;
    float bestScore = -99999.f;

    // ==========================================
    // --- 1. STATE-DRIVEN SUPPORT RADIUS ---
    // ==========================================
    float minSupportDist = 400.f;
    float maxSupportDist = 1400.f + (passLengthPref * 1000.f);

    if (state.subState == TacticalSubState::KeepPossession || state.subState == TacticalSubState::TimeWasting) {
        maxSupportDist *= 0.65f; // Squeeze tight, offer short safe options
    }
    else if (state.subState == TacticalSubState::Transition) {
        minSupportDist = 700.f;  // Do not crowd the counter-attacker!
        maxSupportDist *= 1.3f;  // Look for longer outlet passes
    }

    float currentDistToCarrier = PlayerAI::dist(myPos, carrierPos);

    for (int x = -checkRadius; x <= checkRadius; ++x) {
        for (int y = -checkRadius; y <= checkRadius; ++y) {
            int cx = carrierGrid.x + x;
            int cy = carrierGrid.y + y;

            if (cx >= 0 && cx < COLS && cy >= 0 && cy < ROWS) {
                sf::Vector2f cellWorld = gridToWorld(cx, cy, pitch);
                float distToCarrier = PlayerAI::dist(cellWorld, carrierPos);

                // Check if this cell is within our tactical passing range
                if (distToCarrier > minSupportDist && distToCarrier < maxSupportDist) {
                    float oppInf = (myTeam == Team::Home) ? m_cells[cx][cy].awayInfluence : m_cells[cx][cy].homeInfluence;
                    float myInf = (myTeam == Team::Home) ? m_cells[cx][cy].homeInfluence : m_cells[cx][cy].awayInfluence;

                    float distToMe = PlayerAI::dist(cellWorld, myPos);

                    // Base Score: Low enemy presence in the destination cell
                    float score = (myInf * 500.f) - (oppInf * 2500.f);

                    // ==========================================
                    // --- 2. TIKI-TAKA WORK RATE (Distance Penalty) ---
                    // ==========================================
                    // Short passing teams take a much smaller penalty for running to find space.
                    // A route-one team hates running out of shape (penalty x1.2). A tiki-taka team roams freely (penalty x0.4).
                    float effortPenalty = distToMe * (0.4f + (passLengthPref * 0.8f));
                    score -= effortPenalty;

                    // ==========================================
                    // --- 3. CARRIER SHADOW FIX (Lane Scanning) ---
                    // ==========================================
                    // Shifted to 35%, 50%, 65% to ignore the carrier's immediate presser!
                    sf::Vector2f toPocket = cellWorld - carrierPos;
                    sf::Vector2i grid1 = worldToGrid(carrierPos + (toPocket * 0.35f), pitch);
                    sf::Vector2i grid2 = worldToGrid(carrierPos + (toPocket * 0.50f), pitch);
                    sf::Vector2i grid3 = worldToGrid(carrierPos + (toPocket * 0.65f), pitch);

                    float laneOppInf = 0.f;
                    if (myTeam == Team::Home) {
                        laneOppInf = std::max({ m_cells[grid1.x][grid1.y].awayInfluence, m_cells[grid2.x][grid2.y].awayInfluence, m_cells[grid3.x][grid3.y].awayInfluence });
                    }
                    else {
                        laneOppInf = std::max({ m_cells[grid1.x][grid1.y].homeInfluence, m_cells[grid2.x][grid2.y].homeInfluence, m_cells[grid3.x][grid3.y].homeInfluence });
                    }

                    // MASSIVE penalty. If a defender is standing in the true middle of the lane, reject it.
                    score -= (laneOppInf * 6000.f);

                    // ==========================================
                    // --- 4. DYNAMIC UNMARKING BURSTS ---
                    // ==========================================
                    // If the lane is clean and the destination is empty...
                    if (oppInf < 0.2f && laneOppInf < 0.2f) {

                        // Proactive Angles: Even if NOT marked, finding a 100% clean lane gets a baseline bonus
                        score += 1500.f;

                        if (amICurrentlyMarked) {
                            // THE DART: Explosive lateral run into space. Multiplied by tikiTakaPref!
                            if (distToMe < 800.f) {
                                score += 4000.f * (1.0f + tikiTakaPref);
                            }

                            // THE SHOW: Checking back to the ball carrier.
                            if (distToCarrier < currentDistToCarrier - 200.f) {
                                score += 2500.f * (1.0f + tikiTakaPref);
                            }
                        }
                    }

                    // ==========================================
                    // --- 5. STATE-DRIVEN PROGRESSION ---
                    // ==========================================
                    float forwardProgress = (myTeam == Team::Home) ? (cellWorld.x - carrierPos.x) : (carrierPos.x - cellWorld.x);

                    if (state.subState == TacticalSubState::TimeWasting) {
                        if (forwardProgress > 0.f) score -= forwardProgress; // Punish forward passes
                        if (forwardProgress < 0.f) score += std::abs(forwardProgress); // Reward safe backpasses
                    }
                    else if (state.subState == TacticalSubState::Transition) {
                        if (forwardProgress > 0.f) score += (forwardProgress * 2.0f); // Massive reward for pushing up
                        if (forwardProgress < 0.f) score -= std::abs(forwardProgress * 1.5f); // Punish killing the counter
                    }
                    else {
                        // Normal play slightly prefers forward options
                        if (forwardProgress > 0.f) score += (forwardProgress * 0.5f);
                    }

                    if (score > bestScore) {
                        bestScore = score;
                        bestSpot = cellWorld;
                    }
                }
            }
        }
    }
    return bestSpot;
}