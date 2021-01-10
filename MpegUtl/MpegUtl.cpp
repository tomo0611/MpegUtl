/*
*  MpegUtl
*  Author:
*  * Tomohiro Oi		<tomo0611@hotmail.com>
*/

#include <windows.h>
#include <commctrl.h>
#include <uxtheme.h>
#include <stdlib.h>
#include <string.h>
#include <tchar.h>

#include <xaudio2.h>

#include <d3d11_1.h>
#include <DirectXColors.h>
#include <DirectXMath.h>

#include "resource1.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/opt.h>
#include <libswresample/swresample.h>
#include <libswscale/swscale.h>
}

#define ID_TRACK   100
#define ID_STATIC  101 

#pragma once
#pragma comment(lib, "ComCtl32.lib") //ライブラリのリンク

#pragma comment(lib,"d3d11.lib")
#pragma comment(lib,"d3dCompiler.lib")

using namespace DirectX;

// Global variables
HINSTANCE               g_hInst = nullptr;
HWND                    g_hWnd = nullptr;
D3D_DRIVER_TYPE         g_driverType = D3D_DRIVER_TYPE_NULL;
D3D_FEATURE_LEVEL       g_featureLevel = D3D_FEATURE_LEVEL_11_0;
ID3D11Device* g_pd3dDevice = nullptr;
ID3D11Device1* g_pd3dDevice1 = nullptr;
ID3D11DeviceContext* g_pImmediateContext = nullptr;
ID3D11DeviceContext1* g_pImmediateContext1 = nullptr;
IDXGISwapChain* g_pSwapChain = nullptr;
IDXGISwapChain1* g_pSwapChain1 = nullptr;
ID3D11RenderTargetView* g_pRenderTargetView = nullptr;


//--------------------------------------------------------------------------------------
// Forward declarations
//--------------------------------------------------------------------------------------
HRESULT InitWindow(HINSTANCE hInstance, int nCmdShow);
HRESULT InitDevice();
void CleanupDevice();
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
void render(const void* pixels, int pitch, int syspitch);

// The main window class name.
static TCHAR szWindowClass[] = _T("MpegUtl");

// The string that appears in the application's title bar.
static TCHAR szTitle[] = _T("MpegUtl v0.1");

HINSTANCE hInst;

int print_log(const char* format, ...)
{
    static char s_printf_buf[1024];
    va_list args;
    va_start(args, format);
    _vsnprintf_s(s_printf_buf, sizeof(s_printf_buf), format, args);
    va_end(args);
    OutputDebugStringA(s_printf_buf);
    OutputDebugStringA("\n");
    return 0;
}

#define printf(format, ...) \
        print_log(format, __VA_ARGS__)

class VoiceCallback : public IXAudio2VoiceCallback
{
public:
    HANDLE event;
    VoiceCallback() : event(CreateEvent(NULL, FALSE, FALSE, NULL)) {}
    ~VoiceCallback() { CloseHandle(event); }
    void STDMETHODCALLTYPE OnStreamEnd() {}
    void STDMETHODCALLTYPE OnVoiceProcessingPassEnd() {}
    void STDMETHODCALLTYPE OnVoiceProcessingPassStart(UINT32 samples) {}
    void STDMETHODCALLTYPE OnBufferEnd(void* context) { SetEvent(event); }
    void STDMETHODCALLTYPE OnBufferStart(void* context) {}
    void STDMETHODCALLTYPE OnLoopEnd(void* context) {}
    void STDMETHODCALLTYPE OnVoiceError(void* context, HRESULT Error) {}
};

// グローバル変数
HFONT hFont, hCtrlFont;
HWND hButton1, hButton2;
HANDLE hThread1;

HWND hTrack, hStatic;


// スレッド用データ
typedef struct _PARAM {
    BOOL    bFlag;
    HWND    hWnd;
    HFONT   hFont;
} PARAM, * LPPARAM;

PARAM Param1;

DWORD WINAPI FFmpegTestThread1Proc(_In_ LPVOID lpParameter) {
    ULONG ulCount = 0;
    char Buff[40];

    HDC hdc = GetDC(((LPPARAM)lpParameter)->hWnd);
    SelectObject(hdc, ((LPPARAM)lpParameter)->hFont);
    print_log("Hello!");

    int ret = -1;

    ret = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    if (FAILED(ret)) {
        printf("error CoInitializeEx ret=%d\n", ret);
        return -1;
    }

    IXAudio2* audio = NULL;
    ret = XAudio2Create(&audio);
    if (FAILED(ret)) {
        printf("error XAudio2Create ret=%d\n", ret);
        return -1;
    }

    IXAudio2MasteringVoice* master = NULL;
    ret = audio->CreateMasteringVoice(&master);
    if (FAILED(ret)) {
        printf("error CreateMasteringVoice ret=%d\n", ret);
        return -1;
    }

    const char* filename = "C:\\Users\\tomochan\\Videos\\魔王城でおやすみ OP.ts";
    AVFormatContext* format_context = NULL;

    ret = avformat_open_input(&format_context, filename, NULL, NULL);
    if (ret < 0) {
        print_log("cannot open file. filename=%s, ret=%08x\n", filename, AVERROR(ret));
        return -1;
    }

    ret = avformat_find_stream_info(format_context, NULL);
    if (ret < 0) {
        print_log("avformat_find_stream_info error. ret=%08x\n", AVERROR(ret));
        return -1;
    }

    AVStream* audio_stream = NULL;
    AVStream* video_stream = NULL;

    for (unsigned int i = 0; i < format_context->nb_streams; i++) {
        if (format_context->streams[i]->codecpar->codec_type == AVMediaType::AVMEDIA_TYPE_AUDIO) {
            audio_stream = format_context->streams[i];
            break;
        }
    }
    if (audio_stream == NULL) {
        printf("stream not found\n");
        return -1;
    }

    AVCodec* codec = avcodec_find_decoder(audio_stream->codecpar->codec_id);
    if (codec == NULL) {
        printf("avcodec_find_decoder codec not found. codec_id=%d\n", audio_stream->codecpar->codec_id);
        return -1;
    }

    AVCodecContext* codec_context = avcodec_alloc_context3(codec);
    if (codec_context == NULL) {
        printf("avcodec_alloc_context3 error.\n");
        return -1;
    }

    ret = avcodec_open2(codec_context, codec, NULL);
    if (ret < 0) {
        printf("avcodec_open2 error. ret=%08x\n", AVERROR(ret));
        return -1;
    }

    for (unsigned int i = 0; i < format_context->nb_streams; i++) {
        if (format_context->streams[i]->codecpar->codec_type == AVMediaType::AVMEDIA_TYPE_VIDEO) {
            video_stream = format_context->streams[i];
            break;
        }
    }
    if (video_stream == NULL) {
        printf("stream not found\n");
        return -1;
    }

    AVCodec* codecV = avcodec_find_decoder(video_stream->codecpar->codec_id);
    if (codecV == NULL) {
        printf("avcodec_find_decoder codec not found. codec_id=%d\n", video_stream->codecpar->codec_id);
        return -1;
    }

    AVCodecContext* codecV_context = avcodec_alloc_context3(codecV);
    if (codecV_context == NULL) {
        printf("avcodec_alloc_context3 error.\n");
        return -1;
    }

    ret = avcodec_open2(codecV_context, codecV, NULL);
    if (ret < 0) {
        printf("avcodec_open2 error. ret=%08x\n", AVERROR(ret));
        return -1;
    }

    AVFrame* frame = av_frame_alloc();
    AVPacket packet;
    int frame_number = 0;
    SwrContext* swr = NULL;
    BYTE* swr_buf = 0;
    int swr_buf_len = 0;
    IXAudio2SourceVoice* voice = NULL;
    bool first_submit = true;
    bool first_submitV = true;
    BYTE** buf = NULL;
    int buf_cnt = 0;
    VoiceCallback callback;
    WAVEFORMATEX format = { 0 };

    while (1) {
        // read ES
        if ((ret = av_read_frame(format_context, &packet)) < 0) {
            printf("av_read_frame EOF or error. ret=%08x\n", AVERROR(ret));
            break; // eof or error
        }
        if (packet.stream_index == audio_stream->index) {
            // decode ES
            if ((ret = avcodec_send_packet(codec_context, &packet)) < 0) {
                printf("avcodec_send_packet error. ret=%08x\n", AVERROR(ret));
            }
            if ((ret = avcodec_receive_frame(codec_context, frame)) < 0) {
                if (ret != AVERROR(EAGAIN)) {
                    printf("avcodec_receive_frame error. ret=%08x\n", AVERROR(ret));
                    break;
                }
            }
            else {
                printf("frame_number=%d\n", ++frame_number);


                int c, j, k, max_draw;
                for (c = 0; c < frame->channels; c++) {
                    float* src = (float*)frame->extended_data[c];

                    float peak = 0;
                    for (int i = 0; i < frame->nb_samples; i++) {
                        float tmp = src[i];
                        if (tmp < 0) {
                            tmp = -tmp;
                        }
                        if (peak < tmp) {
                            peak = tmp;
                        }
                    }

                    float volume = 20.0 * log10(peak);

                    const char* channel_name = av_get_channel_name(av_channel_layout_extract_channel(frame->channel_layout, c));
                    printf("%s : %f (dB)", channel_name, volume);
                    _stprintf_s(Buff, "%s : %f (dB)", (LPWSTR)channel_name, volume);
                    TextOut(hdc, 100, 100 + 30 * c, Buff, (int)lstrlen(Buff));
                }

                SendMessage(hTrack, TBM_SETPOS, TRUE, (UINT)(300 * (float)((float)frame->pts*10*(1.1) / (float)format_context->duration)));
                _stprintf_s(Buff, "%f / %f (s)", ((float)frame->pts)/1000/100*(1.1), ((float)format_context->duration)/1000/1000);
                TextOut(hdc, 100, 250, Buff, (int)lstrlen(Buff));

                if (swr == NULL) {
                    swr = swr_alloc();
                    if (swr == NULL) {
                        printf("swr_alloc error.\n");
                        break;
                    }
                    av_opt_set_int(swr, "in_channel_layout", frame->channel_layout, 0);
                    av_opt_set_int(swr, "out_channel_layout", frame->channel_layout, 0);
                    av_opt_set_int(swr, "in_sample_rate", frame->sample_rate, 0);
                    av_opt_set_int(swr, "out_sample_rate", frame->sample_rate, 0);
                    av_opt_set_sample_fmt(swr, "in_sample_fmt", (AVSampleFormat)frame->format, 0);
                    av_opt_set_sample_fmt(swr, "out_sample_fmt", AV_SAMPLE_FMT_S16, 0);
                    ret = swr_init(swr);
                    if (ret < 0) {
                        printf("swr_init error ret=%08x.\n", AVERROR(ret));
                        break;
                    }
                    int buf_size = frame->nb_samples * frame->channels * 2; /* the 2 means S16 */
                    swr_buf = new BYTE[buf_size];
                    swr_buf_len = buf_size;
                }

                ret = swr_convert(swr, &swr_buf, frame->nb_samples, (const uint8_t**)frame->extended_data, frame->nb_samples);
                if (ret < 0) {
                    printf("swr_convert error ret=%08x.\n", AVERROR(ret));
                }
                if (voice == NULL) {
                    format.wFormatTag = WAVE_FORMAT_PCM;
                    format.nChannels = frame->channels;
                    format.wBitsPerSample = 16;
                    format.nSamplesPerSec = frame->sample_rate;
                    format.nBlockAlign = format.wBitsPerSample / 8 * format.nChannels;
                    format.nAvgBytesPerSec = format.nSamplesPerSec * format.nBlockAlign;
                    ret = audio->CreateSourceVoice(
                        &voice,
                        &format,
                        0,                          // UINT32 Flags = 0,
                        XAUDIO2_DEFAULT_FREQ_RATIO, // float MaxFrequencyRatio = XAUDIO2_DEFAULT_FREQ_RATIO,
                        &callback                   // IXAudio2VoiceCallback *pCallback = NULL,
                    );
                    if (FAILED(ret)) {
                        printf("error CreateSourceVoice ret=%d\n", ret);
                    }
                    voice->Start();
                }
                if (buf == NULL) {
                    buf = new BYTE * [2];
                    buf[0] = new BYTE[swr_buf_len];
                    buf[1] = new BYTE[swr_buf_len];
                }
                memcpy(buf[buf_cnt], swr_buf, swr_buf_len);
                XAUDIO2_BUFFER buffer = { 0 };
                buffer.AudioBytes = swr_buf_len;
                buffer.pAudioData = buf[buf_cnt];
                ret = voice->SubmitSourceBuffer(&buffer);
                if (FAILED(ret)) {
                    printf("error SubmitSourceBuffer ret=%d\n", ret);
                }

                if (first_submit) {
                    first_submit = false;
                }
                else {
                    if (WaitForSingleObject(callback.event, INFINITE) != WAIT_OBJECT_0) {
                        printf("error WaitForSingleObject\n");
                    }
                }
                if (2 <= ++buf_cnt)
                    buf_cnt = 0;
            }
        }
        else if (packet.stream_index == video_stream->index) {

        if (avcodec_send_packet(codecV_context, &packet) != 0) {
            printf("avcodec_send_packet failed\n");
        }
        while (avcodec_receive_frame(codecV_context, frame) == 0) {
            //on_frame_decoded(frame);
            if (first_submitV) {
                render(frame->data[0] + frame->linesize[0] * (frame->height - 1), -frame->linesize[0],frame->width*4);
            }
        }

        } else {
            // does not audio ES.
        }
        av_packet_unref(&packet);
    }

    if (WaitForSingleObject(callback.event, INFINITE) != WAIT_OBJECT_0) {
        printf("error WaitForSingleObject\n");
    }

    if (voice)
        voice->Stop();
    return TRUE;
}

// Forward declarations of functions included in this code module:
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);

int CALLBACK WinMain(
    _In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPSTR     lpCmdLine,
    _In_ int       nCmdShow
)
{

    InitCommonControls();   //コモンコントロールを初期化

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

    g_hWnd = hWnd;


    if (FAILED(InitDevice()))
    {
        CleanupDevice();
        return 0;
    }

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

//------------------------------------------------------
//■関数名  ControlCreate
//■用途    ウインドウ(コントロール)を作成
//■引数
//       hwndParent ...親ウインドウのハンドル
//       Left       ...作成するウインドウの左隅のX座標
//       Top        ...作成するウインドウの左隅のY座標
//       Width      ...作成するウインドウの横幅
//       Height     ...作成するウインドウの縦幅
//       dwExStyle  ...ウインドウの拡張フラグ
//       dwFlag     ...ウインドウの作成フラグ
//       Caption    ...作成するウインドウのキャプション
//       ClassName  ...作成するウインドウクラス名
//       ChildID    ...子ウインドウの識別子
//       hInstance  ...インスタンスハンドル 
//■戻り値
//  子ウインドウのハンドル
//------------------------------------------------------
HWND CreateControlWindow(HWND hwndParent, int Left, int Top, int Width, int Height, int dwExStyle, int dwFlag, LPCTSTR Caption, LPCTSTR ClassName, HMENU ChildID, HINSTANCE hInstance)
{
    return CreateWindowEx(dwExStyle, ClassName, Caption, WS_CHILD | WS_VISIBLE | dwFlag,
        Left, Top, Width, Height, hwndParent, ChildID, hInstance, NULL);
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

    char buffer[100];

    //HICON hIcon1 = LoadIcon(hInst, MAKEINTRESOURCE(IDI_ICON1));

    switch (message)
    {
    case WM_CREATE:
        // スレッド1引数設定 & 起動
        Param1.bFlag = TRUE;
        Param1.hWnd = hWnd;
        Param1.hFont = hFont;
        hThread1 = CreateThread(NULL, 0, FFmpegTestThread1Proc, (LPVOID)&Param1, 0, NULL);

        hStatic = CreateControlWindow(hWnd, 10, 10, 250, 45, 0,
            0, "", "STATIC", (HMENU)ID_STATIC, hInst);
        //コモンコントロールの初期化
        InitCommonControls();
        //トラックバーコントロールの作成
        //   ウインドウスタイル
        //          TBS_AUTOTICKS        目盛を自動表示
        //          TBS_VERT            垂直トラックバー
        //          TBS_TOP             目盛を上側に表示
        //          TBS_BOTTOM          目盛を下側に表示
        //          TBS_LEFT            目盛を左側に表示
        //          TBS_RIGHT           目盛を右側に表示
        //          TBS_BOTH            目盛を両側に表示
        //          TBS_NOTICKS         目盛りなし
        //          TBS_ENABLESELRANGE  範囲選択を可能にする
        //          TBS_FIXEDLENGTH     スライダーの長さを可変にできる
        //          TBS_NOTHUMB         つまみなし
        hTrack = CreateControlWindow(hWnd, 10, 60, 280, 55, 0,
            TBS_AUTOTICKS, "", TRACKBAR_CLASS, (HMENU)ID_TRACK, hInst);
        //トラックバーの範囲を設定(0-300)
        SendMessage(hTrack, TBM_SETRANGE, TRUE, MAKELONG(0, 300));
        //ページサイズの設定(10)--->マウスでクリックしたときの移動量
        SendMessage(hTrack, TBM_SETPAGESIZE, 0, 10);
        //ラインサイズの設定 (1)--->矢印キーを押したときの移動量
        SendMessage(hTrack, TBM_SETLINESIZE, 0, 1);
        //ポジションの設定(0)
        SendMessage(hTrack, TBM_SETPOS, TRUE, 0);
        SetFocus(hTrack);
        break;
    case WM_HSCROLL:
        if ((HWND)(lParam) == hTrack)
        {  //現在の位置を表示
            //wsprintf(buffer, "現在のポシションは%dです。", SendMessage(hTrack, TBM_GETPOS, 0, 0));
            //SendMessage(hStatic, WM_SETTEXT, 0, (LPARAM)buffer);
        }
        return(DefWindowProc(hWnd, message, wParam, lParam));
        break;
    case WM_COMMAND:
        break;
    case WM_PAINT:
        hdc = BeginPaint(hWnd, &ps);

        OutputDebugStringW(L"WM_PAINT\n");
        // Here your application is laid out.
        // For this introduction, we just print out "Hello, Windows desktop!"
        // in the top left corner.

        //DrawIcon(hdc, 50, 50, hIcon1);

        // End application-specific layout section.
        EndPaint(hWnd, &ps);
        break;
    case WM_DESTROY:
        // スレッドの削除
        CloseHandle(hThread1);
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
        break;
    }

    return 0;
}

#define IDC_TRACKBAR			14990

//
//  FUNCTION: TrackbarDlgProc(HWND, UINT, WPARAM, LPARAM)
//
//  PURPOSE:  Processes messages for the Trackbar control dialog.
//
//
INT_PTR CALLBACK TrackbarDlgProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    default:
        return FALSE;	// Let system deal with msg
    }
    return 0;
}

//--------------------------------------------------------------------------------------
// Create Direct3D device and swap chain
//--------------------------------------------------------------------------------------
HRESULT InitDevice()
{
    HRESULT hr = S_OK;

    RECT rc;
    GetClientRect(g_hWnd, &rc);
    UINT width = rc.right - rc.left;
    UINT height = rc.bottom - rc.top;

    UINT createDeviceFlags = 0;
#ifdef _DEBUG
    createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    D3D_DRIVER_TYPE driverTypes[] =
    {
        D3D_DRIVER_TYPE_HARDWARE,
        D3D_DRIVER_TYPE_WARP,
        D3D_DRIVER_TYPE_REFERENCE,
    };
    UINT numDriverTypes = ARRAYSIZE(driverTypes);

    D3D_FEATURE_LEVEL featureLevels[] =
    {
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0,
    };
    UINT numFeatureLevels = ARRAYSIZE(featureLevels);

    for (UINT driverTypeIndex = 0; driverTypeIndex < numDriverTypes; driverTypeIndex++)
    {
        g_driverType = driverTypes[driverTypeIndex];
        hr = D3D11CreateDevice(nullptr, g_driverType, nullptr, createDeviceFlags, featureLevels, numFeatureLevels,
            D3D11_SDK_VERSION, &g_pd3dDevice, &g_featureLevel, &g_pImmediateContext);

        if (hr == E_INVALIDARG)
        {
            // DirectX 11.0 platforms will not recognize D3D_FEATURE_LEVEL_11_1 so we need to retry without it
            hr = D3D11CreateDevice(nullptr, g_driverType, nullptr, createDeviceFlags, &featureLevels[1], numFeatureLevels - 1,
                D3D11_SDK_VERSION, &g_pd3dDevice, &g_featureLevel, &g_pImmediateContext);
        }

        if (SUCCEEDED(hr))
            break;
    }
    if (FAILED(hr))
        return hr;

    // Obtain DXGI factory from device (since we used nullptr for pAdapter above)
    IDXGIFactory1* dxgiFactory = nullptr;
    {
        IDXGIDevice* dxgiDevice = nullptr;
        hr = g_pd3dDevice->QueryInterface(__uuidof(IDXGIDevice), reinterpret_cast<void**>(&dxgiDevice));
        if (SUCCEEDED(hr))
        {
            IDXGIAdapter* adapter = nullptr;
            hr = dxgiDevice->GetAdapter(&adapter);
            if (SUCCEEDED(hr))
            {
                hr = adapter->GetParent(__uuidof(IDXGIFactory1), reinterpret_cast<void**>(&dxgiFactory));
                adapter->Release();
            }
            dxgiDevice->Release();
        }
    }
    if (FAILED(hr))
        return hr;

    // Create swap chain
    IDXGIFactory2* dxgiFactory2 = nullptr;
    hr = dxgiFactory->QueryInterface(__uuidof(IDXGIFactory2), reinterpret_cast<void**>(&dxgiFactory2));
    if (dxgiFactory2)
    {
        // DirectX 11.1 or later
        hr = g_pd3dDevice->QueryInterface(__uuidof(ID3D11Device1), reinterpret_cast<void**>(&g_pd3dDevice1));
        if (SUCCEEDED(hr))
        {
            (void)g_pImmediateContext->QueryInterface(__uuidof(ID3D11DeviceContext1), reinterpret_cast<void**>(&g_pImmediateContext1));
        }

        DXGI_SWAP_CHAIN_DESC1 sd = {};
        sd.Width = width;
        sd.Height = height;
        sd.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        sd.SampleDesc.Count = 1;
        sd.SampleDesc.Quality = 0;
        sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        sd.BufferCount = 1;

        hr = dxgiFactory2->CreateSwapChainForHwnd(g_pd3dDevice, g_hWnd, &sd, nullptr, nullptr, &g_pSwapChain1);
        if (SUCCEEDED(hr))
        {
            hr = g_pSwapChain1->QueryInterface(__uuidof(IDXGISwapChain), reinterpret_cast<void**>(&g_pSwapChain));
        }

        dxgiFactory2->Release();
    }
    else
    {
        // DirectX 11.0 systems
        DXGI_SWAP_CHAIN_DESC sd = {};
        sd.BufferCount = 1;
        sd.BufferDesc.Width = width;
        sd.BufferDesc.Height = height;
        sd.BufferDesc.Format = DXGI_FORMAT_YUY2;
        sd.BufferDesc.RefreshRate.Numerator = 60;
        sd.BufferDesc.RefreshRate.Denominator = 1;
        sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        sd.OutputWindow = g_hWnd;
        sd.SampleDesc.Count = 1;
        sd.SampleDesc.Quality = 0;
        sd.Windowed = TRUE;

        hr = dxgiFactory->CreateSwapChain(g_pd3dDevice, &sd, &g_pSwapChain);
    }

    // Note this tutorial doesn't handle full-screen swapchains so we block the ALT+ENTER shortcut
    dxgiFactory->MakeWindowAssociation(g_hWnd, DXGI_MWA_NO_ALT_ENTER);

    dxgiFactory->Release();

    if (FAILED(hr))
        return hr;

    // Create a render target view
    ID3D11Texture2D* pBackBuffer = nullptr;
    hr = g_pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&pBackBuffer));
    if (FAILED(hr))
        return hr;

    hr = g_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &g_pRenderTargetView);
    pBackBuffer->Release();
    if (FAILED(hr))
        return hr;

    g_pImmediateContext->OMSetRenderTargets(1, &g_pRenderTargetView, nullptr);

    // Setup the viewport
    D3D11_VIEWPORT vp;
    vp.Width = (FLOAT)width;
    vp.Height = (FLOAT)height;
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    vp.TopLeftX = 0;
    vp.TopLeftY = 0;
    g_pImmediateContext->RSSetViewports(1, &vp);

    return S_OK;
}

//--------------------------------------------------------------------------------------
// Render the frame
//--------------------------------------------------------------------------------------
void render(const void* pixels,int pitch, int syspitch){

    D3D11_BUFFER_DESC tdesc;

    RECT rc;
    GetClientRect(g_hWnd, &rc);
    UINT width = rc.right - rc.left;
    UINT height = rc.bottom - rc.top;

    tdesc.Usage = D3D11_USAGE_DEFAULT;
    tdesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    tdesc.ByteWidth = 16;
    tdesc.CPUAccessFlags = 0;
    tdesc.MiscFlags = 0; // or D3D11_RESOURCE_MISC_GENERATE_MIPS for auto-mip gen.

    D3D11_SUBRESOURCE_DATA srd; // (or an array of these if you have more than one mip level)
    srd.pSysMem = pixels; // This data should be in raw pixel format
    srd.SysMemPitch = syspitch; // Sometimes pixel rows may be padded so this might not be as simple as width * pixel_size_in_bytes.
    srd.SysMemSlicePitch = 0;

    ID3D11Buffer* m_pBuffer;
    g_pd3dDevice->CreateBuffer(&tdesc, &srd, &m_pBuffer);
    g_pImmediateContext->VSSetConstantBuffers(0, 1, &m_pBuffer);
    //g_pImmediateContext->Map();
    // 頂点バッファの生成
    //ID3D11Buffer* m_pBuffer;
    //g_pd3dDevice->CreateBuffer(&desc, &initialData, &m_pBuffer);
    //g_pImmediateContext->CSSetConstantBuffers(1, 1, &m_pBuffer);

    //g_pImmediateContext->ClearRenderTargetView(g_pRenderTargetView, DirectX::Colors::ForestGreen);
    g_pSwapChain->Present(0, 0);
}

//--------------------------------------------------------------------------------------
// Clean up the objects we've created
//--------------------------------------------------------------------------------------
void CleanupDevice()
{
    if (g_pImmediateContext) g_pImmediateContext->ClearState();

    if (g_pRenderTargetView) g_pRenderTargetView->Release();
    if (g_pSwapChain1) g_pSwapChain1->Release();
    if (g_pSwapChain) g_pSwapChain->Release();
    if (g_pImmediateContext1) g_pImmediateContext1->Release();
    if (g_pImmediateContext) g_pImmediateContext->Release();
    if (g_pd3dDevice1) g_pd3dDevice1->Release();
    if (g_pd3dDevice) g_pd3dDevice->Release();
}