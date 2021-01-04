/*
*  MpegUtl
*  Author:
*  * Tomohiro Oi		<tomo0611@hotmail.com>
*/

#include <windows.h>
#include <uxtheme.h>
#include <stdlib.h>
#include <string.h>
#include <tchar.h>

#include "resource1.h"

// Global variables

// The main window class name.
static TCHAR szWindowClass[] = _T("MpegUtl");

// The string that appears in the application's title bar.
static TCHAR szTitle[] = _T("MpegUtl v0.1");

HINSTANCE hInst;

wchar_t* greeting = new wchar_t[100];

// Forward declarations of functions included in this code module:
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);

int CALLBACK WinMain(
    _In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPSTR     lpCmdLine,
    _In_ int       nCmdShow
)
{
    WNDCLASSEX wcex;

    wcex.cbSize = sizeof(WNDCLASSEX);
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WndProc;
    wcex.cbClsExtra = 0;
    wcex.cbWndExtra = 0;
    wcex.hInstance = hInstance;
    wcex.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_ICON1));
    wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcex.lpszMenuName = MAKEINTRESOURCE(IDR_MENU1);
    wcex.lpszClassName = szWindowClass;
    wcex.hIconSm = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_ICON1));

    if (!RegisterClassEx(&wcex))
    {
        MessageBox(NULL,
            _T("Call to RegisterClassEx failed!"),
            _T("Windows Desktop Guided Tour"),
            NULL);

        return 1;
    }

    // Store instance handle in our global variable
    hInst = hInstance;

    // The parameters to CreateWindow explained:
    // szWindowClass: the name of the application
    // szTitle: the text that appears in the title bar
    // WS_OVERLAPPEDWINDOW: the type of window to create
    // CW_USEDEFAULT, CW_USEDEFAULT: initial position (x, y)
    // 500, 100: initial size (width, length)
    // NULL: the parent of this window
    // NULL: this application does not have a menu bar
    // hInstance: the first parameter from WinMain
    // NULL: not used in this application
    HWND hWnd = CreateWindow(
        szWindowClass,
        szTitle,
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        CW_USEDEFAULT, CW_USEDEFAULT,
        NULL,
        NULL,
        hInstance,
        NULL
    );

    if (!hWnd)
    {
        MessageBox(NULL,
            _T("Call to CreateWindow failed!"),
            _T("Windows Desktop Guided Tour"),
            NULL);

        return 1;
    }


    SetWindowTheme(hWnd, L"DarkMode_Explorer", NULL);

    // The parameters to ShowWindow explained:
    // hWnd: the value returned from CreateWindow
    // nCmdShow: the fourth parameter from WinMain
    ShowWindow(hWnd,
        nCmdShow);
    UpdateWindow(hWnd);
    // Main message loop:
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return (int)msg.wParam;
}

//  FUNCTION: WndProc(HWND, UINT, WPARAM, LPARAM)
//
//  PURPOSE:  Processes messages for the main window.
//
//  WM_PAINT    - Paint the main window
//  WM_DESTROY  - post a quit message and return
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    PAINTSTRUCT ps;
    HDC hdc;

    HICON hIcon1 = LoadIcon(hInst, MAKEINTRESOURCE(IDI_ICON1));

    switch (message)
    {
    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case ID_40003:
            OPENFILENAME ofn;       // common dialog box structure
            TCHAR szFile[2048] = { 0 };       // if using TCHAR macros

            LPWSTR szFileTitle = new TCHAR[256];

            // Initialize OPENFILENAME
            ZeroMemory(&ofn, sizeof(ofn));
            ofn.lStructSize = sizeof(ofn);
            ofn.hwndOwner = hWnd;
            ofn.lpstrFile = szFile;
            ofn.nMaxFile = sizeof(szFile);
            ofn.lpstrFilter = _T("AviUtl Project File(.aup)\0*.aup\0");
            ofn.nFilterIndex = 1;
            ofn.lpstrFileTitle = szFileTitle;
            ofn.nMaxFileTitle = 256;
            ofn.lpstrInitialDir = NULL;
            ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

            if (GetOpenFileName(&ofn) == TRUE)
            {
                // use ofn.lpstrFile
                OutputDebugStringW(L"Opened -> ");
                OutputDebugStringW(szFileTitle);
                OutputDebugStringW(L"\n\n");
                /* ファイルのオープン*/
                HANDLE hFile = ::CreateFile(
                    szFile
                    , GENERIC_READ
                    , 0
                    , NULL
                    , OPEN_EXISTING
                    , FILE_ATTRIBUTE_NORMAL
                    , NULL
                );
                if (INVALID_HANDLE_VALUE == hFile) {
                    OutputDebugStringW(L"ファイルオープン失敗(");
                    //OutputDebugStringW(::GetLastError());
                    OutputDebugStringW(L")\n");
                }
                else {

                    // ファイルサイズの取得
                    DWORD dwFileSize = ::GetFileSize(hFile, NULL);

                    TCHAR buff[80];
                    wsprintf(buff, L"ファイルサイズ = %d\n", dwFileSize);
                    OutputDebugStringW(buff);

                    // メモリの確保
                    BYTE* bpMemory = (BYTE*)::malloc(dwFileSize);
                    if (NULL == bpMemory) {
                        // メモリ不足
                        OutputDebugStringW(L"メモリ不足\n");
                        // ファイルクローズ
                        ::CloseHandle(hFile);

                        // メモリの解放
                        if (NULL != bpMemory) {
                            free(bpMemory);
                        }
                    }
                    else {

                        // 読み取ったバイト数を格納する領域
                        DWORD dwNumberOfByteRead = 0;

                        // ファイルの読み込み
                        if (0 == ::ReadFile(hFile, bpMemory, dwFileSize, &dwNumberOfByteRead, NULL)) {

                            // ファイル読み込み失敗
                            TCHAR buff[80];
                            wsprintf(buff, L"ファイル読み込み失敗(%d)\n", ::GetLastError());
                            OutputDebugStringW(buff);
                        }
                        // ファイルクローズ
                        ::CloseHandle(hFile);



                        // 読み込んだ内容の表示
                        wchar_t* Warray = new wchar_t[dwFileSize];
                        size_t outSize;
                        mbstowcs_s(&outSize, Warray, 64 ,(char*)bpMemory, dwFileSize-1);
                        // AviUtl ProjectFile version 0.18読み込み量(32) なぜなのか

                        OutputDebugStringW(Warray);

                        TCHAR buff[80];
                        wsprintf(buff, L"読み込み量(%d)\n", outSize);
                        OutputDebugStringW(buff);

                        wsprintf(greeting, L"ファイル : (%s)", Warray);

                        TCHAR buff2[80];
                        wsprintf(buff2, L"MpegUtl v0.1 - %s (%s)\n", szFileTitle, Warray);

                        SetWindowText(hWnd, buff2);

                        InvalidateRect(hWnd, NULL, TRUE);  //領域無効化
                        UpdateWindow(hWnd);                //再描画命令

                        OutputDebugStringW(greeting);

                        // メモリの解放
                        if (NULL != bpMemory) {
                            free(bpMemory);
                        }
                    }
                }
            }
            break;
        }
        break;
    case WM_PAINT:
        hdc = BeginPaint(hWnd, &ps);

        OutputDebugStringW(L"WM_PAINT\n");
        // Here your application is laid out.
        // For this introduction, we just print out "Hello, Windows desktop!"
        // in the top left corner.
        TextOut(hdc,
            5, 5,
            greeting, _tcslen(greeting));

        DrawIcon(hdc, 50, 50, hIcon1);

        // End application-specific layout section.
        EndPaint(hWnd, &ps);
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
        break;
    }

    return 0;
}