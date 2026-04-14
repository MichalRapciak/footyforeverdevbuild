#pragma once
#include <string>
#include <vector>

// Define severity levels to dictate if a player is forced to sub off
enum class InjurySeverity {
    Knock,      // Can play on (maybe a slight speed nerf), 0-3 days
    Mild,       // Recommended sub, out for 1-3 weeks
    Severe      // Forced sub, out for months
};

struct InjuryType {
    std::string name;
    InjurySeverity severity;
    int minDays;
    int maxDays;
};

// A static database of your injuries
static const std::vector<InjuryType> InjuryDatabase = {
    {"Bruised Ankle", InjurySeverity::Knock, 1, 3},
    {"Bruised Knee", InjurySeverity::Knock, 1, 5},
    {"Bruised Ribs", InjurySeverity::Knock, 2, 5},
    {"Twisted Ankle", InjurySeverity::Mild, 10, 21},
    {"Hamstring Strain", InjurySeverity::Mild, 14, 30},
    {"Dislocated Shoulder", InjurySeverity::Severe, 45, 90},
    {"Broken Metatarsal", InjurySeverity::Severe, 60, 120},
    {"Torn ACL", InjurySeverity::Severe, 180, 270}
};