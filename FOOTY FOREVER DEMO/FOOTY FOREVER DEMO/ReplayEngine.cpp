#include "ReplayEngine.h"

void ReplayEngine::recordFrame(Ball* ball, const std::vector<Player*>& allPlayers)
{
    if (m_isReplaying) return;

    // ==========================================
    // --- NEW: POST-ROLL FREEZE CHECK ---
    // ==========================================
    if (m_isRecordingLocked) {
        if (m_postRollFrames > 0) {
            m_postRollFrames--; // Count down the frames
        }
        else {
            return; // The timer hit 0! The buffer is perfectly frozen in time.
        }
    }

    ReplayFrame frame;

    // ==========================================
    // 1. RECORD BALL (Index 0 in the bodies array)
    // ==========================================
    if (ball) {
        frame.ballShadow = ball->getShadow(); // Assuming getShadow returns sf::Sprite or sf::CircleShape
        frame.ballPos = ball->getPosition();

        BodySnapshot ballSnap;
        ballSnap.sprite = ball->getSprite();
        ballSnap.sortDepth = ball->getSortDepth();
        frame.bodies.push_back(ballSnap);
    }

    // ==========================================
    // 2. RECORD PLAYERS (Unsorted / Fixed Order)
    // ==========================================
    for (Player* p : allPlayers) {
        if (p && !p->isSentOff()) {

            // --- A. GET RAW DATA ---
            sf::Vector2f groundPos = p->getPosition();
            float z = p->z;
            sf::Vector2f spriteScale = p->getSprite().getScale();
            sf::Vector2f feetPos = groundPos;
            feetPos.x -= 150.f * std::abs(spriteScale.x);

            float zRatio = std::min(z / 100.f, 1.f);
            float airFade = std::max(0.f, 1.0f - (z / 150.f));
            float shadowScale = 1.f - (zRatio * 0.5f);
            float currentRadius = 20.f * shadowScale;

            // --- B. FLOODLIGHT SHADOWS ---
            if (airFade > 0.01f) {
                sf::Vector2f lights[4] = {
                    {-500.f, -500.f},
                    {-500.f, 7500.f},
                    {10500.f, -500.f},
                    {10500.f, 7500.f}
                };

                const float lightHeight = 6000.f;
                const float playerHeight = 180.f;
                const float maxLightDist = 12500.f;

                for (int i = 0; i < 4; ++i) {
                    sf::Vector2f toPlayer = feetPos - lights[i];
                    float distXY = std::sqrt(toPlayer.x * toPlayer.x + toPlayer.y * toPlayer.y);

                    if (distXY > 0.1f) {
                        sf::Vector2f dir = toPlayer / distXY;
                        sf::Vector2f normal(-dir.y, dir.x);

                        float totalHeight = playerHeight + z;
                        float length = totalHeight * (distXY / lightHeight);
                        length = std::max(15.f, length);

                        float normalizedDist = std::min(distXY / maxLightDist, 1.0f);
                        float intensity = std::pow(1.0f - normalizedDist, 2.0f);
                        std::uint8_t alpha = static_cast<std::uint8_t>(60 * intensity * airFade);

                        if (alpha >= 2) {
                            sf::Color baseColor(0, 0, 0, alpha);
                            sf::Color tipColor(0, 0, 0, 0);

                            float diffusion = 1.2f + (normalizedDist * 3.0f);
                            float width = 12.f * shadowScale;
                            sf::Vector2f start = feetPos + (dir * (currentRadius - 2.f));

                            sf::VertexArray floodShadow(sf::PrimitiveType::TriangleStrip, 4);
                            floodShadow[0].position = start + normal * width;
                            floodShadow[0].color = baseColor;
                            floodShadow[1].position = start - normal * width;
                            floodShadow[1].color = baseColor;
                            floodShadow[2].position = start + (dir * length) + normal * (width * diffusion);
                            floodShadow[2].color = tipColor;
                            floodShadow[3].position = start + (dir * length) - normal * (width * diffusion);
                            floodShadow[3].color = tipColor;

                            frame.playerFloodlights.push_back(floodShadow);
                        }
                    }
                }
            }

            // --- C. CORE SHADOW ---
            sf::CircleShape core(20.f);
            core.setFillColor(sf::Color(0, 0, 0, static_cast<std::uint8_t>(100 * airFade)));
            core.setOrigin({ 20.f, 20.f });
            core.setPosition(feetPos);
            core.setScale({ shadowScale, shadowScale });
            frame.playerCoreShadows.push_back(core);

            // --- D. ELEVATED PLAYER VISUALS ---
            sf::Vector2f visualPos = { groundPos.x + (z / 1.5f), groundPos.y };
            sf::Sprite visualSprite = p->getSprite();
            visualSprite.setPosition(visualPos);

            float scaleMultiplier = 1.0f + (z / 750.f);
            visualSprite.setScale({ visualSprite.getScale().x * scaleMultiplier, visualSprite.getScale().y * scaleMultiplier });

            // Push to the unsorted array with depth info
            BodySnapshot pSnap;
            pSnap.sprite = visualSprite;
            pSnap.sortDepth = p->getSortDepth();
            frame.bodies.push_back(pSnap);
        }
    }

    // ==========================================
    // 3. BUFFER MANAGEMENT
    // ==========================================
    m_buffer.push_back(frame);
    if (m_buffer.size() > MAX_FRAMES) {
        m_buffer.pop_front();
    }
}

void ReplayEngine::startReplay(float playbackSpeed)
{
    if (m_buffer.empty()) return;

    m_isReplaying = true;
    m_playbackSpeed = playbackSpeed;
    m_playbackIndex = 0;
    m_frameTimer = 0.0f;

    // Snap the camera to the ball's exact position on frame 1
    if (m_buffer[0].ballPos.has_value()) {
        m_replayCamPos = m_buffer[0].ballPos.value();
    }
}

void ReplayEngine::update(float dt)
{
    if (!m_isReplaying) return;

    m_frameTimer += dt * m_playbackSpeed;
    float timePerFrame = 1.0f / 60.0f;

    while (m_frameTimer >= timePerFrame) {
        m_frameTimer -= timePerFrame;
        m_playbackIndex++;

        if (m_playbackIndex >= m_buffer.size() - 1) {
            m_isReplaying = false;

            // --- NEW: AUTO-UNLOCK ---
            m_isRecordingLocked = false;

            m_buffer.clear();
            return;
        }
    }

    // --- CALCULATE BLEND FACTOR ---
    // If m_frameTimer is half of timePerFrame, this equals 0.5 (50% blend)
    m_blendFactor = m_frameTimer / timePerFrame;
}

// Helper function for linear interpolation
static sf::Vector2f lerpPos(sf::Vector2f a, sf::Vector2f b, float t) {
    return { a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t };
}

void ReplayEngine::render(sf::RenderWindow& window)
{
    if (!m_isReplaying || m_playbackIndex >= m_buffer.size() - 1) return;

    // Grab the current frame and the NEXT frame
    const ReplayFrame& frameA = m_buffer[m_playbackIndex];
    const ReplayFrame& frameB = m_buffer[m_playbackIndex + 1];

    // ==========================================
    // 1. DRAW SHADOWS (Using Frame A is fine for soft shadows)
    // ==========================================
    if (frameA.ballShadow.has_value()) window.draw(frameA.ballShadow.value());
    for (const auto& flood : frameA.playerFloodlights) window.draw(flood);
    for (const auto& core : frameA.playerCoreShadows) window.draw(core);

    // ==========================================
    // 2. INTERPOLATE BODIES
    // ==========================================
    std::vector<BodySnapshot> blendedBodies;

    // Safety check: ensure both frames have the exact same number of entities
    size_t count = std::min(frameA.bodies.size(), frameB.bodies.size());

    for (size_t i = 0; i < count; ++i) {
        BodySnapshot blended;

        // Blend Depth
        blended.sortDepth = frameA.bodies[i].sortDepth + (frameB.bodies[i].sortDepth - frameA.bodies[i].sortDepth) * m_blendFactor;

        // Blend Sprite Properties
        blended.sprite = frameA.bodies[i].sprite; // Copy texture/rect info

        sf::Vector2f posA = frameA.bodies[i].sprite.value().getPosition();
        sf::Vector2f posB = frameB.bodies[i].sprite.value().getPosition();
        blended.sprite.value().setPosition(lerpPos(posA, posB, m_blendFactor));

        sf::Vector2f scaleA = frameA.bodies[i].sprite.value().getScale();
        sf::Vector2f scaleB = frameB.bodies[i].sprite.value().getScale();
        blended.sprite.value().setScale(lerpPos(scaleA, scaleB, m_blendFactor));

        blendedBodies.push_back(blended);
    }

    // ==========================================
    // 3. DEPTH SORT & DRAW
    // ==========================================
    std::sort(blendedBodies.begin(), blendedBodies.end(), [](const BodySnapshot& a, const BodySnapshot& b) {
        return a.sortDepth > b.sortDepth;
        });

    for (const auto& body : blendedBodies) {
        window.draw(body.sprite.value());
    }
}

void ReplayEngine::replayCam(sf::RenderWindow& window)
{
    if (!m_isReplaying || m_buffer.empty() || m_playbackIndex >= m_buffer.size()) {
        return;
    }

    const ReplayFrame& frame = m_buffer[m_playbackIndex];

    sf::View replayView = window.getDefaultView();
    replayView.zoom(0.8f);
    replayView.setRotation(sf::degrees(90.f));

    // Smooth Camera Follow (LERP)
    if (frame.ballPos.has_value()) {
        sf::Vector2f targetPos = frame.ballPos.value();

        m_replayCamPos.x += (targetPos.x - m_replayCamPos.x) * 0.1f;
        m_replayCamPos.y += (targetPos.y - m_replayCamPos.y) * 0.1f;
    }

    replayView.setCenter(m_replayCamPos);
    window.setView(replayView);
}