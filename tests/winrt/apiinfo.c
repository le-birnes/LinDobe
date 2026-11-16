/* apiinfo.c: Windows.Foundation.Metadata.ApiInformation answers instead of E_NOTIMPL (patch 0180).
 * Lightroom (desktop, not Classic) asks IsTypePresent("Windows.Devices.Haptics.InputHapticsManager") at start-up
 * through C++/WinRT, which throws hresult_not_implemented on E_NOTIMPL and aborts its Organize module.
 *   x86_64-w64-mingw32-gcc -O1 -o apiinfo.exe apiinfo.c
 *   wine apiinfo.exe   ->  "PASS ..." or "FAIL ..." */
#define COBJMACROS
#include <windows.h>
#include <stdio.h>

typedef HRESULT (WINAPI *RoInitialize_t)(int);
typedef HRESULT (WINAPI *RoGetActivationFactory_t)(void *, REFIID, void **);
typedef HRESULT (WINAPI *WindowsCreateString_t)(const WCHAR *, UINT32, void **);

/* IApiInformationStatics: IUnknown + IInspectable (6 slots), then the ten queries in this order */
typedef HRESULT (WINAPI *q1_t)(void *, void *, BOOLEAN *);
typedef HRESULT (WINAPI *q2_t)(void *, void *, void *, BOOLEAN *);
typedef HRESULT (WINAPI *q2n_t)(void *, void *, void *, UINT32, BOOLEAN *);
typedef HRESULT (WINAPI *qm_t)(void *, void *, UINT16, BOOLEAN *);
typedef HRESULT (WINAPI *qmm_t)(void *, void *, UINT16, UINT16, BOOLEAN *);

static const GUID IID_IApiInformationStatics =
    {0x997439fe, 0xf681, 0x4a11, {0xb4, 0x16, 0xc1, 0x3a, 0x47, 0xe8, 0xba, 0x36}};

static WindowsCreateString_t pWindowsCreateString;
static int failures;

static void *hs(const WCHAR *s)
{
    void *h = NULL;
    pWindowsCreateString(s, lstrlenW(s), &h);
    return h;
}

static void check(const char *what, HRESULT hr, BOOLEAN got, BOOLEAN want)
{
    if (hr != S_OK || got != want)
    {
        printf("FAIL %s: hr %#lx, value %d (want S_OK, %d)\n", what, (unsigned long)hr, got, want);
        failures++;
    }
}

int main(void)
{
    HMODULE combase = LoadLibraryA("combase.dll");
    RoInitialize_t pRoInitialize = (void *)GetProcAddress(combase, "RoInitialize");
    RoGetActivationFactory_t pRoGetActivationFactory = (void *)GetProcAddress(combase, "RoGetActivationFactory");
    void *statics = NULL, **vt, *type, *member;
    BOOLEAN v;
    HRESULT hr;

    pWindowsCreateString = (void *)GetProcAddress(combase, "WindowsCreateString");
    pRoInitialize(1);
    hr = pRoGetActivationFactory(hs(L"Windows.Foundation.Metadata.ApiInformation"), &IID_IApiInformationStatics, &statics);
    if (FAILED(hr)) { printf("FAIL no ApiInformation statics, hr %#lx\n", (unsigned long)hr); return 1; }
    vt = *(void ***)statics;

    type = hs(L"Windows.Devices.Haptics.InputHapticsManager");
    member = hs(L"Foo");
    v = TRUE; hr = ((q1_t)vt[6])(statics, type, &v);           check("IsTypePresent(InputHapticsManager)", hr, v, FALSE);
    v = FALSE; hr = ((q1_t)vt[6])(statics, hs(L"Windows.Foundation.Metadata.ApiInformation"), &v);
    check("IsTypePresent(ApiInformation)", hr, v, TRUE);
    v = FALSE; hr = ((q1_t)vt[6])(statics, hs(L"Windows.Foundation.UniversalApiContract"), &v);
    check("IsTypePresent(UniversalApiContract)", hr, v, TRUE);
    v = TRUE; hr = ((q2_t)vt[7])(statics, type, member, &v);   check("IsMethodPresent", hr, v, FALSE);
    v = TRUE; hr = ((q2n_t)vt[8])(statics, type, member, 1, &v); check("IsMethodPresentWithArity", hr, v, FALSE);
    v = TRUE; hr = ((q2_t)vt[9])(statics, type, member, &v);   check("IsEventPresent", hr, v, FALSE);
    v = TRUE; hr = ((q2_t)vt[10])(statics, type, member, &v);  check("IsPropertyPresent", hr, v, FALSE);
    v = TRUE; hr = ((q2_t)vt[11])(statics, type, member, &v);  check("IsReadOnlyPropertyPresent", hr, v, FALSE);
    v = TRUE; hr = ((q2_t)vt[12])(statics, type, member, &v);  check("IsWriteablePropertyPresent", hr, v, FALSE);
    v = TRUE; hr = ((q2_t)vt[13])(statics, type, member, &v);  check("IsEnumNamedValuePresent", hr, v, FALSE);
    v = FALSE; hr = ((qmm_t)vt[15])(statics, hs(L"Windows.Foundation.UniversalApiContract"), 10, 0, &v);
    check("IsApiContractPresentByMajorAndMinor(Universal 10.0)", hr, v, TRUE);
    v = TRUE; hr = ((qmm_t)vt[15])(statics, hs(L"Windows.Foundation.UniversalApiContract"), 99, 0, &v);
    check("IsApiContractPresentByMajorAndMinor(Universal 99.0)", hr, v, FALSE);

    if (!failures) printf("PASS ApiInformation: 12 queries answered\n");
    return failures != 0;
}
