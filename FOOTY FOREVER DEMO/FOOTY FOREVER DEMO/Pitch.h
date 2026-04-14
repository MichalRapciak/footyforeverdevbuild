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
    Team team;
    sf::Vector2f center;
    bool isHomeGoal;

    // --- 2. THE POSTS (Physics & Visuals) ---
    sf::CircleShape topPost;
    sf::CircleShape bottomPost;
    float postRadius = 12.0f;
    float crossbarHeight = 244.f;
    float barThickness = 24.f;

    sf::RectangleShape visualCrossbar;
    sf::RectangleShape topUpright;
    sf::RectangleShape bottomUpright;

    // --- 3. THE NET ZONES (Collision Boxes) ---
    sf::FloatRect backNet;
    sf::FloatRect topSideNet;
    sf::FloatRect bottomSideNet;

    // --- 4. VISUAL MESHES ---
    sf::VertexArray netMesh;
    sf::VertexArray shadowMesh; // Baked static shadows (Ambient + Floodlights)
    sf::Color netColor = sf::Color(255, 255, 255, 120);

    // ==========================================
    // --- SHADOW HELPERS ---
    // ==========================================

    // NEW: Helper to build baked ambient ground shadows (Ambient Occlusion)
    void appendAmbientRect(sf::Vector2f center, sf::Vector2f size, std::uint8_t alpha = 100) {
        sf::Color shadowColor(0, 0, 0, alpha); // Uses the custom alpha

        float left = center.x - size.x / 2.f;
        float right = center.x + size.x / 2.f;
        float top = center.y - size.y / 2.f;
        float bottom = center.y + size.y / 2.f;

        // Two triangles making a flat rectangular quad on the grass
        sf::Vertex v0{ {left, top}, shadowColor };
        sf::Vertex v1{ {right, top}, shadowColor };
        sf::Vertex v2{ {right, bottom}, shadowColor };
        sf::Vertex v3{ {left, bottom}, shadowColor };

        shadowMesh.append(v0);
        shadowMesh.append(v1);
        shadowMesh.append(v2);

        shadowMesh.append(v0);
        shadowMesh.append(v2);
        shadowMesh.append(v3);
    }

    // Helper to build baked floodlight shadows
    void appendPostShadow(sf::Vector2f postBase) {
        sf::Vector2f lights[4] = {
            {-500.f, -500.f},
            {-500.f, 7500.f},
            {10500.f, -500.f},
            {10500.f, 7500.f}
        };

        const float lightHeight = 6000.f;
        const float maxLightDist = 12500.f;

        for (int i = 0; i < 4; ++i) {
            sf::Vector2f toPost = postBase - lights[i];
            float distXY = std::sqrt(toPost.x * toPost.x + toPost.y * toPost.y);

            if (distXY > 0.1f) {
                sf::Vector2f dir = toPost / distXY;
                sf::Vector2f normal(-dir.y, dir.x);

                float length = crossbarHeight * (distXY / lightHeight);
                length = std::max(20.f, length);

                float normalizedDist = std::min(distXY / maxLightDist, 1.0f);
                float intensity = std::pow(1.0f - normalizedDist, 2.0f);
                std::uint8_t alpha = static_cast<std::uint8_t>(70 * intensity);

                if (alpha < 2) continue;

                sf::Color baseColor(0, 0, 0, alpha);
                sf::Color tipColor(0, 0, 0, 0);

                float diffusion = 1.0f + (normalizedDist * 2.0f);
                float width = postRadius;

                sf::Vertex v0{ {postBase + normal * width}, baseColor };
                sf::Vertex v1{ {postBase - normal * width}, baseColor };
                sf::Vertex v2{ {postBase + (dir * length) + normal * (width * diffusion)}, tipColor };
                sf::Vertex v3{ {postBase + (dir * length) - normal * (width * diffusion)}, tipColor };

                shadowMesh.append(v0);
                shadowMesh.append(v1);
                shadowMesh.append(v2);

                shadowMesh.append(v1);
                shadowMesh.append(v2);
                shadowMesh.append(v3);
            }
        }
    }

    // --- 5. INITIALIZER ---
    void initialize(sf::Vector2f pos, bool homeSide)
    {
        isHomeGoal = homeSide;
        center = pos;
        float goalWidth = 732.0f;
        float netDepth = 225.0f;
        float step = 40.f;
        float overhangX = 180.f;

        float topY = pos.y - (goalWidth / 2.f);
        float bottomY = pos.y + (goalWidth / 2.f);

        // 1. Post Bases 
        topPost.setRadius(postRadius);
        topPost.setOrigin({ postRadius, postRadius });
        topPost.setFillColor(sf::Color::White);
        topPost.setPosition(sf::Vector2f{ pos.x, topY });

        bottomPost.setRadius(postRadius);
        bottomPost.setOrigin({ postRadius, postRadius });
        bottomPost.setFillColor(sf::Color::White);
        bottomPost.setPosition(sf::Vector2f{ pos.x, bottomY });

        // 2. Uprights 
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
        visualCrossbar.setPosition({ pos.x + overhangX, pos.y });

        // 4. Net Physics 
        float netX = homeSide ? pos.x - netDepth : pos.x;
        float backX = homeSide ? pos.x - netDepth : pos.x + netDepth;
        backNet = sf::FloatRect({ backX, topY }, { 10.f, goalWidth });
        topSideNet = sf::FloatRect({ netX, topY }, { netDepth, 10.f });
        bottomSideNet = sf::FloatRect({ netX, bottomY }, { netDepth, 10.f });

        // 5. 3D Visual Net Mesh
        netMesh.setPrimitiveType(sf::PrimitiveType::Lines);
        netMesh.clear();

        for (float y = topY; y <= bottomY; y += step) {
            netMesh.append(sf::Vertex{ sf::Vector2f{ pos.x + overhangX, y }, netColor });
            netMesh.append(sf::Vertex{ sf::Vector2f{ backX, y }, netColor });
        }

        int numHorizLines = std::abs(backX - (pos.x + overhangX)) / step;
        for (int i = 0; i <= numHorizLines; ++i) {
            float t = (float)i / numHorizLines;
            float currX = (pos.x + overhangX) + t * (backX - (pos.x + overhangX));
            netMesh.append(sf::Vertex{ sf::Vector2f{ currX, topY }, netColor });
            netMesh.append(sf::Vertex{ sf::Vector2f{ currX, bottomY }, netColor });
        }

        for (float x = 0; x <= overhangX; x += step) {
            netMesh.append(sf::Vertex{ sf::Vector2f{ pos.x + x, topY }, netColor });
            netMesh.append(sf::Vertex{ sf::Vector2f{ backX, topY }, netColor });
            netMesh.append(sf::Vertex{ sf::Vector2f{ pos.x + x, bottomY }, netColor });
            netMesh.append(sf::Vertex{ sf::Vector2f{ backX, bottomY }, netColor });
        }

        // ==========================================
        // 6. BAKE ALL SHADOWS
        // ==========================================
        shadowMesh.setPrimitiveType(sf::PrimitiveType::Triangles);
        shadowMesh.clear();

        // A. Ambient Ground Shadows (The physical U-shaped footprint of the net)
        // Main Goal Line (Shadow from crossbar & thick posts)
        appendAmbientRect(pos, { barThickness + 6.f, goalWidth + barThickness + 6.f });

        // Back Frame Line
        appendAmbientRect({ backX, pos.y }, { 10.f, goalWidth + 10.f });

        // Side Frame Lines
        float sideCenterX = (pos.x + backX) / 2.f;
        appendAmbientRect({ sideCenterX, topY }, { netDepth, 10.f });
        appendAmbientRect({ sideCenterX, bottomY }, { netDepth, 10.f });

        // --- NEW: The Faint Net Canopy Shadow ---
        // Fills the entire interior of the frame with a very subtle (alpha 20) diffusion shadow
        appendAmbientRect({ sideCenterX, pos.y }, { netDepth, goalWidth }, 20);

        // B. Dynamic Floodlight Cast Shadows
        appendPostShadow(topPost.getPosition());
        appendPostShadow(bottomPost.getPosition());
    }

    // ==========================================
        // --- SPLIT RENDERING LAYERS ---
        // ==========================================

        // 1. FLOOR: Baked ambient and cast shadows
    void drawFloor(sf::RenderWindow& window) {
        window.draw(shadowMesh);
    }

    // 2. BACK: The net fabric 
    void drawNet(sf::RenderWindow& window) {
        window.draw(netMesh);
    }

    // 3. MID: The vertical posts and brackets (Interleaved using X-coordinate)
    void drawPosts(sf::RenderWindow& window) {
        window.draw(topPost);
        window.draw(bottomPost);
        window.draw(topUpright);
        window.draw(bottomUpright);
    }

    // 4. ROOF: The crossbar (Simulates Z-height by rendering on top)
    void drawCrossbar(sf::RenderWindow& window) {
        window.draw(visualCrossbar);
    }
};