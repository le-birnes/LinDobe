#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

static BOOL late_parent;
static unsigned int failures;

static void pump(DWORD duration)
{
    DWORD end = GetTickCount() + duration;
    MSG message;

    do
    {
        while (PeekMessageA(&message, NULL, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&message);
            DispatchMessageA(&message);
        }
        Sleep(10);
    } while ((LONG)(end - GetTickCount()) > 0);
}

static LRESULT CALLBACK window_proc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
{
    if (message == WM_PAINT)
    {
        PAINTSTRUCT paint;
        BeginPaint(hwnd, &paint);
        EndPaint(hwnd, &paint);
        return 0;
    }
    return DefWindowProcA(hwnd, message, wparam, lparam);
}

static BOOL update_layered(HWND hwnd)
{
    BITMAPINFO info = {0};
    HDC dc = CreateCompatibleDC(NULL);
    DWORD *pixels;
    HBITMAP bitmap, previous;
    POINT position = {100, 100}, source = {0};
    SIZE size = {499, 43};
    BLENDFUNCTION blend = {AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};
    BOOL result;
    unsigned int i;

    info.bmiHeader.biSize = sizeof(info.bmiHeader);
    info.bmiHeader.biWidth = size.cx;
    info.bmiHeader.biHeight = -size.cy;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    bitmap = CreateDIBSection(dc, &info, DIB_RGB_COLORS, (void **)&pixels, NULL, 0);
    if (!bitmap) return FALSE;
    previous = SelectObject(dc, bitmap);
    for (i = 0; i < (unsigned int)(size.cx * size.cy); i++) pixels[i] = 0xff333333;
    result = UpdateLayeredWindow(hwnd, NULL, &position, &size, dc, &source, 0, &blend, ULW_ALPHA);
    SelectObject(dc, previous);
    DeleteObject(bitmap);
    DeleteDC(dc);
    return result;
}

static void inspect(HWND edit, unsigned int mode, unsigned int layered)
{
    HDC dc = GetDC(edit);
    RECT clip;
    int type = GetClipBox(dc, &clip);
    COLORREF pixel = GetPixel(dc, 4, 5);
    BOOL painted = mode != 2 || layered;
    char text[32];

    if (painted ? type == NULLREGION || pixel != RGB(255, 255, 255) : type != NULLREGION)
        failures++;

    GetWindowTextA(edit, text, sizeof(text));
    printf("parent=%s child_layered=%u clip_type=%d clip=%ld,%ld,%ld,%ld text=[%s] pixel=%08lx\n",
           mode == 0 ? "opaque" : mode == 1 ? "constant-alpha" : "per-pixel-alpha", layered,
           type, (long)clip.left, (long)clip.top, (long)clip.right, (long)clip.bottom, text,
           (unsigned long)pixel);
    ReleaseDC(edit, dc);
}

static void inspect_native(HWND parent, RECT rect, const char *phase)
{
    Display *display = XOpenDisplay(NULL);
    Window window = (Window)GetPropA(parent, "__wine_x11_whole_window");
    XImage *image;
    RECT outer;
    XWindowAttributes attrs;
    int left, top, right, bottom;
    unsigned int white = 0, dark = 0, x, y;

    if (!display || !window)
    {
        failures++;
        if (display) XCloseDisplay(display);
        return;
    }
    GetWindowRect(parent, &outer);
    XGetWindowAttributes(display, window, &attrs);
    left = MulDiv(rect.left - outer.left, attrs.width, outer.right - outer.left);
    top = MulDiv(rect.top - outer.top, attrs.height, outer.bottom - outer.top);
    right = MulDiv(rect.right - outer.left, attrs.width, outer.right - outer.left);
    bottom = MulDiv(rect.bottom - outer.top, attrs.height, outer.bottom - outer.top);
    image = XGetImage(display, window, left, top, right - left, bottom - top, AllPlanes, ZPixmap);
    if (image)
    {
        for (y = 0; y < (unsigned int)image->height; y++)
            for (x = 0; x < (unsigned int)image->width; x++)
            {
                unsigned long pixel = XGetPixel(image, x, y);
                if ((pixel & 0xffffff) == 0xffffff) white++;
                if (!(pixel & 0xffffff)) dark++;
            }
        BOOL background = !strcmp(phase, "old-location") || !strcmp(phase, "hidden") || !strcmp(phase, "destroyed") ||
                          !strcmp(phase, "old-container") || !strcmp(phase, "hidden-container") ||
                          !strcmp(phase, "reset-container");
        BOOL pass = background ? !white && !dark : white && dark;
        printf("native phase=%s white=%u dark=%u depth=%d check=%s\n", phase, white, dark, image->depth,
               pass ? "PASS" : "FAIL");
        if (!pass) failures++;
        XDestroyImage(image);
    }
    else failures++;
    XCloseDisplay(display);
}

static LONG stop_painter;

static DWORD WINAPI paint_worker(void *argument)
{
    HWND edit = argument;
    RECT spot = {80, 0, 82, 2};
    MSG message;
    unsigned int i;

    for (i = 0; i < 500 && !InterlockedCompareExchange(&stop_painter, 0, 0); i++)
    {
        HDC dc = GetDC(edit);
        if (dc)
        {
            FillRect(dc, &spot, GetStockObject(WHITE_BRUSH));
            ReleaseDC(edit, dc);
        }
        PeekMessageA(&message, NULL, 0, 0, PM_NOREMOVE);
        Sleep(1);
    }
    return 0;
}

static void test_threaded_paint(HWND edit)
{
    HANDLE worker;
    unsigned int i;

    InterlockedExchange(&stop_painter, 0);
    worker = CreateThread(NULL, 0, paint_worker, edit, 0, NULL);
    if (!worker)
    {
        failures++;
        return;
    }
    for (i = 0; i < 100; i++)
    {
        SetWindowPos(edit, NULL, 14 + (i & 1), 12, 0, 0, SWP_NOACTIVATE | SWP_NOZORDER | SWP_NOSIZE);
        pump(1);
    }
    InterlockedExchange(&stop_painter, 1);
    if (WaitForSingleObject(worker, 5000) != WAIT_OBJECT_0) failures++;
    CloseHandle(worker);
    SetWindowPos(edit, NULL, 14, 12, 0, 0, SWP_NOACTIVATE | SWP_NOZORDER | SWP_NOSIZE);
    RedrawWindow(edit, NULL, NULL, RDW_INVALIDATE | RDW_ERASE | RDW_UPDATENOW);
    pump(150);
}

static int test_children(HINSTANCE instance)
{
    unsigned int mode, layered;
    HFONT font = CreateFontA(-10, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                            0, 0, DEFAULT_QUALITY, 0, "Tahoma");

    for (mode = 0; mode < 3; mode++)
        for (layered = 0; layered < 2; layered++)
        {
            HWND parent = CreateWindowExA(mode ? WS_EX_LAYERED | WS_EX_DLGMODALFRAME | WS_EX_WINDOWEDGE : 0,
                                          "ContextRepro", "Context child test", WS_POPUP | WS_CLIPSIBLINGS,
                                          100, 100, 499, 43, NULL, NULL, instance, NULL);
            HWND container;
            HWND edit;
            if (mode == 2 && !late_parent) update_layered(parent);
            container = CreateWindowExA(0, "ContextRepro", "", 0x56020000,
                                             3, 3, 493, 37, parent, NULL, instance, NULL);
            edit = CreateWindowExA(layered ? WS_EX_LAYERED : 0, "Edit", "72 pt",
                                        WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL,
                                        14, 12, 101, 12, container, NULL, instance, NULL);
            if (!parent || !container || !edit) return 1;
            SendMessageA(edit, WM_SETFONT, (WPARAM)font, FALSE);
            if (layered && !SetLayeredWindowAttributes(edit, 0, 255, LWA_ALPHA))
                printf("FAIL child alpha error=%lu\n", (unsigned long)GetLastError());
            if (mode == 1) SetLayeredWindowAttributes(parent, 0, 255, LWA_ALPHA);
            if (mode == 2 && !update_layered(parent))
                printf("FAIL parent bitmap error=%lu\n", (unsigned long)GetLastError());
            ShowWindow(parent, SW_SHOWNOACTIVATE);
            RedrawWindow(parent, NULL, NULL, RDW_INVALIDATE | RDW_ALLCHILDREN | RDW_UPDATENOW);
            pump(300);
            inspect(edit, mode, layered);
            if (mode == 2 && layered)
            {
                RECT original, moved;
                unsigned int repeat;
                GetWindowRect(edit, &original);
                inspect_native(parent, original, "initial");
                SetWindowTextA(edit, "48");
                pump(150);
                inspect_native(parent, original, "text-change");
                for (repeat = 0; repeat < 5; repeat++)
                {
                    update_layered(parent);
                    pump(40);
                }
                inspect_native(parent, original, "parent-repaint");
                test_threaded_paint(edit);
                inspect_native(parent, original, "threaded-paint");
                SetWindowPos(edit, NULL, 0, 0, 150, 12, SWP_NOACTIVATE | SWP_NOZORDER | SWP_NOMOVE);
                pump(150);
                GetWindowRect(edit, &moved);
                inspect_native(parent, moved, "resized");
                SetWindowPos(edit, NULL, 0, 0, 101, 12, SWP_NOACTIVATE | SWP_NOZORDER | SWP_NOMOVE);
                pump(150);
                SetWindowPos(container, NULL, 300, 3, 0, 0, SWP_NOACTIVATE | SWP_NOZORDER | SWP_NOSIZE);
                pump(150);
                GetWindowRect(edit, &moved);
                inspect_native(parent, original, "old-container");
                inspect_native(parent, moved, "moved-container");
                ShowWindow(container, SW_HIDE);
                pump(150);
                inspect_native(parent, moved, "hidden-container");
                ShowWindow(container, SW_SHOWNOACTIVATE);
                pump(150);
                inspect_native(parent, moved, "shown-container");
                SetWindowPos(container, NULL, 3, 3, 0, 0, SWP_NOACTIVATE | SWP_NOZORDER | SWP_NOSIZE);
                pump(150);
                inspect_native(parent, moved, "reset-container");
                inspect_native(parent, original, "restored-container");
                SetWindowPos(edit, NULL, 180, 12, 0, 0, SWP_NOACTIVATE | SWP_NOZORDER | SWP_NOSIZE);
                pump(150);
                GetWindowRect(edit, &moved);
                inspect_native(parent, original, "old-location");
                inspect_native(parent, moved, "new-location");
                ShowWindow(edit, SW_HIDE);
                pump(150);
                inspect_native(parent, moved, "hidden");
                ShowWindow(edit, SW_SHOWNOACTIVATE);
                pump(150);
                inspect_native(parent, moved, "shown-again");
                DestroyWindow(edit);
                pump(150);
                inspect_native(parent, moved, "destroyed");
            }
            DestroyWindow(parent);
            pump(50);
        }
    DeleteObject(font);
    return 0;
}

static int test_move(HINSTANCE instance)
{
    Display *display = XOpenDisplay(NULL);
    HWND popup;
    Window window, child;
    RECT rect;
    POINT physical;
    int x, y;

    if (!display) return 1;
    popup = CreateWindowExA(WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, "ContextRepro", "Popup move test",
                            WS_POPUP, 100, 100, 300, 200, NULL, NULL, instance, NULL);
    ShowWindow(popup, SW_SHOWNOACTIVATE);
    pump(300);
    window = (Window)GetPropA(popup, "__wine_x11_whole_window");
    if (!window) return 1;
    {
        XWindowAttributes attrs;
        XGetWindowAttributes(display, window, &attrs);
        printf("popup override_redirect=%d\n", attrs.override_redirect);
        if (!attrs.override_redirect) failures++;
    }
    XMoveWindow(display, window, 100, 135);
    XSync(display, False);
    pump(300);
    GetWindowRect(popup, &rect);
    XTranslateCoordinates(display, window, DefaultRootWindow(display), 0, 0, &x, &y, &child);
    physical.x = rect.left;
    physical.y = rect.top;
    LogicalToPhysicalPointForPerMonitorDPI(popup, &physical);
    printf("popup native=%d,%d logical=%ld,%ld physical=%ld,%ld mismatch=%d\n", x, y,
           (long)rect.left, (long)rect.top, (long)physical.x, (long)physical.y,
           abs(x - physical.x) > 1 || abs(y - physical.y) > 1);
    if (abs(x - physical.x) > 1 || abs(y - physical.y) > 1) failures++;
    DestroyWindow(popup);
    XCloseDisplay(display);
    return 0;
}

int main(int argc, char **argv)
{
    WNDCLASSA cls = {0};
    int result;

    late_parent = argc > 1 && !strcmp(argv[1], "--late-parent");
    setvbuf(stdout, NULL, _IONBF, 0);
    cls.lpfnWndProc = window_proc;
    cls.hInstance = GetModuleHandleA(NULL);
    cls.lpszClassName = "ContextRepro";
    cls.hbrBackground = GetStockObject(DKGRAY_BRUSH);
    if (!RegisterClassA(&cls)) return 1;
    result = test_children(cls.hInstance);
    result |= test_move(cls.hInstance);
    return result || failures != 0;
}
