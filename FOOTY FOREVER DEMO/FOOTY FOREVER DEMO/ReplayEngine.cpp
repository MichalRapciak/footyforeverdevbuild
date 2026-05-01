#include "ReplayEngine.h"
#include "GameDatabase.h"
#include "AnimationServer.h"

void ReplayEngine::recordFrame(Ball* ball, const std::vector<Player*>& allPlayers)
{
    if (m_isReplaying) return;

    if (m_isRecordingLocked) {
        if (m_postRollFrames > 0) {
            m_postRollFrames--;
        }
        else {
            return;
        }
    }

    ReplayFrame frame;

    // ==========================================
    // 1. RECORD BALL 
    // ==========================================
    if (ball) {
        frame.ballShadow = ball->getShadow();
        frame.ballPos = ball->getPosition();

        // THE FIX: Record possession state so we can rewind to the pass!
        frame.ballHasOwner = ball->hasOwner();

        BodySnapshot ballSnap;
        ballSnap.sprite = ball->getSprite();
        ballSnap.sortDepth = ball->getSortDepth();
        ballSnap.isPlayer = false;
        frame.bodies.push_back(ballSnap);
    }

    // ==========================================
    // 2. RECORD PLAYERS 
    // ==========================================
    for (Player* p : allPlayers) {
        if (p && !p->isSentOff()) {

            sf::Vector2f groundPos = p->getPosition();
            float z = p->z;
            sf::Vector2f spriteScale = p->getSprite().getScale();
            sf::Vector2f feetPos = groundPos;
            feetPos.x -= 150.f * std::abs(spriteScale.x);

            float zRatio = std::min(z / 100.f, 1.f);
            float airFade = std::max(0.f, 1.0f - (z / 150.f));
            float shadowScale = 1.f - (zRatio * 0.5f);
            float currentRadius = 20.f * shadowScale;

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

            sf::CircleShape core(20.f);
            core.setFillColor(sf::Color(0, 0, 0, static_cast<std::uint8_t>(100 * airFade)));
            core.setOrigin({ 20.f, 20.f });
            core.setPosition(feetPos);
            core.setScale({ shadowScale, shadowScale });
            frame.playerCoreShadows.push_back(core);

            sf::Vector2f visualPos = { groundPos.x + (z / 1.5f), groundPos.y };
            sf::Sprite visualSprite = p->getSprite();
            visualSprite.setPosition(visualPos);

            float scaleMultiplier = 1.0f + (z / 750.f);
            visualSprite.setScale({ visualSprite.getScale().x * scaleMultiplier, visualSprite.getScale().y * scaleMultiplier });

            BodySnapshot pSnap;
            pSnap.sprite = visualSprite;
            pSnap.sortDepth = p->getSortDepth();

            pSnap.isPlayer = true;
            pSnap.skinColor = p->getSkinColor();

            // ==========================================
            // --- THE FIX 3: DEEP COPY THE KIT STACK ---
            // ==========================================
            // We MUST copy the layer vector, otherwise the shader has nothing to draw!
            pSnap.kitLayers = p->getKitLayers();

            frame.bodies.push_back(pSnap);
        }
    }

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

    // ==========================================
    // --- THE FIX: 300-FRAME STANDARD REPLAY ---
    // ==========================================
    // Instead of playing the entire buffer, we only play the last 300 frames (~5 seconds).
    int startFrame = static_cast<int>(m_buffer.size()) - 300;
    m_playbackIndex = std::max(0, startFrame);

    m_frameTimer = 0.0f;

    // Wipe VAR State Clean
    m_isOffsideReplay = false;
    m_freezeTimer = 0.0f;
    m_currentZoom = 0.8f;

    // Snap the camera to the NEW starting frame, not frame 0!
    if (m_buffer[m_playbackIndex].ballPos.has_value()) {
        m_replayCamPos = m_buffer[m_playbackIndex].ballPos.value();
    }
}

void ReplayEngine::startOffsideReplay(float playbackSpeed, float defenderLineX, sf::Vector2f receiverPos)
{
    if (m_buffer.empty()) return;

    m_isReplaying = true;
    m_playbackSpeed = playbackSpeed;
    m_isOffsideReplay = true;
    m_freezeTimer = 3.5f;
    m_defenderLineX = defenderLineX;
    m_receiverPos = receiverPos;
    m_showVarLines = false;

    int passFrameIndex = -1;

    // ==========================================
    // --- THE FIX 2: BULLETPROOF TIMELINE SCAN ---
    // ==========================================
    // Scan backward to find the EXACT frame the ball left the passer's foot!
    for (int i = static_cast<int>(m_buffer.size()) - 1; i > 0; --i) {
        if (!m_buffer[i].ballHasOwner && m_buffer[i - 1].ballHasOwner) {
            passFrameIndex = i - 1;
            break;
        }
    }

    // Failsafe: If the pass was a weird deflection, just start 5 seconds back
    if (passFrameIndex == -1) {
        passFrameIndex = std::max(0, static_cast<int>(m_buffer.size()) - 300);
    }

    m_varFreezeFrameIndex = std::min(passFrameIndex + 2, static_cast<int>(m_buffer.size()) - 1);

    // Start the replay 45 frames (~0.75 seconds) before the pass
    m_playbackIndex = std::max(0, m_varFreezeFrameIndex - 45);
    m_frameTimer = 0.0f;

    if (m_buffer[m_varFreezeFrameIndex].ballPos.has_value()) {
        m_frozenPasserPos = m_buffer[m_varFreezeFrameIndex].ballPos.value();
    }
    else {
        m_frozenPasserPos = m_receiverPos; // fallback
    }

    if (m_buffer[m_playbackIndex].ballPos.has_value()) {
        m_replayCamPos = m_buffer[m_playbackIndex].ballPos.value();
    }
    m_currentZoom = 0.8f;
}

void ReplayEngine::replayCam(sf::RenderWindow& window)
{
    if (!m_isReplaying || m_buffer.empty() || m_playbackIndex >= m_buffer.size()) return;

    const ReplayFrame& frame = m_buffer[m_playbackIndex];
    sf::View replayView = window.getDefaultView();

    if (m_isOffsideReplay && m_showVarLines) {
        // Use the updated variable names
        float minX = std::min({ m_frozenPasserPos.x, m_defenderLineX, m_receiverPos.x });
        float maxX = std::max({ m_frozenPasserPos.x, m_defenderLineX, m_receiverPos.x });
        float minY = std::min(m_frozenPasserPos.y, m_receiverPos.y);
        float maxY = std::max(m_frozenPasserPos.y, m_receiverPos.y);

        float distX = maxX - minX;
        float distY = maxY - minY;

        float viewWidth = replayView.getSize().x;
        float viewHeight = replayView.getSize().y;

        float zoomForX = (distX + 1500.f) / viewHeight;
        float zoomForY = (distY + 1500.f) / viewWidth;

        float targetZoom = std::max({ 0.8f, zoomForX, zoomForY });
        m_currentZoom += (targetZoom - m_currentZoom) * 0.05f;
        replayView.zoom(m_currentZoom);

        sf::Vector2f targetPos(minX + (distX / 2.f), minY + (distY / 2.f));
        m_replayCamPos.x += (targetPos.x - m_replayCamPos.x) * 0.05f;
        m_replayCamPos.y += (targetPos.y - m_replayCamPos.y) * 0.05f;
    }
    else {
        m_currentZoom += (0.8f - m_currentZoom) * 0.1f;
        replayView.zoom(m_currentZoom);

        if (frame.ballPos.has_value()) {
            sf::Vector2f targetPos = frame.ballPos.value();
            m_replayCamPos.x += (targetPos.x - m_replayCamPos.x) * 0.1f;
            m_replayCamPos.y += (targetPos.y - m_replayCamPos.y) * 0.1f;
        }
    }

    replayView.setRotation(sf::degrees(90.f));
    replayView.setCenter(m_replayCamPos);
    window.setView(replayView);
}

void ReplayEngine::update(float dt)
{
    if (!m_isReplaying) return;

    // ==========================================
    // --- THE FIX 4: DYNAMIC MID-REPLAY FREEZE ---
    // ==========================================
    // If the replay has reached the exact frame the pass left the foot, freeze it!
    if (m_isOffsideReplay && m_playbackIndex == m_varFreezeFrameIndex && m_freezeTimer > 0.0f) {
        m_showVarLines = true;
        m_freezeTimer -= dt;
        m_blendFactor = 0.0f;
        return;
    }

    m_frameTimer += dt * m_playbackSpeed;
    float timePerFrame = 1.0f / 60.0f;

    while (m_frameTimer >= timePerFrame) {
        m_frameTimer -= timePerFrame;
        m_playbackIndex++;

        if (m_playbackIndex >= m_buffer.size() - 1) {
            m_isReplaying = false;
            m_isRecordingLocked = false;
            m_isOffsideReplay = false;
            m_showVarLines = false;
            m_buffer.clear();
            return;
        }
    }

    m_blendFactor = m_frameTimer / timePerFrame;
}

static sf::Vector2f lerpPos(sf::Vector2f a, sf::Vector2f b, float t) {
    return { a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t };
}

void ReplayEngine::render(sf::RenderWindow& window, sf::Shader* kitShader)
{
    if (!m_isReplaying || m_playbackIndex >= m_buffer.size() - 1) return;

    const ReplayFrame& frameA = m_buffer[m_playbackIndex];
    const ReplayFrame& frameB = m_buffer[m_playbackIndex + 1];

    if (frameA.ballShadow.has_value()) window.draw(frameA.ballShadow.value());
    for (const auto& flood : frameA.playerFloodlights) window.draw(flood);
    for (const auto& core : frameA.playerCoreShadows) window.draw(core);

    std::vector<BodySnapshot> blendedBodies;
    size_t count = std::min(frameA.bodies.size(), frameB.bodies.size());

    for (size_t i = 0; i < count; ++i) {
        BodySnapshot blended;

        blended.sortDepth = frameA.bodies[i].sortDepth + (frameB.bodies[i].sortDepth - frameA.bodies[i].sortDepth) * m_blendFactor;
        blended.sprite = frameA.bodies[i].sprite;

        blended.isPlayer = frameA.bodies[i].isPlayer;
        blended.skinColor = frameA.bodies[i].skinColor;
        blended.kitLayers = frameA.bodies[i].kitLayers;

        sf::Vector2f posA = frameA.bodies[i].sprite.value().getPosition();
        sf::Vector2f posB = frameB.bodies[i].sprite.value().getPosition();
        blended.sprite.value().setPosition(lerpPos(posA, posB, m_blendFactor));

        sf::Vector2f scaleA = frameA.bodies[i].sprite.value().getScale();
        sf::Vector2f scaleB = frameB.bodies[i].sprite.value().getScale();
        blended.sprite.value().setScale(lerpPos(scaleA, scaleB, m_blendFactor));

        blendedBodies.push_back(blended);
    }

    std::sort(blendedBodies.begin(), blendedBodies.end(), [](const BodySnapshot& a, const BodySnapshot& b) {
        return a.sortDepth > b.sortDepth;
        });

    for (const auto& body : blendedBodies) 
    {
        if (body.isPlayer && kitShader) 
        {
            auto toGlslColor = [](sf::Color c) 
            {
                return sf::Glsl::Vec4(c.r / 255.f, c.g / 255.f, c.b / 255.f, c.a / 255.f);
            };

            kitShader->setUniform("skinColor", toGlslColor(body.skinColor));
            kitShader->setUniform("skinTex", sf::Shader::CurrentTexture);

            // ==========================================
            // --- THE FIX 1: EXPAND TO 15 LAYER MAXIMUM ---
            // ==========================================
            static const std::string uUse[15] = {
                "use0", "use1", "use2", "use3", "use4", "use5", "use6", "use7",
                "use8", "use9", "use10", "use11", "use12", "use13", "use14"
            };
            static const std::string uTex[15] = {
                "tex0", "tex1", "tex2", "tex3", "tex4", "tex5", "tex6", "tex7",
                "tex8", "tex9", "tex10", "tex11", "tex12", "tex13", "tex14"
            };
            static const std::string uCol[15] = {
                "col0", "col1", "col2", "col3", "col4", "col5", "col6", "col7",
                "col8", "col9", "col10", "col11", "col12", "col13", "col14"
            };

            for (int i = 0; i < 15; ++i) { // CRITICAL: Loop up to 15!
                if (i < body.kitLayers.size()) {
                    sf::Texture* tex = AnimationServer::getKitTexture(body.kitLayers[i].textureId);
                    if (tex) {
                        kitShader->setUniform(uUse[i], true);
                        kitShader->setUniform(uTex[i], *tex);
                        kitShader->setUniform(uCol[i], toGlslColor(body.kitLayers[i].color));
                    }
                    else {
                        kitShader->setUniform(uUse[i], false);
                    }
                }
                else {
                    kitShader->setUniform(uUse[i], false);
                }
            }

            window.draw(body.sprite.value(), kitShader);
        }
        else {
            window.draw(body.sprite.value());
        }
    }

    // ==========================================
    // --- NEW: DRAW VAR OFFSIDE LINES ---
    // ==========================================
    // We now draw the lines for the entire replay, not just during the freeze.
    if (m_isOffsideReplay && m_showVarLines) {
        float alpha = (m_freezeTimer > 0.0f) ? 160.f : 70.f;

        // ==========================================
        // --- THE FIX 3: COLOR MAPPING ---
        // ==========================================
        // 1. Draw Attacking Red Line (Where the Receiver was)
        sf::RectangleShape blueLine(sf::Vector2f(20.f, 8000.f));
        blueLine.setOrigin({ 10.f, 4000.f });
        blueLine.setPosition({ m_receiverPos.x, m_replayCamPos.y });
        blueLine.setFillColor(sf::Color(255, 0, 0, static_cast<std::uint8_t>(alpha)));

        // 2. Draw Defensive Blue Line (Where the Last Defender was)
        sf::RectangleShape redLine(sf::Vector2f(20.f, 8000.f));
        redLine.setOrigin({ 10.f, 4000.f });
        redLine.setPosition({ m_defenderLineX, m_replayCamPos.y });
        redLine.setFillColor(sf::Color(0, 100, 255, static_cast<std::uint8_t>(alpha)));

        window.draw(redLine); // Draw red first so it sits under the blue line
        window.draw(blueLine);
    }
}