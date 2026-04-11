#pragma once
#include "Playstyle.h"

class PlaystyleDatabase {
public:
    static Playstyle getPlaystyle(PlaystyleType type);
};