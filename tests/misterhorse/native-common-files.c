#include <windows.h>
#include <stdio.h>

int main(void)
{
    WCHAR native[MAX_PATH], common[MAX_PATH];
    DWORD native_size = sizeof(native), common_size = sizeof(common), type;
    HKEY key;
    LSTATUS status;

    status = RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"Software\\Microsoft\\Windows\\CurrentVersion",
            0, KEY_QUERY_VALUE | KEY_WOW64_64KEY, &key);
    if (status != ERROR_SUCCESS) return 1;
    status = RegQueryValueExW(key, L"CommonW6432Dir", NULL, &type, (BYTE *)native, &native_size);
    if (status != ERROR_SUCCESS || type != REG_SZ || native_size < sizeof(WCHAR)
            || native_size > sizeof(native) || native[native_size / sizeof(WCHAR) - 1])
    {
        printf("FAIL CommonW6432Dir: status %lu\n", status);
        RegCloseKey(key);
        return 1;
    }
    status = RegQueryValueExW(key, L"CommonFilesDir", NULL, &type, (BYTE *)common, &common_size);
    RegCloseKey(key);
    if (status != ERROR_SUCCESS || type != REG_SZ || common_size < sizeof(WCHAR)
            || common_size > sizeof(common) || common[common_size / sizeof(WCHAR) - 1]
            || !native[0] || lstrcmpiW(native, common))
    {
        puts("FAIL native common-files aliases differ");
        return 1;
    }
    puts("PASS native common-files directory");
    return 0;
}
