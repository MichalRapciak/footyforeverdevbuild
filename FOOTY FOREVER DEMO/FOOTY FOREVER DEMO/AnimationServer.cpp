#include "AnimationServer.h"
#include <iostream>

std::map<Direction, Animation> AnimationServer::s_runningAnimations;
std::map<Direction, std::vector<Animation>> AnimationServer::s_tackleAnimations;

// Allocate static memory
sf::Texture AnimationServer::s_skinTexture;
sf::Texture AnimationServer::s_shirtTexture;
sf::Texture AnimationServer::s_shortsTexture;
sf::Texture AnimationServer::s_socksTexture;
std::map<std::string, sf::Texture> AnimationServer::s_kitTextures;
sf::Texture AnimationServer::s_tackleTexture;
bool AnimationServer::s_texturesLoaded = false;

void AnimationServer::init() {
    loadMasterTextures();
    buildAnimations();
}

void AnimationServer::loadMasterTextures() {
    if (s_texturesLoaded) return;

    std::cout << "Loading Master Textures into VRAM...\n";
    if (!s_skinTexture.loadFromFile("ASSETS/PLAYER/player_run_ing.png")) std::cerr << "Failed to load skin!\n";
    if (!s_kitTextures["shirt_base"].loadFromFile("ASSETS/PLAYER/shirt_run_ing.png")) std::cerr << "Failed to load shirt!\n";
    if (!s_kitTextures["shirt_sleeves"].loadFromFile("ASSETS/PLAYER/sleeve_run_ing.png")) std::cerr << "Failed to load sleeves!\n";
    if (!s_kitTextures["shorts_base"].loadFromFile("ASSETS/PLAYER/shorts_run_ing.png")) std::cerr << "Failed to load shorts!\n";
    if (!s_kitTextures["socks_base"].loadFromFile("ASSETS/PLAYER/socks_run_ing.png")) std::cerr << "Failed to load socks!\n";
    if (!s_tackleTexture.loadFromFile("ASSETS/PLAYER/player_tackle_ing.png")) std::cerr << "Failed to load tackle!\n";

    // ==========================================
    // --- NEW: FACES AND BEARDS ---
    // ==========================================
    if (!s_kitTextures["player_face_ing"].loadFromFile("ASSETS/PLAYER/player_face_ing.png")) std::cerr << "Failed to load face!\n";
    if (!s_kitTextures["player_beard_ing"].loadFromFile("ASSETS/PLAYER/player_beard_ing.png")) std::cerr << "Failed to load beard!\n";
    if (!s_kitTextures["player_goatee_ing"].loadFromFile("ASSETS/PLAYER/player_goatee_ing.png")) std::cerr << "Failed to load goatee!\n";

    // --- HAIR ---
    if (!s_kitTextures["hair_bun"].loadFromFile("ASSETS/PLAYER/hair_bun.png")) std::cerr << "Failed to load hair bun!\n";
    if (!s_kitTextures["hair_curlytop"].loadFromFile("ASSETS/PLAYER/hair_curlytop.png")) std::cerr << "Failed to load curly top!\n";
    if (!s_kitTextures["hair_dreads"].loadFromFile("ASSETS/PLAYER/hair_dreads.png")) std::cerr << "Failed to load dreads!\n";
    if (!s_kitTextures["hair_flattop"].loadFromFile("ASSETS/PLAYER/hair_flattop.png")) std::cerr << "Failed to load flat top!\n";
    if (!s_kitTextures["hair_short"].loadFromFile("ASSETS/PLAYER/hair_short.png")) std::cerr << "Failed to load short hair!\n";
    if (!s_kitTextures["hair_skinfade"].loadFromFile("ASSETS/PLAYER/hair_skinfade.png")) std::cerr << "Failed to load skin fade!\n";

    s_texturesLoaded = true;
}

void AnimationServer::buildAnimations() {
    const int frameW = 500;
    const int frameH = 500;
    const int numRunFrames = 12;
    const int runLoopStart = 3;

    // Standard Directions
    s_runningAnimations[Direction::Down] = sliceRow(0, numRunFrames, frameW, frameH, false, runLoopStart);
    s_runningAnimations[Direction::Up] = sliceRow(500, numRunFrames, frameW, frameH, false, runLoopStart);
    s_runningAnimations[Direction::DownLeft] = sliceRow(1000, numRunFrames, frameW, frameH, false, runLoopStart);
    s_runningAnimations[Direction::Left] = sliceRow(1500, numRunFrames, frameW, frameH, false, runLoopStart);
    s_runningAnimations[Direction::UpLeft] = sliceRow(2000, numRunFrames, frameW, frameH, false, runLoopStart);

    // Flipped Directions 
    s_runningAnimations[Direction::DownRight] = sliceRow(1000, numRunFrames, frameW, frameH, true, runLoopStart);
    s_runningAnimations[Direction::Right] = sliceRow(1500, numRunFrames, frameW, frameH, true, runLoopStart);
    s_runningAnimations[Direction::UpRight] = sliceRow(2000, numRunFrames, frameW, frameH, true, runLoopStart);

    auto sliceTackleSeq = [&](int startY, int startFrame, bool flipX) {
        Animation anim;
        anim.loopStartIndex = 0;
        for (int i = 0; i < 4; ++i) {
            int xPos = (startFrame + i) * frameW;
            if (flipX) anim.frames.push_back(sf::IntRect({ xPos + frameW, startY }, { -frameW, frameH }));
            else       anim.frames.push_back(sf::IntRect({ xPos, startY }, { frameW, frameH }));
        }
        return anim;
        };

    // Standard Tackles
    s_tackleAnimations[Direction::Down].push_back(sliceTackleSeq(0, 0, false));
    s_tackleAnimations[Direction::Down].push_back(sliceTackleSeq(0, 4, false));
    s_tackleAnimations[Direction::Down].push_back(sliceTackleSeq(0, 8, false));
    s_tackleAnimations[Direction::Up].push_back(sliceTackleSeq(500, 0, false));
    s_tackleAnimations[Direction::Up].push_back(sliceTackleSeq(500, 4, false));
    s_tackleAnimations[Direction::Up].push_back(sliceTackleSeq(500, 8, false));
    s_tackleAnimations[Direction::DownLeft].push_back(sliceTackleSeq(1000, 0, false));
    s_tackleAnimations[Direction::DownLeft].push_back(sliceTackleSeq(1000, 4, false));
    s_tackleAnimations[Direction::DownLeft].push_back(sliceTackleSeq(1000, 8, false));
    s_tackleAnimations[Direction::Left].push_back(sliceTackleSeq(1500, 0, false));
    s_tackleAnimations[Direction::Left].push_back(sliceTackleSeq(1500, 4, false));
    s_tackleAnimations[Direction::Left].push_back(sliceTackleSeq(1500, 8, false));
    s_tackleAnimations[Direction::UpLeft].push_back(sliceTackleSeq(2000, 0, false));
    s_tackleAnimations[Direction::UpLeft].push_back(sliceTackleSeq(2000, 4, false));
    s_tackleAnimations[Direction::UpLeft].push_back(sliceTackleSeq(2000, 8, false));

    // Flipped Tackles
    s_tackleAnimations[Direction::DownRight].push_back(sliceTackleSeq(1000, 0, true));
    s_tackleAnimations[Direction::DownRight].push_back(sliceTackleSeq(1000, 4, true));
    s_tackleAnimations[Direction::DownRight].push_back(sliceTackleSeq(1000, 8, true));
    s_tackleAnimations[Direction::Right].push_back(sliceTackleSeq(1500, 0, true));
    s_tackleAnimations[Direction::Right].push_back(sliceTackleSeq(1500, 4, true));
    s_tackleAnimations[Direction::Right].push_back(sliceTackleSeq(1500, 8, true));
    s_tackleAnimations[Direction::UpRight].push_back(sliceTackleSeq(2000, 0, true));
    s_tackleAnimations[Direction::UpRight].push_back(sliceTackleSeq(2000, 4, true));
    s_tackleAnimations[Direction::UpRight].push_back(sliceTackleSeq(2000, 8, true));
}

Animation AnimationServer::sliceRow(int startY, int numFrames, int frameW, int frameH, bool flipX, int loopStart) {
    Animation anim;
    anim.loopStartIndex = loopStart;
    for (int i = 0; i < numFrames; ++i) {
        int xPos = i * frameW;
        if (flipX) anim.frames.push_back(sf::IntRect({ xPos + frameW, startY }, { -frameW, frameH }));
        else       anim.frames.push_back(sf::IntRect({ xPos, startY }, { frameW, frameH }));
    }
    return anim;
}

const Animation& AnimationServer::getRunningAnimation(Direction dir) { return s_runningAnimations.at(dir); }

const Animation& AnimationServer::getTackleAnimation(Direction dir, int currentRunFrame) {
    if (currentRunFrame <= 2) return s_tackleAnimations.at(dir)[0];
    else if (currentRunFrame <= 6) return s_tackleAnimations.at(dir)[1];
    else return s_tackleAnimations.at(dir)[2];
}