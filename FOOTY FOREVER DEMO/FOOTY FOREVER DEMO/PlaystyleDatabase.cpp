#include "PlaystyleDatabase.h"

Playstyle PlaystyleDatabase::getPlaystyle(PlaystyleType type) {
    Playstyle ps;
    ps.type = type;

    switch (type) {
        // ==========================================
        // --- GOALKEEPERS ---
        // ==========================================
    case PlaystyleType::SweeperKeeper:
        // Pushes high up the pitch to intercept through-balls
        ps.zoneMod = { 800.f, 200.f, 1200.f, 0.9f, 400.f, 0.4f, 0.0f, 1.0f, 600.f };
        // Looks to start quick counter-attacks, willing to dribble out of the box if pressured
        ps.behavior = { 0.4f, 0.8f, 0.0f, 0.0f, 0.8f, 0.6f };
        break;

    case PlaystyleType::OnTheLine:
        // Glued to the goal line, rarely steps out
        ps.zoneMod = { 150.f, 50.f, 400.f, 0.2f, 200.f, 0.0f, 0.0f, -1.0f, 100.f };
        // Plays it extremely safe. Boots it long if pressured.
        ps.behavior = { 0.0f, 0.1f, 0.0f, 0.0f, 0.1f, 0.1f };
        break;

    case PlaystyleType::Distributor:
        // Standard positioning
        ps.zoneMod = { 350.f, 200.f, 800.f, 0.75f, 400.f, 0.1f, 0.0f, 0.0f, 250.f };
        // Very high pass risk. Looks for line-breaking throws and ground passes to the DM.
        ps.behavior = { 0.1f, 0.9f, 0.0f, 0.0f, 0.2f, 0.2f };
        break;


        // ==========================================
        // --- CENTER BACKS ---
        // ==========================================
    case PlaystyleType::Sweeper:
        // Drops slightly deeper than the rest of the defensive line to clean up messes
        ps.zoneMod = { 3000.f, 2000.f, 1200.f, 0.2f, 1500.f, 0.1f, 0.0f, -0.5f, 150.f };
        // Rarely slides in, stays on feet to jockey. Safe passing.
        ps.behavior = { 0.1f, 0.3f, 0.1f, 0.0f, 0.1f, 0.1f };
        break;

    case PlaystyleType::TheWall:
        // Standard rigid block positioning
        ps.zoneMod = { 3500.f, 1500.f, 800.f, 0.3f, 1500.f, 0.0f, 0.0f, 0.0f, 250.f };
        // Highly aggressive tackling, takes no risks on the ball (clearances only)
        ps.behavior = { 0.0f, 0.1f, 0.2f, 0.0f, 0.1f, 0.8f };
        break;

    case PlaystyleType::TheKiller:
        // Steps out of the defensive line aggressively to press the ball carrier
        ps.zoneMod = { 4500.f, 1500.f, 1000.f, 0.6f, 2000.f, 0.3f, 0.0f, 0.5f, 800.f };
        // Will slide tackle instantly. 
        ps.behavior = { 0.2f, 0.4f, 0.1f, 0.0f, 0.4f, 1.0f };
        break;

    case PlaystyleType::CalmAndCollected:
        // A Ball-Playing Defender. Holds shape perfectly.
        ps.zoneMod = { 3500.f, 1500.f, 800.f, 0.2f, 1200.f, 0.0f, 0.0f, 0.0f, 200.f };
        // Willing to dribble past a pressing striker, extremely high pass risk (long diagonals)
        ps.behavior = { 0.6f, 0.85f, 0.1f, 0.1f, 0.2f, 0.2f };
        break;


        // ==========================================
        // --- FULLBACKS & WINGBACKS ---
        // ==========================================
    case PlaystyleType::DefensiveFB:
        // Acts almost like a 3rd Centerback. Narrow width preference.
        ps.zoneMod = { 3000.f, 1500.f, 800.f, 0.3f, 1200.f, 0.0f, -0.5f, -0.5f, 300.f };
        // Never runs forward. Safe passes back to the CBs.
        ps.behavior = { 0.2f, 0.2f, 0.1f, 0.2f, 0.05f, 0.6f };
        break;

    case PlaystyleType::UpAndDown:
        // The Engine. Massive forward and backward leashes to cover the whole flank.
        ps.zoneMod = { 6000.f, 1500.f, 1000.f, 0.4f, 1200.f, 0.2f, 1.0f, 0.5f, 400.f };
        // Constantly making overlapping runs. Balanced passing and crossing.
        ps.behavior = { 0.5f, 0.5f, 0.3f, 0.6f, 0.9f, 0.5f };
        break;

    case PlaystyleType::TheRoamerFB:
        // The "Inverted" Fullback. Extremely high roaming, negative width (drifts into midfield).
        ps.zoneMod = { 5000.f, 1500.f, 2500.f, 0.6f, 1000.f, 0.8f, -0.8f, 0.5f, 500.f };
        // Plays like a midfielder. High dribbling, extreme pass risk (playmaker), low crossing.
        ps.behavior = { 0.7f, 0.9f, 0.5f, 0.2f, 0.6f, 0.4f };
        break;

    case PlaystyleType::TheCrosser:
        // Hugs the touchline absolutely (width = 1.0). Pushes extremely high.
        ps.zoneMod = { 7000.f, 1000.f, 500.f, 0.5f, 1000.f, 0.0f, 1.0f, 1.0f, 300.f };
        // Will almost never cut inside to shoot. Exclusively looks to whip balls into the box.
        ps.behavior = { 0.4f, 0.6f, 0.1f, 1.0f, 0.8f, 0.4f };
        break;
    // ==========================================
    // --- DEFENSIVE MIDFIELDERS ---
    // ==========================================
    case PlaystyleType::OrchestratorDM:
        // Deep-Lying Playmaker (Pirlo/Jorginho). Stays central, drops deep.
        ps.zoneMod = { 3000.f, 1500.f, 1000.f, 0.5f, 600.f, 0.2f, 0.0f, -0.6f, 250.f };
        // Rarely dribbles, immense pass risk (Hollywood passes), low tackling aggression.
        ps.behavior = { 0.2f, 0.9f, 0.1f, 0.1f, 0.2f, 0.3f };
        break;

    case PlaystyleType::TheKillerDM:
        // The Destroyer (Casemiro/Gattuso). Hunts the ball carrier aggressively.
        ps.zoneMod = { 4000.f, 1200.f, 1800.f, 0.8f, 1200.f, 0.4f, 0.0f, 0.0f, 800.f };
        // High tackle aggression, safe passing when possession is won.
        ps.behavior = { 0.2f, 0.3f, 0.1f, 0.0f, 0.4f, 0.95f };
        break;

    case PlaystyleType::ThreeLungDM:
        // Box-to-Box DM. Massive vertical engine.
        ps.zoneMod = { 5000.f, 2000.f, 1500.f, 0.6f, 1000.f, 0.4f, 0.0f, 0.3f, 500.f };
        // Runs constantly, balanced stats everywhere else.
        ps.behavior = { 0.4f, 0.5f, 0.3f, 0.2f, 0.9f, 0.7f };
        break;

    case PlaystyleType::DefensiveRoamer:
        // Fluid ball-winner (Kante). High freedom to leave formation to win the ball.
        ps.zoneMod = { 4000.f, 1500.f, 2500.f, 0.8f, 1500.f, 0.8f, 0.0f, 0.1f, 600.f };
        ps.behavior = { 0.5f, 0.5f, 0.2f, 0.1f, 0.7f, 0.8f };
        break;

    case PlaystyleType::BacklineBrawler:
        // Acts as a 3rd Centerback sitting right in front of the defensive line.
        ps.zoneMod = { 2000.f, 1000.f, 800.f, 0.3f, 1000.f, 0.0f, 0.0f, -0.8f, 300.f };
        ps.behavior = { 0.1f, 0.2f, 0.0f, 0.0f, 0.1f, 0.9f };
        break;


        // ==========================================
        // --- CENTRAL MIDFIELDERS ---
        // ==========================================
    case PlaystyleType::OrchestratorCM:
        // Dictates tempo (Kroos). Negative support depth means they show for the ball.
        ps.zoneMod = { 4000.f, 2000.f, 1500.f, 0.6f, 600.f, 0.3f, 0.0f, -0.4f, 300.f };
        ps.behavior = { 0.3f, 0.85f, 0.3f, 0.3f, 0.3f, 0.3f };
        break;

    case PlaystyleType::BoxToBox:
        // The Engine (Valverde/Gerrard). High runs, arrives late in the box for shots.
        ps.zoneMod = { 6000.f, 3000.f, 1500.f, 0.5f, 800.f, 0.4f, 0.0f, 0.5f, 400.f };
        ps.behavior = { 0.5f, 0.6f, 0.7f, 0.4f, 0.95f, 0.6f };
        break;

    case PlaystyleType::PlaymakerCM:
        // Advanced 8 (De Bruyne). High pass risk, hangs around the edge of the box.
        ps.zoneMod = { 5000.f, 1500.f, 2000.f, 0.7f, 500.f, 0.6f, 0.0f, 0.3f, 250.f };
        ps.behavior = { 0.6f, 0.95f, 0.5f, 0.6f, 0.6f, 0.2f };
        break;

    case PlaystyleType::ThreeLungCM:
        // Pure stamina, heavy pressing, lower technical output.
        ps.zoneMod = { 5500.f, 2500.f, 2500.f, 0.7f, 1000.f, 0.5f, 0.0f, 0.2f, 600.f };
        ps.behavior = { 0.4f, 0.4f, 0.2f, 0.2f, 0.9f, 0.8f };
        break;

    case PlaystyleType::QuickPasser:
        // Tiki-Taka specialist (Xavi/Pedri). Very low pass risk, very low dribbling.
        ps.zoneMod = { 4000.f, 2000.f, 1500.f, 0.8f, 500.f, 0.5f, 0.0f, -0.2f, 300.f };
        // Pops the ball off instantly (low dribble bias, low risk, high support frequency).
        ps.behavior = { 0.1f, 0.3f, 0.2f, 0.1f, 0.7f, 0.3f };
        break;

    case PlaystyleType::RoamerCM:
        // Given absolute freedom to find pockets of space (Bellingham).
        ps.zoneMod = { 5500.f, 2000.f, 2500.f, 0.8f, 600.f, 0.9f, 0.0f, 0.4f, 400.f };
        ps.behavior = { 0.7f, 0.6f, 0.6f, 0.3f, 0.8f, 0.5f };
        break;


        // ==========================================
        // --- ATTACKING MIDFIELDERS ---
        // ==========================================
    case PlaystyleType::PlaymakerAM:
        // Classic #10 (Ozil). Finds the gap between Midfield and Defense.
        ps.zoneMod = { 4500.f, 1000.f, 2500.f, 0.8f, 400.f, 0.7f, 0.0f, 0.2f, 200.f };
        // Pure assists. Very low shoot bias.
        ps.behavior = { 0.6f, 0.9f, 0.2f, 0.4f, 0.5f, 0.2f };
        break;

    case PlaystyleType::HardcorePress:
        // Leads the line defensively (Mount/Muller). High pressing trigger!
        ps.zoneMod = { 5000.f, 1500.f, 2000.f, 0.8f, 600.f, 0.5f, 0.0f, 0.6f, 800.f };
        ps.behavior = { 0.4f, 0.5f, 0.5f, 0.3f, 0.8f, 0.7f };
        break;

    case PlaystyleType::TricksterAM:
        // Flair player (Ronaldinho/Musiala). Ball to feet, isolated take-ons.
        ps.zoneMod = { 4000.f, 1000.f, 2000.f, 0.9f, 300.f, 0.8f, 0.0f, 0.3f, 200.f };
        // Max dribble bias. They want to beat a man before passing.
        ps.behavior = { 0.95f, 0.5f, 0.6f, 0.2f, 0.6f, 0.1f };
        break;

    case PlaystyleType::FinisherAM:
        // Shadow Striker (Lampard/Dele). Makes extreme forward runs into the box.
        ps.zoneMod = { 6000.f, 1000.f, 1500.f, 0.5f, 400.f, 0.4f, 0.0f, 0.8f, 250.f };
        ps.behavior = { 0.4f, 0.4f, 0.9f, 0.1f, 0.95f, 0.3f };
        break;
        // ==========================================
        // --- WIDE MIDFIELDERS (LM / RM) ---
        // ==========================================
        // Distinct from pure Wingers (LW/RW) because they have heavy defensive responsibilities
        // and start deeper on the pitch.

    case PlaystyleType::ClassicWideMid:
        // Traditional LM/RM (Beckham/Giggs). Holds the width, crosses from deep.
        // Notice the massive 3500.f backwardLeash compared to a Winger's 2000.f!
        ps.zoneMod = { 5000.f, 3500.f, 800.f, 0.5f, 800.f, 0.1f, 1.0f, 0.0f, 400.f };
        // High cross bias, balanced work rate.
        ps.behavior = { 0.4f, 0.7f, 0.2f, 0.9f, 0.6f, 0.5f };
        break;

    case PlaystyleType::DefensiveWinger:
        // A wide workhorse (Park Ji-Sung / Milner). Exists to protect the fullback.
        // Extreme backward leash, presses aggressively on the flank.
        ps.zoneMod = { 4500.f, 4500.f, 1000.f, 0.6f, 1000.f, 0.2f, 0.8f, -0.2f, 600.f };
        // High tackling aggression, high run frequency (stamina drainer).
        ps.behavior = { 0.3f, 0.4f, 0.2f, 0.6f, 0.8f, 0.8f };
        break;

    case PlaystyleType::InvertedWideMid:
        // Starts wide but drifts centrally to help win the midfield battle.
        ps.zoneMod = { 4500.f, 3000.f, 2000.f, 0.6f, 800.f, 0.5f, -0.6f, 0.0f, 400.f };
        ps.behavior = { 0.6f, 0.7f, 0.4f, 0.3f, 0.5f, 0.4f };
        break;


        // ==========================================
        // --- WINGERS ---
        // ==========================================
    case PlaystyleType::WideWinger:
        // Classic Winger (Navas). Glued to the touchline (Width = 1.0).
        ps.zoneMod = { 6000.f, 2000.f, 500.f, 0.4f, 400.f, 0.1f, 1.0f, 0.5f, 250.f };
        // Never cuts inside to shoot, solely wants to sprint and cross.
        ps.behavior = { 0.6f, 0.3f, 0.1f, 0.95f, 0.7f, 0.2f };
        break;

    case PlaystyleType::FalseWinger:
        // Inside Forward (Salah/Robben). Width is negative (cuts into the half-space)!
        ps.zoneMod = { 6000.f, 1000.f, 2000.f, 0.6f, 400.f, 0.6f, -0.8f, 0.6f, 300.f };
        // Highly selfish, wants to shoot, rarely crosses.
        ps.behavior = { 0.8f, 0.6f, 0.85f, 0.2f, 0.8f, 0.2f };
        break;

    case PlaystyleType::RoamerWinger:
        // Floats across the entire front line (Vinicius Jr).
        ps.zoneMod = { 5000.f, 1500.f, 3500.f, 0.7f, 400.f, 0.9f, -0.4f, 0.4f, 300.f };
        ps.behavior = { 0.8f, 0.7f, 0.6f, 0.4f, 0.7f, 0.3f };
        break;

        // ==========================================
        // --- CENTER FORWARDS / SECOND STRIKERS ---
        // ==========================================
        // Distinct from pure Strikers (9s) or pure Attacking Mids (10s).
        // They operate specifically in the "hole" between the defensive and midfield lines.

    case PlaystyleType::SecondStriker:
        // Links the midfield and the main striker (Griezmann/Dybala). 
        // Support depth is negative (-0.6) to drop into pockets, but has high roaming freedom.
        ps.zoneMod = { 5000.f, 1500.f, 2000.f, 0.7f, 300.f, 0.8f, 0.0f, -0.6f, 300.f };
        // High dribbling, high passing risk, balanced shooting.
        ps.behavior = { 0.7f, 0.8f, 0.6f, 0.2f, 0.7f, 0.4f };
        break;

    case PlaystyleType::ShadowStriker:
        // Starts deep but makes aggressive, late runs past the main Target Man (Thomas Muller).
        // Support depth is positive (+0.5) to push against the defensive line when attacking.
        ps.zoneMod = { 6000.f, 1000.f, 2500.f, 0.6f, 400.f, 0.9f, 0.0f, 0.5f, 400.f };
        // Extreme run frequency and shooting bias. They are goal-scorers, not playmakers.
        ps.behavior = { 0.4f, 0.5f, 0.85f, 0.1f, 0.95f, 0.5f };
        break;

        // ==========================================
        // --- STRIKERS ---
        // ==========================================
    case PlaystyleType::Finisher:
        // The Poacher (Inzaghi/Haaland). Plays on the shoulder of the last defender (Support = 1.0).
        ps.zoneMod = { 7000.f, 500.f, 1000.f, 0.3f, 200.f, 0.1f, 0.0f, 1.0f, 200.f };
        // The ultimate selfish behavior. If he has the ball, he shoots.
        ps.behavior = { 0.2f, 0.1f, 1.0f, 0.0f, 0.6f, 0.1f };
        break;

    case PlaystyleType::TheTarget:
        // Target Man (Giroud). Drops slightly to receive the ball with his back to goal.
        ps.zoneMod = { 5000.f, 1000.f, 1000.f, 0.6f, 200.f, 0.1f, 0.0f, -0.2f, 250.f };
        // Hold up play. Low dribbling, looks to pass off to the wingers.
        ps.behavior = { 0.1f, 0.7f, 0.7f, 0.0f, 0.3f, 0.2f };
        break;

    case PlaystyleType::False9:
        // The Maestro (Messi/Firmino). Drops completely into the AM slot (Support = -0.8).
        ps.zoneMod = { 4000.f, 2000.f, 2000.f, 0.8f, 300.f, 0.8f, 0.0f, -0.8f, 400.f };
        // Plays exactly like a #10 playmaker, but starts as a Striker.
        ps.behavior = { 0.7f, 0.9f, 0.4f, 0.1f, 0.6f, 0.3f };
        break;

    default:
        // A generic failsafe if a type isn't matched
        ps.zoneMod = { 1000.f, 1000.f, 1000.f, 0.5f, 800.f, 0.0f, 0.0f, 0.0f, 250.f };
        ps.behavior = { 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f };
        break;
    }

    return ps;
}