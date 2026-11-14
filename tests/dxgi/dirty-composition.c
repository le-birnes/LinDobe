#define COBJMACROS
#define INITGUID
#include <windows.h>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <dcomp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static ID3D11Device *device;
static ID3D11DeviceContext *context;
static IDXGISwapChain1 *swapchain;

static HRESULT paint(const float *color, RECT *rects, UINT count, BOOL legacy)
{
    ID3D11Texture2D *texture;
    ID3D11RenderTargetView *view;
    DXGI_PRESENT_PARAMETERS parameters = {0};
    HRESULT hr;
    MSG message;
    unsigned int wait, r;

    if (FAILED(hr = IDXGISwapChain1_GetBuffer(swapchain, 0, &IID_ID3D11Texture2D, (void **)&texture)))
        return hr;
    if (count)
    {
        for (r = 0; r < count; ++r)
        {
            RECT *rect = &rects[r];
            UINT width = rect->right - rect->left, height = rect->bottom - rect->top;
            DWORD *pixels = HeapAlloc(GetProcessHeap(), 0, width * height * sizeof(*pixels));
            D3D11_BOX box = {rect->left, rect->top, 0, rect->right, rect->bottom, 1};
            unsigned int i;
            DWORD value = 0xff000000 | ((DWORD)(color[0] * 255) << 16)
                    | ((DWORD)(color[1] * 255) << 8) | (DWORD)(color[2] * 255);
            if (!pixels)
            {
                ID3D11Texture2D_Release(texture);
                return E_OUTOFMEMORY;
            }
            for (i = 0; i < width * height; ++i) pixels[i] = value;
            ID3D11DeviceContext_UpdateSubresource(context, (ID3D11Resource *)texture, 0, &box,
                    pixels, width * sizeof(*pixels), 0);
            HeapFree(GetProcessHeap(), 0, pixels);
        }
        parameters.DirtyRectsCount = count;
        parameters.pDirtyRects = rects;
    }
    else
    {
        if (FAILED(hr = ID3D11Device_CreateRenderTargetView(device, (ID3D11Resource *)texture, NULL, &view)))
        {
            ID3D11Texture2D_Release(texture);
            return hr;
        }
        ID3D11DeviceContext_ClearRenderTargetView(context, view, color);
        ID3D11RenderTargetView_Release(view);
    }
    ID3D11Texture2D_Release(texture);
    hr = legacy ? IDXGISwapChain1_Present(swapchain, 0, 0)
            : IDXGISwapChain1_Present1(swapchain, 0, 0, &parameters);
    for (wait = 0; wait < 20; ++wait)
    {
        while (PeekMessageA(&message, NULL, 0, 0, PM_REMOVE)) DispatchMessageA(&message);
        Sleep(10);
    }
    return hr;
}

static BOOL check(HWND window, int x, COLORREF expected, const char *name)
{
    POINT point = {x, 120};
    HDC dc = GetDC(NULL);
    COLORREF actual;
    ClientToScreen(window, &point);
    actual = GetPixel(dc, point.x, point.y);
    ReleaseDC(NULL, dc);
    printf("%s: got %06x expected %06x\n", name, actual, expected);
    return actual == expected;
}

int main(int argc, char **argv)
{
    const float red[] = {1, 0, 0, 1}, blue[] = {0, 0, 1, 1};
    const float green[] = {0, 1, 0, 1}, yellow[] = {1, 1, 0, 1};
    RECT left = {0, 0, 64, 240}, right = {256, 0, 320, 240};
    RECT pair[] = {{0, 0, 64, 240}, {256, 0, 320, 240}};
    RECT resized_right = {336, 0, 400, 240};
    DXGI_PRESENT_PARAMETERS parameters = {0};
    ID3D11Texture2D *held_buffer = NULL;
    UINT buffer_count = argc > 1 ? atoi(argv[1]) : 2;
    DXGI_SWAP_CHAIN_DESC1 desc = {0};
    IDCompositionDevice *composition = NULL;
    IDCompositionTarget *target = NULL;
    IDCompositionVisual *visual = NULL;
    IDXGIFactory2 *factory = NULL;
    IDXGIDevice *dxgi_device = NULL;
    WNDCLASSA wc = {0};
    HWND window;
    HRESULT hr;
    BOOL passed = FALSE;
    BOOL composition_mode = argc < 3 || strcmp(argv[2], "--hwnd");
    HRESULT (WINAPI *create_composition)(IDXGIDevice *, REFIID, void **);

    if (buffer_count < 2 || buffer_count > 3) return 2;
    printf("Composition buffer count: %u\n", buffer_count);
    wc.lpfnWndProc = DefWindowProcA;
    wc.lpszClassName = "dirty-composition";
    RegisterClassA(&wc);
    window = CreateWindowA(wc.lpszClassName, "dirty-composition", WS_POPUP | WS_VISIBLE,
                          50, 50, 320, 240, NULL, NULL, NULL, NULL);
    hr = D3D11CreateDevice(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, D3D11_CREATE_DEVICE_BGRA_SUPPORT,
                          NULL, 0, D3D11_SDK_VERSION, &device, NULL, &context);
    if (FAILED(hr)) goto fail;
    if (FAILED(hr = CreateDXGIFactory1(&IID_IDXGIFactory2, (void **)&factory))) goto fail;
    desc.Width = 320;
    desc.Height = 240;
    desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.BufferCount = buffer_count;
    desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
    desc.AlphaMode = composition_mode ? DXGI_ALPHA_MODE_PREMULTIPLIED : DXGI_ALPHA_MODE_IGNORE;
    if (!composition_mode)
    {
        hr = IDXGIFactory2_CreateSwapChainForHwnd(factory, (IUnknown *)device, window, &desc, NULL, NULL, &swapchain);
        if (FAILED(hr) || FAILED(hr = paint(blue, NULL, 0, FALSE))) goto fail;
        passed = check(window, 160, RGB(0, 0, 255), "ordinary HWND Present1");
        if (FAILED(hr = paint(red, NULL, 0, TRUE))) goto fail;
        passed &= check(window, 160, RGB(255, 0, 0), "ordinary HWND Present");
        printf("%s ordinary HWND swapchain\n", passed ? "PASS" : "FAIL");
        goto cleanup;
    }
    if (FAILED(hr = IDXGIFactory2_CreateSwapChainForComposition(factory, (IUnknown *)device, &desc, NULL, &swapchain))) goto fail;
    if (FAILED(hr = ID3D11Device_QueryInterface(device, &IID_IDXGIDevice, (void **)&dxgi_device))) goto fail;
    create_composition = (void *)GetProcAddress(LoadLibraryA("dcomp.dll"), "DCompositionCreateDevice");
    if (!create_composition) goto fail;
    if (FAILED(hr = create_composition(dxgi_device, &IID_IDCompositionDevice, (void **)&composition))) goto fail;
    if (FAILED(hr = IDCompositionDevice_CreateTargetForHwnd(composition, window, TRUE, &target))) goto fail;
    if (FAILED(hr = IDCompositionDevice_CreateVisual(composition, &visual))) goto fail;
    IDCompositionVisual_SetContent(visual, (IUnknown *)swapchain);
    IDCompositionTarget_SetRoot(target, visual);
    IDCompositionDevice_Commit(composition);
    if (FAILED(hr = paint(red, NULL, 0, FALSE)) || FAILED(hr = paint(blue, NULL, 0, FALSE))) goto fail;
    if (!check(window, 160, RGB(0, 0, 255), "full blue frame"))
    {
        hr = E_FAIL;
        goto fail;
    }
    if (FAILED(hr = paint(green, &left, 1, FALSE))) goto fail;
    passed = check(window, 32, RGB(0, 255, 0), "new green left");
    passed &= check(window, 160, RGB(0, 0, 255), "retained blue center");
    if (FAILED(hr = paint(yellow, &right, 1, FALSE))) goto fail;
    passed &= check(window, 32, RGB(0, 255, 0), "retained green left");
    passed &= check(window, 160, RGB(0, 0, 255), "retained blue center again");
    passed &= check(window, 288, RGB(255, 255, 0), "new yellow right");
    if (FAILED(hr = paint(yellow, pair, 2, FALSE))) goto fail;
    passed &= check(window, 32, RGB(255, 255, 0), "multiple rectangles left");
    passed &= check(window, 160, RGB(0, 0, 255), "multiple rectangles retain center");
    passed &= check(window, 288, RGB(255, 255, 0), "multiple rectangles right");
    if (FAILED(hr = paint(red, NULL, 0, TRUE)) || FAILED(hr = paint(green, &left, 1, FALSE))) goto fail;
    passed &= check(window, 160, RGB(255, 0, 0), "Present resets history for Present1");
    if (FAILED(hr = IDXGISwapChain1_GetBuffer(swapchain, 0, &IID_ID3D11Texture2D, (void **)&held_buffer))) goto fail;
    hr = IDXGISwapChain1_ResizeBuffers(swapchain, 0, 400, 240, DXGI_FORMAT_UNKNOWN, 0);
    passed &= hr == DXGI_ERROR_INVALID_CALL;
    printf("Resize with held buffer: %#x\n", hr);
    ID3D11Texture2D_Release(held_buffer);
    held_buffer = NULL;
    if (FAILED(hr = paint(blue, &right, 1, FALSE))) goto fail;
    passed &= check(window, 32, RGB(0, 255, 0), "failed resize retains history");
    passed &= check(window, 160, RGB(255, 0, 0), "failed resize retains center");
    if (FAILED(hr = IDXGISwapChain1_Present1(swapchain, 0, DXGI_PRESENT_TEST, &parameters))) goto fail;
    if (FAILED(hr = IDXGISwapChain1_ResizeBuffers(swapchain, 0, 400, 240, DXGI_FORMAT_UNKNOWN, 0))) goto fail;
    SetWindowPos(window, NULL, 0, 0, 400, 240, SWP_NOMOVE | SWP_NOZORDER);
    if (FAILED(hr = paint(blue, NULL, 0, FALSE)) || FAILED(hr = paint(yellow, NULL, 0, FALSE))
            || FAILED(hr = paint(green, &left, 1, FALSE))
            || FAILED(hr = paint(red, &resized_right, 1, FALSE))) goto fail;
    passed &= check(window, 32, RGB(0, 255, 0), "resized left");
    passed &= check(window, 160, RGB(255, 255, 0), "resized history center");
    passed &= check(window, 368, RGB(255, 0, 0), "resized right");
    printf("%s composition dirty rectangles\n", passed ? "PASS" : "FAIL");
    goto cleanup;

fail:
    printf("FAIL setup or present %#x\n", hr);
    passed = FALSE;

cleanup:
    if (held_buffer) ID3D11Texture2D_Release(held_buffer);
    if (target) IDCompositionTarget_SetRoot(target, NULL);
    if (composition) IDCompositionDevice_Commit(composition);
    if (visual) IDCompositionVisual_Release(visual);
    if (target) IDCompositionTarget_Release(target);
    if (composition) IDCompositionDevice_Release(composition);
    if (swapchain) IDXGISwapChain1_Release(swapchain);
    if (dxgi_device) IDXGIDevice_Release(dxgi_device);
    if (factory) IDXGIFactory2_Release(factory);
    if (context) ID3D11DeviceContext_Release(context);
    if (device) ID3D11Device_Release(device);
    DestroyWindow(window);
    return !passed;
}
