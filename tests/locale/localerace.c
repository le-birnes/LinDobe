/* localerace.c: one thread switches the global C locale back and forth (setlocale), two others parse numbers with
 * sscanf and strtod on the thread's current locale. After Effects 2027's LUTEngine does this at start-up; Wine's
 * strtod/scanf fetched the locale, then called isspace(), whose own get_locinfo() dropped (freed) that locale after
 * the global one changed, while the parser still used it: lconv NULL -> crash in fpnum_parse_bnum (patch 0146).
 *   x86_64-w64-mingw32-gcc -O1 -o localerace.exe localerace.c -lucrtbase
 *   wine localerace.exe   ->  "PASS ..." or a crash / "FAIL ..." */
#include <windows.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>

static volatile LONG stop, bad;

static DWORD WINAPI flipper(void *arg)
{
    while (!stop)
    {
        setlocale(LC_ALL, "German_Germany.1252");    /* decimal comma */
        setlocale(LC_ALL, "C");
    }
    return 0;
}

static DWORD WINAPI parser(void *arg)
{
    int i;
    for (i = 0; i < 200000 && !stop; i++)
    {
        double d = 0, e;
        int n = sscanf("   7", " %lf", &d);          /* whitespace in the format and the input: isspace() */
        e = strtod("   8", NULL);                    /* leading blanks: _isspace_l() */
        if (n != 1 || d != 7 || e != 8) InterlockedIncrement(&bad);
    }
    return 0;
}

int main(void)
{
    HANDLE t[3];
    DWORD start = GetTickCount();
    t[0] = CreateThread(NULL, 0, flipper, NULL, 0, NULL);
    t[1] = CreateThread(NULL, 0, parser, NULL, 0, NULL);
    t[2] = CreateThread(NULL, 0, parser, NULL, 0, NULL);
    WaitForMultipleObjects(2, t + 1, TRUE, 60000);
    stop = 1;
    WaitForSingleObject(t[0], 5000);
    printf("%s localerace: %ld wrong results, %lu ms\n", bad ? "FAIL" : "PASS", bad, GetTickCount() - start);
    return bad != 0;
}
