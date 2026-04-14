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
    JogaBonito, WideWinger, FalseWinger, RoamerWinger, TricksterWinger,

    // Center Forward
    SecondStriker, ShadowStriker,

    // Strikers
    Finisher, TheTarget, TricksterStriker, False9, PlaymakerStriker
};

struct Playstyle {
    PlaystyleType type;
    /// <summary>
    ///  // forwardLeash, backwardLeash, lateralLeash, ballInfluence, markingRange, roamingFreedom, widthPreference, supportDepth, pressingTrigger
    /// </summary>
    TacticalZone zoneMod;
    /// <summary>
/// dribbleBias, passRiskBias, shootBias, crossBias, runFrequency, tackleAggression
/// </summary>
    PlayerBehavior behavior;
};