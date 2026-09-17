/*
 * Copyright 2016 Chris Young <chris@unsatisfactorysoftware.co.uk>
 *
 * This file is part of NetSurf, http://www.netsurf-browser.org/
 *
 * NetSurf is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * NetSurf is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef AMIGA_MEMORY_H
#define AMIGA_MEMORY_H

#include <stddef.h>
#include <exec/types.h>
#ifndef __amigaos4__
#include <exec/memory.h>
#include <proto/exec.h>
#endif

/*
 * Memory policy (classic / OS3):
 *
 * - Normal CPU-side allocations use default Public memory with Fast
 *   preferred (ami_memory_allocvec / pools / clear_alloc). That keeps
 *   Chip free for graphics.
 * - Chip RAM is ONLY for blitter/graphics scratch "VRAM": TmpRas plane
 *   buffers, BltTemplate glyph planes, AllocRaster masks/pointers, and
 *   AGA friend BitMaps from graphics.library. Use ami_memory_chip_* or
 *   AllocRaster/AllocBitMap — never MEMF_CHIP for general data.
 *
 * clib malloc/calloc follow the same preference (Fast when available).
 */

/* Alloc/free chip memory (graphics/blitter buffers only) */
#ifdef __amigaos4__
#define ami_memory_chip_alloc(s) malloc(s)
#define ami_memory_chip_free(p) free(p)
#else
#define ami_memory_chip_alloc(s) AllocVec((s), MEMF_CHIP | MEMF_PUBLIC)
#define ami_memory_chip_free(p) FreeVec(p)
#endif

#ifndef __amigaos4__
/**
 * AllocVec for normal (non-Chip) data.
 * Prefers Fast; falls back to any Public (chip-only machines).
 * \param extra_flags e.g. MEMF_CLEAR, or 0
 */
APTR ami_memory_allocvec(ULONG size, ULONG extra_flags);
#endif

/* Alloc/free a block cleared to non-zero */
#ifdef __amigaos4__
#define ami_memory_clear_alloc(s,v) AllocVecTags(s, AVT_ClearWithValue, v, TAG_DONE)
#define ami_memory_clear_free(p) FreeVec(p)
#else
void *ami_memory_clear_alloc(size_t size, UBYTE value);
void ami_memory_clear_free(void *p);
#endif

/* Fixed-size item pools: OS4 ItemPool, OS3 Exec CreatePool */
#ifdef __amigaos4__
#define ami_memory_itempool_create(s) AllocSysObjectTags(ASOT_ITEMPOOL, \
		ASOITEM_MFlags, MEMF_PRIVATE, \
		ASOITEM_ItemSize, s, \
		ASOITEM_GCPolicy, ITEMGC_AFTERCOUNT, \
		ASOITEM_GCParameter, 100, \
		TAG_DONE)
#define ami_memory_itempool_delete(p) FreeSysObject(ASOT_ITEMPOOL, p)
#define ami_memory_itempool_alloc(p,s) ItemPoolAlloc(p)
#define ami_memory_itempool_free(p,i,s) ItemPoolFree(p,i)
#else
APTR ami_memory_itempool_create(ULONG item_size);
void ami_memory_itempool_delete(APTR pool);
APTR ami_memory_itempool_alloc(APTR pool, ULONG size);
void ami_memory_itempool_free(APTR pool, APTR item, ULONG size);
#endif

#ifndef __amigaos4__
void ami_memory_slab_dump(BPTR fh);
struct Interrupt *ami_memory_init(void);
void ami_memory_fini(struct Interrupt *memhandler);
void ami_memory_poll(void);
BOOL ami_memory_under_pressure(void);
void ami_memory_try_purge(void);
/**
 * Force-release unused llcache after a tab/window closes (content already
 * unreffed by browser_window_destroy). Safe to call from the main loop.
 */
void ami_memory_purge_after_close(void);
/**
 * Cap decoded image dimensions so one banner cannot eat half of Fast.
 * Adjusts *width / *height in place (aspect preserved).
 */
void ami_memory_cap_image_dims(int *width, int *height);
#endif

#endif /* AMIGA_MEMORY_H */
