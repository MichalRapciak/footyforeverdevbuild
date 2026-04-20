#include "SettingsScreen.h"
#include "GlobalSettings.h"
#include "Game.h" // Wherever your GameState enum lives
#include "imgui-1.92.6/imgui.h"

void SettingsState::init(sf::RenderWindow& window) {
    m_background.setSize(sf::Vector2f(window.getSize()));
    m_background.setFillColor(sf::Color(20, 20, 30, 255)); // Dark slate background
}

void SettingsState::update(sf::RenderWindow& window) {
    ImGui::SetNextWindowSize(ImVec2(600, 600), ImGuiCond_Always); // Made slightly taller
    ImGui::SetNextWindowPos(ImVec2((window.getSize().x - 600) / 2.f, (window.getSize().y - 600) / 2.f), ImGuiCond_Always);

    if (ImGui::Begin("Game Settings", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize))
    {
        // ==========================================
        // 0. VIDEO SETTINGS
        // ==========================================
        ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "Video Settings");
        ImGui::Separator();

        // Standard 16:9 Resolutions
        const char* resolutions[] = { "1280x720", "1600x900", "1920x1080", "2560x1440", "3840x2160" };
        int currentResIndex = 2; // Default to 1080p
        if (GlobalSettings::windowWidth == 1280) currentResIndex = 0;
        else if (GlobalSettings::windowWidth == 1600) currentResIndex = 1;
        else if (GlobalSettings::windowWidth == 1920) currentResIndex = 2;
        else if (GlobalSettings::windowWidth == 2560) currentResIndex = 3;
        else if (GlobalSettings::windowWidth == 3840) currentResIndex = 4;

        if (ImGui::Combo("Resolution", &currentResIndex, resolutions, IM_ARRAYSIZE(resolutions))) {
            if (currentResIndex == 0) { GlobalSettings::windowWidth = 1280; GlobalSettings::windowHeight = 720; }
            else if (currentResIndex == 1) { GlobalSettings::windowWidth = 1600; GlobalSettings::windowHeight = 900; }
            else if (currentResIndex == 2) { GlobalSettings::windowWidth = 1920; GlobalSettings::windowHeight = 1080; }
            else if (currentResIndex == 3) { GlobalSettings::windowWidth = 2560; GlobalSettings::windowHeight = 1440; }
            else if (currentResIndex == 4) { GlobalSettings::windowWidth = 3840; GlobalSettings::windowHeight = 2160; }
        }

        ImGui::Checkbox("Fullscreen", &GlobalSettings::isFullscreen);
        ImGui::SliderInt("Framerate Limit", &GlobalSettings::targetFPS, 30, 240);

        ImGui::Spacing(); ImGui::Spacing();

        // ==========================================
        // 1. MATCH SETTINGS
        // ==========================================
        ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "Match Settings");
        ImGui::Separator();

        ImGui::SliderInt("Match Length (Minutes)", &GlobalSettings::matchLengthMinutes, 1, 90);
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Total real-world time the match will take.");
        ImGui::Spacing(); ImGui::Spacing();

        // ==========================================
        // 2. AUDIO SETTINGS
        // ==========================================
        ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "Audio Settings");
        ImGui::Separator();

        ImGui::SliderFloat("SFX Volume", &GlobalSettings::volumeSFX, 0.0f, 100.0f, "%.0f%%");
        ImGui::SliderFloat("Crowd Volume", &GlobalSettings::volumeCrowd, 0.0f, 100.0f, "%.0f%%");
        ImGui::Spacing(); ImGui::Spacing();

        // ==========================================
                // 3. CAMERA SETTINGS
                // ==========================================
        ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "Camera Settings");
        ImGui::Separator();

        ImGui::SliderFloat("Camera Zoom", &GlobalSettings::cameraZoom, 1.0f, 2.5f, "%.2fx");
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("1.0x = Wide Broadcast, 2.5x = Tight Player Cam");

        ImGui::SliderFloat("Ball Tracking", &GlobalSettings::cameraBallFollow, 0.0f, 1.0f, "%.2f");
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("0.0 = Lock to Player, 1.0 = Lock to Ball");

        // ==========================================
        // --- NEW: AIM PULL SLIDER ---
        // ==========================================
        ImGui::SliderFloat("Aim Pull Strength", &GlobalSettings::cameraAimPull, 0.0f, 2.0f, "%.2f");
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("How far the camera pans towards your mouse cursor.");

        ImGui::Spacing(); ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();

        // ==========================================
        // EXIT BUTTON
        // ==========================================
        if (ImGui::Button("Save & Return to Main Menu", ImVec2(-1, 40))) {

            GlobalSettings::save();

            // THE FIX: Instantly recreate the SFML window with the new settings!
            auto windowState = GlobalSettings::isFullscreen ? sf::State::Fullscreen : sf::State::Windowed;

            window.create(
                sf::VideoMode({ static_cast<unsigned int>(GlobalSettings::windowWidth),
                                static_cast<unsigned int>(GlobalSettings::windowHeight) }),
                "FOOTY FOREVER DEMO",
                windowState
            );
            window.setVerticalSyncEnabled(true);

            Game::currentState = GameState::MainMenu;
        }
    }
    ImGui::End();
}

void SettingsState::render(sf::RenderWindow& window) {
    window.setView(window.getDefaultView());
    window.draw(m_background);
}