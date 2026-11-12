/* mfautoplay.c: does IMFMediaEngine start on its own with SetAutoPlay(TRUE)? Photoshop's What's New dialog (a UXP
 * <video autoplay loop>) stayed blank under Wine. Frame-server mode, no window. Prints each engine event, then
 * "PASS autoplay: playing" or "FAIL autoplay: never played" after 10 s.
 *   x86_64-w64-mingw32-gcc -O1 -o mfautoplay.exe mfautoplay.c -lmfplat -lmfuuid -lole32 -luuid
 *   wine mfautoplay.exe <video file> [play]   (play: call Play() as well, the control case) */
#define COBJMACROS
#include <windows.h>
#include <mfapi.h>
#include <mfmediaengine.h>
#include <stdio.h>

static volatile LONG playing, canplay;
static HRESULT WINAPI qi(IMFMediaEngineNotify *i, REFIID r, void **o)
{ if (IsEqualIID(r, &IID_IUnknown) || IsEqualIID(r, &IID_IMFMediaEngineNotify)) { *o = i; return S_OK; } *o = NULL; return E_NOINTERFACE; }
static ULONG WINAPI ar(IMFMediaEngineNotify *i) { return 2; }
static ULONG WINAPI rl(IMFMediaEngineNotify *i) { return 1; }
static HRESULT WINAPI ev(IMFMediaEngineNotify *i, DWORD e, DWORD_PTR p1, DWORD p2)
{
    if (e == MF_MEDIA_ENGINE_EVENT_PLAYING) playing = 1;
    if (e == MF_MEDIA_ENGINE_EVENT_CANPLAY) canplay = 1;
    if (e != MF_MEDIA_ENGINE_EVENT_TIMEUPDATE && e != MF_MEDIA_ENGINE_EVENT_PROGRESS) { printf("event %lu\n", e); fflush(stdout); }
    return S_OK;
}
static IMFMediaEngineNotifyVtbl vt = { qi, ar, rl, ev };
static IMFMediaEngineNotify notify = { &vt };

int main(int argc, char **argv)
{
    IMFMediaEngineClassFactory *f; IMFMediaEngine *e; IMFAttributes *a; WCHAR url[MAX_PATH]; HRESULT hr; int i;
    if (argc < 2) return 2;
    MultiByteToWideChar(CP_ACP, 0, argv[1], -1, url, MAX_PATH);
    CoInitializeEx(NULL, COINIT_MULTITHREADED); MFStartup(MF_VERSION, 0);
    hr = CoCreateInstance(&CLSID_MFMediaEngineClassFactory, NULL, CLSCTX_INPROC_SERVER, &IID_IMFMediaEngineClassFactory, (void **)&f);
    if (FAILED(hr)) { printf("factory %#lx\n", hr); return 1; }
    MFCreateAttributes(&a, 2);
    IMFAttributes_SetUnknown(a, &MF_MEDIA_ENGINE_CALLBACK, (IUnknown *)&notify);
    IMFAttributes_SetUINT32(a, &MF_MEDIA_ENGINE_VIDEO_OUTPUT_FORMAT, DXGI_FORMAT_B8G8R8A8_UNORM);
    hr = IMFMediaEngineClassFactory_CreateInstance(f, 0, a, &e);
    if (FAILED(hr)) { printf("engine %#lx\n", hr); return 1; }
    IMFMediaEngine_SetAutoPlay(e, TRUE);
    IMFMediaEngine_SetLoop(e, TRUE);
    IMFMediaEngine_SetMuted(e, TRUE);
    hr = IMFMediaEngine_SetSource(e, url);
    printf("SetSource %#lx\n", hr); fflush(stdout);
    for (i = 0; i < 100 && !playing; i++)
    {
        if (argc > 2 && canplay == 1) { canplay = 2; printf("Play %#lx\n", IMFMediaEngine_Play(e)); fflush(stdout); }
        Sleep(100);
    }
    printf("%s autoplay: %s, paused %d, time %.2f\n", playing ? "PASS" : "FAIL", playing ? "playing" : "never played",
           IMFMediaEngine_IsPaused(e), IMFMediaEngine_GetCurrentTime(e));
    IMFMediaEngine_Shutdown(e);
    return !playing;
}
