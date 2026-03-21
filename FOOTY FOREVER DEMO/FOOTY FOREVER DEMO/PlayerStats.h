#pragma once

struct PlayerStats
{
	// CURL, TOP SPEED, ACCELERATION, AGILITY, KICK POWER, AWARENESS, AGGRESSION, BLOCKING, GK COVER, GK REACT, GK CATCH, GK THROW< GK AWARE, GK BLOCK

	/// SHOOTING STATS
	float m_finishing = 0.0f;
	float m_heading = 0.0f;
	float getFinishing() const
	{
		return m_finishing;
	}
	float getHeading() const
	{
		return m_heading;
	}

	/// DRIBBLING STATS
	float m_ballControl = 0.f; // BALL CONTROL
	float m_balancing = 0.f; // BALANCING
	float getBallControl() const
	{
		return m_ballControl;
	}
	float getBalancing() const
	{
		return m_balancing;
	}

	float m_curl = 0.f; // CURL
	float m_deadBall = 0.0f;
	float getCurl() const
	{
		return m_curl;
	}
	float getDeadBall() const
	{
		return m_deadBall;
	}

	/// PASSING STATS
	float m_shortPassing = 0.0f;
	float m_longPassing = 0.0f;
	float getShortPassing() const
	{
		return m_shortPassing;
	}
	float getLongPassing() const
	{
		return m_longPassing;
	}

	///  SPEED STATS
	float m_topSpeed = 0.f;
	float m_acceleration = 0.f;
	float m_agility = 0.f;
	float getTopSpeed() const
	{
		return m_topSpeed;
	}
	float getAccel() const
	{
		return m_acceleration;
	}
	float getAgility() const
	{
		return m_agility;
	}

	/// PHYSICAL STATS
	float m_bodyStrength = 0.0f;
	float m_kickPower = 0.f;
	float m_jumpingStrength = 0.f;
	float getBodyStrength() const
	{
		return m_bodyStrength;
	}
	float getKickPower() const
	{
		return m_kickPower;
	}
	float getJumpingStrength() const
	{
		return m_jumpingStrength;
	}

	/// MENTAL STATS
	float m_awareness = 0.0f;
	float m_aggression = 0.0f;
	float m_blocking = 0.0f;
	float getAwareness() const
	{
		return m_awareness;
	}
	float getAggression() const
	{
		return m_aggression;
	}
	float getBlocking() const
	{
		return m_blocking;
	}

	/// GOALKEEPING STATS
	float m_gkCoverage = 0.f; 	/// <summary>Ability to position well, cut off angles, and diving reach.</summary>
	float m_gkReactions = 0.f; 	/// <summary>Speed of responding to close-range shots, deflections, and penalties.</summary
	float m_gkCatching = 0.f; 	/// <summary>Ability to hold onto the ball securely to prevent rebounds.</summary>
	float m_gkThrowing = 0.f; 	/// <summary>Accuracy and distance of manual distribution to start counter-attacks.</summary>
	float m_gkAwareness = 0.f; 	/// <summary>Anticipation of crosses, through-balls, and general game reading.</summary>
	float m_gkBlocking = 0.f; 	/// <summary>Ability to make yourself big in 1v1s and stop shots with the body/feet.</summary>
	float getGkCoverage() const
	{
		return m_gkCoverage;
	}
	float getGkReactions() const
	{
		return m_gkReactions;
	}
	float getGkCatching() const
	{
		return m_gkCatching;
	}
	float getGkThrowing() const
	{
		return m_gkThrowing;
	}
	float getGkAwareness() const
	{
		return m_gkAwareness;
	}
	float getGkBlocking() const
	{
		return m_gkBlocking;
	}

};
