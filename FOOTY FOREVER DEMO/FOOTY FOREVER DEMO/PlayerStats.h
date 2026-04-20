#pragma once
#include "PositionRole.h"
#include <cmath>

struct PlayerStats
{

	float overallRating = 0.0f;


	float naturalFitness = 0.0f;
	float getFitness() const
	{
		return naturalFitness;
	}
	int weakFootAccuracy = 0;
	int getWeakFootAccuracy() const
	{
		return weakFootAccuracy;
	}
	int injuryResistance = 0;
	int getInjuryResistance() const
	{
		return injuryResistance;
	}

	/// SHOOTING STATS
	float finishing = 0.0f;
	float heading = 0.0f;
	float kickPower = 0.f;
	float getFinishing() const
	{
		return finishing;
	}
	float getHeading() const
	{
		return heading;
	}
	float getKickPower() const
	{
		return kickPower;
	}

	/// PASSING STATS
	float shortPassing = 0.0f;
	float longPassing = 0.0f;
	float getShortPassing() const
	{
		return shortPassing;
	}
	float getLongPassing() const
	{
		return longPassing;
	}

	// TECHNIQUE STATS
	float deadBall = 0.0f;
	float curl = 0.f; // CURL
	float ballControl = 0.f; // BALL CONTROL
	float getDeadBall() const
	{
		return deadBall;
	}
	float getCurl() const
	{
		return curl;
	}
	float getBallControl() const
	{
		return ballControl;
	}


	///  SPEED STATS
	float topSpeed = 0.f;
	float acceleration = 0.f;
	float agility = 0.f;
	float getTopSpeed() const
	{
		return topSpeed;
	}
	float getAccel() const
	{
		return acceleration;
	}
	float getAgility() const
	{
		return agility;
	}

	/// PHYSICAL STATS
	float bodyStrength = 0.0f;
	float jumpingStrength = 0.f;
	float balancing = 0.f; // BALANCING
	float getBodyStrength() const
	{
		return bodyStrength;
	}
	float getJumpingStrength() const
	{
		return jumpingStrength;
	}
	float getBalancing() const
	{
		return balancing;
	}

	/// MENTAL STATS
	float awareness = 0.0f;
	float aggression = 0.0f;
	float blocking = 0.0f;
	float getAwareness() const
	{
		return awareness;
	}
	float getAggression() const
	{
		return aggression;
	}
	float getBlocking() const
	{
		return blocking;
	}

	/// GOALKEEPING STATS
	float gkCoverage = 0.f; 	/// <summary>Ability to position well, cut off angles, and diving reach.</summary>
	float gkReactions = 0.f; 	/// <summary>Speed of responding to close-range shots, deflections, and penalties.</summary
	float gkCatching = 0.f; 	/// <summary>Ability to hold onto the ball securely to prevent rebounds.</summary>
	float gkThrowing = 0.f; 	/// <summary>Accuracy and distance of manual distribution to start counter-attacks.</summary>
	float gkAwareness = 0.f; 	/// <summary>Anticipation of crosses, through-balls, and general game reading.</summary>
	float gkBlocking = 0.f; 	/// <summary>Ability to make yourself big in 1v1s and stop shots with the body/feet.</summary>
	float getGkCoverage() const
	{
		return gkCoverage;
	}
	float getGkReactions() const
	{
		return gkReactions;
	}
	float getGkCatching() const
	{
		return gkCatching;
	}
	float getGkThrowing() const
	{
		return gkThrowing;
	}
	float getGkAwareness() const
	{
		return gkAwareness;
	}
	float getGkBlocking() const
	{
		return gkBlocking;
	}


	/// <summary>
	/// HEXAGON UI AVERAGES
	/// Calculates the mean average of each core stat group for radar charts.
	/// </summary>

	float getShootingAverage() const {
		return std::ceil((finishing + heading + kickPower) / 3.0f);
	}

	float getPassingAverage() const {
		return std::ceil((shortPassing + longPassing) / 2.0f);
	}

	float getTechniqueAverage() const {
		return std::ceil((deadBall + curl + ballControl) / 3.0f);
	}

	float getSpeedAverage() const {
		return std::ceil((topSpeed + acceleration + agility) / 3.0f);
	}

	float getPhysicalAverage() const {
		return std::ceil((jumpingStrength + bodyStrength + balancing) / 3.0f);
	}

	float getMentalAverage() const {
		return std::ceil((awareness + aggression + blocking) / 3.0f);
	}

	void calculateOverallRating(PositionRole role) {
		switch (role) {
		case PositionRole::Goalkeeper:
			overallRating = std::ceil(
				gkCoverage * 0.17f + gkReactions * 0.17f + gkCatching * 0.16f +
				gkThrowing * 0.08f + gkAwareness * 0.18f + gkBlocking * 0.16f +
				longPassing * 0.08
				- (std::max((70 - naturalFitness) * 0.15f, 0.0f))
			);
			break;

		case PositionRole::CenterBack:
			overallRating = std::ceil(
				naturalFitness * 0.02f +
				finishing * 0.01f + heading * 0.15f + kickPower * 0.01f +
				shortPassing * 0.05f + longPassing * 0.05f +
				deadBall * 0.01f + curl * 0.01f + ballControl * 0.04f +
				topSpeed * 0.03f + acceleration * 0.02f + agility * 0.02f +
				jumpingStrength * 0.10f + bodyStrength * 0.14f + balancing * 0.02f +
				awareness * 0.14f + aggression * 0.08f + blocking * 0.18f
				- (std::max((70 - naturalFitness) * 0.15f, 0.0f))
			);
			break;

		case PositionRole::LeftBack:
		case PositionRole::RightBack:
			overallRating = std::ceil(
				naturalFitness * 0.05f +
				finishing * 0.01f + heading * 0.03f + kickPower * 0.02f +
				shortPassing * 0.08f + longPassing * 0.12f +
				deadBall * 0.01f + curl * 0.01f + ballControl * 0.07f +
				topSpeed * 0.11f + acceleration * 0.09f + agility * 0.07f +
				jumpingStrength * 0.02f + bodyStrength * 0.05f + balancing * 0.08f +
				awareness * 0.08f + aggression * 0.06f + blocking * 0.11f
				- (std::max((70 - naturalFitness) * 0.2f, 0.0f))
			);
			break;

		case PositionRole::LeftWingBack:
		case PositionRole::RightWingBack:
			overallRating = std::ceil(
				naturalFitness * 0.06f +
				finishing * 0.02f + heading * 0.01f + kickPower * 0.01f +
				shortPassing * 0.08f + longPassing * 0.14f +
				deadBall * 0.01f + curl * 0.04f + ballControl * 0.09f +
				topSpeed * 0.12f + acceleration * 0.12f + agility * 0.08f +
				jumpingStrength * 0.01f + bodyStrength * 0.04f + balancing * 0.05f +
				awareness * 0.06f + aggression * 0.05f + blocking * 0.07f
				- (std::max((70 - naturalFitness) * 0.25f, 0.0f))
			);
			break;

		case PositionRole::DefensiveMid:
			overallRating = std::ceil(
				naturalFitness * 0.05f +
				finishing * 0.01f + heading * 0.05f + kickPower * 0.01f +
				shortPassing * 0.14f + longPassing * 0.11f +
				deadBall * 0.01f + curl * 0.01f + ballControl * 0.08f +
				topSpeed * 0.03f + acceleration * 0.02f + agility * 0.04f +
				jumpingStrength * 0.02f + bodyStrength * 0.09f + balancing * 0.06f +
				awareness * 0.12f + aggression * 0.08f + blocking * 0.12f
				- (std::max((70 - naturalFitness) * 0.15f, 0.0f))
			);
			break;

		case PositionRole::CenterMid:
			overallRating = std::ceil(
				naturalFitness * 0.08f +
				finishing * 0.02f + heading * 0.02f + kickPower * 0.03f +
				shortPassing * 0.16f + longPassing * 0.12f +
				deadBall * 0.02f + curl * 0.02f + ballControl * 0.14f +
				topSpeed * 0.04f + acceleration * 0.04f + agility * 0.06f +
				jumpingStrength * 0.01f + bodyStrength * 0.05f + balancing * 0.06f +
				awareness * 0.12f + aggression * 0.04f + blocking * 0.05f
				- (std::max((70 - naturalFitness) * 0.25f, 0.0f))
			);
			break;

		case PositionRole::LeftMid:
		case PositionRole::RightMid:
			overallRating = std::ceil(
				naturalFitness * 0.08f +
				finishing * 0.04f + heading * 0.01f + kickPower * 0.04f +
				shortPassing * 0.10f + longPassing * 0.12f +
				deadBall * 0.02f + curl * 0.05f + ballControl * 0.12f +
				topSpeed * 0.12f + acceleration * 0.12f + agility * 0.10f +
				jumpingStrength * 0.01f + bodyStrength * 0.03f + balancing * 0.06f +
				awareness * 0.08f + aggression * 0.01f + blocking * 0.02f
				- (std::max((70 - naturalFitness) * 0.2f, 0.0f))
			);
			break;

		case PositionRole::AttackingMid:
			overallRating = std::ceil(
				finishing * 0.05f + heading * 0.01f + kickPower * 0.02f +
				shortPassing * 0.10f + longPassing * 0.08f +
				deadBall * 0.04f + curl * 0.03f + ballControl * 0.14f +
				topSpeed * 0.05f + acceleration * 0.09f + agility * 0.12f +
				jumpingStrength * 0.01f + bodyStrength * 0.01f + balancing * 0.08f +
				awareness * 0.18f + aggression * 0.01f + blocking * 0.01f
				- (std::max((70 - naturalFitness) * 0.2f, 0.0f))
			);
			break;

		case PositionRole::LeftWing:
		case PositionRole::RightWing:
			overallRating = std::ceil(
				finishing * 0.12f + heading * 0.01f + kickPower * 0.02f +
				shortPassing * 0.06f + longPassing * 0.09f +
				deadBall * 0.02f + curl * 0.06f + ballControl * 0.12f +
				topSpeed * 0.14f + acceleration * 0.12f + agility * 0.10f +
				jumpingStrength * 0.01f + bodyStrength * 0.01f + balancing * 0.08f +
				awareness * 0.07f + aggression * 0.01f + blocking * 0.01f
				- (std::max((70 - naturalFitness) * 0.22f, 0.0f))
			);
			break;

		case PositionRole::CenterForward:
			overallRating = std::ceil(
				finishing * 0.14f + heading * 0.02f + kickPower * 0.04f +
				shortPassing * 0.07f + longPassing * 0.03f +
				deadBall * 0.03f + curl * 0.03f + ballControl * 0.14f +
				topSpeed * 0.08f + acceleration * 0.10f + agility * 0.09f +
				jumpingStrength * 0.02f + bodyStrength * 0.02f + balancing * 0.06f +
				awareness * 0.14f + aggression * 0.01f + blocking * 0.01f
				- (std::max((70 - naturalFitness) * 0.2f, 0.0f))
			);
			break;

		case PositionRole::Striker:
			overallRating = std::ceil(
				finishing * 0.17f + heading * 0.10f + kickPower * 0.08f +
				shortPassing * 0.02f + longPassing * 0.01f +
				deadBall * 0.04f + curl * 0.01f + ballControl * 0.08f +
				topSpeed * 0.06f + acceleration * 0.07f + agility * 0.04f +
				jumpingStrength * 0.06f + bodyStrength * 0.08f + balancing * 0.06f +
				awareness * 0.16f + aggression * 0.01f + blocking * 0.01f
				- (std::max((70 - naturalFitness) * 0.2f, 0.0f))
			);
			break;
		}
	}

	///DEFAULT CONSTRUCTOR
	static PlayerStats createFromRole(PositionRole role) {
		PlayerStats s; // Creates a baseline 0-overall player

		switch (role) {
		case PositionRole::Goalkeeper:
			s.finishing = 0.0f; s.heading = 20.0f; // SHOOTING
			s.ballControl = 40.0f; s.balancing = 24.0f; // DRIBBLING
			s.curl = 55.0f; s.deadBall = 15.0f; // TECHNIQUE
			s.shortPassing = 56.0f; s.longPassing = 68.0f; // PASSING
			s.topSpeed = 34.0f; s.acceleration = 46.0f; s.agility = 76.0f; // SPEED
			s.bodyStrength = 78.0f; s.kickPower = 84.0f; s.jumpingStrength = 88.0f; // PHYSICAL
			s.awareness = 37.0f; s.aggression = 23.0f; s.blocking = 28.0f; // MENTAL
			s.gkCoverage = 86.0f; s.gkReactions = 86.0f; s.gkCatching = 87.0f; s.gkThrowing = 76.0f; s.gkAwareness = 85.0f; s.gkBlocking = 87.0f; // GOALKEEPING
			break;

		case PositionRole::CenterBack:
			s.finishing = 30.0f; s.heading = 84.0f; // SHOOTING
			s.ballControl = 58.0f; s.balancing = 52.0f; // DRIBBLING
			s.curl = 35.0f; s.deadBall = 30.0f; // TECHNIQUE
			s.shortPassing = 68.0f; s.longPassing = 64.0f; // PASSING
			s.topSpeed = 66.0f; s.acceleration = 62.0f; s.agility = 55.0f; // SPEED
			s.bodyStrength = 88.0f; s.kickPower = 70.0f; s.jumpingStrength = 85.0f; // PHYSICAL
			s.awareness = 82.0f; s.aggression = 86.0f; s.blocking = 88.0f; // MENTAL
			break;

		case PositionRole::LeftBack:
		case PositionRole::RightBack:
			s.finishing = 52.0f; s.heading = 68.0f; // SHOOTING
			s.ballControl = 74.0f; s.balancing = 72.0f; // DRIBBLING
			s.curl = 70.0f; s.deadBall = 55.0f; // TECHNIQUE
			s.shortPassing = 76.0f; s.longPassing = 72.0f; // PASSING
			s.topSpeed = 84.0f; s.acceleration = 86.0f; s.agility = 80.0f; // SPEED
			s.bodyStrength = 72.0f; s.kickPower = 74.0f; s.jumpingStrength = 74.0f; // PHYSICAL
			s.awareness = 76.0f; s.aggression = 78.0f; s.blocking = 77.0f; // MENTAL
			break;

		case PositionRole::DefensiveMid:
			s.finishing = 58.0f; s.heading = 75.0f; // SHOOTING
			s.ballControl = 78.0f; s.balancing = 75.0f; // DRIBBLING
			s.curl = 65.0f; s.deadBall = 60.0f; // TECHNIQUE
			s.shortPassing = 84.0f; s.longPassing = 80.0f; // PASSING
			s.topSpeed = 72.0f; s.acceleration = 68.0f; s.agility = 66.0f; // SPEED
			s.bodyStrength = 84.0f; s.kickPower = 80.0f; s.jumpingStrength = 78.0f; // PHYSICAL
			s.awareness = 84.0f; s.aggression = 86.0f; s.blocking = 82.0f; // MENTAL
			break;

		case PositionRole::CenterMid:
			s.finishing = 70.0f; s.heading = 68.0f; // SHOOTING
			s.ballControl = 84.0f; s.balancing = 80.0f; // DRIBBLING
			s.curl = 76.0f; s.deadBall = 72.0f; // TECHNIQUE
			s.shortPassing = 86.0f; s.longPassing = 84.0f; // PASSING
			s.topSpeed = 74.0f; s.acceleration = 76.0f; s.agility = 78.0f; // SPEED
			s.bodyStrength = 74.0f; s.kickPower = 80.0f; s.jumpingStrength = 70.0f; // PHYSICAL
			s.awareness = 85.0f; s.aggression = 72.0f; s.blocking = 68.0f; // MENTAL
			break;

		case PositionRole::AttackingMid:
			s.finishing = 78.0f; s.heading = 62.0f; // SHOOTING
			s.ballControl = 88.0f; s.balancing = 85.0f; // DRIBBLING
			s.curl = 82.0f; s.deadBall = 80.0f; // TECHNIQUE
			s.shortPassing = 86.0f; s.longPassing = 78.0f; // PASSING
			s.topSpeed = 78.0f; s.acceleration = 82.0f; s.agility = 86.0f; // SPEED
			s.bodyStrength = 66.0f; s.kickPower = 78.0f; s.jumpingStrength = 62.0f; // PHYSICAL
			s.awareness = 86.0f; s.aggression = 55.0f; s.blocking = 48.0f; // MENTAL
			break;

		case PositionRole::LeftWing:
		case PositionRole::RightWing:
			s.finishing = 80.0f; s.heading = 58.0f; // SHOOTING
			s.ballControl = 86.0f; s.balancing = 88.0f; // DRIBBLING
			s.curl = 84.0f; s.deadBall = 74.0f; // TECHNIQUE
			s.shortPassing = 80.0f; s.longPassing = 72.0f; // PASSING
			s.topSpeed = 90.0f; s.acceleration = 93.0f; s.agility = 92.0f; // SPEED
			s.bodyStrength = 62.0f; s.kickPower = 76.0f; s.jumpingStrength = 65.0f; // PHYSICAL
			s.awareness = 80.0f; s.aggression = 50.0f; s.blocking = 42.0f; // MENTAL
			break;

		case PositionRole::Striker:
			s.finishing = 88.0f; s.heading = 84.0f; // SHOOTING
			s.ballControl = 82.0f; s.balancing = 78.0f; // DRIBBLING
			s.curl = 76.0f; s.deadBall = 68.0f; // TECHNIQUE
			s.shortPassing = 74.0f; s.longPassing = 58.0f; // PASSING
			s.topSpeed = 83.0f; s.acceleration = 86.0f; s.agility = 80.0f; // SPEED
			s.bodyStrength = 82.0f; s.kickPower = 86.0f; s.jumpingStrength = 84.0f; // PHYSICAL
			s.awareness = 86.0f; s.aggression = 60.0f; s.blocking = 45.0f; // MENTAL
			break;
		}
		return s;
	}


};
