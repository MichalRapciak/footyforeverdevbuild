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

    Pitch() {
        // Playable area starts at (500, 500) and is 9000x6000
        playableArea = sf::FloatRect({ margin, margin }, { totalWidth - (2 * margin), totalHeight - (2 * margin) });
        halfwayLineX = totalWidth / 2.f;

        centerSpot = { totalWidth / 2.f, totalHeight / 2.f };

        // Penalty Spots: margin + 11m
        homePenaltySpot = { margin + 1100.f, totalHeight / 2.f };
        awayPenaltySpot = { totalWidth - margin - 1100.f, totalHeight / 2.f };
        homeGoalCenter = { margin + 300, totalHeight / 2.0f};
        awayGoalCenter = { totalWidth - margin - 300, totalHeight / 2.0f };

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
    // Using Circles for posts creates realistic "angled" bounces
    sf::CircleShape topPost;
    sf::CircleShape bottomPost;
    float postRadius = 12.0f;   // Matches ball radius for satisfying "clangs"
    float crossbarHeight = 244.f; // 2.44 meters
    float barThickness = 24.f;    // To match your postRadius
    // We'll add a visual rectangle for the crossbar in SFML
    sf::RectangleShape visualCrossbar;

    // --- 3. THE NET ZONES (Collision Boxes) ---
    // These boxes catch the ball. They should be thick (50px+) to prevent tunneling.
    sf::FloatRect backNet;      // The long vertical bar behind the goal
    sf::FloatRect topSideNet;   // The horizontal "roof/side" at the top
    sf::FloatRect bottomSideNet; // The horizontal "roof/side" at the bottom

    // --- 4. VISUAL MESH ---
    // A collection of lines to represent the netting
    sf::VertexArray netMesh;
    sf::Color netColor = sf::Color(255, 255, 255, 120); // Semi-transparent white

    // --- 5. INITIALIZER ---
    void initialize(sf::Vector2f pos, bool homeSide) 
    {
        isHomeGoal = homeSide;
        center = pos;
        float goalWidth = 732.0f;
        float netDepth = 225.0f;
        float step = 40.f; // Mesh density

        // 1. Posts (Physical circles)
        topPost.setRadius(postRadius);
        topPost.setOrigin({ postRadius, postRadius });
        topPost.setFillColor(sf::Color::White);
        topPost.setPosition(sf::Vector2f{ pos.x, pos.y - (goalWidth / 2.f) });

        bottomPost.setRadius(postRadius);
        bottomPost.setOrigin({ postRadius, postRadius });
        bottomPost.setFillColor(sf::Color::White);
        bottomPost.setPosition(sf::Vector2f{ pos.x, pos.y + (goalWidth / 2.f) });

        visualCrossbar.setSize({ barThickness, 732.f });
        visualCrossbar.setOrigin({ barThickness / 2.f, 732.f / 2.f });
        visualCrossbar.setFillColor(sf::Color::White);
        visualCrossbar.setPosition({ pos.x, pos.y });

        // 2. Net Physics (sf::FloatRect {position}, {size})
        float netX = homeSide ? pos.x - netDepth : pos.x;
        backNet = sf::FloatRect({ homeSide ? pos.x - netDepth : pos.x + netDepth, pos.y - (goalWidth / 2.f) }, { 10.f, goalWidth });
        topSideNet = sf::FloatRect({ netX, pos.y - (goalWidth / 2.f) }, { netDepth, 10.f });
        bottomSideNet = sf::FloatRect({ netX, pos.y + (goalWidth / 2.f) }, { netDepth, 10.f });

        // 3. Visual Net Mesh
        netMesh.setPrimitiveType(sf::PrimitiveType::Lines);
        netMesh.clear();

        float topY = pos.y - (goalWidth / 2.f);
        float bottomY = pos.y + (goalWidth / 2.f);

        // Vertical Depth Lines (Connecting the posts to the back of the net)
        for (float y = topY; y <= bottomY; y += step) {
            netMesh.append(sf::Vertex{ sf::Vector2f{ pos.x, y }, netColor });
            netMesh.append(sf::Vertex{ sf::Vector2f{ homeSide ? pos.x - netDepth : pos.x + netDepth, y }, netColor });
        }

        // Horizontal Depth Lines (The "Roof" and "Floor" lines)
        for (float x = 0; x <= netDepth; x += step) {
            float currentX = homeSide ? pos.x - x : pos.x + x;
            netMesh.append(sf::Vertex{ sf::Vector2f{ currentX, topY }, netColor });
            netMesh.append(sf::Vertex{ sf::Vector2f{ currentX, bottomY }, netColor });
        }

        // Back Wall Vertical Lines (The Mesh at the very back)
        float backX = homeSide ? pos.x - netDepth : pos.x + netDepth;
        for (float y = topY; y <= bottomY; y += step) {
            netMesh.append(sf::Vertex{ sf::Vector2f{ backX, y }, netColor });
            netMesh.append(sf::Vertex{ sf::Vector2f{ backX, y + step > bottomY ? bottomY : y + step }, netColor });
        }
    }
    void draw(sf::RenderWindow& window) 
    {
        window.draw(netMesh);
        window.draw(topPost);
        window.draw(bottomPost);
        window.draw(visualCrossbar);
    }
};