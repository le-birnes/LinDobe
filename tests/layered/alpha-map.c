#include <windows.h>
#include <stdio.h>
#include <string.h>

static unsigned int paint_events;

static LRESULT CALLBACK window_proc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
{
    if (message == WM_PAINT)
    {
        PAINTSTRUCT paint;
        paint_events++;
        BeginPaint(hwnd, &paint);
        EndPaint(hwnd, &paint);
        return 0;
    }
    return DefWindowProcA(hwnd, message, wparam, lparam);
}

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

static int check_foreground(HWND hwnd)
{
    SetForegroundWindow(hwnd);
    pump(100);
    if (GetForegroundWindow() == hwnd) return 0;
    puts("FAIL window did not become foreground");
    return 1;
}

static int update(HWND overlay, HDC memory, DWORD *pixels, POINT position, unsigned int step, BOOL empty, BOOL managed)
{
    BLENDFUNCTION blend = {AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};
    SIZE size = {256, 256};
    POINT source = {0, 0};
    unsigned int x, y, width = step & 1 ? 72 : 56;

    memset(pixels, 0, 256 * 256 * sizeof(*pixels));
    if (managed && !empty)
        for (y = 0; y < 256; y++)
            for (x = 0; x < 256; x++) pixels[y * 256 + x] = 0xff303030;
    if (!empty)
        for (y = 2; y < 30; y++)
            for (x = 2; x < width; x++) pixels[y * 256 + x] = 0xff00ff00;

    if (UpdateLayeredWindow(overlay, NULL, &position, &size, memory, &source, 0, &blend, ULW_ALPHA))
        return 0;
    printf("FAIL UpdateLayeredWindow error=%lu\n", (unsigned long)GetLastError());
    return 1;
}

int main(int argc, char **argv)
{
    WNDCLASSA cls = {0};
    BITMAPINFO info = {0};
    HWND owner, overlay, alternate = NULL;
    HDC memory;
    HBITMAP bitmap, old;
    DWORD *pixels;
    POINT position;
    unsigned int phase, frame, repeat;
    BOOL managed = argc > 1 && !strcmp(argv[1], "--managed");
    DWORD ex_style = WS_EX_LAYERED | WS_EX_NOACTIVATE;
    DWORD style = WS_POPUP;
    int failed = 0;

    if (argc > 1 && (!managed || argc != 2)) return 2;
    if (managed) style |= WS_CAPTION;
    else ex_style |= WS_EX_TRANSPARENT | WS_EX_TOPMOST | WS_EX_TOOLWINDOW;
    setvbuf(stdout, NULL, _IONBF, 0);
    cls.lpfnWndProc = window_proc;
    cls.hInstance = GetModuleHandleA(NULL);
    cls.lpszClassName = "LinDobeOverlayProbe";
    cls.hbrBackground = GetStockObject(WHITE_BRUSH);
    cls.hCursor = LoadCursorA(NULL, (LPCSTR)IDC_ARROW);
    if (!RegisterClassA(&cls)) return 1;
    owner = CreateWindowExA(0, cls.lpszClassName, "LinDobe overlay probe: only the small green label should be visible",
                            WS_OVERLAPPEDWINDOW | WS_VISIBLE, 150, 150, 800, 600, NULL, NULL, cls.hInstance, NULL);
    if (!owner) return 1;
    if (managed)
    {
        alternate = CreateWindowExA(0, cls.lpszClassName, "LinDobe focus switch probe",
                                    WS_OVERLAPPEDWINDOW | WS_VISIBLE, 1100, 100, 400, 400,
                                    NULL, NULL, cls.hInstance, NULL);
        if (!alternate) return 1;
    }
    cls.lpszClassName = "LinDobeLayeredReadout";
    cls.hbrBackground = NULL;
    if (!RegisterClassA(&cls)) return 1;
    memory = CreateCompatibleDC(NULL);
    info.bmiHeader.biSize = sizeof(info.bmiHeader);
    info.bmiHeader.biWidth = 256;
    info.bmiHeader.biHeight = -256;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    bitmap = CreateDIBSection(memory, &info, DIB_RGB_COLORS, (void **)&pixels, NULL, 0);
    if (!bitmap) return 1;
    old = SelectObject(memory, bitmap);
    pump(500);
    if (managed) failed = check_foreground(owner);
    printf("MODE: %s\n", managed ? "managed layered popup with focus switches" : "unmanaged click-through readout");
    puts(managed ? "Expected: an opaque 256x256 popup containing a small green label."
                 : "Expected: a small green label; the remainder of its 256x256 window is transparent.");

    for (phase = 0; phase < 2 && !failed; phase++)
    {
        printf("PHASE %u: %s\n", phase + 1, phase ? "paint after mapping an empty window" : "paint before showing the window");
        for (repeat = 0; repeat < 3 && !failed; repeat++)
        {
            position.x = 70;
            position.y = 80;
            ClientToScreen(owner, &position);
            overlay = CreateWindowExA(ex_style, cls.lpszClassName, "Layered measurement label", style, position.x, position.y,
                                      256, 256, owner, NULL, cls.hInstance, NULL);
            if (!overlay) { failed = 1; break; }
            failed = update(overlay, memory, pixels, position, 0, phase != 0, managed);
            ShowWindow(overlay, SW_SHOWNOACTIVATE);
            if (phase) pump(150);
            for (frame = 0; frame < 60 && !failed; frame++)
            {
                position.x = 70 + frame * 2;
                position.y = 80 + frame;
                ClientToScreen(owner, &position);
                failed = update(overlay, memory, pixels, position, frame / 10, FALSE, managed);
                pump(30);
                if (!(frame % 10))
                {
                    RECT rect;
                    POINT inside = {position.x + 10, position.y + 12};
                    POINT outside = {position.x + 128, position.y + 128};
                    if (managed)
                    {
                        failed |= check_foreground(alternate);
                        failed |= check_foreground(owner);
                        if (GetAncestor(WindowFromPoint(inside), GA_ROOT) != overlay)
                        {
                            puts("FAIL opaque popup label lost hit testing after focus switch");
                            failed = 1;
                        }
                    }
                    if (!GetWindowRect(overlay, &rect) || rect.right - rect.left != 256 || rect.bottom - rect.top != 256)
                    {
                        puts("FAIL logical overlay dimensions changed");
                        failed = 1;
                    }
                    if (WindowFromPoint(outside) != (managed ? overlay : owner))
                    {
                        puts(managed ? "FAIL opaque popup body lost hit testing"
                                     : "FAIL transparent padding intercepted hit testing");
                        failed = 1;
                    }
                }
            }
            DestroyWindow(overlay);
            pump(150);
        }
    }
    printf("API checks: %s; paint_events=%u; display result requires user confirmation.\n", failed ? "FAIL" : "PASS", paint_events);
    SelectObject(memory, old);
    DeleteObject(bitmap);
    DeleteDC(memory);
    DestroyWindow(owner);
    if (alternate) DestroyWindow(alternate);
    return failed;
}
