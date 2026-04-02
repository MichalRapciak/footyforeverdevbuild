#ifndef EDITORSCREEN_HPP
#define EDITORSCREEN_HPP

#include "GameDatabase.h"
#include <SFML/Graphics.hpp>
#include <string>

class EditorScreen
{
public:
    EditorScreen();
    ~EditorScreen();

    // Pass the database in via reference so the editor can modify the live data
    void init(sf::Font& font, GameDatabase& database);

    // Core loop functions
    void update(sf::Time dt, sf::RenderWindow& window);
    void render(sf::RenderWindow& window);
    void processEvents(sf::Event& event);

private:
    GameDatabase* m_db; // Pointer to the master database
    sf::Font     m_font;

    // Variables to track what we are currently looking at in the ImGui windows
    std::string m_selectedPlayerId;
    std::string m_selectedTeamId;
};

#endif // EDITORSCREEN_HPP