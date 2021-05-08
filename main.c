#include <stdio.h>
#include <windows.h>
#include <psapi.h>

#pragma warning(disable: 5045)
#pragma warning(disable: 4820)

#include <stdint.h>
#include <emmintrin.h>
#include "types.h"
#include "graphics.h"
#include "main.h"

#pragma comment(lib, "winmm.lib")

HANDLE gMutex;

int __fastcall WinMain(HINSTANCE instance, HINSTANCE previousInstance, PSTR cmdLine, int cmdShow)
{
    UNREFERENCED_PARAMETER(instance);
    UNREFERENCED_PARAMETER(previousInstance);
    UNREFERENCED_PARAMETER(cmdLine);
    UNREFERENCED_PARAMETER(cmdShow);

    srand(time(NULL));

    MSG message = { 0 };
    int64_t frameStart = 0, frameEnd = 0, elapsedMicroACCRaw = 0, elapsedMicroACCCook = 0;

    // Check if our game is already running
    if (isGameAlreadyRunning() == FALSE) {

        gGameIsRunning = TRUE;

        // Load external modules
        if (loadModules() != ERROR_SUCCESS) {
            MessageBoxA(NULL, "No se pudieron cargar los módulos dll del programa!",
                "Error!", MB_ICONEXCLAMATION | MB_OK);
            goto Exit;
        }

        // Create Game Window
        if (createGameWindow() != ERROR_SUCCESS) {
            MessageBoxA(NULL, "No se pudo crear la ventana principal del juego!",
                "Error!", MB_ICONEXCLAMATION | MB_OK);
            goto Exit;
        }
        
        // Pre-init main game structures
        if (preInitGame() != ERROR_SUCCESS) {
            MessageBoxA(NULL, "No se pudieron inicializar las estructuras principales del juego!",
                "Error!", MB_ICONEXCLAMATION | MB_OK);
            goto Exit;
        }

        // Init player structure
        if (initPlayer() != ERROR_SUCCESS) {
            MessageBoxA(NULL, "No se pudo inicializar el jugador!",
                "Error!", MB_ICONEXCLAMATION | MB_OK);
            goto Exit;
        }

        // Main Game loop
        while (gGameIsRunning == TRUE) {

            QueryPerformanceCounter((LARGE_INTEGER*)&frameStart);

            while (PeekMessageA(&message, gWindowHandler, 0, 0, PM_REMOVE)) {
                TranslateMessage(&message);
                DispatchMessageA(&message);
            }

            // Process Player Inputs
            processPlayerInputs();

            // Process GObject Scripts
            processSceneEvents();

            // Render Frame Graphics
            renderFrameGraphics();

            QueryPerformanceCounter((LARGE_INTEGER*)&frameEnd);

            perfomanceMeasures(frameStart, frameEnd, &elapsedMicroACCRaw, &elapsedMicroACCCook);



        }
    }
    else {
    MessageBox(NULL, TEXT("Una instancia del juego ya se encuentra en ejecución!"),
        NULL, MB_ICONERROR);
    }

Exit:
freeAllMemory();
return 0;
}

DWORD _stdcall loadModules(void) {
    HMODULE ntDllModuleHandle;
    DWORD result = ERROR_SUCCESS;

    if ((ntDllModuleHandle = GetModuleHandleA("ntdll.dll")) == NULL)
    {
        MessageBoxA(NULL, "No se puede cargar ntdll.dll!", "Error!", MB_ICONEXCLAMATION | MB_OK);
        result = GetLastError();
        goto Exit;
    }

    if ((NtQueryTimerResolution = (_NtQueryTimerResolution)GetProcAddress(ntDllModuleHandle, "NtQueryTimerResolution")) == NULL)
    {
        MessageBoxA(NULL, "No se puede encontrar la función NtQueryTimerResolution en ntdll.dll!", "Error!", MB_ICONEXCLAMATION | MB_OK);
        result = GetLastError();
        goto Exit;
    }

Exit:
    return result;
}


DWORD _stdcall preInitGame(void) {
    DWORD result = ERROR_SUCCESS;

    // Set Windows Timer Resolution to 1ms
    if (timeBeginPeriod(1) == TIMERR_NOCANDO) {
        MessageBox(NULL, TEXT("No se ha podido establecer la resolución del timer de Windows!"),
            NULL, MB_ICONERROR);
        result = GetLastError();
        goto Exit;
    }

    // Set current process priority class
    if (SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS) == 0)
    {
        MessageBoxA(NULL, "Fallo al establecer la prioridad del proceso!", "Error!", MB_ICONEXCLAMATION | MB_OK);
        result = GetLastError();
        goto Exit;
    }

    // Set current thread priority level
    if (SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST) == 0)
    {
        MessageBoxA(NULL, "Fallo al establecer la prioridad del hilo!", "Error!", MB_ICONEXCLAMATION | MB_OK);
        result = GetLastError();
        goto Exit;
    }

    // Get Windows Timer Resolution
    NtQueryTimerResolution(&gPerfomanceInfo.minimumTimerRes,
        &gPerfomanceInfo.maximumTimerRes, &gPerfomanceInfo.currentTimerRes);

    // Get System and Process Times
    GetSystemInfo(&gPerfomanceInfo.cpuInfo.systemInfo);

    GetSystemTimeAsFileTime((FILETIME*)&gPerfomanceInfo.cpuInfo.previousSystemTime);

    GetProcessTimes(GetCurrentProcess(),
        &gPerfomanceInfo.cpuInfo.processCreationTime,
        &gPerfomanceInfo.cpuInfo.processExitTime,
        (FILETIME*)&gPerfomanceInfo.cpuInfo.previousKernelCPUTime,
        (FILETIME*)&gPerfomanceInfo.cpuInfo.previousUserCPUTime);


    gPerfomanceInfo.monitorInfo.cbSize = sizeof(gPerfomanceInfo.monitorInfo);

    QueryPerformanceFrequency((LARGE_INTEGER*)&gPerfomanceInfo.perfFrecuency);

    // Create Drawing Surface
    gScene.backBuffer.bitmapInfo.bmiHeader.biSize = sizeof(gScene.backBuffer.bitmapInfo.bmiHeader);
    gScene.backBuffer.bitmapInfo.bmiHeader.biWidth = GAME_WIDTH;
    gScene.backBuffer.bitmapInfo.bmiHeader.biHeight = GAME_HEIGHT;
    gScene.backBuffer.bitmapInfo.bmiHeader.biBitCount = GAME_BPP;
    gScene.backBuffer.bitmapInfo.bmiHeader.biCompression = BI_RGB;
    gScene.backBuffer.bitmapInfo.bmiHeader.biPlanes = 1;


    gScene.backBuffer.memory = VirtualAlloc(NULL, GAME_DRAWING_AREA_MEM_SIZE, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);

    if (gScene.backBuffer.memory == NULL) {
        MessageBox(NULL, TEXT("No se ha podido reservar memoria para el canvas!"),
            NULL, MB_ICONERROR);
        result = GetLastError();
        goto Exit;
    }

    memset(gScene.backBuffer.memory, 0x24, GAME_DRAWING_AREA_MEM_SIZE);

    if((result = loadBitmapFromFile("C:\\Users\\ohvish\\Pictures\\grass.bmp", &gGrassBlock)) != ERROR_SUCCESS)
        goto Exit;

    // Init UI Tiles
    if ((result = loadBitmapFromFile("C:\\Users\\ohvish\\Pictures\\uil.bmp", &gUIBlocks[0])) != ERROR_SUCCESS)
        goto Exit;
    if ((result = loadBitmapFromFile("C:\\Users\\ohvish\\Pictures\\uic.bmp", &gUIBlocks[1])) != ERROR_SUCCESS)
        goto Exit;
    if ((result = loadBitmapFromFile("C:\\Users\\ohvish\\Pictures\\uir.bmp", &gUIBlocks[2])) != ERROR_SUCCESS)
        goto Exit;


    if ((result = loadBitmapFromFile("C:\\Users\\ohvish\\Pictures\\6x7Font.bmpx", &g6x7font.bitmap)) != ERROR_SUCCESS)
        goto Exit;
    g6x7font.fontCharacters = 98;
    

    initList(&gScene.sceneObjects);

    for (int i = 0; i < GAME_WIDTH ; i+=16) {
        GObject aux = { 0 };
        aux.screenPosX = i;
        aux.screenPosY = 0;
        if ((result = loadBitmapFromFile("C:\\Users\\ohvish\\Pictures\\mountain.bmp", &aux.sprite)) != ERROR_SUCCESS)
            goto Exit;
        aux.existHitbox = TRUE;
        aux.script = NULL;
        aux.canBeDestroyed = FALSE;
        aux.toDestroy = FALSE;
        insertElement(&gScene.sceneObjects, &aux);
        aux.screenPosY = GAME_HEIGHT - 16;
        insertElement(&gScene.sceneObjects, &aux);
    }
    
    for (int i = 16; i < GAME_HEIGHT; i += 16) {
        GObject aux = { 0 };
        aux.screenPosY = i;
        aux.screenPosX = 0;
        if ((result = loadBitmapFromFile("C:\\Users\\ohvish\\Pictures\\mountain.bmp", &aux.sprite)) != ERROR_SUCCESS)
            goto Exit;
        aux.existHitbox = TRUE;
        aux.script = NULL;
        aux.canBeDestroyed = FALSE;
        aux.toDestroy = FALSE;
        insertElement(&gScene.sceneObjects, &aux);
        aux.screenPosX = GAME_WIDTH - 16;
        insertElement(&gScene.sceneObjects, &aux);
    }

    generateLevel();

Exit:
    return result;
}

void _stdcall generatePath(_In_ int x, _In_ int y, _In_ int nCellsWidth, _In_ int nCellsHeight,
    _Inout_ BOOL occupiedMatrix[(GAME_WIDTH - 16 - 16) / 16][(GAME_HEIGHT - 16 - 16) / 16]) {
    int floorCount = 0;
    int previousCellX = x;
    int previousCellY = y;

    int auxCount = 0;
    while (floorCount < DRUNKEN_FLOOR) {
        int direction = rand() % 4;
        int cellX = previousCellX;
        int cellY = previousCellY;
        switch (direction)
        {
        case DIR_DOWN:
            cellY++;
            break;
        case DIR_LEFT:
            cellX--;
            break;
        case DIR_RIGHT:
            cellX++;
            break;
        case DIR_UP:
            cellY--;
            break;
        }

        if (cellY >= 0 && cellY < nCellsHeight \
            && cellX >= 0 && cellX < nCellsWidth \
            && occupiedMatrix[cellX][cellY] == FALSE) {

            occupiedMatrix[cellX][cellY] = TRUE;
            floorCount++;
            previousCellX = cellX;
            previousCellY = cellY;
            auxCount = 0;
        }
        else if (cellY >= 0 && cellY < nCellsHeight \
            && cellX >= 0 && cellX < nCellsWidth \
            && auxCount >= 10) {
            auxCount = 0;
            previousCellX = cellX;
            previousCellY = cellY;
        }
        else
            auxCount++;
    }

}

void _stdcall generateLevel(void) {
    BOOL occupiedMatrix[(GAME_WIDTH - 16 - 16) / 16][(GAME_HEIGHT - 16 - 16) / 16];

    int nCellsWidth = (GAME_WIDTH - 16 - 16) / 16;
    int nCellsHeight = (GAME_HEIGHT - 16 - 16) / 16;

    for (int i = 0; i < nCellsWidth; ++i)
        for (int j = 0; j < nCellsHeight; ++j)
            occupiedMatrix[i][j] = FALSE;
    
    int playerCellX = rand() % nCellsWidth;
    int playerCellY = rand() % nCellsHeight;
    occupiedMatrix[playerCellX][playerCellY] = TRUE;
    gPlayer.screenPosX = (playerCellX + 1) * 16;
    gPlayer.screenPosY = (playerCellY + 1) * 16;

    int actualPaths = 0, prob = 0;

    do {
        generatePath(playerCellX, playerCellY, nCellsWidth, nCellsHeight, occupiedMatrix);
        actualPaths++;
        prob = rand() % 100;
    } while (prob < 75 && actualPaths < MAX_PATH);


    int enemyCellX = rand() % nCellsWidth;
    int enemyCellY = rand() % nCellsHeight;
    while (occupiedMatrix[enemyCellX][enemyCellY] == FALSE || (enemyCellX == playerCellX\
        && enemyCellY == playerCellY)) {
        enemyCellX = rand() % nCellsWidth;
        enemyCellY = rand() % nCellsHeight;
    }

    gEnemy.screenPosX = (enemyCellX + 1) * 16;
    gEnemy.screenPosY = (enemyCellY + 1) * 16;

    // Create GObjects
    for (int i = 0; i < nCellsWidth; ++i)
        for (int j = 0; j < nCellsHeight; ++j) 
            if (occupiedMatrix[i][j] == FALSE) {
                GObject aux = { 0 };
                aux.screenPosY = (j + 1) * 16;
                aux.screenPosX = (i + 1) * 16;
                loadBitmapFromFile("C:\\Users\\ohvish\\Pictures\\mountain.bmp", &aux.sprite);
                aux.existHitbox = TRUE;
                aux.script = NULL;
                aux.canBeDestroyed = FALSE;
                aux.toDestroy = FALSE;
                insertElement(&gScene.sceneObjects, &aux);
            }
}

DWORD _stdcall initPlayer(void) {
    DWORD result = ERROR_SUCCESS;

    strcpy_s(gPlayer.name, 12, PLAYER_NAME);
    gPlayer.healthPoints = 6;
    gPlayer.playerDirection = DIR_DOWN;
    gPlayer.currentSprite = FACING_DOWN0;

    // Load Player Sprites
    result = loadBitmapFromFile("C:\\Users\\ohvish\\Pictures\\hero_facingdown0.bmp", &gPlayer.sprite[FACING_DOWN0]);
    if (result != ERROR_SUCCESS){
        MessageBoxA(NULL, "Load32BppBitmapFromFile falló!", "Error!", MB_ICONEXCLAMATION | MB_OK);
        goto Exit;
    }

    result = loadBitmapFromFile("C:\\Users\\ohvish\\Pictures\\hero_facingdown1.bmp", &gPlayer.sprite[FACING_DOWN1]);
    if (result != ERROR_SUCCESS) {
        MessageBoxA(NULL, "Load32BppBitmapFromFile falló!", "Error!", MB_ICONEXCLAMATION | MB_OK);
        goto Exit;
    }

    result = loadBitmapFromFile("C:\\Users\\ohvish\\Pictures\\hero_facingleft0.bmp", &gPlayer.sprite[FACING_LEFT0]);
    if (result != ERROR_SUCCESS) {
        MessageBoxA(NULL, "Load32BppBitmapFromFile falló!", "Error!", MB_ICONEXCLAMATION | MB_OK);
        goto Exit;
    }

    result = loadBitmapFromFile("C:\\Users\\ohvish\\Pictures\\hero_facingleft1.bmp", &gPlayer.sprite[FACING_LEFT1]);
    if (result != ERROR_SUCCESS) {
        MessageBoxA(NULL, "Load32BppBitmapFromFile falló!", "Error!", MB_ICONEXCLAMATION | MB_OK);
        goto Exit;
    }

    result = loadBitmapFromFile("C:\\Users\\ohvish\\Pictures\\hero_facingright0.bmp", &gPlayer.sprite[FACING_RIGHT0]);
    if (result != ERROR_SUCCESS) {
        MessageBoxA(NULL, "Load32BppBitmapFromFile falló!", "Error!", MB_ICONEXCLAMATION | MB_OK);
        goto Exit;
    }

    result = loadBitmapFromFile("C:\\Users\\ohvish\\Pictures\\hero_facingright1.bmp", &gPlayer.sprite[FACING_RIGHT1]);
    if (result != ERROR_SUCCESS) {
        MessageBoxA(NULL, "Load32BppBitmapFromFile falló!", "Error!", MB_ICONEXCLAMATION | MB_OK);
        goto Exit;
    }

    result = loadBitmapFromFile("C:\\Users\\ohvish\\Pictures\\hero_facingup0.bmp", &gPlayer.sprite[FACING_UP0]);
    if (result != ERROR_SUCCESS) {
        MessageBoxA(NULL, "Load32BppBitmapFromFile falló!", "Error!", MB_ICONEXCLAMATION | MB_OK);
        goto Exit;
    }

    result = loadBitmapFromFile("C:\\Users\\ohvish\\Pictures\\hero_facingup1.bmp", &gPlayer.sprite[FACING_UP1]);
    if (result != ERROR_SUCCESS) {
        MessageBoxA(NULL, "Load32BppBitmapFromFile falló!", "Error!", MB_ICONEXCLAMATION | MB_OK);
        goto Exit;
    }

    // Load Player Heartbar Sprites
    result = loadBitmapFromFile("C:\\Users\\ohvish\\Pictures\\heartbar0.bmp", &gPlayer.heartBar[6]);
    if (result != ERROR_SUCCESS) {
        MessageBoxA(NULL, "Load32BppBitmapFromFile falló!", "Error!", MB_ICONEXCLAMATION | MB_OK);
        goto Exit;
    }
    
    result = loadBitmapFromFile("C:\\Users\\ohvish\\Pictures\\heartbar1.bmp", &gPlayer.heartBar[5]);
    if (result != ERROR_SUCCESS) {
        MessageBoxA(NULL, "Load32BppBitmapFromFile falló!", "Error!", MB_ICONEXCLAMATION | MB_OK);
        goto Exit;
    }
    
    
    result = loadBitmapFromFile("C:\\Users\\ohvish\\Pictures\\heartbar2.bmp", &gPlayer.heartBar[4]);
    if (result != ERROR_SUCCESS) {
        MessageBoxA(NULL, "Load32BppBitmapFromFile falló!", "Error!", MB_ICONEXCLAMATION | MB_OK);
        goto Exit;
    }
  
    result = loadBitmapFromFile("C:\\Users\\ohvish\\Pictures\\heartbar3.bmp", &gPlayer.heartBar[3]);
    if (result != ERROR_SUCCESS) {
        MessageBoxA(NULL, "Load32BppBitmapFromFile falló!", "Error!", MB_ICONEXCLAMATION | MB_OK);
        goto Exit;
    }

    result = loadBitmapFromFile("C:\\Users\\ohvish\\Pictures\\heartbar4.bmp", &gPlayer.heartBar[2]);
    if (result != ERROR_SUCCESS) {
        MessageBoxA(NULL, "Load32BppBitmapFromFile falló!", "Error!", MB_ICONEXCLAMATION | MB_OK);
        goto Exit;
    }

    result = loadBitmapFromFile("C:\\Users\\ohvish\\Pictures\\heartbar5.bmp", &gPlayer.heartBar[1]);
    if (result != ERROR_SUCCESS) {
        MessageBoxA(NULL, "Load32BppBitmapFromFile falló!", "Error!", MB_ICONEXCLAMATION | MB_OK);
        goto Exit;
    }

    result = loadBitmapFromFile("C:\\Users\\ohvish\\Pictures\\heartbar6.bmp", &gPlayer.heartBar[0]);
    if (result != ERROR_SUCCESS) {
        MessageBoxA(NULL, "Load32BppBitmapFromFile falló!", "Error!", MB_ICONEXCLAMATION | MB_OK);
        goto Exit;
    }

    gEnemy.healthPoints = 6;
    gEnemy.playerDirection = DIR_DOWN;
    gEnemy.currentSprite = FACING_DOWN0;
    gEnemy.script = &EnemyScript;

    // Load Player Sprites
    result = loadBitmapFromFile("C:\\Users\\ohvish\\Pictures\\enemy_facedown0.bmp", &gEnemy.sprite[FACING_DOWN0]);
    if (result != ERROR_SUCCESS) {
        MessageBoxA(NULL, "Load32BppBitmapFromFile falló!", "Error!", MB_ICONEXCLAMATION | MB_OK);
        goto Exit;
    }

    result = loadBitmapFromFile("C:\\Users\\ohvish\\Pictures\\enemy_facedown1.bmp", &gEnemy.sprite[FACING_DOWN1]);
    if (result != ERROR_SUCCESS) {
        MessageBoxA(NULL, "Load32BppBitmapFromFile falló!", "Error!", MB_ICONEXCLAMATION | MB_OK);
        goto Exit;
    }

    result = loadBitmapFromFile("C:\\Users\\ohvish\\Pictures\\enemy_faceleft0.bmp", &gEnemy.sprite[FACING_LEFT0]);
    if (result != ERROR_SUCCESS) {
        MessageBoxA(NULL, "Load32BppBitmapFromFile falló!", "Error!", MB_ICONEXCLAMATION | MB_OK);
        goto Exit;
    }

    result = loadBitmapFromFile("C:\\Users\\ohvish\\Pictures\\enemy_faceleft1.bmp", &gEnemy.sprite[FACING_LEFT1]);
    if (result != ERROR_SUCCESS) {
        MessageBoxA(NULL, "Load32BppBitmapFromFile falló!", "Error!", MB_ICONEXCLAMATION | MB_OK);
        goto Exit;
    }

    result = loadBitmapFromFile("C:\\Users\\ohvish\\Pictures\\enemy_faceright0.bmp", &gEnemy.sprite[FACING_RIGHT0]);
    if (result != ERROR_SUCCESS) {
        MessageBoxA(NULL, "Load32BppBitmapFromFile falló!", "Error!", MB_ICONEXCLAMATION | MB_OK);
        goto Exit;
    }

    result = loadBitmapFromFile("C:\\Users\\ohvish\\Pictures\\enemy_faceright1.bmp", &gEnemy.sprite[FACING_RIGHT1]);
    if (result != ERROR_SUCCESS) {
        MessageBoxA(NULL, "Load32BppBitmapFromFile falló!", "Error!", MB_ICONEXCLAMATION | MB_OK);
        goto Exit;
    }

    result = loadBitmapFromFile("C:\\Users\\ohvish\\Pictures\\enemy_faceup0.bmp", &gEnemy.sprite[FACING_UP0]);
    if (result != ERROR_SUCCESS) {
        MessageBoxA(NULL, "Load32BppBitmapFromFile falló!", "Error!", MB_ICONEXCLAMATION | MB_OK);
        goto Exit;
    }

    result = loadBitmapFromFile("C:\\Users\\ohvish\\Pictures\\enemy_faceup1.bmp", &gEnemy.sprite[FACING_UP1]);
    if (result != ERROR_SUCCESS) {
        MessageBoxA(NULL, "Load32BppBitmapFromFile falló!", "Error!", MB_ICONEXCLAMATION | MB_OK);
        goto Exit;
    }

    // Load Enemy Heartbar Sprites
    result = loadBitmapFromFile("C:\\Users\\ohvish\\Pictures\\heartbar0.bmp", &gEnemy.heartBar[6]);
    if (result != ERROR_SUCCESS) {
        MessageBoxA(NULL, "Load32BppBitmapFromFile falló!", "Error!", MB_ICONEXCLAMATION | MB_OK);
        goto Exit;
    }

    result = loadBitmapFromFile("C:\\Users\\ohvish\\Pictures\\heartbar1.bmp", &gEnemy.heartBar[5]);
    if (result != ERROR_SUCCESS) {
        MessageBoxA(NULL, "Load32BppBitmapFromFile falló!", "Error!", MB_ICONEXCLAMATION | MB_OK);
        goto Exit;
    }


    result = loadBitmapFromFile("C:\\Users\\ohvish\\Pictures\\heartbar2.bmp", &gEnemy.heartBar[4]);
    if (result != ERROR_SUCCESS) {
        MessageBoxA(NULL, "Load32BppBitmapFromFile falló!", "Error!", MB_ICONEXCLAMATION | MB_OK);
        goto Exit;
    }

    result = loadBitmapFromFile("C:\\Users\\ohvish\\Pictures\\heartbar3.bmp", &gEnemy.heartBar[3]);
    if (result != ERROR_SUCCESS) {
        MessageBoxA(NULL, "Load32BppBitmapFromFile falló!", "Error!", MB_ICONEXCLAMATION | MB_OK);
        goto Exit;
    }

    result = loadBitmapFromFile("C:\\Users\\ohvish\\Pictures\\heartbar4.bmp", &gEnemy.heartBar[2]);
    if (result != ERROR_SUCCESS) {
        MessageBoxA(NULL, "Load32BppBitmapFromFile falló!", "Error!", MB_ICONEXCLAMATION | MB_OK);
        goto Exit;
    }

    result = loadBitmapFromFile("C:\\Users\\ohvish\\Pictures\\heartbar5.bmp", &gEnemy.heartBar[1]);
    if (result != ERROR_SUCCESS) {
        MessageBoxA(NULL, "Load32BppBitmapFromFile falló!", "Error!", MB_ICONEXCLAMATION | MB_OK);
        goto Exit;
    }

    result = loadBitmapFromFile("C:\\Users\\ohvish\\Pictures\\heartbar6.bmp", &gEnemy.heartBar[0]);
    if (result != ERROR_SUCCESS) {
        MessageBoxA(NULL, "Load32BppBitmapFromFile falló!", "Error!", MB_ICONEXCLAMATION | MB_OK);
        goto Exit;
    }




Exit:
    return result;
}



BOOL _stdcall isGameAlreadyRunning(void) {
    BOOL result = FALSE;

    gMutex = CreateMutexA(NULL, TRUE, "GAME_MUTEX");
    if ( gMutex == NULL) {
        MessageBox(NULL, TEXT("El juego ya se encuentra en ejecución!"), NULL, MB_ICONERROR);
        result = TRUE;
    }

    return result;
}

void _stdcall processPlayerInputs(void) {
    if (gWindowHasFocus == TRUE) {
        NPC player = gPlayer;

        // Get Keys States
        int16_t escapeKeyIsDown = GetAsyncKeyState(VK_ESCAPE);
        int16_t perfDataIsDown = GetAsyncKeyState(VK_F1);
        int16_t downKeyIsDown = GetAsyncKeyState('S'), upKeyIsDown = GetAsyncKeyState('W'),
            leftKeyIsDown = GetAsyncKeyState('A'), rightKeyIsDown = GetAsyncKeyState('D');
        int16_t spaceKeyIsDown = GetAsyncKeyState(VK_SPACE);

        // Get Keys Previous States
        static int16_t perfDataWasDown;
        static int16_t downKeyWasDown;
        static int16_t upKeyWasDown;
        static int16_t rightKeyWasDown;
        static int16_t leftKeyWasDown;

        // Get timeFromPreviousAttack
        static int16_t timeAfterAttack = ATTACK_SPEED;

        // Get Player Sprite
        int16_t currentSprite = gPlayer.currentSprite;

        if (escapeKeyIsDown)
            SendMessageA(gWindowHandler, WM_CLOSE, 0, 0);

        if (perfDataIsDown && !perfDataWasDown)
            gPerfomanceInfo.showPerfData = !gPerfomanceInfo.showPerfData;

        if (downKeyIsDown) {
            gPlayer.playerDirection = DIR_DOWN;
            player.screenPosY++;
            if (gPlayer.screenPosY < GAME_HEIGHT - 16 && checkObstacles(&player) == FALSE)
                gPlayer.screenPosY++;
            
            if (gPlayer.currentSprite == FACING_DOWN0)
                currentSprite = FACING_DOWN1;
            else {
                if (gPlayer.currentSprite == FACING_DOWN1)
                    currentSprite = FACING_DOWN0;
                else
                    gPlayer.currentSprite = FACING_DOWN0;
            }
        }

        else if (upKeyIsDown) {
            player.screenPosY--;
            gPlayer.playerDirection = DIR_UP;
            if (gPlayer.screenPosY > 0 && checkObstacles(&player) == FALSE)
                gPlayer.screenPosY--;
            if (gPlayer.currentSprite == FACING_UP0)
                currentSprite = FACING_UP1;
            else {
                if (gPlayer.currentSprite == FACING_UP1)
                    currentSprite = FACING_UP0;
                else
                    gPlayer.currentSprite = FACING_UP0;
            }
        }

        else if (leftKeyIsDown) {
            player.screenPosX--;
            gPlayer.playerDirection = DIR_LEFT;
            if (gPlayer.screenPosX > 0 && checkObstacles(&player) == FALSE)
                gPlayer.screenPosX--;
            if (gPlayer.currentSprite == FACING_LEFT0)
                currentSprite = FACING_LEFT1;
            else {
                if (gPlayer.currentSprite == FACING_LEFT1)
                    currentSprite = FACING_LEFT0;
                else
                    gPlayer.currentSprite = FACING_LEFT0;
            }
        }


        else if (rightKeyIsDown) {
            player.screenPosX++;
            gPlayer.playerDirection = DIR_RIGHT;
            if (gPlayer.screenPosX < GAME_WIDTH - 16 && checkObstacles(&player) == FALSE)
                gPlayer.screenPosX++;
            if (gPlayer.currentSprite == FACING_RIGHT0)
                currentSprite = FACING_RIGHT1;
            else {
                if (gPlayer.currentSprite == FACING_RIGHT1)
                    currentSprite = FACING_RIGHT0;
                else
                    gPlayer.currentSprite = FACING_RIGHT0;
            }
        }

        if (spaceKeyIsDown && timeAfterAttack >= ATTACK_SPEED) {
            GObject fireBall;
            switch (gPlayer.playerDirection) {
            case DIR_DOWN:
                if (gPlayer.screenPosY + 17 < GAME_HEIGHT - 17) {
                    fireBall.screenPosX = gPlayer.screenPosX;
                    fireBall.screenPosY = gPlayer.screenPosY + 17;
                    fireBall.direction = DIR_DOWN;
                    fireBall.existHitbox = TRUE;
                    fireBall.canBeDestroyed = TRUE;
                    fireBall.damage = 1;
                    fireBall.toDestroy = FALSE;
                    fireBall.script = &FireballScript;
                    loadBitmapFromFile("C:\\Users\\ohvish\\Pictures\\fireball_down.bmp", &fireBall.sprite);
                    insertElement(&gScene.sceneObjects, &fireBall);
                }
                break;
            case DIR_UP:
                if (gPlayer.screenPosY - 17 > 0) {
                    fireBall.screenPosX = gPlayer.screenPosX;
                    fireBall.screenPosY = gPlayer.screenPosY - 17;
                    fireBall.direction = DIR_UP;
                    fireBall.existHitbox = TRUE;
                    fireBall.canBeDestroyed = TRUE;
                    fireBall.damage = 1;
                    fireBall.toDestroy = FALSE;
                    fireBall.script = &FireballScript;
                    loadBitmapFromFile("C:\\Users\\ohvish\\Pictures\\fireball_up.bmp", &fireBall.sprite);
                    insertElement(&gScene.sceneObjects, &fireBall);
                }
                break;
            case DIR_LEFT:
                if (gPlayer.screenPosX - 17 > 0) {
                    fireBall.screenPosX = gPlayer.screenPosX - 17;
                    fireBall.screenPosY = gPlayer.screenPosY;
                    fireBall.direction = DIR_LEFT;
                    fireBall.existHitbox = TRUE;
                    fireBall.canBeDestroyed = TRUE;
                    fireBall.damage = 1;
                    fireBall.toDestroy = FALSE;
                    fireBall.script = &FireballScript;
                    loadBitmapFromFile("C:\\Users\\ohvish\\Pictures\\fireball_left.bmp", &fireBall.sprite);
                    insertElement(&gScene.sceneObjects, &fireBall);
                }
                break;
            case DIR_RIGHT:
                if (gPlayer.screenPosX + 17 < GAME_WIDTH - 17) {
                    fireBall.screenPosX = gPlayer.screenPosX + 17;
                    fireBall.screenPosY = gPlayer.screenPosY;
                    fireBall.direction = DIR_RIGHT;
                    fireBall.existHitbox = TRUE;
                    fireBall.canBeDestroyed = TRUE;
                    fireBall.damage = 1;
                    fireBall.toDestroy = FALSE;
                    fireBall.script = &FireballScript;
                    loadBitmapFromFile("C:\\Users\\ohvish\\Pictures\\fireball_right.bmp", &fireBall.sprite);
                    insertElement(&gScene.sceneObjects, &fireBall);
                }
                break;
            }
            timeAfterAttack = 0;
        }

        perfDataWasDown = perfDataIsDown;
        downKeyWasDown = downKeyIsDown;
        upKeyWasDown = upKeyIsDown;
        leftKeyWasDown = leftKeyIsDown;
        rightKeyWasDown = rightKeyIsDown;
        ++timeAfterAttack;
        
        // Check if enough frames has been rendered for the animation
        if (gPerfomanceInfo.totalFramesRendered % FRAMES_PER_ANIMATION == 0)
            gPlayer.currentSprite = currentSprite;

    }
}

BOOL _stdcall checkObstacles(_In_ NPC* npc) {
    BOOL result = FALSE;
    List* aux = gScene.sceneObjects;
   
    while (aux) {
        if (aux->element && aux->element->existHitbox)
            result = checkCollision(npc->screenPosX, npc->screenPosY, 16, 16, aux->element->screenPosX,
                aux->element->screenPosY, 16, 16);
        if (aux->next == gScene.sceneObjects || result == TRUE)
            aux = NULL;
        else
            aux = aux->next;
    }
    
    if (result == FALSE && strcmp(npc->name, PLAYER_NAME) == 0)
        result = checkCollision(npc->screenPosX, npc->screenPosY, 16, 16, gEnemy.screenPosX,
            gEnemy.screenPosY, 16, 16);
    else if (result == FALSE)
        result = checkCollision(npc->screenPosX, npc->screenPosY, 16, 16, gPlayer.screenPosX,
            gPlayer.screenPosY, 16, 16);

    return result;
}

void _stdcall checkObstaclesDamage(_In_ NPC* npc) {
    BOOL result = FALSE;
    List* aux = gScene.sceneObjects;
    int damage = 0;

    while (aux) {
        if (aux->element && aux->element->existHitbox && aux->element->damage != 0) {
            result = checkCollision(npc->screenPosX, npc->screenPosY, 16, 16, aux->element->screenPosX,
                aux->element->screenPosY, 16, 16);
            if (result == TRUE)
                damage = aux->element->damage;
        }
        if (aux->next == gScene.sceneObjects || result == TRUE)
            aux = NULL;
        else
            aux = aux->next;
    }

    if (npc->healthPoints - damage < 0)
        npc->healthPoints = 0;
    else
        npc->healthPoints -= damage;

}

BOOL _stdcall checkObstaclesObject(_In_ GObject* object) {
    BOOL result = FALSE;
    List* aux = gScene.sceneObjects;

    while (aux) {
        if (aux->element && aux->element->existHitbox && object!= aux->element)
            result = checkCollision(object->screenPosX, object->screenPosY, 16, 16, aux->element->screenPosX,
                aux->element->screenPosY, 16, 16);
        if ( result == TRUE)
            aux = NULL;
        else if(aux->next == gScene.sceneObjects)
            aux = NULL;
        else
            aux = aux->next;
    }
    
    if(result == FALSE)
        result = checkCollision(object->screenPosX, object->screenPosY, 16, 16, gEnemy.screenPosX,
            gEnemy.screenPosY, 16, 16);
    if(result == FALSE)
        result = checkCollision(object->screenPosX, object->screenPosY, 16, 16, gPlayer.screenPosX,
            gPlayer.screenPosY, 16, 16);

    return result;
}


BOOL _stdcall checkCollision(_In_ int x, _In_ int y, _In_ int w, _In_ int h, _In_ int x2, _In_ int y2, _In_ int w2, _In_ int h2) {

    return x2 + w2 > x && \
        y2 + h2 > y &&\
        x + w > x2 &&\
        y + h > y2;
}

void _stdcall processSceneEvents(void) {

    // Execute GObjects scripts
    List* aux = gScene.sceneObjects;
    while (aux) {
        if (aux->element && aux->element->script!=NULL) 
            aux->element->script(&gScene, aux->element, &gPlayer);
        if (gScene.sceneObjects == aux->next)
            aux = NULL;
        else
            aux = aux->next;
    }

    checkObstaclesDamage(&gPlayer);
    checkObstaclesDamage(&gEnemy);

    // Execute if GObjects collides
    aux = gScene.sceneObjects;
    while (aux) {
        if (aux->element && aux->element->canBeDestroyed == TRUE)
            checkIfDestroyGObject(&gScene, aux->element, &gPlayer);
        if (gScene.sceneObjects == aux->next)
            aux = NULL;
        else
            aux = aux->next;
    }

    // Destroy Elements
    aux = gScene.sceneObjects;
    while (aux) {
        if (aux->element && aux->element->toDestroy == TRUE)
            destroyElement(&aux);
        if (gScene.sceneObjects == aux->next)
            aux = NULL;
        else
            aux = aux->next;
    }

    gEnemy.script(&gScene, &gEnemy, &gPlayer);

    //Check if player has death
    if (gPlayer.healthPoints <= 0) {
        gGameIsRunning = FALSE;
        MessageBoxA(NULL, "Has perdido la partida!", "GAME OVER!", MB_ICONWARNING | MB_OK);
        PostQuitMessage(0);
    }


    //Check if enemy has death
    if (gEnemy.healthPoints <= 0) {
        gGameIsRunning = FALSE;
        MessageBoxA(NULL, "Has ganado la partida!", "VICTORY!", MB_ICONHAND | MB_OK);
        PostQuitMessage(0);
    }

}

void _stdcall freeAllMemory(void) {

    for(int i=0; i<N_PLAYER_SPRITES; ++i)
        HeapFree(GetProcessHeap(), NULL, gPlayer.sprite[i].memory);

    HeapFree(GetProcessHeap(), NULL, gGrassBlock.memory);

    HeapFree(GetProcessHeap(), NULL, g6x7font.bitmap.memory);

    freeList(&gScene.sceneObjects);

    VirtualFree(gScene.backBuffer.memory, 0, MEM_RELEASE);

    DestroyWindow(gWindowHandler);

    CloseHandle(gMutex);
}


// Script functions
void _stdcall FireballScript(_Inout_ Scene* scene, _Inout_ GObject* object, _Inout_ NPC* player) {
    float attackSpeed = 2.0f;

    switch (object->direction){
        case DIR_DOWN:
            if (object->screenPosY < GAME_HEIGHT - 16 - attackSpeed && checkObstaclesObject(object) == FALSE) {
                object->screenPosY+=attackSpeed;
                goto Exit;
            }
            break;
        case DIR_LEFT:
            if (object->screenPosX > 0 + attackSpeed && checkObstaclesObject(object) == FALSE) {
                object->screenPosX-=attackSpeed;
                goto Exit;
            }
            break;
        case DIR_RIGHT:
            if (object->screenPosX < GAME_WIDTH - 16 - attackSpeed && checkObstaclesObject(object) == FALSE) {
                object->screenPosX+=attackSpeed;
                goto Exit;
            }
            break;
        case DIR_UP:
            if (object->screenPosY > 0 + attackSpeed && checkObstaclesObject(object) == FALSE) {
                object->screenPosY-=attackSpeed;
                goto Exit;
            }
            break;
    }

Exit:
    return;
}
void _stdcall checkIfDestroyGObject(_Inout_ Scene* scene, _Inout_ GObject* object, _Inout_ NPC* player) {
    float attackSpeed = 2.0f;

    switch (object->direction) {
        case DIR_DOWN:
            if (object->screenPosY < GAME_HEIGHT - 16 - attackSpeed && checkObstaclesObject(object) == FALSE)
                goto Exit;
            break;
        case DIR_LEFT:
            if (object->screenPosX > 0 + attackSpeed && checkObstaclesObject(object) == FALSE) 
                goto Exit;
            break;
        case DIR_RIGHT:
            if (object->screenPosX < GAME_WIDTH - 16 - attackSpeed && checkObstaclesObject(object) == FALSE) 
                goto Exit;
            break;
        case DIR_UP:
            if (object->screenPosY > 0 + attackSpeed && checkObstaclesObject(object) == FALSE) 
                goto Exit;
            break;
    }

    object->toDestroy = TRUE;
Exit:
    return;
}

void _stdcall EnemyScript(_Inout_ Scene* scene, _Inout_ NPC* enemy, _Inout_ NPC* player) {
    NPC auxEnemy = *enemy;
    // Get npc Sprite
    int16_t currentSprite = enemy->currentSprite;
    static int movementCount = 0;
    static uint8_t previousIndex = 0;
    uint8_t index = (rand() % 4);
    // Get timeFromPreviousAttack
    static int16_t timeAfterAttack = ATTACK_SPEED;
    if (movementCount > 0)
        index = previousIndex;

    switch (index) {
        case DIR_DOWN:
            enemy->playerDirection = DIR_DOWN;
            auxEnemy.screenPosY++;
            if (enemy->screenPosY < GAME_HEIGHT - 16 && checkObstacles(&auxEnemy) == FALSE) {
                enemy->screenPosY++;
                if (movementCount == 0) {
                    previousIndex = index;
                    movementCount = (rand() % 100);
                }
                else
                    movementCount--;
            }
            else
                movementCount = 0;
            if (enemy->currentSprite == FACING_DOWN0)
                currentSprite = FACING_DOWN1;
            else {
                if (enemy->currentSprite == FACING_DOWN1)
                    currentSprite = FACING_DOWN0;
                else
                    enemy->currentSprite = FACING_DOWN0;
            }
            break;
        case DIR_LEFT:
            auxEnemy.screenPosX--;
            enemy->playerDirection = DIR_LEFT;
            if (enemy->screenPosX > 0 && checkObstacles(&auxEnemy) == FALSE) {
                enemy->screenPosX--;
                if (movementCount == 0) {
                    previousIndex = index;
                    movementCount = (rand() % 100);
                }
                else
                    movementCount--;
            }
            else
                movementCount = 0;
            if (enemy->currentSprite == FACING_LEFT0)
                currentSprite = FACING_LEFT1;
            else {
                if (enemy->currentSprite == FACING_LEFT1)
                    currentSprite = FACING_LEFT0;
                else
                    enemy->currentSprite = FACING_LEFT0;
            }

            break;
        case DIR_RIGHT:
            auxEnemy.screenPosX++;
            enemy->playerDirection = DIR_RIGHT;
            if (enemy->screenPosX < GAME_WIDTH - 16 && checkObstacles(&auxEnemy) == FALSE) {
                enemy->screenPosX++;
                if (movementCount == 0) {
                    previousIndex = index;
                    movementCount = (rand() % 100);
                }
                else
                    movementCount--;
            }
            else
                movementCount = 0;
            if (enemy->currentSprite == FACING_RIGHT0)
                currentSprite = FACING_RIGHT1;
            else {
                if (enemy->currentSprite == FACING_RIGHT1)
                    currentSprite = FACING_RIGHT0;
                else
                    enemy->currentSprite = FACING_RIGHT0;
            }
            break;
        case DIR_UP:
            auxEnemy.screenPosY--;
            enemy->playerDirection = DIR_UP;
            if (enemy->screenPosY > 0 && checkObstacles(&auxEnemy) == FALSE) {
                enemy->screenPosY--;
                if (movementCount == 0) {
                    previousIndex = index;
                    movementCount = (rand() % 100);
                }
                else
                    movementCount--;
            }
            else
                movementCount = 0;
            if (enemy->currentSprite == FACING_UP0)
                currentSprite = FACING_UP1;
            else {
                if (enemy->currentSprite == FACING_UP1)
                    currentSprite = FACING_UP0;
                else
                    enemy->currentSprite = FACING_UP0;
            }
            break;
    }


    // Check if enough frames has been rendered for the animation
    if (gPerfomanceInfo.totalFramesRendered % FRAMES_PER_ANIMATION == 0) 
        enemy->currentSprite = currentSprite;

    
    // Check if player is vision line
    if ((enemy->screenPosX + 16 > player->screenPosX && \
        player->screenPosX + 16 > enemy->screenPosX || \
        enemy->screenPosY + 16 > player->screenPosY && \
        player->screenPosY + 16 > enemy->screenPosY) && timeAfterAttack >= ATTACK_SPEED) {
        if (enemy->screenPosX < player->screenPosX && enemy->screenPosY + 16 > player->screenPosY && \
            player->screenPosY + 16 > enemy->screenPosY) {
            enemy->playerDirection = DIR_RIGHT;
            enemy->currentSprite = FACING_RIGHT0;
        }
        else if (enemy->screenPosY + 16 > player->screenPosY && \
            player->screenPosY + 16 > enemy->screenPosY) {
            enemy->playerDirection = DIR_LEFT;
            enemy->currentSprite = FACING_LEFT0;
        }
        else if (enemy->screenPosY < player->screenPosY && enemy->screenPosX + 16 > player->screenPosX && \
            player->screenPosX + 16 > enemy->screenPosX) {
            enemy->playerDirection = DIR_DOWN;
            enemy->currentSprite = FACING_DOWN0;
        }
        else {
            enemy->playerDirection = DIR_UP;
            enemy->currentSprite = FACING_UP0;
        }
        GObject fireBall;
        switch (enemy->playerDirection) {
            case DIR_DOWN:
                if (enemy->screenPosY + 17 < GAME_HEIGHT - 17) {
                    fireBall.screenPosX = enemy->screenPosX;
                    fireBall.screenPosY = enemy->screenPosY + 17;
                    fireBall.direction = DIR_DOWN;
                    fireBall.existHitbox = TRUE;
                    fireBall.canBeDestroyed = TRUE;
                    fireBall.script = &FireballScript;
                    fireBall.damage = 1;
                    fireBall.toDestroy = FALSE;
                    loadBitmapFromFile("C:\\Users\\ohvish\\Pictures\\fireball_down.bmp", &fireBall.sprite);
                    insertElement(&scene->sceneObjects, &fireBall);
                }
                break;
            case DIR_UP:
                if (enemy->screenPosY - 17 > 0 ) {
                    fireBall.screenPosX = enemy->screenPosX;
                    fireBall.screenPosY = enemy->screenPosY - 17;
                    fireBall.direction = DIR_UP;
                    fireBall.existHitbox = TRUE;
                    fireBall.canBeDestroyed = TRUE;
                    fireBall.damage = 1;
                    fireBall.toDestroy = FALSE;
                    fireBall.script = &FireballScript;
                    loadBitmapFromFile("C:\\Users\\ohvish\\Pictures\\fireball_up.bmp", &fireBall.sprite);
                    insertElement(&scene->sceneObjects, &fireBall);
                }
                break;
            case DIR_LEFT:
                if (enemy->screenPosX - 17 > 0 ) {
                    fireBall.screenPosX = enemy->screenPosX - 17;
                    fireBall.screenPosY = enemy->screenPosY;
                    fireBall.direction = DIR_LEFT;
                    fireBall.existHitbox = TRUE;
                    fireBall.canBeDestroyed = TRUE;
                    fireBall.damage = 1;
                    fireBall.toDestroy = FALSE;
                    fireBall.script = &FireballScript;
                    loadBitmapFromFile("C:\\Users\\ohvish\\Pictures\\fireball_left.bmp", &fireBall.sprite);
                    insertElement(&scene->sceneObjects, &fireBall);
                }
                break;
            case DIR_RIGHT:
                if (enemy->screenPosX + 17 < GAME_WIDTH - 17) {
                    fireBall.screenPosX = enemy->screenPosX + 17;
                    fireBall.screenPosY = enemy->screenPosY;
                    fireBall.direction = DIR_RIGHT;
                    fireBall.existHitbox = TRUE;
                    fireBall.canBeDestroyed = TRUE;
                    fireBall.damage = 1;
                    fireBall.toDestroy = FALSE;
                    fireBall.script = &FireballScript;
                    loadBitmapFromFile("C:\\Users\\ohvish\\Pictures\\fireball_right.bmp", &fireBall.sprite);
                    insertElement(&scene->sceneObjects, &fireBall);
                }
                break;
        }
        timeAfterAttack = 0;
    }

    ++timeAfterAttack;

}

