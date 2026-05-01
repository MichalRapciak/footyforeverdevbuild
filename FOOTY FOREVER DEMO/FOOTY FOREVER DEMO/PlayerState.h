#pragma once

enum class PlayerState {
	Normal,
	Tackling,
	Stunned, // For when they miss a tackle or get hit
	Diving,
	Stumbled,
	Jumping,
	FallOver,
	Injured,
	Kicking
};