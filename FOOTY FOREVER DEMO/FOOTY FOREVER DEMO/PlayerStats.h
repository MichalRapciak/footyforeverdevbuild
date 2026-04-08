#pragma once
#include "PositionRole.h"

struct PlayerStats
{
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

	/// SHOOTING STATS
	float finishing = 0.0f;
	float heading = 0.0f;
	float getFinishing() const
	{
		return finishing;
	}
	float getHeading() const
	{
		return heading;
	}

	/// DRIBBLING STATS
	float ballControl = 0.f; // BALL CONTROL
	float balancing = 0.f; // BALANCING
	float getBallControl() const
	{
		return ballControl;
	}
	float getBalancing() const
	{
		return balancing;
	}

	// TECHNIQUE STATS
	float curl = 0.f; // CURL
	float deadBall = 0.0f;
	float getCurl() const
	{
		return curl;
	}
	float getDeadBall() const
	{
		return deadBall;
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
	float kickPower = 0.f;
	float jumpingStrength = 0.f;
	float getBodyStrength() const
	{
		return bodyStrength;
	}
	float getKickPower() const
	{
		return kickPower;
	}
	float getJumpingStrength() const
	{
		return jumpingStrength;
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

		case PositionRole::LCenterBack:
		case PositionRole::RCenterBack:
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
