#pragma once
#include "SFML/Graphics.hpp"

struct Pitch {
    // 1. Core Dimensions (Pixels)
    const float totalWidth = 10000.f;
    const float totalHeight = 7000.f;
    const float margin = 500.f; // 5 meters

    // 2. Playable Boundaries
    sf::FloatRect playableArea;
    float halfwayLineX;

    // 3. Goal & Boxes (Metric Standard translated to Pixels)
    sf::FloatRect homePenaltyBox;  // Left side
    sf::FloatRect awayPenaltyBox;  // Right side
    sf::FloatRect homeGoalArea;    // Six-yard box
    sf::FloatRect awayGoalArea;
    sf::Vector2f centerSpot;

    // Penalty Arcs (The "D") 
    // It's a circle centered on the penalty spot with a 9.15m radius
    // only drawn outside the penalty box.
    const float centerCircleRadius = 915.f; // 9.15m
    const float penaltyArcRadius = 915.f;   // 9.15m
    const float cornerArcRadius = 100.f;    // 1m

    // Penalty Spots (11 meters from goal line)
    sf::Vector2f homePenaltySpot;
    sf::Vector2f awayPenaltySpot;
    sf::Vector2f homeGoalCenter;
    sf::Vector2f awayGoalCenter;

    // 4. Goal Mouths (For collision detection)
    sf::FloatRect homeGoalBox;
    sf::FloatRect awayGoalBox;

    float boxDepth = 1650.f; // 16.5 meters
    float boxWidth = 4032.f; // 40.32 meters (16.5 + 7.32 goal + 16.5)
    float centerY = totalHeight / 2.f;
    float boxTopY = centerY - (boxWidth / 2.f);

    Pitch() {
        // Playable area starts at (500, 500) and is 9000x6000
        playableArea = sf::FloatRect({ margin, margin }, { totalWidth - (2 * margin), totalHeight - (2 * margin) });
        halfwayLineX = totalWidth / 2.f;

        centerSpot = { totalWidth / 2.f, totalHeight / 2.f };

        // Penalty Spots: margin + 11m
        homePenaltySpot = { margin + 1100.f, totalHeight / 2.f };
        awayPenaltySpot = { totalWidth - margin - 1100.f, totalHeight / 2.f };
        homeGoalCenter = { margin, totalHeight / 2.0f};
        awayGoalCenter = { totalWidth - margin, totalHeight / 2.0f };
        homePenaltyBox = sf::FloatRect({ margin, boxTopY }, { boxDepth, boxWidth });
        awayPenaltyBox = sf::FloatRect({ totalWidth - margin - boxDepth, boxTopY }, { boxDepth, boxWidth });

        // Goal Line: 7.32m wide
        float goalWidth = 732.f;
        float goalYOffset = (totalHeight - goalWidth) / 2.f;
        homeGoalArea = sf::FloatRect({ margin - 10.f, goalYOffset }, { 20.f, goalWidth });
        awayGoalArea = sf::FloatRect({ totalWidth - margin - 10.f, goalYOffset }, { 20.f, goalWidth });
    };

    // Helper to check if someone is in the "D" (for penalty encroachment logic)
    bool isInPenaltyArc(sf::Vector2f pos, bool homeSide) const {
        sf::Vector2f spot = homeSide ? homePenaltySpot : awayPenaltySpot;
        sf::Vector2f delta = pos - spot;
        float distSq = delta.x * delta.x + delta.y * delta.y;

        // Inside the 9.15m radius but NOT inside the box itself
        bool insideRadius = distSq < (penaltyArcRadius * penaltyArcRadius);
        bool insideBox = homeSide ? homePenaltyBox.contains(pos) : awayPenaltyBox.contains(pos);

        return insideRadius && !insideBox;
    }

    // Helper: Is the ball/player out of bounds?
    bool isOutOfBounds(sf::Vector2f pos) const {
        return !playableArea.contains(pos);
    }
};

#include <vector>

struct Goal {
    // --- 1. CORE PROPERTIES ---
    Team team;                  // Home or Away
    sf::Vector2f center;        // The center point of the goal line (3500.f)
    bool isHomeGoal;            // Side of the pitch

    // --- 2. THE POSTS (Physics & Visuals) ---
    sf::CircleShape topPost;
    sf::CircleShape bottomPost;
    float postRadius = 12.0f;
    float crossbarHeight = 244.f;
    float barThickness = 24.f;

    // NEW: We need uprights to connect the ground to the shifted crossbar!
    sf::RectangleShape visualCrossbar;
    sf::RectangleShape topUpright;
    sf::RectangleShape bottomUpright;

    // --- 3. THE NET ZONES (Collision Boxes) ---
    sf::FloatRect backNet;
    sf::FloatRect topSideNet;
    sf::FloatRect bottomSideNet;

    // --- 4. VISUAL MESH ---
    sf::VertexArray netMesh;
    sf::Color netColor = sf::Color(255, 255, 255, 120);

    // --- 5. INITIALIZER ---
    void initialize(sf::Vector2f pos, bool homeSide)
    {
        isHomeGoal = homeSide;
        center = pos;
        float goalWidth = 732.0f;
        float netDepth = 225.0f;
        float step = 40.f;

        // THE 3D MAGIC: Shift the top of the goal 120px in the +X direction
        float overhangX = 180.f;

        float topY = pos.y - (goalWidth / 2.f);
        float bottomY = pos.y + (goalWidth / 2.f);

        // 1. Post Bases (Physical circles on the ground)
        topPost.setRadius(postRadius);
        topPost.setOrigin({ postRadius, postRadius });
        topPost.setFillColor(sf::Color::White);
        topPost.setPosition(sf::Vector2f{ pos.x, topY });

        bottomPost.setRadius(postRadius);
        bottomPost.setOrigin({ postRadius, postRadius });
        bottomPost.setFillColor(sf::Color::White);
        bottomPost.setPosition(sf::Vector2f{ pos.x, bottomY });

        // 2. Uprights (Connecting the ground to the shifted crossbar)
        topUpright.setSize({ overhangX, barThickness });
        topUpright.setOrigin({ 0.f, barThickness / 2.f });
        topUpright.setFillColor(sf::Color::White);
        topUpright.setPosition({ pos.x, topY });

        bottomUpright.setSize({ overhangX, barThickness });
        bottomUpright.setOrigin({ 0.f, barThickness / 2.f });
        bottomUpright.setFillColor(sf::Color::White);
        bottomUpright.setPosition({ pos.x, bottomY });

        // 3. Shifted Crossbar
        visualCrossbar.setSize({ barThickness, goalWidth });
        visualCrossbar.setOrigin({ barThickness / 2.f, goalWidth / 2.f });
        visualCrossbar.setFillColor(sf::Color::White);
        visualCrossbar.setPosition({ pos.x + overhangX, pos.y }); // Shifted!

        // 4. Net Physics (Untouched, they stay flat on the grass!)
        float netX = homeSide ? pos.x - netDepth : pos.x;
        float backX = homeSide ? pos.x - netDepth : pos.x + netDepth;
        backNet = sf::FloatRect({ backX, topY }, { 10.f, goalWidth });
        topSideNet = sf::FloatRect({ netX, topY }, { netDepth, 10.f });
        bottomSideNet = sf::FloatRect({ netX, bottomY }, { netDepth, 10.f });

        // 5. 3D Visual Net Mesh
        netMesh.setPrimitiveType(sf::PrimitiveType::Lines);
        netMesh.clear();

        // A. Sloped Back Net (From the overhang crossbar down to the back ground line)
        for (float y = topY; y <= bottomY; y += step) {
            netMesh.append(sf::Vertex{ sf::Vector2f{ pos.x + overhangX, y }, netColor });
            netMesh.append(sf::Vertex{ sf::Vector2f{ backX, y }, netColor });
        }

        // B. Horizontal Net Lines (Following the slope)
        int numHorizLines = std::abs(backX - (pos.x + overhangX)) / step;
        for (int i = 0; i <= numHorizLines; ++i) {
            float t = (float)i / numHorizLines;
            float currX = (pos.x + overhangX) + t * (backX - (pos.x + overhangX));
            netMesh.append(sf::Vertex{ sf::Vector2f{ currX, topY }, netColor });
            netMesh.append(sf::Vertex{ sf::Vector2f{ currX, bottomY }, netColor });
        }

        // C. Side Netting (Fanning from the upright down to the back corner)
        for (float x = 0; x <= overhangX; x += step) {
            // Top Side Net
            netMesh.append(sf::Vertex{ sf::Vector2f{ pos.x + x, topY }, netColor });
            netMesh.append(sf::Vertex{ sf::Vector2f{ backX, topY }, netColor });
            // Bottom Side Net
            netMesh.append(sf::Vertex{ sf::Vector2f{ pos.x + x, bottomY }, netColor });
            netMesh.append(sf::Vertex{ sf::Vector2f{ backX, bottomY }, netColor });
        }
    }

    void draw(sf::RenderWindow& window)
    {
        window.draw(netMesh);
        window.draw(topPost);
        window.draw(bottomPost);
        window.draw(topUpright);    // Draw the new uprights!
        window.draw(bottomUpright);
        window.draw(visualCrossbar);
    }
};