#include <stdio.h>
#include <windows.h>
#include <psapi.h>

#pragma warning(disable: 5045)
#pragma warning(disable: 4820)

#include <stdint.h>
#include <emmintrin.h>
#include "types.h"
#include "graphics.h"

#pragma comment(lib, "winmm.lib")

LRESULT __stdcall windowProc(HWND windowHandler, UINT message, WPARAM wParam, LPARAM lParam)
{
    LRESULT result = 0;
    switch (message) {
    case WM_CLOSE:
        gGameIsRunning = FALSE;
        PostQuitMessage(0);
        break;
    case WM_ACTIVATE:
        if (wParam == 0) {
            // Our window has lost focus
            gWindowHasFocus = FALSE;
        }
        else {
            // Our window has gained focu
            ShowCursor(FALSE);
            gWindowHasFocus = TRUE;
        }
        break;
    default:
        result = DefWindowProcA(windowHandler, message, wParam, lParam);
    }

    return result;
}


DWORD _stdcall createGameWindow(void) {
    DWORD result = ERROR_SUCCESS;


    // Create a Window Class
    WNDCLASSEX windowClass = { 0 };

    windowClass.cbSize = sizeof(windowClass);
    windowClass.style = 0;
    windowClass.lpfnWndProc = windowProc;
    windowClass.cbClsExtra = 0;
    windowClass.cbWndExtra = 0;
    windowClass.hInstance = (NULL);
    windowClass.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    windowClass.hCursor = LoadCursor(NULL, IDC_ARROW);
    windowClass.hbrBackground = CreateSolidBrush(RGB(255, 0, 255));
    windowClass.lpszMenuName = NULL;
    windowClass.lpszClassName = GAME_NAME"_WINDOW";
    windowClass.hIconSm = LoadIcon(NULL, IDI_APPLICATION);

    // Register Window Class
    if (RegisterClassExA(&windowClass) == 0) {
        MessageBox(NULL, TEXT("No se ha podido registrar la Window Class!"),
            NULL, MB_ICONERROR);
        result = GetLastError();
        goto Exit;
    }

    // Create a Window Instance

    gWindowHandler = CreateWindowExA(0, GAME_NAME"_WINDOW", "Game In C",
        (WS_OVERLAPPEDWINDOW | WS_VISIBLE) & ~WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 640,
        480, NULL, NULL, GetModuleHandleA(NULL), NULL);

    if (gWindowHandler == NULL) {
        MessageBox(NULL, TEXT("No se ha podido crear la ventana!"), NULL, MB_ICONERROR);
        result = GetLastError();
    }
    else {

        // Get Current Monitor Info
        gPerfomanceInfo.monitorInfo.cbSize = sizeof(MONITORINFO);

        if (GetMonitorInfoA(MonitorFromWindow(gWindowHandler, MONITOR_DEFAULTTONEAREST), &gPerfomanceInfo.monitorInfo) == 0) {
            result = ERROR_MONITOR_NO_DESCRIPTOR;
            goto Exit;
        }

        int monitorWidth = gPerfomanceInfo.monitorInfo.rcMonitor.right - gPerfomanceInfo.monitorInfo.rcMonitor.left;
        int monitorHeight = gPerfomanceInfo.monitorInfo.rcMonitor.bottom - gPerfomanceInfo.monitorInfo.rcMonitor.top;

        // Set Window Style
        if (SetWindowLongPtrA(gWindowHandler, GWL_STYLE, (WS_OVERLAPPEDWINDOW | WS_VISIBLE) & ~WS_OVERLAPPEDWINDOW) == 0) {
            result = GetLastError();
            goto Exit;
        }

        // Set Window Position and Dimension
        if (SetWindowPos(gWindowHandler, HWND_TOP, gPerfomanceInfo.monitorInfo.rcMonitor.left,
            gPerfomanceInfo.monitorInfo.rcMonitor.top, monitorWidth, monitorHeight, SWP_FRAMECHANGED) == 0) {
            result = GetLastError();
            goto Exit;
        }

    }


Exit:
    return result;
}


DWORD _stdcall loadBitmapFromFile(_In_ char* filename, _Inout_ GameBitMap* bitmap) {
    DWORD result = ERROR_SUCCESS;
    HANDLE fileHandle = INVALID_HANDLE_VALUE;
    DWORD bytesRead = 0;
    DWORD bitmapHeader = 0;
    DWORD pixelOffset = 0;

    // Open File in Read Mode
    fileHandle = CreateFileA(filename, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL, NULL);
    if (fileHandle == INVALID_HANDLE_VALUE) {
        result = GetLastError();
        goto Exit;
    }

    // Check if first two bytes has 'B' 'M' value
    if (ReadFile(fileHandle, &bitmapHeader, 2, bytesRead, NULL) == 0) {
        result = GetLastError();
        goto Exit;
    }
    if (bitmapHeader != 0x4d42) {
        result = ERROR_FILE_INVALID;
        goto Exit;
    }

    // Store Pixel Offset in a variable
    if (SetFilePointer(fileHandle, 0xA, NULL, FILE_BEGIN) == INVALID_SET_FILE_POINTER) {
        result = GetLastError();
        goto Exit;
    }
    if (ReadFile(fileHandle, &pixelOffset, sizeof(DWORD), &bytesRead, NULL) == 0) {
        result = GetLastError();
        goto Exit;
    }

    // Store BitmapInfoHeader structure in a variable
    if (SetFilePointer(fileHandle, 0xE, NULL, FILE_BEGIN) == INVALID_SET_FILE_POINTER) {
        result = GetLastError();
        goto Exit;
    }
    if (ReadFile(fileHandle, &bitmap->bitmapInfo, sizeof(BITMAPINFOHEADER), &bytesRead, NULL) == 0) {
        result = GetLastError();
        goto Exit;
    }

    // Store Pixel values in heap memory
    bitmap->memory = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, bitmap->bitmapInfo.bmiHeader.biSizeImage);
    if (bitmap->memory == NULL) {
        result = ERROR_NOT_ENOUGH_MEMORY;
        goto Exit;
    }
    if (SetFilePointer(fileHandle, pixelOffset, NULL, FILE_BEGIN) == INVALID_SET_FILE_POINTER) {
        result = GetLastError();
        goto Exit;
    }
    if (ReadFile(fileHandle, bitmap->memory, bitmap->bitmapInfo.bmiHeader.biSizeImage, &bytesRead, NULL) == 0) {
        result = GetLastError();
        goto Exit;
    }

Exit:
    if (fileHandle && (fileHandle != INVALID_HANDLE_VALUE))
        CloseHandle(fileHandle);
    return result;
}

void _stdcall renderFrameGraphics(void) {
    char bufferStr[128] = { 0 };
    Pixel32 px = { 0x00, 0x00, 0x00, 0xFF };
    // Paint back-buffer
    #ifdef SIMD
        // MME extensions memcpy
        /*__m128i quadPx = { 0x7F, 0, 0, 0xFF, 0x7F, 0, 0, 0xFF ,0x7F, 0, 0, 0xFF ,0x7F, 0, 0, 0xFF };
        clearScreen(&quadPx);*/
        fillScreen(&gGrassBlock);
    #else
        // Sequential memcpy
        Pixel32 px = { 0x7F, 0, 0, 0xFF };
        clearScreen(&px);
    #endif // SIMD

    // Render Player sprite on screen
    renderBitmapOnScreen(gPlayer.screenPosX, gPlayer.screenPosY, &gPlayer.sprite[gPlayer.currentSprite]);

    // Render Enemy sprite on screen
    renderBitmapOnScreen(gEnemy.screenPosX, gEnemy.screenPosY, &gEnemy.sprite[gEnemy.currentSprite]);

    renderSceneObjects();

    // Render Player HP on screen
    renderBitmapOnScreen(0, GAME_HEIGHT - 16, &gUIBlocks[0]);
    for (int i = 16; i < 80; i += 16)
        renderBitmapOnScreen(i, GAME_HEIGHT - 16, &gUIBlocks[1]);
    renderBitmapOnScreen(80, GAME_HEIGHT - 16, &gUIBlocks[2]);
    _snprintf_s(bufferStr, _countof(bufferStr), _TRUNCATE, "Player HP: ");
    renderPixelString(bufferStr, &g6x7font, 8, GAME_HEIGHT - 8, &px);
    renderBitmapOnScreen(68, GAME_HEIGHT - 8, &gPlayer.heartBar[gPlayer.healthPoints]);

    // Render Enemy HP on screen
    renderBitmapOnScreen(GAME_WIDTH - 96, GAME_HEIGHT - 16, &gUIBlocks[0]);
    for (int i = 80; i > 16; i -= 16)
        renderBitmapOnScreen(GAME_WIDTH - i, GAME_HEIGHT - 16, &gUIBlocks[1]);
    renderBitmapOnScreen(GAME_WIDTH - 16, GAME_HEIGHT - 16, &gUIBlocks[2]);
    _snprintf_s(bufferStr, _countof(bufferStr), _TRUNCATE, "Enemy HP: ");
    renderPixelString(bufferStr, &g6x7font, GAME_WIDTH - 88, GAME_HEIGHT - 8, &px);
    renderBitmapOnScreen(GAME_WIDTH - 28, GAME_HEIGHT - 8, &gEnemy.heartBar[gEnemy.healthPoints]);



    // Print Debug text on the window
    if (gPerfomanceInfo.showPerfData) {
        Pixel32 fontColor = { 0xFF, 0xFF, 0xFF, 0xFF };
        //SelectObject(deviceContext, (HFONT)GetStockObject(ANSI_FIXED_FONT));

        _snprintf_s(bufferStr, _countof(bufferStr), _TRUNCATE, "Avg Raw FPS: %.01f",
            gPerfomanceInfo.rawFPS);
        renderPixelString(bufferStr, &g6x7font, 0, 0, &fontColor);
        //TextOutA(deviceContext, 0, 0, bufferStr, (int)strlen(bufferStr));

        _snprintf_s(bufferStr, _countof(bufferStr), _TRUNCATE, "Avg Cooked FPS: %.01f",
            gPerfomanceInfo.cookedFPS);
        renderPixelString(bufferStr, &g6x7font, 0, 8, &fontColor);
        //TextOutA(deviceContext, 0, 18, bufferStr, (int)strlen(bufferStr));

        _snprintf_s(bufferStr, _countof(bufferStr), _TRUNCATE, "Current Timer Resolution: %.01f",
            gPerfomanceInfo.currentTimerRes * 0.0001);
        renderPixelString(bufferStr, &g6x7font, 0, 16, &fontColor);
        //TextOutA(deviceContext, 0, 36, bufferStr, (int)strlen(bufferStr));

        _snprintf_s(bufferStr, _countof(bufferStr), _TRUNCATE, "Minimum Timer Resolution: %.01f",
            gPerfomanceInfo.minimumTimerRes * 0.0001);
        renderPixelString(bufferStr, &g6x7font, 0, 24, &fontColor);
        //TextOutA(deviceContext, 0, 54, bufferStr, (int)strlen(bufferStr));

        _snprintf_s(bufferStr, _countof(bufferStr), _TRUNCATE, "Maximum Timer Resolution: %.01f",
            gPerfomanceInfo.maximumTimerRes * 0.0001);
        renderPixelString(bufferStr, &g6x7font, 0, 32, &fontColor);
        //TextOutA(deviceContext, 0, 72, bufferStr, (int)strlen(bufferStr));

        _snprintf_s(bufferStr, _countof(bufferStr), _TRUNCATE, "Handle Count: %lu",
            gPerfomanceInfo.handleCount);
        renderPixelString(bufferStr, &g6x7font, 0, 40, &fontColor);
        //TextOutA(deviceContext, 0, 90, bufferStr, (int)strlen(bufferStr));

        _snprintf_s(bufferStr, _countof(bufferStr), _TRUNCATE, "Memory Usage: %llu KB",
            (gPerfomanceInfo.memoryInfo.PrivateUsage / 1024));
        renderPixelString(bufferStr, &g6x7font, 0, 48, &fontColor);
        //TextOutA(deviceContext, 0, 108, bufferStr, (int)strlen(bufferStr));

        _snprintf_s(bufferStr, _countof(bufferStr), _TRUNCATE, "CPU Usage: %lf %%",
            gPerfomanceInfo.cpuInfo.cpuPercent);
        renderPixelString(bufferStr, &g6x7font, 0, 56, &fontColor);
        //TextOutA(deviceContext, 0, 126, bufferStr, (int)strlen(bufferStr));

        _snprintf_s(bufferStr, _countof(bufferStr), _TRUNCATE, "Total Frames Rendered: %llu",
            gPerfomanceInfo.totalFramesRendered);
        renderPixelString(bufferStr, &g6x7font, 0, 64, &fontColor);
        //TextOutA(deviceContext, 0, 144, bufferStr, (int)strlen(bufferStr));

        _snprintf_s(bufferStr, _countof(bufferStr), _TRUNCATE, "Player Position: (%i, %i)",
            gPlayer.screenPosX, gPlayer.screenPosY);
        renderPixelString(bufferStr, &g6x7font, 0, 72, &fontColor);
        //TextOutA(deviceContext, 0, 162, bufferStr, (int)strlen(bufferStr));

        _snprintf_s(bufferStr, _countof(bufferStr), _TRUNCATE, "Count: (%i)",
            gCount);
        renderPixelString(bufferStr, &g6x7font, 0, 84, &fontColor);
    }

    HDC deviceContext = GetDC(gWindowHandler);

    StretchDIBits(deviceContext, 0, 0, GetDeviceCaps(deviceContext, HORZRES), GetDeviceCaps(deviceContext, VERTRES),
        0, 0, GAME_WIDTH, GAME_HEIGHT, gScene.backBuffer.memory, &gScene.backBuffer.bitmapInfo, DIB_RGB_COLORS, SRCCOPY);

    ReleaseDC(gWindowHandler, deviceContext);
}

void _stdcall renderSceneObjects(void) {
    List* aux = gScene.sceneObjects;
    while (aux) {
        if (aux->element)
            renderBitmapOnScreen(aux->element->screenPosX, aux->element->screenPosY, &aux->element->sprite);
        if (gScene.sceneObjects == aux->next)
            aux = NULL;
        else
            aux = aux->next;
    }
}

void _stdcall renderBitmapOnScreen(_In_ int screenX, _In_ int screenY, _Inout_ GameBitMap* bitmap) {
    // Paint player sprite
    int startScreenPixel = X_Y_TO_START_PIXEL(screenX, screenY, GAME_WIDTH, GAME_HEIGHT);
    int startBitmapPixel = X_Y_TO_START_PIXEL(0, 0, bitmap->bitmapInfo.bmiHeader.biWidth,
        bitmap->bitmapInfo.bmiHeader.biHeight);

    int screenOffset = 0, bitmapOffset = 0;
    Pixel32 pixel;

    for (int y = 0; y < bitmap->bitmapInfo.bmiHeader.biHeight; ++y)
        for (int x = 0; x < bitmap->bitmapInfo.bmiHeader.biWidth; ++x) {

            screenOffset = startScreenPixel + x - y * GAME_WIDTH;
            bitmapOffset = startBitmapPixel + x - y * bitmap->bitmapInfo.bmiHeader.biWidth;



            memcpy_s(&pixel, sizeof(Pixel32), (Pixel32*)bitmap->memory + bitmapOffset, sizeof(Pixel32));

            if (pixel.alpha != 0x00)
            {
                memcpy_s((Pixel32*)gScene.backBuffer.memory + screenOffset, sizeof(Pixel32), &pixel, sizeof(Pixel32));
            }
        }

}

void _stdcall renderPixelString(_In_ char* string, _In_ Font* font, _In_ int screenX,
    _In_ int screenY, _In_ Pixel32* color) {

    int sFontPixel = X_Y_TO_START_PIXEL(0, 0, font->bitmap.bitmapInfo.bmiHeader.biWidth,
        font->bitmap.bitmapInfo.bmiHeader.biHeight),
        sCharPixel = 0,
        fontOffset = 0,
        stringOffset = 0;

    Pixel32 alphaPixel = { 0 };

    size_t charWidth = font->bitmap.bitmapInfo.bmiHeader.biWidth / font->fontCharacters,
        charHeight = font->bitmap.bitmapInfo.bmiHeader.biHeight;

    GameBitMap stringBitmap = { 0 };
    stringBitmap.bitmapInfo.bmiHeader.biSize = sizeof(stringBitmap.bitmapInfo.bmiHeader);
    stringBitmap.bitmapInfo.bmiHeader.biWidth = charWidth * strlen(string);
    stringBitmap.bitmapInfo.bmiHeader.biHeight = charHeight;
    stringBitmap.bitmapInfo.bmiHeader.biBitCount = GAME_BPP;
    stringBitmap.bitmapInfo.bmiHeader.biCompression = BI_RGB;
    stringBitmap.bitmapInfo.bmiHeader.biPlanes = 1;
    sCharPixel = X_Y_TO_START_PIXEL(0, 0, stringBitmap.bitmapInfo.bmiHeader.biWidth,
        stringBitmap.bitmapInfo.bmiHeader.biHeight);

    stringBitmap.memory = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY,
        charWidth * charHeight * strlen(string) * font->bitmap.bitmapInfo.bmiHeader.biBitCount / 8);

    if (stringBitmap.memory) {
        for (int c = 0; c < strlen(string); ++c) {
            switch (string[c]) {
            case 'A':
                sCharPixel = sFontPixel;
                break;
            case 'B':
                sCharPixel = sFontPixel + charWidth;
                break;
            case 'C':
                sCharPixel = sFontPixel + charWidth * 2;
                break;
            case 'D':
                sCharPixel = sFontPixel + charWidth * 3;
                break;
            case 'E':
                sCharPixel = sFontPixel + charWidth * 4;
                break;
            case 'F':
                sCharPixel = sFontPixel + charWidth * 5;
                break;
            case 'G':
                sCharPixel = sFontPixel + charWidth * 6;
                break;
            case 'H':
                sCharPixel = sFontPixel + charWidth * 7;
                break;
            case 'I':
                sCharPixel = sFontPixel + charWidth * 8;
                break;
            case 'J':
                sCharPixel = sFontPixel + charWidth * 9;
                break;
            case 'K':
                sCharPixel = sFontPixel + charWidth * 10;
                break;
            case 'L':
                sCharPixel = sFontPixel + charWidth * 11;
                break;
            case 'M':
                sCharPixel = sFontPixel + charWidth * 12;
                break;
            case 'N':
                sCharPixel = sFontPixel + charWidth * 13;
                break;
            case 'O':
                sCharPixel = sFontPixel + charWidth * 14;
                break;
            case 'P':
                sCharPixel = sFontPixel + charWidth * 15;
                break;
            case 'Q':
                sCharPixel = sFontPixel + charWidth * 16;
                break;
            case 'R':
                sCharPixel = sFontPixel + charWidth * 17;
                break;
            case 'S':
                sCharPixel = sFontPixel + charWidth * 18;
                break;
            case 'T':
                sCharPixel = sFontPixel + charWidth * 19;
                break;
            case 'U':
                sCharPixel = sFontPixel + charWidth * 20;
                break;
            case 'V':
                sCharPixel = sFontPixel + charWidth * 21;
                break;
            case 'W':
                sCharPixel = sFontPixel + charWidth * 22;
                break;
            case 'X':
                sCharPixel = sFontPixel + charWidth * 23;
                break;
            case 'Y':
                sCharPixel = sFontPixel + charWidth * 24;
                break;
            case 'Z':
                sCharPixel = sFontPixel + charWidth * 25;
                break;
            case 'a':
                sCharPixel = sFontPixel + charWidth * 26;
                break;
            case 'b':
                sCharPixel = sFontPixel + charWidth * 27;
                break;
            case 'c':
                sCharPixel = sFontPixel + charWidth * 28;
                break;
            case 'd':
                sCharPixel = sFontPixel + charWidth * 29;
                break;
            case 'e':
                sCharPixel = sFontPixel + charWidth * 30;
                break;
            case 'f':
                sCharPixel = sFontPixel + charWidth * 31;
                break;
            case 'g':
                sCharPixel = sFontPixel + charWidth * 32;
                break;
            case 'h':
                sCharPixel = sFontPixel + charWidth * 33;
                break;
            case 'i':
                sCharPixel = sFontPixel + charWidth * 34;
                break;
            case 'j':
                sCharPixel = sFontPixel + charWidth * 35;
                break;
            case 'k':
                sCharPixel = sFontPixel + charWidth * 36;
                break;
            case 'l':
                sCharPixel = sFontPixel + charWidth * 37;
                break;
            case 'm':
                sCharPixel = sFontPixel + charWidth * 38;
                break;
            case 'n':
                sCharPixel = sFontPixel + charWidth * 39;
                break;
            case 'o':
                sCharPixel = sFontPixel + charWidth * 40;
                break;
            case 'p':
                sCharPixel = sFontPixel + charWidth * 41;
                break;
            case 'q':
                sCharPixel = sFontPixel + charWidth * 42;
                break;
            case 'r':
                sCharPixel = sFontPixel + charWidth * 43;
                break;
            case 's':
                sCharPixel = sFontPixel + charWidth * 44;
                break;
            case 't':
                sCharPixel = sFontPixel + charWidth * 45;
                break;
            case 'u':
                sCharPixel = sFontPixel + charWidth * 46;
                break;
            case 'v':
                sCharPixel = sFontPixel + charWidth * 47;
                break;
            case 'w':
                sCharPixel = sFontPixel + charWidth * 48;
                break;
            case 'x':
                sCharPixel = sFontPixel + charWidth * 49;
                break;
            case 'y':
                sCharPixel = sFontPixel + charWidth * 50;
                break;
            case 'z':
                sCharPixel = sFontPixel + charWidth * 51;
                break;
            case '0':
                sCharPixel = sFontPixel + charWidth * 52;
                break;
            case '1':
                sCharPixel = sFontPixel + charWidth * 53;
                break;
            case '2':
                sCharPixel = sFontPixel + charWidth * 54;
                break;
            case '3':
                sCharPixel = sFontPixel + charWidth * 55;
                break;
            case '4':
                sCharPixel = sFontPixel + charWidth * 56;
                break;
            case '5':
                sCharPixel = sFontPixel + charWidth * 57;
                break;
            case '6':
                sCharPixel = sFontPixel + charWidth * 58;
                break;
            case '7':
                sCharPixel = sFontPixel + charWidth * 59;
                break;
            case '8':
                sCharPixel = sFontPixel + charWidth * 60;
                break;
            case '9':
                sCharPixel = sFontPixel + charWidth * 61;
                break;
            case '`':
                sCharPixel = sFontPixel + charWidth * 62;
                break;
            case '~':
                sCharPixel = sFontPixel + charWidth * 63;
                break;
            case '!':
                sCharPixel = sFontPixel + charWidth * 64;
                break;
            case '@':
                sCharPixel = sFontPixel + charWidth * 65;
                break;
            case '#':
                sCharPixel = sFontPixel + charWidth * 66;
                break;
            case '$':
                sCharPixel = sFontPixel + charWidth * 67;
                break;
            case '%':
                sCharPixel = sFontPixel + charWidth * 68;
                break;
            case '^':
                sCharPixel = sFontPixel + charWidth * 69;
                break;
            case '&':
                sCharPixel = sFontPixel + charWidth * 70;
                break;
            case '*':
                sCharPixel = sFontPixel + charWidth * 71;
                break;
            case '(':
                sCharPixel = sFontPixel + charWidth * 72;
                break;
            case ')':
                sCharPixel = sFontPixel + charWidth * 73;
                break;
            case '-':
                sCharPixel = sFontPixel + charWidth * 74;
                break;
            case '=':
                sCharPixel = sFontPixel + charWidth * 75;
                break;
            case '_':
                sCharPixel = sFontPixel + charWidth * 76;
                break;
            case '+':
                sCharPixel = sFontPixel + charWidth * 77;
                break;
            case '\\':
                sCharPixel = sFontPixel + charWidth * 78;
                break;
            case '|':
                sCharPixel = sFontPixel + charWidth * 79;
                break;
            case '[':
                sCharPixel = sFontPixel + charWidth * 80;
                break;
            case ']':
                sCharPixel = sFontPixel + charWidth * 81;
                break;
            case '{':
                sCharPixel = sFontPixel + charWidth * 82;
                break;
            case '}':
                sCharPixel = sFontPixel + charWidth * 83;
                break;
            case ';':
                sCharPixel = sFontPixel + charWidth * 84;
                break;
            case '\'':
                sCharPixel = sFontPixel + charWidth * 85;
                break;
            case ':':
                sCharPixel = sFontPixel + charWidth * 86;
                break;
            case '\"':
                sCharPixel = sFontPixel + charWidth * 87;
                break;
            case ',':
                sCharPixel = sFontPixel + charWidth * 88;
                break;
            case '<':
                sCharPixel = sFontPixel + charWidth * 89;
                break;
            case '>':
                sCharPixel = sFontPixel + charWidth * 90;
                break;
            case '.':
                sCharPixel = sFontPixel + charWidth * 91;
                break;
            case '/':
                sCharPixel = sFontPixel + charWidth * 92;
                break;
            case '?':
                sCharPixel = sFontPixel + charWidth * 93;
                break;
            case ' ':
                sCharPixel = sFontPixel + charWidth * 94;
                break;
            case '»':
                sCharPixel = sFontPixel + charWidth * 95;
                break;
            case '«':
                sCharPixel = sFontPixel + charWidth * 96;
                break;
            case '\xf2':
                sCharPixel = sFontPixel + charWidth * 97;
                break;
            default:
                sCharPixel = sFontPixel + charWidth * 2;
            }

            for (int y = 0; y < charHeight; ++y)
                for (int x = 0; x < charWidth; ++x) {
                    fontOffset = sCharPixel + x - y * font->bitmap.bitmapInfo.bmiHeader.biWidth;
                    stringOffset = (c * charWidth) + X_Y_TO_START_PIXEL(x, y,
                        stringBitmap.bitmapInfo.bmiHeader.biWidth, stringBitmap.bitmapInfo.bmiHeader.biHeight);

                    memcpy_s(&alphaPixel, sizeof(Pixel32),
                        (Pixel32*)font->bitmap.memory + fontOffset, sizeof(Pixel32));

                    color->alpha = alphaPixel.alpha;

                    memcpy_s((Pixel32*)stringBitmap.memory + stringOffset, sizeof(Pixel32),
                        color, sizeof(Pixel32));
                }
        }

        renderBitmapOnScreen(screenX, screenY, &stringBitmap);

        HeapFree(GetProcessHeap(), NULL, stringBitmap.memory);
    }
}


void _stdcall perfomanceMeasures(_In_ int64_t frameStart, _In_ int64_t frameEnd, _Inout_ int64_t* elapsedMicroACCRaw, _Inout_ int64_t* elapsedMicroACCCook) {
    int64_t elapsedMicro = 0;

    elapsedMicro = frameEnd - frameStart;
    elapsedMicro *= 1000000;
    elapsedMicro /= gPerfomanceInfo.perfFrecuency;

    *elapsedMicroACCRaw += elapsedMicro;

    while (elapsedMicro < TARGET_MICROSECONDS_X_FRAME) {

        if (elapsedMicro < ((int64_t)TARGET_MICROSECONDS_X_FRAME - (gPerfomanceInfo.currentTimerRes * 0.1f))) {
            Sleep(1);
        }

        QueryPerformanceCounter((LARGE_INTEGER*)&frameEnd);

        elapsedMicro = frameEnd - frameStart;
        elapsedMicro *= 1000000;
        elapsedMicro /= gPerfomanceInfo.perfFrecuency;


    }

    *elapsedMicroACCCook += elapsedMicro;
    ++gPerfomanceInfo.totalFramesRendered;

    if ((gPerfomanceInfo.totalFramesRendered % CALC_FPS_X_FRAMES) == 0) {
        int64_t avgMicroPerFrameRaw = *elapsedMicroACCRaw / CALC_FPS_X_FRAMES;
        int64_t avgMicroPerFrameCook = *elapsedMicroACCCook / CALC_FPS_X_FRAMES;
        gPerfomanceInfo.rawFPS = 1.0f / (avgMicroPerFrameRaw * 0.000001f);
        gPerfomanceInfo.cookedFPS = 1.0f / (avgMicroPerFrameCook * 0.000001f);

        // Process Handle Count
        GetProcessHandleCount(GetCurrentProcess(), (PDWORD)&gPerfomanceInfo.handleCount);

        // Get Memory Info
        GetProcessMemoryInfo(GetCurrentProcess(), (PPROCESS_MEMORY_COUNTERS)&gPerfomanceInfo.memoryInfo,
            sizeof(gPerfomanceInfo.memoryInfo));

        // CPU Usage Info

        GetSystemTimeAsFileTime((FILETIME*)&gPerfomanceInfo.cpuInfo.currentSystemTime);
        GetProcessTimes(GetCurrentProcess(),
            &gPerfomanceInfo.cpuInfo.processCreationTime,
            &gPerfomanceInfo.cpuInfo.processExitTime,
            (FILETIME*)&gPerfomanceInfo.cpuInfo.currentKernelCPUTime,
            (FILETIME*)&gPerfomanceInfo.cpuInfo.currentUserCPUTime);
        gPerfomanceInfo.cpuInfo.cpuPercent = (gPerfomanceInfo.cpuInfo.currentKernelCPUTime - gPerfomanceInfo.cpuInfo.previousKernelCPUTime) + \
            (gPerfomanceInfo.cpuInfo.currentUserCPUTime - gPerfomanceInfo.cpuInfo.previousUserCPUTime);
        gPerfomanceInfo.cpuInfo.cpuPercent /= (gPerfomanceInfo.cpuInfo.currentSystemTime - gPerfomanceInfo.cpuInfo.previousSystemTime);
        gPerfomanceInfo.cpuInfo.cpuPercent /= gPerfomanceInfo.cpuInfo.systemInfo.dwNumberOfProcessors;
        gPerfomanceInfo.cpuInfo.cpuPercent *= 100;

        gPerfomanceInfo.cpuInfo.previousKernelCPUTime = gPerfomanceInfo.cpuInfo.currentKernelCPUTime;
        gPerfomanceInfo.cpuInfo.previousUserCPUTime = gPerfomanceInfo.cpuInfo.currentUserCPUTime;
        gPerfomanceInfo.cpuInfo.previousSystemTime = gPerfomanceInfo.cpuInfo.currentSystemTime;

        *elapsedMicroACCRaw = 0;
        *elapsedMicroACCCook = 0;
    }

}


#ifdef SIMD
__forceinline void clearScreen(_In_ __m128i* pixel) {
    for (int x = 0; x < GAME_WIDTH * GAME_HEIGHT; x += 4)
        _mm_store_si128((__m128i*)((Pixel32*)gScene.backBuffer.memory + x), *pixel);
}

void _stdcall fillScreen(_In_ GameBitMap* blockTile) {
    for (int y = 0; y < GAME_HEIGHT; y += 16)
        for (int x = 0; x < GAME_WIDTH; x += 16)
            renderBitmapOnScreen(x, y, blockTile);
}
#else
__forceinline void clearScreen(_In_ Pixel32* pixel) {
    for (int x = 0; x < GAME_HEIGHT * GAME_WIDTH; ++x)
        memcpy_s((Pixel32*)gScene.backBuffer.memory + x, sizeof(Pixel32), pixel, sizeof(Pixel32));
}

void _stdcall fillScreen(_In_ GameBitMap* blockTile) {

}
#endif