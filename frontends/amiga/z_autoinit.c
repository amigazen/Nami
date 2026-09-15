/*
 * z_autoinit.c - ZBase + lazy OpenLibrary for vbcc z.library stubs.
 * Vendored from z.library autoinit_z_base.c for the Amiga NetSurf build.
 */

#define __USE_SYSBASE

#include <exec/types.h>
#include <proto/exec.h>

#ifndef __NOLIBBASE__
struct Library *ZBase = NULL;
#endif

static struct Library *ZBaseAuto;

int z_stub_ensure_open(void)
{
    if (ZBase != NULL) {
        return 1;
    }
    ZBaseAuto = OpenLibrary((STRPTR)"z.library", 2);
    ZBase = ZBaseAuto;
    return (ZBase != NULL);
}

void _INIT_5_ZBase(void)
{
    (void)z_stub_ensure_open();
}

void _EXIT_5_ZBase(void)
{
    if (ZBaseAuto != NULL) {
        CloseLibrary(ZBaseAuto);
        ZBaseAuto = NULL;
        ZBase = NULL;
    }
}
