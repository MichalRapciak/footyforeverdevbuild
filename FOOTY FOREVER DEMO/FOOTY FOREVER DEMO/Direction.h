#pragma once
#include <SFML/Graphics.hpp>
#include <vector>

enum class Direction {
    Down,
    Up,
    DownLeft,
    Left,
    UpLeft,
    DownRight,
    Right,
    UpRight
};

// This just holds the math for the cutouts, not the heavy image!
struct Animation {
    std::vector<sf::IntRect> frames;
    int loopStartIndex = 0; // <-- NEW: Tells the animator where to loop back to!
};