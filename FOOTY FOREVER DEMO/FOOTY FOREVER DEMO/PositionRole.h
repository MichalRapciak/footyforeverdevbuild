#pragma once

enum class PositionRole {
    Goalkeeper,
    LeftBack,
    LCenterBack,
    RCenterBack,
    RightBack,
    DefensiveMid,
    CenterMid,
    AttackingMid,
    LeftWing,
    RightWing,
    Striker
};

PositionRole stringToRole(const std::string& str);
std::string roleToString(PositionRole role);