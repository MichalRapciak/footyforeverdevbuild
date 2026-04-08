#include "AnimationServer.h"
#include <iostream>

AnimationServer::AnimationServer() {}

void AnimationServer::init(const std::string& texturePath) {
    // 1. LOAD TEXTURES
    if (!m_playerTexture.loadFromFile(texturePath)) {
        std::cerr << "Failed to load run sprite sheet: " << texturePath << "\n";
    }
    if (!m_tackleTexture.loadFromFile("ASSETS/PLAYER/player_tackle_ing.png")) {
        std::cerr << "Failed to load tackle sprite sheet: ASSETS/PLAYER/player_tackle.png\n";
    }

    const int frameW = 500;
    const int frameH = 500;

    // ==========================================
    // 2. BUILD RUNNING ANIMATIONS
    // ==========================================
    const int numRunFrames = 12;
    const int runLoopStart = 3; // Frames 3 through 11 are the loop, 0-2 are startup

    // Standard Directions
    m_runningAnimations[Direction::Down] = sliceRow(0, numRunFrames, frameW, frameH, false, runLoopStart);
    m_runningAnimations[Direction::Up] = sliceRow(500, numRunFrames, frameW, frameH, false, runLoopStart);
    m_runningAnimations[Direction::DownLeft] = sliceRow(1000, numRunFrames, frameW, frameH, false, runLoopStart);
    m_runningAnimations[Direction::Left] = sliceRow(1500, numRunFrames, frameW, frameH, false, runLoopStart);
    m_runningAnimations[Direction::UpLeft] = sliceRow(2000, numRunFrames, frameW, frameH, false, runLoopStart);

    // Flipped Directions (Using negative width math)
    m_runningAnimations[Direction::DownRight] = sliceRow(1000, numRunFrames, frameW, frameH, true, runLoopStart);
    m_runningAnimations[Direction::Right] = sliceRow(1500, numRunFrames, frameW, frameH, true, runLoopStart);
    m_runningAnimations[Direction::UpRight] = sliceRow(2000, numRunFrames, frameW, frameH, true, runLoopStart);


    // ==========================================
    // 3. BUILD TACKLE ANIMATIONS
    // ==========================================
    // Helper lambda to slice 4 frames at a time for the tackle sequences
    auto sliceTackleSeq = [&](int startY, int startFrame, bool flipX) {
        Animation anim;
        anim.loopStartIndex = 0; // Tackles don't loop, so this doesn't matter much
        for (int i = 0; i < 4; ++i) {
            int xPos = (startFrame + i) * frameW;
            if (flipX) anim.frames.push_back(sf::IntRect({ xPos + frameW, startY }, { -frameW, frameH }));
            else       anim.frames.push_back(sf::IntRect({ xPos, startY }, { frameW, frameH }));
        }
        return anim;
        };

    // Standard Directions
    // Down (Y = 0)
    m_tackleAnimations[Direction::Down].push_back(sliceTackleSeq(0, 0, false)); // Seq A
    m_tackleAnimations[Direction::Down].push_back(sliceTackleSeq(0, 4, false)); // Seq B
    m_tackleAnimations[Direction::Down].push_back(sliceTackleSeq(0, 8, false)); // Seq C

    // Up (Y = 1000)
    m_tackleAnimations[Direction::Up].push_back(sliceTackleSeq(500, 0, false));
    m_tackleAnimations[Direction::Up].push_back(sliceTackleSeq(500, 4, false));
    m_tackleAnimations[Direction::Up].push_back(sliceTackleSeq(500, 8, false));

    // DownLeft (Y = 2000)
    m_tackleAnimations[Direction::DownLeft].push_back(sliceTackleSeq(1000, 0, false));
    m_tackleAnimations[Direction::DownLeft].push_back(sliceTackleSeq(1000, 4, false));
    m_tackleAnimations[Direction::DownLeft].push_back(sliceTackleSeq(1000, 8, false));

    // Left (Y = 3000)
    m_tackleAnimations[Direction::Left].push_back(sliceTackleSeq(1500, 0, false));
    m_tackleAnimations[Direction::Left].push_back(sliceTackleSeq(1500, 4, false));
    m_tackleAnimations[Direction::Left].push_back(sliceTackleSeq(1500, 8, false));

    // UpLeft (Y = 4000)
    m_tackleAnimations[Direction::UpLeft].push_back(sliceTackleSeq(2000, 0, false));
    m_tackleAnimations[Direction::UpLeft].push_back(sliceTackleSeq(2000, 4, false));
    m_tackleAnimations[Direction::UpLeft].push_back(sliceTackleSeq(2000, 8, false));


    // Flipped Directions
    // DownRight (Flipped DownLeft, Y = 2000)
    m_tackleAnimations[Direction::DownRight].push_back(sliceTackleSeq(1000, 0, true));
    m_tackleAnimations[Direction::DownRight].push_back(sliceTackleSeq(1000, 4, true));
    m_tackleAnimations[Direction::DownRight].push_back(sliceTackleSeq(1000, 8, true));

    // Right (Flipped Left, Y = 3000)
    m_tackleAnimations[Direction::Right].push_back(sliceTackleSeq(1500, 0, true));
    m_tackleAnimations[Direction::Right].push_back(sliceTackleSeq(1500, 4, true));
    m_tackleAnimations[Direction::Right].push_back(sliceTackleSeq(1500, 8, true));

    // UpRight (Flipped UpLeft, Y = 4000)
    m_tackleAnimations[Direction::UpRight].push_back(sliceTackleSeq(2000, 0, true));
    m_tackleAnimations[Direction::UpRight].push_back(sliceTackleSeq(2000, 4, true));
    m_tackleAnimations[Direction::UpRight].push_back(sliceTackleSeq(2000, 8, true));
}

Animation AnimationServer::sliceRow(int startY, int numFrames, int frameW, int frameH, bool flipX, int loopStart) {
    Animation anim;
    anim.loopStartIndex = loopStart;

    for (int i = 0; i < numFrames; ++i) {
        int xPos = i * frameW;

        if (flipX) {
            anim.frames.push_back(sf::IntRect({ xPos + frameW, startY }, { -frameW, frameH }));
        }
        else {
            anim.frames.push_back(sf::IntRect({ xPos, startY }, {frameW, frameH}));
        }
    }
    return anim;
}

const Animation& AnimationServer::getRunningAnimation(Direction dir) const {
    return m_runningAnimations.at(dir);
}

const Animation& AnimationServer::getTackleAnimation(Direction dir, int currentRunFrame) const {
    // Map the current run frame to the correct 4-frame tackle sequence
    if (currentRunFrame <= 2) {
        return m_tackleAnimations.at(dir)[0]; // Run frame 0,1,2 -> Seq A
    }
    else if (currentRunFrame <= 6) {
        return m_tackleAnimations.at(dir)[1]; // Run frame 3,4,5,6 -> Seq B
    }
    else {
        return m_tackleAnimations.at(dir)[2]; // Run frame 7,8,9,10,11 -> Seq C
    }
}

sf::Texture& AnimationServer::getPlayerTexture() {
    return m_playerTexture;
}

sf::Texture& AnimationServer::getTackleTexture() {
    return m_tackleTexture;
}