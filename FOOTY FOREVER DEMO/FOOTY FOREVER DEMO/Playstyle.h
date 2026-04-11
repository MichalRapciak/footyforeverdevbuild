#pragma once
#include "TacticalZone.h"
#include "PlayerBehavior.h"

enum class PlaystyleType {
    // Goalkeepers
    SweeperKeeper, OnTheLine, Distributor,

    // Centerbacks
    Sweeper, TheWall, TheKiller, CalmAndCollected,

    // Fullbacks / Wingbacks
    DefensiveFB, UpAndDown, TheRoamerFB, TheCrosser,

    // Defensive Mids
    OrchestratorDM, TheKillerDM, ThreeLungDM, DefensiveRoamer, BacklineBrawler,

    // Central Mids
    OrchestratorCM, BoxToBox, PlaymakerCM, ThreeLungCM, QuickPasser, RoamerCM,

    // Attacking Mids
    PlaymakerAM, OrchestratorAM, HardcorePress, QuickPasserAM, TricksterAM, RoamerAM, FinisherAM,

    // Wide Mids
    ClassicWideMid, DefensiveWinger, InvertedWideMid,

    // Wingers
    WideWinger, FalseWinger, RoamerWinger, TricksterWinger,

    // Center Forward
    SecondStriker, ShadowStriker,

    // Strikers
    Finisher, TheTarget, TricksterStriker, False9, PlaymakerStriker
};

struct Playstyle {
    PlaystyleType type;
    TacticalZone zoneMod;
    PlayerBehavior behavior;
};