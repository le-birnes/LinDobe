/* dropdlg.c: public #43. IDropTarget::Drop opens a modal MessageBox, as Illustrator does ("Missing Fonts") when it
 * opens a dropped file. Without the fix winex11 handles no X events while the XdndDrop event is being dispatched, so
 * the box never receives input. The test presses Enter on the box; prints "PASS drop dialog answered" when the box
 * closes and Drop returns, "FAIL timeout" after 40 s.
 *   x86_64-w64-mingw32-gcc -O1 -o dropdlg.exe dropdlg.c -lole32 -luser32 -lgdi32 -luuid
 * Window at (100,100) 500x400. */
#define COBJMACROS
#include <windows.h>
#include <ole2.h>
#include <stdio.h>

static HWND main_wnd;
static HRESULT WINAPI qi(IDropTarget *t, REFIID r, void **o)
{ if (IsEqualIID(r, &IID_IUnknown) || IsEqualIID(r, &IID_IDropTarget)) { *o = t; return S_OK; } *o = NULL; return E_NOINTERFACE; }
static ULONG WINAPI addref(IDropTarget *t) { return 2; }
static ULONG WINAPI release(IDropTarget *t) { return 1; }
static HRESULT WINAPI enter(IDropTarget *t, IDataObject *d, DWORD k, POINTL pt, DWORD *e) { *e = DROPEFFECT_COPY; return S_OK; }
static HRESULT WINAPI over(IDropTarget *t, DWORD k, POINTL pt, DWORD *e) { *e = DROPEFFECT_COPY; return S_OK; }
static HRESULT WINAPI leave(IDropTarget *t) { return S_OK; }
static void CALLBACK watchdog(HWND h, UINT m, UINT_PTR id, DWORD t)
{ printf("FAIL timeout: the dialog opened in Drop got no input\n"); fflush(stdout); ExitProcess(1); }
static HRESULT WINAPI drop(IDropTarget *t, IDataObject *d, DWORD k, POINTL pt, DWORD *e)
{
    int r;
    printf("Drop at %ld,%ld: opening the dialog\n", pt.x, pt.y); fflush(stdout);
    SetTimer(NULL, 0, 40000, watchdog);
    r = MessageBoxA(main_wnd, "Dropped file needs fonts", "dropdlg-box", MB_OK);
    printf("PASS drop dialog answered (%d)\n", r); fflush(stdout);
    *e = DROPEFFECT_COPY; PostQuitMessage(0); return S_OK;
}
static IDropTargetVtbl vt = { qi, addref, release, enter, over, leave, drop };
static IDropTarget target = { &vt };

int main(void)
{
    WNDCLASSA wc = {0}; MSG m;
    SetProcessDPIAware(); OleInitialize(NULL);
    wc.lpfnWndProc = DefWindowProcA; wc.lpszClassName = "dropdlg"; wc.hbrBackground = GetStockObject(WHITE_BRUSH);
    RegisterClassA(&wc);
    main_wnd = CreateWindowA("dropdlg", "dropdlg", WS_OVERLAPPEDWINDOW | WS_VISIBLE, 100, 100, 500, 400, 0, 0, 0, 0);
    printf("RegisterDragDrop %#lx\n", RegisterDragDrop(main_wnd, &target)); fflush(stdout);
    SetTimer(main_wnd, 1, 90000, NULL);
    while (GetMessageA(&m, 0, 0, 0)) { if (m.message == WM_TIMER && m.hwnd == main_wnd) { printf("FAIL no drop\n"); break; } DispatchMessageA(&m); }
    RevokeDragDrop(main_wnd); OleUninitialize();
    return 0;
}
