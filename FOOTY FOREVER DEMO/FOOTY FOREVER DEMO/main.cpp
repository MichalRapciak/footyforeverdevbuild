#ifdef _DEBUG 
#pragma comment(lib,"sfml-graphics-s-d.lib") 
#pragma comment(lib,"sfml-audio-s-d.lib") 
#pragma comment(lib,"sfml-system-s-d.lib") 
#pragma comment(lib,"sfml-window-s-d.lib") 
#pragma comment(lib,"sfml-network-s-d.lib") 

// --- SFML Audio Dependencies (Debug) ---
#pragma comment(lib,"FLACd.lib")
#pragma comment(lib,"vorbisencd.lib")
#pragma comment(lib,"vorbisfiled.lib")
#pragma comment(lib,"vorbisd.lib")
#pragma comment(lib,"oggd.lib")

#else 
#pragma comment(lib,"sfml-graphics-s.lib") 
#pragma comment(lib,"sfml-audio-s.lib") 
#pragma comment(lib,"sfml-system-s.lib")
#pragma comment(lib,"sfml-window-s.lib") 
#pragma comment(lib,"sfml-network-s.lib") 

// --- SFML Audio Dependencies (Release) ---
#pragma comment(lib,"FLAC.lib")
#pragma comment(lib,"vorbisenc.lib")
#pragma comment(lib,"vorbisfile.lib")
#pragma comment(lib,"vorbis.lib")
#pragma comment(lib,"ogg.lib")

#endif

#include "Game.h"

int main() // Entry Point to "Game", if 1 = success
{
	Game game;
	game.run();

	return 1;
}