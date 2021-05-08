#pragma once

// Enum Structure for sprite direction
typedef enum Direction {
	DIR_DOWN,
	DIR_LEFT,
	DIR_RIGHT,
	DIR_UP
} Direction;

// Enum Structure for sprite indexing
enum PlayerSprite {
	FACING_DOWN0,
	FACING_DOWN1,
	FACING_LEFT0,
	FACING_LEFT1,
	FACING_RIGHT0,
	FACING_RIGHT1,
	FACING_UP0,
	FACING_UP1,
	N_PLAYER_SPRITES
};

// 32 bit Pixel Information Structure
typedef struct Pixel32 {
	uint8_t blue;

	uint8_t green;

	uint8_t red;

	uint8_t alpha;
} Pixel32;

// GameBitMap Information Structure
typedef struct GameBitMap {
	BITMAPINFO bitmapInfo;
	void* memory;
} GameBitMap;

// Font Information Structure
typedef struct Font {
	size_t fontCharacters;
	GameBitMap bitmap;
} Font;

// CPU Information Structure
typedef struct CPUInfo {
	SYSTEM_INFO systemInfo;
	int64_t currentSystemTime;
	int64_t previousSystemTime;
	FILETIME processCreationTime;
	FILETIME processExitTime;
	int64_t currentUserCPUTime;
	int64_t currentKernelCPUTime;
	int64_t previousUserCPUTime;
	int64_t previousKernelCPUTime;
	double cpuPercent;
} CPUInfo;

// Performance Information Structure
typedef struct PerformanceInfo {
	uint64_t totalFramesRendered;
	MONITORINFO monitorInfo;
	float rawFPS;
	float cookedFPS;
	int64_t perfFrecuency;
	BOOL showPerfData;
	ULONG minimumTimerRes;
	ULONG currentTimerRes;
	ULONG maximumTimerRes;
	uint32_t handleCount;
	PROCESS_MEMORY_COUNTERS_EX  memoryInfo;
	CPUInfo cpuInfo;
} PerformanceInfo;


// NPC Information Structure
typedef struct NPC {
	char name[12];
	int32_t screenPosX;
	int32_t screenPosY;
	int32_t healthPoints;
	GameBitMap heartBar[7];
	int32_t manaPoints;
	int32_t strength;
	uint8_t currentSprite;
	Direction playerDirection;
	void(*script)(struct Scene*, struct NPC*, struct NPC*);
	GameBitMap sprite[N_PLAYER_SPRITES];
} NPC;

// Forward declaration of scene
struct Scene;

//GameObject Structure
typedef struct GObject {
	GameBitMap sprite;
	int screenPosX;
	int screenPosY;
	BOOL existHitbox;
	BOOL toDestroy;
	BOOL canBeDestroyed;
	int damage;
	Direction direction;
	void (*script)(struct Scene*, struct GObject*, struct NPC*);
}GObject;

// List Structure
typedef struct List {
	GObject* element;
	struct List* next;
}List;

// Scene Information Structure
typedef struct Scene {
	GameBitMap backBuffer;
	List* sceneObjects;
} Scene;


// List Structure Functions
DWORD _stdcall initList(_Out_ List** list);
DWORD _stdcall insertElement(_Inout_ List** list, _In_ GObject* element);
DWORD _stdcall destroyElement(_Inout_ List** list);
void _stdcall freeList(_In_ List** list);