#pragma once

enum class PositionRole {
    Goalkeeper,
    LeftBack,
    CenterBack,
    RightBack,
    LeftWingBack,
    RightWingBack,
    DefensiveMid,
    CenterMid,
    LeftMid,
    RightMid,
    AttackingMid,
    LeftWing,
    RightWing,
    CenterForward,
    Striker
};

PositionRole stringToRole(const std::string& str);
std::string roleToString(PositionRole role);