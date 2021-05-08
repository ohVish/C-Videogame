#pragma once

#define SIMD

#define GAME_NAME	"GAME_IN_C"

#define GAME_WIDTH	384

#define GAME_HEIGHT	240

#define GAME_BPP	32

#define CALC_FPS_X_FRAMES 100

#define GAME_DRAWING_AREA_MEM_SIZE	( GAME_WIDTH * GAME_HEIGHT * (GAME_BPP / 2))

#define FRAMES_PER_ANIMATION 20

#define TARGET_MICROSECONDS_X_FRAME 16667

#define X_Y_TO_START_PIXEL(screenX, screenY, width, height) (((height * width) - width)\
		- (width * screenY) + screenX)

// Global variables
BOOL gGameIsRunning;
BOOL gWindowHasFocus;
HWND gWindowHandler;
Scene gScene;
PerformanceInfo gPerfomanceInfo;
GameBitMap gGrassBlock;
GameBitMap gUIBlocks[3];
Font g6x7font;
NPC gPlayer;
NPC gEnemy;
int gCount;

DWORD _stdcall createGameWindow(void);

LRESULT CALLBACK windowProc(HWND windowHandler, UINT message, WPARAM wParam, LPARAM lParam);

void _stdcall renderFrameGraphics(void);

void _stdcall renderSceneObjects(void);

void _stdcall renderBitmapOnScreen(_In_ int screenX, _In_ int screenY, _Inout_ GameBitMap* bitmap);

void _stdcall renderPixelString(_In_ char* string, _In_ Font* font, _In_ int screenX,
	_In_ int screenY, _In_ Pixel32* color);

void _stdcall perfomanceMeasures(_In_ int64_t frameStart, _In_ int64_t frameEnd, _Inout_ int64_t* elapsedMicroACCRaw, _Inout_ int64_t* elapsedMicroACCCook);

#ifdef SIMD
	void _stdcall clearScreen(_In_ __m128i* pixel);
	void _stdcall fillScreen(_In_ GameBitMap* blockTile);
#else
	void _stdcall clearScreen(_In_ Pixel32* pixel);
	void _stdcall fillScreen(_In_ GameBitMap* blockTile);
#endif