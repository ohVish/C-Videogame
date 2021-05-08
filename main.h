#pragma once

#define ATTACK_SPEED 25

#define DRUNKEN_FLOOR 50

#define MAX_PATH 50

#define PLAYER_NAME "Dealox"

// Function type definition for a internal DLL Windows function
typedef LONG(NTAPI* _NtQueryTimerResolution) (OUT PULONG MinimumResolution, OUT PULONG MaximumResolution, OUT PULONG CurrentResolution);
_NtQueryTimerResolution NtQueryTimerResolution;

DWORD _stdcall loadModules(void);

DWORD _stdcall preInitGame(void);

void _stdcall generateLevel(void);
void _stdcall generatePath(_In_ int x, _In_ int y, _In_ int nCellsWidth, _In_ int nCellsHeight,
	_Inout_ BOOL occupiedMatrix[(GAME_WIDTH - 16 - 16) / 16][(GAME_HEIGHT - 16 - 16) / 16]);


DWORD _stdcall initPlayer(void);

DWORD _stdcall loadBitmapFromFile(_In_ char* filename, _Out_ GameBitMap* bitmap);

BOOL _stdcall isGameAlreadyRunning(void);

void _stdcall processPlayerInputs(void);

BOOL _stdcall checkObstaclesObject(_In_ GObject* object);

BOOL _stdcall checkCollision(_In_ int x, _In_ int y, _In_ int w, _In_ int h, _In_ int x2, _In_ int y2, _In_ int w2, _In_ int h2);

BOOL _stdcall checkObstacles(_In_ NPC * npc);
void _stdcall checkObstaclesDamage(_In_ NPC* npc);

void _stdcall processSceneEvents(void);

void _stdcall freeAllMemory(void);

void _stdcall checkIfDestroyGObject(_Inout_ Scene* scene, _Inout_ GObject* object, _Inout_ NPC* player);

// Script functions
void _stdcall FireballScript(_Inout_ Scene* scene, _Inout_ GObject* object, _Inout_ NPC* player);

void _stdcall EnemyScript(_Inout_ Scene* scene, _Inout_ NPC* enemy, _Inout_ NPC* player);