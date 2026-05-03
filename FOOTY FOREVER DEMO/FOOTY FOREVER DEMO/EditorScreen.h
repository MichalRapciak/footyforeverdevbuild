#ifndef EDITORSCREEN_HPP
#define EDITORSCREEN_HPP

#include "GameDatabase.h"
#include <SFML/Graphics.hpp>
#include <SFML/Graphics/RenderTexture.hpp> // <-- NEW
#include <SFML/Graphics/Shader.hpp>        // <-- NEW
#include <string>

class EditorScreen {
public:
    EditorScreen();
    ~EditorScreen();

    void init(sf::Font& font, GameDatabase& database);
    void update(sf::Time dt, sf::RenderWindow& window);
    void render(sf::RenderWindow& window);
    void processEvents(sf::Event& event);

private:
    // --- UI Panel Helpers ---
    void drawPlayerTab(float availableHeight);
    void drawTeamTab(float availableHeight);
    void drawLeagueTab(float availableHeight);

    // Team Sub-Tabs
    void drawTeamGeneralTab(TeamData* t);
    void drawTeamKitsTab(TeamData* t);
    void drawTeamRosterTab(TeamData* t);
    void drawTeamTacticsTab(TeamData* t);

    // Global Footer
    void drawFooter();

    // ==========================================
    // --- NEW: OFFFSCREEN RENDER PIPELINE ---
    // ==========================================
    void updatePreviewTexture(sf::RenderTexture& target, sf::Color skin, const std::vector<KitLayer>& kitLayers, float heightCm, float weightKg);

    sf::Shader m_kitShader;
    sf::RenderTexture m_playerPreviewTexture;
    sf::RenderTexture m_teamPreviewTexture;

    // --- State ---
    sf::Font m_font;
    GameDatabase* m_db;
    std::string m_selectedPlayerId;
    std::string m_selectedTeamId;
    std::string m_selectedLeagueId;
    sf::Sprite bg_s;
    sf::Texture bg_txt;
};

#endif // EDITORSCREEN_HPP