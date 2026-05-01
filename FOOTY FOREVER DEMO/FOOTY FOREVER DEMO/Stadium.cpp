#include "Stadium.h"
#include <iostream>
#include "MatchEngine.h"
#include <random>

Stadium::Stadium()
    : m_grassBG(m_grassTXT), m_linesBG(m_linesTXT) // Initialize sprites with their textures
{
    initialiseStadium();
    srand(time(nullptr));
}

Stadium::~Stadium()
{
}

void Stadium::initialiseStadium()
{
    // ==========================================
    // 1. THE GRASS LAYER (Tiled / Repeated)
    // ==========================================
    if (!m_grassTXT.loadFromFile("ASSETS/STADIUM/grass.png"))
    {
        std::cout << "Can't load grass texture" << std::endl;
    }

    m_grassTXT.setRepeated(true); // Enable tiling
    // Optional: m_grassTXT.setSmooth(true); if you want texture filtering

    m_grassBG.setTexture(m_grassTXT);
    m_grassBG.setPosition({ 0.f, 0.f });

    // By setting the TextureRect larger than the 1000x700 texture, 
    // SFML automatically repeats the image to fill the 10000x7000 space!
    m_grassBG.setTextureRect(sf::IntRect({ 0, 0 }, { 10000, 7000 }));
    // Notice: NO scaling applied to the grass! It tiles at its crisp native resolution.


    // ==========================================
    // 2. THE PITCH LINES LAYER (Scaled / Stretched)
    // ==========================================
    if (!m_linesTXT.loadFromFile("ASSETS/STADIUM/pitchlines2.png"))
    {
        std::cout << "Can't load pitch lines texture" << std::endl;
    }

    m_linesTXT.setRepeated(false);
    m_linesBG.setTexture(m_linesTXT);
    m_linesBG.setPosition({ 0.f, 0.f });
    m_linesBG.setColor({ 255,255,255,150 });
    m_linesBG.setTextureRect(sf::IntRect({ 0, 0 }, { 1000, 700 }));

    // Stretch the lines to cover the 10000x7000 pitch
    m_linesBG.setScale({ 10.0f, 10.0f });
}

void Stadium::update(MatchEngine& game)
{
}

void Stadium::draw(sf::RenderWindow& window) const
{
    // Order matters: Draw the grass first, then the lines on top
    window.draw(m_grassBG);
    window.draw(m_linesBG);
}