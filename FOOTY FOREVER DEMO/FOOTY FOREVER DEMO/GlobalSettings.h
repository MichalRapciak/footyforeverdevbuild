#pragma once
#include <fstream>
#include <string>
#include <sstream>
#include <iostream>

struct GlobalSettings {
    // --- VIDEO SETTINGS ---
    static inline int windowWidth = 1920;
    static inline int windowHeight = 1080;
    static inline bool isFullscreen = true;
    static inline int targetFPS = 75;

    // --- MATCH SETTINGS ---
    static inline int matchLengthMinutes = 9;

    // --- AUDIO SETTINGS ---
    static inline float volumeSFX = 100.f;
    static inline float volumeCrowd = 50.f;

    // --- CAMERA SETTINGS ---
    static inline float cameraZoom = 2.0f;
    static inline float cameraBallFollow = 0.35f;
    static inline float cameraAimPull = 0.35f;

    // ==========================================
    // --- SAVE & LOAD SYSTEM ---
    // ==========================================
    static inline const std::string FILENAME = "settings.cfg";

    static inline void save() {
        std::ofstream file(FILENAME);
        if (file.is_open()) {
            // Video
            file << "windowWidth=" << windowWidth << "\n";
            file << "windowHeight=" << windowHeight << "\n";
            file << "isFullscreen=" << (isFullscreen ? 1 : 0) << "\n";
            file << "targetFPS=" << targetFPS << "\n";
            // Match & Audio
            file << "matchLengthMinutes=" << matchLengthMinutes << "\n";
            file << "volumeSFX=" << volumeSFX << "\n";
            file << "volumeCrowd=" << volumeCrowd << "\n";
            // Camera
            file << "cameraZoom=" << cameraZoom << "\n";
            file << "cameraBallFollow=" << cameraBallFollow << "\n";
            file << "cameraAimPull=" << cameraAimPull << "\n";
            file.close();
            std::cout << "Settings saved successfully to " << FILENAME << "\n";
        }
    }

    static inline void load() {
        std::ifstream file(FILENAME);
        if (!file.is_open()) {
            std::cout << "No settings file found. Using defaults.\n";
            return;
        }

        std::string line;
        while (std::getline(file, line)) {
            std::istringstream is_line(line);
            std::string key;
            if (std::getline(is_line, key, '=')) {
                std::string value;
                if (std::getline(is_line, value)) {
                    try {
                        if (key == "windowWidth") windowWidth = std::stoi(value);
                        else if (key == "windowHeight") windowHeight = std::stoi(value);
                        else if (key == "isFullscreen") isFullscreen = (std::stoi(value) == 1);
                        else if (key == "targetFPS") targetFPS = std::stoi(value);

                        else if (key == "matchLengthMinutes") matchLengthMinutes = std::stoi(value);
                        else if (key == "volumeSFX") volumeSFX = std::stof(value);
                        else if (key == "volumeCrowd") volumeCrowd = std::stof(value);

                        else if (key == "cameraZoom") cameraZoom = std::stof(value);
                        else if (key == "cameraBallFollow") cameraBallFollow = std::stof(value);
                        else if (key == "cameraAimPull") cameraAimPull = std::stof(value);
                    }
                    catch (const std::exception& e) {
                        std::cerr << "Settings Parse Error on key [" << key << "]: " << e.what() << "\n";
                    }
                }
            }
        }
        std::cout << "Settings loaded successfully from " << FILENAME << "\n";
    }
};