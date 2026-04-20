#include "MatchIntroState.h"
#include <algorithm> // For std::clamp

MatchIntroState::MatchIntroState() {}

void MatchIntroState::init(sf::Font& font, GameDatabase& database, const std::string& homeId, const std::string& awayId, const std::string& userId) {
    m_font = font;
    m_db = &database;
    m_homeTeamId = homeId;
    m_awayTeamId = awayId;
    m_userPlayerId = userId;

    // Set up visuals
    m_bg.setFillColor(sf::Color(15, 15, 20));

    m_barBg.setFillColor(sf::Color(50, 50, 50));
    m_barBg.setOutlineThickness(2.f);
    m_barBg.setOutlineColor(sf::Color::White);

    m_barFill.setFillColor(sf::Color(50, 200, 50));

    // Fetch team info for the UI labels
    TeamData* h = m_db->getTeam(m_homeTeamId);
    TeamData* a = m_db->getTeam(m_awayTeamId);
    if (h) m_homeData = *h;
    if (a) m_awayData = *a;

    // Reset progress tracking
    m_currentProgress = 0.0f;
    m_barFill.setSize({ 0.f, 20.f });
}

void MatchIntroState::update(sf::Time dt, sf::RenderWindow& window, float progress) {
    // Keep background scaled to window
    m_bg.setSize(sf::Vector2f(window.getSize()));

    // Store the progress so the render function can use it!
    m_currentProgress = std::clamp(progress, 0.0f, 1.0f);
}

void MatchIntroState::render(sf::RenderWindow& window) {
    window.setView(window.getDefaultView());
    window.draw(m_bg);

    sf::Vector2u size = window.getSize();
    float cx = size.x / 2.f;
    float cy = size.y / 2.f;

    // 1. Draw Matchup Text
    sf::Text matchText(m_font, m_homeData.shortName + "  VS  " + m_awayData.shortName, 60);
    matchText.setFillColor(sf::Color::White);
    sf::FloatRect bounds = matchText.getLocalBounds();
    matchText.setPosition({ cx - bounds.size.x / 2.f, cy - 100.f });
    window.draw(matchText);

    // ==========================================
    // --- THE FIX: DYNAMIC TEXT & PROGRESS ---
    // ==========================================

    // Calculate how many players we have loaded so far out of 22
    int playersLoaded = static_cast<int>(m_currentProgress * 22.0f);
    std::string loadString = "Loading Player " + std::to_string(playersLoaded) + " of 22...";

    if (m_currentProgress >= 1.0f) {
        loadString = "Initializing Match...";
    }

    // 2. Draw Loading Status
    sf::Text statusText(m_font, loadString, 24);
    statusText.setFillColor(sf::Color(200, 200, 200));
    bounds = statusText.getLocalBounds();
    statusText.setPosition({ cx - bounds.size.x / 2.f, cy + 20.f });
    window.draw(statusText);

    // 3. Draw Progress Bar
    float barWidth = 600.f;
    float barHeight = 20.f;
    sf::Vector2f barPos(cx - barWidth / 2.f, cy + 60.f);

    m_barBg.setSize({ barWidth, barHeight });
    m_barBg.setPosition(barPos);
    window.draw(m_barBg);

    // Apply the stored progress to the green fill
    m_barFill.setSize({ barWidth * m_currentProgress, barHeight });
    m_barFill.setPosition(barPos);
    window.draw(m_barFill);
}