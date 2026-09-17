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

#ifndef __amigaos4__
#include <proto/dos.h>
#include <proto/exec.h>
#include <exec/interrupts.h>
#include <exec/memory.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "amiga/memory.h"
#include "amiga/os3support.h"
#include "content/llcache.h"
#include "utils/log.h"

enum {
	PURGE_NONE = 0,
	PURGE_STEP1,
	PURGE_STEP2,
	PURGE_DONE
};
static int low_mem_status = PURGE_NONE;

/* When largest Public block drops below this, purge caches proactively. */
#define AMI_MEM_PUBLIC_FLOOR	(384UL * 1024UL)
/* Chip floor — leave room for TmpRas / layers on AGA. */
#define AMI_MEM_CHIP_FLOOR	(96UL * 1024UL)

/**
 * Normal AllocVec: prefer Fast so Chip stays for graphics/blitter buffers.
 * Fall back to any Public memory on chip-only machines or Fast exhaustion.
 */
APTR ami_memory_allocvec(ULONG size, ULONG extra_flags)
{
	APTR p;

	p = AllocVec(size, MEMF_PUBLIC | MEMF_FAST | extra_flags);
	if (p == NULL) {
		p = AllocVec(size, MEMF_PUBLIC | extra_flags);
	}
	return p;
}

/*
 * Plain AllocVec + memset — no tracking header.
 * Prepended magic headers + FreeVec(h) / FreePooled(h) were a MemList
 * hazard whenever any caller passed a non-wrapped pointer.
 */
void *ami_memory_clear_alloc(size_t size, UBYTE value)
{
	void *p;

	p = ami_memory_allocvec((ULONG)size, 0);
	if (p == NULL) {
		return NULL;
	}
	memset(p, value, size);
	return p;
}

void ami_memory_clear_free(void *p)
{
	if (p != NULL) {
		FreeVec(p);
	}
}

/* Exec memory pools for fixed-size item allocations (bitmap structs, pens, etc.) */
APTR ami_memory_itempool_create(ULONG item_size)
{
	ULONG puddle;
	APTR pool;

	puddle = item_size * 32UL;
	if (puddle < 4096UL) {
		puddle = 4096UL;
	}

	/* thresh == puddle so AllocPooled pulls from puddles for these sizes.
	 * Prefer Fast puddles; fall back if the machine has no Fast. */
	pool = CreatePool(MEMF_PUBLIC | MEMF_FAST, puddle, puddle);
	if (pool == NULL) {
		pool = CreatePool(MEMF_PUBLIC, puddle, puddle);
	}
	return pool;
}

void ami_memory_itempool_delete(APTR pool)
{
	if (pool != NULL) {
		DeletePool(pool);
	}
}

APTR ami_memory_itempool_alloc(APTR pool, ULONG size)
{
	if (pool == NULL) {
		return ami_memory_clear_alloc(size, 0);
	}
	/* Direct AllocPooled — size passed to FreePooled must match exactly. */
	return AllocPooled(pool, size);
}

void ami_memory_itempool_free(APTR pool, APTR item, ULONG size)
{
	if (item == NULL) {
		return;
	}
	if (pool == NULL) {
		FreeVec(item);
		return;
	}
	FreePooled(pool, item, size);
}

void ami_memory_slab_dump(BPTR fh)
{
	char line[160];

	sprintf(line,
		"Exec memory: largest=%lu avail=%lu chip largest=%lu avail=%lu\n",
		(unsigned long)AvailMem(MEMF_ANY | MEMF_LARGEST),
		(unsigned long)AvailMem(MEMF_ANY),
		(unsigned long)AvailMem(MEMF_CHIP | MEMF_LARGEST),
		(unsigned long)AvailMem(MEMF_CHIP));

	if (fh != 0) {
		FPuts(fh, line);
	} else {
		NSLOG(netsurf, INFO, "%s", line);
	}
}

/**
 * True when Public or Chip free space is dangerously low for another
 * page of images / plot tiles.
 */
BOOL ami_memory_under_pressure(void)
{
	ULONG pub;
	ULONG chip;

	pub = AvailMem(MEMF_ANY | MEMF_LARGEST);
	chip = AvailMem(MEMF_CHIP | MEMF_LARGEST);
	if (pub < AMI_MEM_PUBLIC_FLOOR) {
		return TRUE;
	}
	if (chip < AMI_MEM_CHIP_FLOOR) {
		return TRUE;
	}
	return FALSE;
}

/* Low memory handler — purge llcache (main task only). */
static void ami_memory_low_mem_handler(void *p)
{
	ULONG before;
	ULONG after;

	(void)p;

	before = AvailMem(MEMF_ANY | MEMF_LARGEST);

	if (low_mem_status == PURGE_STEP1) {
		NSLOG(netsurf, INFO,
		      "Purging llcache (step1) largest_public=%lu",
		      (unsigned long)before);
		llcache_clean(true);
		after = AvailMem(MEMF_ANY | MEMF_LARGEST);
		NSLOG(netsurf, INFO,
		      "llcache purge done largest_public %lu -> %lu",
		      (unsigned long)before, (unsigned long)after);
		low_mem_status = PURGE_STEP2;
		return;
	}

	if (low_mem_status == PURGE_STEP2) {
		NSLOG(netsurf, INFO,
		      "Purging llcache (step2 force) largest_public=%lu",
		      (unsigned long)before);
		llcache_clean(true);
		after = AvailMem(MEMF_ANY | MEMF_LARGEST);
		NSLOG(netsurf, INFO,
		      "llcache force purge done largest_public %lu -> %lu",
		      (unsigned long)before, (unsigned long)after);
		low_mem_status = PURGE_DONE;
	}
}

/**
 * Cap intrinsic image size for classic Amiga RAM budgets.
 * A 1920×64 banner alone is ~480KB RGBA — scale to screen-ish width.
 */
void ami_memory_cap_image_dims(int *width, int *height)
{
	int ow;
	int oh;
	int w;
	int h;
	int max_w;
	int max_h;
	ULONG avail;
	ULONG max_pixels;

	if (width == NULL || height == NULL) {
		return;
	}
	ow = *width;
	oh = *height;
	if (ow <= 0 || oh <= 0) {
		return;
	}

	avail = AvailMem(MEMF_ANY | MEMF_LARGEST);
	max_w = 640;
	max_h = 512;
	max_pixels = 640UL * 160UL; /* ~400KB RGBA */

	if (avail < 1024UL * 1024UL) {
		max_w = 480;
		max_h = 360;
		max_pixels = 400UL * 120UL;
	}
	if (avail < 512UL * 1024UL) {
		max_w = 320;
		max_h = 256;
		max_pixels = 256UL * 96UL;
	}

	w = ow;
	h = oh;

	if (w > max_w) {
		h = (oh * max_w) / ow;
		w = max_w;
		if (h < 1) {
			h = 1;
		}
	}
	if (h > max_h) {
		w = (ow * max_h) / oh;
		h = max_h;
		if (w < 1) {
			w = 1;
		}
		if (w > max_w) {
			h = (h * max_w) / w;
			w = max_w;
			if (h < 1) {
				h = 1;
			}
		}
	}
	while (((ULONG)w * (ULONG)h) > max_pixels && (w > 1 || h > 1)) {
		w = (w * 9) / 10;
		h = (h * 9) / 10;
		if (w < 1) {
			w = 1;
		}
		if (h < 1) {
			h = 1;
		}
	}

	if (w == ow && h == oh) {
		return;
	}

	NSLOG(netsurf, INFO,
	      "cap image %dx%d -> %dx%d (avail_largest=%lu)",
	      ow, oh, w, h, (unsigned long)avail);
	*width = w;
	*height = h;
}

/**
 * Run pending llcache purge steps immediately (before a large alloc).
 */
void ami_memory_try_purge(void)
{
	if (low_mem_status == PURGE_NONE && ami_memory_under_pressure()) {
		low_mem_status = PURGE_STEP1;
	}
	while (low_mem_status == PURGE_STEP1 || low_mem_status == PURGE_STEP2) {
		ami_memory_low_mem_handler(NULL);
	}
}

/**
 * Tab/window close: drop unused cached objects so their Fast RAM returns
 * promptly rather than waiting for background hlcache housekeeping.
 */
void ami_memory_purge_after_close(void)
{
	ULONG before;
	ULONG after;

	before = AvailMem(MEMF_ANY | MEMF_LARGEST);
	llcache_clean(true);
	after = AvailMem(MEMF_ANY | MEMF_LARGEST);
	NSLOG(netsurf, INFO,
	      "purge after close: largest_public %lu -> %lu",
	      (unsigned long)before, (unsigned long)after);
	low_mem_status = PURGE_NONE;
}

/* Run from the main task only — never from the mem-handler interrupt. */
void ami_memory_poll(void)
{
	if (low_mem_status == PURGE_NONE && ami_memory_under_pressure()) {
		NSLOG(netsurf, WARNING,
		      "memory pressure: pub_largest=%lu chip_largest=%lu — scheduling purge",
		      (unsigned long)AvailMem(MEMF_ANY | MEMF_LARGEST),
		      (unsigned long)AvailMem(MEMF_CHIP | MEMF_LARGEST));
		low_mem_status = PURGE_STEP1;
	}

	if (low_mem_status == PURGE_STEP1 || low_mem_status == PURGE_STEP2) {
		ami_memory_low_mem_handler(NULL);
	}

	if (low_mem_status == PURGE_DONE &&
	    ami_memory_under_pressure() == FALSE) {
		low_mem_status = PURGE_NONE;
	}
}

static ASM ULONG ami_memory_handler(REG(a0, struct MemHandlerData *mhd),
		REG(a1, void *userdata), REG(a6, struct ExecBase *execbase))
{
	(void)mhd;
	(void)userdata;
	(void)execbase;

	if (low_mem_status == PURGE_DONE) {
		low_mem_status = PURGE_NONE;
		return MEM_ALL_DONE;
	}

	if (low_mem_status == PURGE_NONE) {
		low_mem_status = PURGE_STEP1;
	}

	/*
	 * Do not call ami_schedule() here: mem handlers run in Exec's
	 * allocation path / interrupt-like context. Scheduling caused an
	 * infinite MEM_TRY_AGAIN hang when full-screen chip allocs failed.
	 * Fail this allocation; ami_memory_poll() purges from the main loop.
	 */
	return MEM_ALL_DONE;
}

struct Interrupt *ami_memory_init(void)
{
	struct Interrupt *memhandler = malloc(sizeof(struct Interrupt));
	if (memhandler == NULL) {
		return NULL;
	}

	memhandler->is_Node.ln_Pri = -127;
	memhandler->is_Node.ln_Name = "Nami low memory handler";
	memhandler->is_Data = NULL;
	memhandler->is_Code = (APTR)&ami_memory_handler;
	AddMemHandler(memhandler);

	return memhandler;
}

void ami_memory_fini(struct Interrupt *memhandler)
{
	if (memhandler != NULL) {
		RemMemHandler(memhandler);
		free(memhandler);
	}
}

#endif
