/*
 * Copyright 2026 amigazen project
 *
 * This file is part of NetSurf / Tsunami (MUI frontend).
 */

#ifndef MUI_LIBS_H
#define MUI_LIBS_H

#include <exec/types.h>
#include <intuition/classes.h>

struct Library;
struct IntuitionBase;
struct GfxBase;
struct DosLibrary;

extern struct Library *MUIMasterBase;
extern struct IntuitionBase *IntuitionBase;
extern struct GfxBase *GfxBase;
extern struct DosLibrary *DOSBase;
extern struct Library *UtilityBase;
extern struct Library *LocaleBase;
extern struct Library *DiskfontBase;
extern struct Library *TTEngineBase;
extern struct Library *CyberGfxBase;
extern struct Library *DataTypesBase;
extern struct Library *CodesetsBase;
extern struct Library *IconBase;
extern struct Library *LayersBase;

/*
 * Reaction images/bitmap.image — same path as Amiga FE (BitMapObj + AISS).
 * BitMapBase must be the OpenLibrary result for BITMAP_GetClass() inlines.
 */
extern struct Library *BitMapBase;
extern Class *BitMapClass;

#define BitMapObj	NewObject(BitMapClass, NULL
#define BitMapEnd	TAG_END)

/**
 * Open libraries required by Tsunami.
 *
 * \return TRUE on success, FALSE if a required library could not be opened.
 */
BOOL tsunami_libs_open(void);

/**
 * Close libraries opened by tsunami_libs_open.
 */
void tsunami_libs_close(void);

#endif /* MUI_LIBS_H */
