#ifdef _DEBUG 
#pragma comment(lib,"sfml-graphics-s-d.lib") 
#pragma comment(lib,"sfml-audio-s-d.lib") 
#pragma comment(lib,"sfml-system-s-d.lib") 
#pragma comment(lib,"sfml-window-s-d.lib") 
#pragma comment(lib,"sfml-network-s-d.lib") 
#else 
#pragma comment(lib,"sfml-graphics-s.lib") 
#pragma comment(lib,"sfml-audio-s.lib") 
#pragma comment(lib,"sfml-system-s.lib")
#pragma comment(lib,"sfml-window-s.lib") 
#pragma comment(lib,"sfml-network-s.lib") 
#endif 

#include "Game.h"

int main() // Entry Point to "Game", if 1 = success
{
	Game game;
	game.run();

	return 1;
}