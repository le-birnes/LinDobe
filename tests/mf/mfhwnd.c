/* mfhwnd.c: IMFMediaEngine rendering mode (MF_MEDIA_ENGINE_PLAYBACK_HWND, no output format), as Photoshop's UXP <video>
 * creates it for the What's New and tip videos (0144). Plays the file into a 854x480 window for 3 s, then reads the
 * window back and counts pixels that are not the black background. PASS when the video drew (> 10 % of pixels).
 * With "layout", the file must be layout.mp4 (854x480, white top quarter, red left and blue right below; make it
 * with mkvideo.sh), and five points must have those colours: 854-wide rows are padded to 856 by GStreamer and were
 * read as 854 (sheared, 0145), and RGB without a stride is bottom-up (upside down).
 *   x86_64-w64-mingw32-gcc -O1 -o mfhwnd.exe mfhwnd.c -lmfplat -lmfuuid -lole32 -luuid -lgdi32 -luser32
 *   wine mfhwnd.exe <video file> [layout] */
#define VW 854
#define VH 480
#define COBJMACROS
#include <windows.h>
#include <mfapi.h>
#include <mfmediaengine.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static HRESULT WINAPI qi(IMFMediaEngineNotify *i, REFIID r, void **o)
{ if (IsEqualIID(r, &IID_IUnknown) || IsEqualIID(r, &IID_IMFMediaEngineNotify)) { *o = i; return S_OK; } *o = NULL; return E_NOINTERFACE; }
static ULONG WINAPI ar(IMFMediaEngineNotify *i) { return 2; }
static ULONG WINAPI rl(IMFMediaEngineNotify *i) { return 1; }
static HRESULT WINAPI ev(IMFMediaEngineNotify *i, DWORD e, DWORD_PTR p1, DWORD p2) { return S_OK; }
static IMFMediaEngineNotifyVtbl vt = { qi, ar, rl, ev };
static IMFMediaEngineNotify notify = { &vt };

int main(int argc, char **argv)
{
    IMFMediaEngineClassFactory *f; IMFMediaEngine *e; IMFAttributes *a; WCHAR url[MAX_PATH]; HRESULT hr;
    WNDCLASSA wc = {0}; HWND w; MSG m; DWORD t0; HDC dc, mem; HBITMAP bmp; BITMAPINFO bi = {{sizeof(BITMAPINFOHEADER)}};
    DWORD *px; int i, lit = 0;
    if (argc < 2) return 2;
    MultiByteToWideChar(CP_ACP, 0, argv[1], -1, url, MAX_PATH);
    wc.lpfnWndProc = DefWindowProcA; wc.lpszClassName = "mfhwnd"; wc.hbrBackground = GetStockObject(BLACK_BRUSH);
    RegisterClassA(&wc);
    w = CreateWindowA("mfhwnd", "mfhwnd", WS_POPUP | WS_VISIBLE, 50, 50, VW, VH, 0, 0, 0, 0);
    CoInitializeEx(NULL, COINIT_MULTITHREADED); MFStartup(MF_VERSION, 0);
    CoCreateInstance(&CLSID_MFMediaEngineClassFactory, NULL, CLSCTX_INPROC_SERVER, &IID_IMFMediaEngineClassFactory, (void **)&f);
    MFCreateAttributes(&a, 2);
    IMFAttributes_SetUnknown(a, &MF_MEDIA_ENGINE_CALLBACK, (IUnknown *)&notify);
    IMFAttributes_SetUINT64(a, &MF_MEDIA_ENGINE_PLAYBACK_HWND, (UINT64)(ULONG_PTR)w);
    hr = IMFMediaEngineClassFactory_CreateInstance(f, MF_MEDIA_ENGINE_WAITFORSTABLE_STATE, a, &e);
    if (FAILED(hr)) { printf("engine %#lx\n", hr); return 1; }
    IMFMediaEngine_SetMuted(e, TRUE); IMFMediaEngine_SetAutoPlay(e, TRUE);
    IMFMediaEngine_SetSource(e, url);
    for (t0 = GetTickCount(); GetTickCount() - t0 < 3000;)
    { while (PeekMessageA(&m, 0, 0, 0, PM_REMOVE)) DispatchMessageA(&m); Sleep(10); }
    bi.bmiHeader.biWidth = VW; bi.bmiHeader.biHeight = -VH; bi.bmiHeader.biPlanes = 1; bi.bmiHeader.biBitCount = 32;
    dc = GetDC(w); mem = CreateCompatibleDC(dc); bmp = CreateDIBSection(dc, &bi, DIB_RGB_COLORS, (void **)&px, 0, 0);
    SelectObject(mem, bmp); BitBlt(mem, 0, 0, VW, VH, dc, 0, 0, SRCCOPY);
    for (i = 0; i < VW * VH; i++) if (px[i] & 0xffffff) lit++;
    if (argc > 2 && !strcmp(argv[2], "layout"))
    {
        static const struct { int x, y; DWORD rgb; const char *what; } pt[] =
        {
            {100, 60, 0xffffff, "white top left"}, {750, 60, 0xffffff, "white top right"},
            {100, 300, 0xff0000, "red middle left"}, {100, 450, 0xff0000, "red bottom left"},
            {750, 450, 0x0000ff, "blue bottom right"},
        };
        int bad = 0;
        for (i = 0; i < ARRAYSIZE(pt); i++)
        {
            DWORD c = px[pt[i].y * VW + pt[i].x] & 0xffffff;
            int dr = abs((int)(c >> 16) - (int)(pt[i].rgb >> 16)), dg = abs((int)((c >> 8) & 0xff) - (int)((pt[i].rgb >> 8) & 0xff));
            int db = abs((int)(c & 0xff) - (int)(pt[i].rgb & 0xff));
            if (dr > 40 || dg > 40 || db > 40) { printf("%s at %d,%d is %06lx\n", pt[i].what, pt[i].x, pt[i].y, c); bad++; }
        }
        printf("%s layout: %d of %d points wrong, time %.2f s\n", bad ? "FAIL" : "PASS", bad, (int)ARRAYSIZE(pt),
               IMFMediaEngine_GetCurrentTime(e));
        IMFMediaEngine_Shutdown(e); IMFMediaEngine_Release(e);
        return bad != 0;
    }
    printf("%s video in window: %d of %d pixels not black, time %.2f s\n", lit > VW * VH / 10 ? "PASS" : "FAIL",
           lit, VW * VH, IMFMediaEngine_GetCurrentTime(e));
    IMFMediaEngine_Shutdown(e); IMFMediaEngine_Release(e);
    return lit <= VW * VH / 10;
}
