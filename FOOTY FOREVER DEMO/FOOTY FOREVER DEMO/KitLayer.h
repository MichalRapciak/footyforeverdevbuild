#pragma once
#include <SFML/Graphics/Color.hpp>
#include <string>

struct KitLayer {
    std::string textureId; // e.g., "shirt_base", "sleeve_long", "shorts_trim"
    sf::Color color;
};