/* mfwinrt.c <file.mp4>: Windows.Media.Playback.MediaPlayer in frame-server mode, as Lightroom Classic's What's New
 * window uses it: MediaSource.CreateFromUri -> MediaPlaybackItem -> MediaPlayer.Source, IsVideoFrameServerEnabled,
 * VideoFrameAvailable -> CopyFrameToVideoSurface into a D3D11 texture (half the video size, so the copy scales).
 * The texture is wrapped by d3d11's CreateDirect3D11SurfaceFromDXGISurface when d3d11.dll exports it (DXVK with
 * LinDobe's patch), else by a small wrapper here. Expects tests/mf/mkvideo.sh's layout.mp4 (854x480, white top
 * quarter, red left, blue right). Prints PASS or FAIL. */
#define COBJMACROS
#define INITGUID
#include <windows.h>
#include <d3d11.h>
#include <roapi.h>
#include <winstring.h>
#include <stdio.h>

typedef struct { INT64 value; } EventRegistrationToken;
typedef HRESULT (WINAPI *create_surface_fn)(IDXGISurface *, IUnknown **);

DEFINE_GUID(IID_IInspectable_, 0xaf86e2e0, 0xb12d, 0x4c6a, 0x9c, 0x5a, 0xd7, 0xaa, 0x65, 0x10, 0x1e, 0x90);
DEFINE_GUID(IID_IUriRuntimeClassFactory_, 0x44a9796f, 0x723e, 0x4fdf, 0xa2, 0x18, 0x03, 0x3e, 0x75, 0xb0, 0xc0, 0x84);
DEFINE_GUID(IID_IMediaSourceStatics_, 0xf77d6fa4, 0x4652, 0x410e, 0xb1, 0xd8, 0xe9, 0xa5, 0xe2, 0x45, 0xa4, 0x5c);
DEFINE_GUID(IID_IMediaPlaybackItemFactory_, 0x7133fce1, 0x1769, 0x4ff9, 0xa7, 0xc1, 0x38, 0xd2, 0xc4, 0xd4, 0x23, 0x60);
DEFINE_GUID(IID_IMediaPlayer_, 0x381a83cb, 0x6fff, 0x499b, 0x8d, 0x64, 0x28, 0x85, 0xdf, 0xc1, 0x24, 0x9e);
DEFINE_GUID(IID_IMediaPlayer3_, 0xee0660da, 0x031b, 0x4feb, 0xbd, 0x9b, 0x92, 0xe0, 0xa0, 0xa8, 0xd2, 0x99);
DEFINE_GUID(IID_IMediaPlayer5_, 0xcfe537fd, 0xf86a, 0x4446, 0xbf, 0x4d, 0xc8, 0xe7, 0x92, 0xb7, 0xb4, 0xb3);
DEFINE_GUID(IID_IMediaPlayerSource2_, 0x82449b9f, 0x7322, 0x4c0b, 0xb0, 0x3b, 0x3e, 0x69, 0xa4, 0x82, 0x60, 0xc5);
DEFINE_GUID(IID_IDirect3DSurface_, 0x0bf4a146, 0x13c1, 0x4694, 0xbe, 0xe3, 0x7a, 0xbf, 0x15, 0xea, 0xf5, 0x86);
DEFINE_GUID(IID_IDirect3DDxgiInterfaceAccess_, 0xa9b3d012, 0x3df2, 0x4ee3, 0xb8, 0xd1, 0x86, 0x95, 0xf4, 0x57, 0xd3, 0xc1);

/* call vtable slot n of a COM object */
#define SLOT(obj, n, type) ((type)((*(void ***)(obj))[n]))

static HANDLE frame_event, failed_event;
static LONG frames;

/* delegate: QI, AddRef, Release, Invoke(sender, args) */
static HRESULT WINAPI delegate_QueryInterface(IUnknown *iface, REFIID iid, void **out) { *out = iface; return S_OK; }
static ULONG WINAPI delegate_AddRef(IUnknown *iface) { return 2; }
static ULONG WINAPI delegate_Release(IUnknown *iface) { return 1; }
static HRESULT WINAPI frame_Invoke(IUnknown *iface, IUnknown *sender, IUnknown *args)
{
    InterlockedIncrement(&frames);
    SetEvent(frame_event);
    return S_OK;
}
static HRESULT WINAPI failed_Invoke(IUnknown *iface, IUnknown *sender, IUnknown *args)
{
    HRESULT code = 0;
    if (args) SLOT(args, 7, HRESULT (WINAPI *)(IUnknown *, HRESULT *))(args, &code);  /* ExtendedErrorCode */
    printf("MediaFailed %#lx\n", code);
    SetEvent(failed_event);
    return S_OK;
}
static void *frame_vtbl[] = { delegate_QueryInterface, delegate_AddRef, delegate_Release, frame_Invoke };
static void *failed_vtbl[] = { delegate_QueryInterface, delegate_AddRef, delegate_Release, failed_Invoke };
static struct { void **vtbl; } frame_handler = { frame_vtbl }, failed_handler = { failed_vtbl };

/* fallback IDirect3DSurface: IInspectable + get_Description, and IDirect3DDxgiInterfaceAccess */
struct surface { void **vtbl; void **access_vtbl; IUnknown *texture; };
static HRESULT WINAPI surface_QueryInterface(IUnknown *iface, REFIID iid, void **out)
{
    struct surface *s = (struct surface *)iface;
    if (IsEqualGUID(iid, &IID_IDirect3DDxgiInterfaceAccess_)) { *out = &s->access_vtbl; return S_OK; }
    if (IsEqualGUID(iid, &IID_IUnknown) || IsEqualGUID(iid, &IID_IInspectable_) || IsEqualGUID(iid, &IID_IDirect3DSurface_))
    { *out = s; return S_OK; }
    *out = NULL;
    return E_NOINTERFACE;
}
static HRESULT WINAPI surface_notimpl(IUnknown *iface, void *a) { return E_NOTIMPL; }
static HRESULT WINAPI access_QueryInterface(void **iface, REFIID iid, void **out)
{
    return surface_QueryInterface((IUnknown *)(iface - 1), iid, out);
}
static HRESULT WINAPI access_GetInterface(void **iface, REFIID iid, void **out)
{
    struct surface *s = (struct surface *)(iface - 1);
    return IUnknown_QueryInterface(s->texture, iid, out);
}
static void *surface_vtbl[] = { surface_QueryInterface, delegate_AddRef, delegate_Release, surface_notimpl, surface_notimpl,
                                surface_notimpl, surface_notimpl };
static void *access_vtbl[] = { access_QueryInterface, delegate_AddRef, delegate_Release, access_GetInterface };

static HRESULT get_factory(const WCHAR *name, REFIID iid, void **out)
{
    HSTRING str;
    HRESULT hr;
    WindowsCreateString(name, wcslen(name), &str);
    hr = RoGetActivationFactory(str, iid, out);
    WindowsDeleteString(str);
    if (FAILED(hr)) printf("no factory for %ls: %#lx\n", name, hr);
    return hr;
}

int wmain(int argc, WCHAR **argv)
{
    IUnknown *uri_factory, *source_statics, *item_factory, *uri = NULL, *source = NULL, *item = NULL, *player = NULL;
    IUnknown *player5, *player3, *player_source, *media_player, *session, *wrapped = NULL;
    IInspectable *player_insp;
    ID3D11Texture2D *texture, *staging;
    D3D11_TEXTURE2D_DESC desc = {0};
    D3D11_MAPPED_SUBRESOURCE map;
    ID3D11DeviceContext *context;
    ID3D11Device *device;
    create_surface_fn create_surface;
    struct surface fallback = { surface_vtbl, access_vtbl };
    EventRegistrationToken token;
    WCHAR url[MAX_PATH + 16], *p;
    HSTRING str;
    UINT32 width = 0, height = 0;
    DWORD *px, white, red, blue;
    HRESULT hr;
    int i;

    if (argc < 2) { printf("usage: mfwinrt <file.mp4>\n"); return 2; }
    setvbuf(stdout, NULL, _IONBF, 0);
    frame_event = CreateEventW(NULL, FALSE, FALSE, NULL);
    failed_event = CreateEventW(NULL, TRUE, FALSE, NULL);
    RoInitialize(RO_INIT_MULTITHREADED);

    wcscpy(url, L"file:///");
    wcscat(url, argv[1]);
    for (p = url; *p; p++) if (*p == '\\') *p = '/';

    if (FAILED(hr = get_factory(L"Windows.Foundation.Uri", &IID_IUriRuntimeClassFactory_, (void **)&uri_factory))) goto fail;
    WindowsCreateString(url, wcslen(url), &str);
    hr = SLOT(uri_factory, 6, HRESULT (WINAPI *)(IUnknown *, HSTRING, IUnknown **))(uri_factory, str, &uri);
    WindowsDeleteString(str);
    if (FAILED(hr)) { printf("CreateUri %#lx\n", hr); goto fail; }

    if (FAILED(hr = get_factory(L"Windows.Media.Core.MediaSource", &IID_IMediaSourceStatics_, (void **)&source_statics))) goto fail;
    hr = SLOT(source_statics, 13, HRESULT (WINAPI *)(IUnknown *, IUnknown *, IUnknown **))(source_statics, uri, &source);
    if (FAILED(hr)) { printf("CreateFromUri %#lx\n", hr); goto fail; }

    if (FAILED(hr = get_factory(L"Windows.Media.Playback.MediaPlaybackItem", &IID_IMediaPlaybackItemFactory_, (void **)&item_factory))) goto fail;
    hr = SLOT(item_factory, 6, HRESULT (WINAPI *)(IUnknown *, IUnknown *, IUnknown **))(item_factory, source, &item);
    if (FAILED(hr)) { printf("MediaPlaybackItem %#lx\n", hr); goto fail; }

    WindowsCreateString(L"Windows.Media.Playback.MediaPlayer", 34, &str);
    hr = RoActivateInstance(str, &player_insp);
    WindowsDeleteString(str);
    if (FAILED(hr)) { printf("MediaPlayer %#lx\n", hr); goto fail; }
    player = (IUnknown *)player_insp;
    if (FAILED(hr = IUnknown_QueryInterface(player, &IID_IMediaPlayer5_, (void **)&player5))
        || FAILED(hr = IUnknown_QueryInterface(player, &IID_IMediaPlayer3_, (void **)&player3))
        || FAILED(hr = IUnknown_QueryInterface(player, &IID_IMediaPlayerSource2_, (void **)&player_source))
        || FAILED(hr = IUnknown_QueryInterface(player, &IID_IMediaPlayer_, (void **)&media_player)))
    { printf("MediaPlayer interfaces %#lx\n", hr); goto fail; }

    SLOT(media_player, 19, HRESULT (WINAPI *)(IUnknown *, boolean))(media_player, TRUE);                      /* IsMuted */
    SLOT(media_player, 29, HRESULT (WINAPI *)(IUnknown *, void *, EventRegistrationToken *))(media_player, &failed_handler, &token);
    SLOT(player5, 9, HRESULT (WINAPI *)(IUnknown *, boolean))(player5, TRUE);                               /* frame server */
    SLOT(player5, 6, HRESULT (WINAPI *)(IUnknown *, void *, EventRegistrationToken *))(player5, &frame_handler, &token);
    if (FAILED(hr = SLOT(player_source, 7, HRESULT (WINAPI *)(IUnknown *, IUnknown *))(player_source, item)))
    { printf("put_Source %#lx\n", hr); goto fail; }
    SLOT(media_player, 45, HRESULT (WINAPI *)(IUnknown *))(media_player);                                   /* Play */

    for (i = 0; i < 3; i++)   /* a few frames, so the copy below is not the first one */
    {
        HANDLE handles[2] = { frame_event, failed_event };
        if (WaitForMultipleObjects(2, handles, FALSE, 10000) != WAIT_OBJECT_0)
        { printf("FAIL: no VideoFrameAvailable (%ld frames)\n", frames); return 1; }
    }

    SLOT(player3, 24, HRESULT (WINAPI *)(IUnknown *, IUnknown **))(player3, &session);                   /* PlaybackSession */
    SLOT(session, 39, HRESULT (WINAPI *)(IUnknown *, UINT32 *))(session, &width);
    SLOT(session, 38, HRESULT (WINAPI *)(IUnknown *, UINT32 *))(session, &height);
    printf("natural size %ux%u, %ld frames\n", width, height, frames);

    if (FAILED(hr = D3D11CreateDevice(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, D3D11_CREATE_DEVICE_BGRA_SUPPORT, NULL, 0,
                                      D3D11_SDK_VERSION, &device, NULL, &context)))
    { printf("D3D11CreateDevice %#lx\n", hr); goto fail; }
    desc.Width = 427;
    desc.Height = 240;
    desc.MipLevels = desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
    ID3D11Device_CreateTexture2D(device, &desc, NULL, &texture);
    desc.Usage = D3D11_USAGE_STAGING;
    desc.BindFlags = 0;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    ID3D11Device_CreateTexture2D(device, &desc, NULL, &staging);

    create_surface = (create_surface_fn)GetProcAddress(GetModuleHandleA("d3d11.dll"), "CreateDirect3D11SurfaceFromDXGISurface");
    if (create_surface)
    {
        IDXGISurface *dxgi;
        ID3D11Texture2D_QueryInterface(texture, &IID_IDXGISurface, (void **)&dxgi);
        hr = create_surface(dxgi, &wrapped);
        IDXGISurface_Release(dxgi);
        printf("d3d11 CreateDirect3D11SurfaceFromDXGISurface %#lx\n", hr);
        if (FAILED(hr)) goto fail;
    }
    else
    {
        fallback.texture = (IUnknown *)texture;
        wrapped = (IUnknown *)&fallback;
        printf("d3d11 has no CreateDirect3D11SurfaceFromDXGISurface, own wrapper\n");
    }

    WaitForSingleObject(frame_event, 2000);
    if (FAILED(hr = SLOT(player5, 10, HRESULT (WINAPI *)(IUnknown *, IUnknown *))(player5, wrapped)))
    { printf("CopyFrameToVideoSurface %#lx\n", hr); goto fail; }

    ID3D11DeviceContext_CopyResource(context, (ID3D11Resource *)staging, (ID3D11Resource *)texture);
    if (FAILED(hr = ID3D11DeviceContext_Map(context, (ID3D11Resource *)staging, 0, D3D11_MAP_READ, 0, &map)))
    { printf("Map %#lx\n", hr); goto fail; }
    px = map.pData;
#define AT(x, y) (px[(y) * (map.RowPitch / 4) + (x)] & 0xffffff)
    white = AT(213, 25);
    red = AT(100, 180);
    blue = AT(320, 180);
    ID3D11DeviceContext_Unmap(context, (ID3D11Resource *)staging, 0);
    printf("pixels: white %06lx red %06lx blue %06lx\n", white, red, blue);

#define CLOSE_TO(c, r, g, b) (abs((int)((c) >> 16 & 0xff) - (r)) < 40 && abs((int)((c) >> 8 & 0xff) - (g)) < 40 && abs((int)((c) & 0xff) - (b)) < 40)
    if (width == 854 && height == 480 && CLOSE_TO(white, 255, 255, 255) && CLOSE_TO(red, 255, 0, 0) && CLOSE_TO(blue, 0, 0, 255))
    {
        printf("PASS\n");
        IUnknown_Release(media_player);
        IUnknown_Release(player);
        return 0;
    }
    printf("FAIL: wrong size or colours\n");
    return 1;

fail:
    printf("FAIL\n");
    return 1;
}
