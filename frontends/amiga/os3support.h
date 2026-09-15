/*
 * Copyright 2010 John-Mark Bell <jmb@netsurf-browser.org>
 * Copyright 2014 Chris Young <chris@unsatisfactorysoftware.co.uk>
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

/** \file
 * Minimal AmigaOS 3 compatibility for APIs NetSurf still uses that are
 * OS4-only or absent from NDK 3.2. Prefer NDK3.2R4 headers for everything
 * else (ShowWindow, ESetInfo, OutlineFont, SaveDTObjectA, ReAction tags, …).
 */

#ifndef AMIGA_OS3SUPPORT_H_
#define AMIGA_OS3SUPPORT_H_

#include "amiga/vbcc_defs.h"

#ifndef __amigaos4__

#include <stdint.h>
#include <dirent.h>
#include <string.h>

#include <clib/compiler-specific.h>

#include <proto/exec.h>
#include <proto/dos.h>

#include <clib/alib_protos.h>

#ifndef EXEC_MEMORY_H
#include <exec/memory.h>
#endif

#include <devices/timer.h>
#include <graphics/gfx.h>
#include <intuition/classusr.h> /* Object */

/* Registerised calls: use NDK clib/compiler-specific.h (__reg for vbcc) */
#ifndef ASM
#define ASM __ASM__
#endif
#ifndef REG
#ifdef __VBCC__
#define REG(reg,arg) __reg(#reg) arg
#else
#define REG(reg,arg) __REG__(reg, arg)
#endif
#endif

#define MIN(a,b) (((a)<(b))?(a):(b))

/* vbcc has no GCC __builtin_expect */
#ifndef __builtin_expect
#define __builtin_expect(expr, value) (expr)
#endif

/*
 * NDK timeval uses tv_secs/tv_micro. Map POSIX names for frontend call sites.
 * Must come after any <time.h>/<dirent.h> include above so PosixLib's
 * struct timespec { time_t tv_sec; } is not rewritten during its parse.
 * PosixLib <sys/time.h> does the same when TIMERNAME is already set.
 */
#ifndef tv_sec
#define tv_sec tv_secs
#endif
#ifndef tv_usec
#define tv_usec tv_micro
#endif

#define IsMinListEmpty(L) (L)->mlh_Head->mln_Succ == 0
#define LIB_IS_AT_LEAST(B,V,R) ((B)->lib_Version>(V)) || \
	((B)->lib_Version==(V) && (B)->lib_Revision>=(R))
#define EAD_IS_FILE(E) ((E)->ed_Type<0)

#ifndef MEMF_PRIVATE
#define MEMF_PRIVATE	MEMF_ANY
#endif
#ifndef MEMF_SHARED
#define MEMF_SHARED	MEMF_ANY
#endif

/*
 * Tag / constant fallbacks — only for symbols NDK3.2R4 does not define.
 * Do not stub names that exist in NDK (e.g. TNA_CloseGadget, ShowWindow):
 * #ifndef on a function name is always true and a later #define breaks protos.
 */
#ifndef ASO_NoTrack
#define ASO_NoTrack			TAG_IGNORE
#endif
#ifndef BLITA_UseSrcAlpha
#define BLITA_UseSrcAlpha		TAG_IGNORE
#endif
#ifndef BLITA_MaskPlane
#define BLITA_MaskPlane			TAG_IGNORE
#endif
#ifndef PDTA_PromoteMask
#define PDTA_PromoteMask		TAG_IGNORE
#endif
#ifndef RPTAG_APenColor
#define RPTAG_APenColor			TAG_IGNORE
#endif
#ifndef GA_ContextMenu
#define GA_ContextMenu			TAG_IGNORE
#endif
#ifndef GA_HintInfo
#define GA_HintInfo			TAG_IGNORE
#endif
#ifndef GAUGEIA_Level
#define GAUGEIA_Level			TAG_IGNORE
#endif
#ifndef IA_InBorder
#define IA_InBorder			TAG_IGNORE
#endif
#ifndef SA_Compositing
#define SA_Compositing			TAG_IGNORE
#endif
#ifndef WA_ContextMenuHook
#define WA_ContextMenuHook		TAG_IGNORE
#endif
#ifndef WA_ToolBox
#define WA_ToolBox			TAG_IGNORE
#endif

/* raw keycodes */
#ifndef RAWKEY_BACKSPACE
#define RAWKEY_BACKSPACE	0x41
#endif
#ifndef RAWKEY_TAB
#define RAWKEY_TAB	0x42
#endif
#ifndef RAWKEY_ESC
#define RAWKEY_ESC	0x45
#endif
#ifndef RAWKEY_DEL
#define RAWKEY_DEL	0x46
#endif
#ifndef RAWKEY_PAGEUP
#define RAWKEY_PAGEUP	0x48
#endif
#ifndef RAWKEY_PAGEDOWN
#define RAWKEY_PAGEDOWN	0x49
#endif
#ifndef RAWKEY_CRSRUP
#define RAWKEY_CRSRUP	0x4C
#endif
#ifndef RAWKEY_CRSRDOWN
#define RAWKEY_CRSRDOWN	0x4D
#endif
#ifndef RAWKEY_CRSRRIGHT
#define RAWKEY_CRSRRIGHT	0x4E
#endif
#ifndef RAWKEY_CRSRLEFT
#define RAWKEY_CRSRLEFT	0x4F
#endif
#ifndef RAWKEY_F5
#define RAWKEY_F5	0x54
#endif
#ifndef RAWKEY_F8
#define RAWKEY_F8	0x57
#endif
#ifndef RAWKEY_F9
#define RAWKEY_F9	0x58
#endif
#ifndef RAWKEY_F10
#define RAWKEY_F10	0x59
#endif
#ifndef RAWKEY_F12
#define RAWKEY_F12	0x6F
#endif
#ifndef RAWKEY_HELP
#define RAWKEY_HELP	0x5F
#endif
#ifndef RAWKEY_HOME
#define RAWKEY_HOME	0x70
#endif
#ifndef RAWKEY_END
#define RAWKEY_END	0x71
#endif

#ifndef DISABLEDTEXTPEN
#define DISABLEDTEXTPEN HIGHLIGHTTEXTPEN
#endif
#ifndef TITLEPEN
#define TITLEPEN FILLPEN
#endif

#ifndef DN_FULLPATH
#define DN_FULLPATH 0
#endif
#ifndef BGBACKFILL
#define BGBACKFILL JAM1
#endif
#ifndef ML_SEPARATOR
#define ML_SEPARATOR NM_BARLABEL
#endif
/* LBS_ROWS is defined by NDK gadgets/listbrowser.h — do not stub it */

/* BVS_DISPLAY is defined by NDK images/bevel.h — do not stub it */

#ifndef AnchorPathOld
#define AnchorPathOld AnchorPath
#endif

#ifndef GetFileEnd
#define GetFileEnd End
#endif
#ifndef GetFontEnd
#define GetFontEnd End
#endif
#ifndef GetScreenModeEnd
#define GetScreenModeEnd End
#endif

#ifndef MINTERM_SRCMASK
#define MINTERM_SRCMASK (ABC|ABNC|ANBC)
#endif

/* application.library (OS4) */
#ifndef Notify
#define Notify(...) (void)0
#endif

/* DOS OS4 helpers */
#ifndef AllocSysObjectTags
#define AllocSysObjectTags(A,B,C,D) CreateMsgPort()
#endif
#ifndef FOpen
#define FOpen(A,B,C) Open(A,B)
#endif
#ifndef FClose
#define FClose(A) Close(A)
#endif
#ifndef CreateDirTree
#define CreateDirTree(D) CreateDir(D)
#endif
#ifndef SetCurrentDir
#define SetCurrentDir(L) CurrentDir(L)
#endif
#ifndef DevNameFromLock
#define DevNameFromLock(A,B,C,D) NameFromLock(A,B,C)
#endif

/* Exec */
#ifndef FindIName
#define FindIName FindName
#endif

/* OS4 I* Method naming → classic alib DoMethod* (NDK has DoMethod, not IDoMethod) */
#ifndef ICoerceMethod
#define ICoerceMethod CoerceMethod
#endif
#ifndef IDoMethod
#define IDoMethod DoMethod
#endif
#ifndef IDoMethodA
#define IDoMethodA DoMethodA
#endif
#ifndef IDoSuperMethodA
#define IDoSuperMethodA DoSuperMethodA
#endif

/* Utility */
#ifndef SetMem
#define SetMem memset
#endif

typedef int8_t int8;
typedef uint8_t uint8;
typedef int16_t int16;
typedef uint16_t uint16;
typedef int32_t int32;
typedef uint32_t uint32;
typedef int64_t int64;
typedef uint64_t uint64;

/* BackFillMessage — Rectangle from graphics/gfx.h */
struct BackFillMessage {
	struct Layer *Layer;
	struct Rectangle Bounds;
	LONG OffsetX;
	LONG OffsetY;
};

/* icon.library v51 (AfA_OS) */
#ifndef ICONCTRLA_SetImageDataFormat
#define ICONCTRLA_SetImageDataFormat	(ICONA_Dummy + 0x67)
#endif
#ifndef ICONCTRLA_GetImageDataFormat
#define ICONCTRLA_GetImageDataFormat	(ICONA_Dummy + 0x68)
#endif
#ifndef IDFMT_BITMAPPED
#define IDFMT_BITMAPPED		(0)
#endif
#ifndef IDFMT_PALETTEMAPPED
#define IDFMT_PALETTEMAPPED	(1)
#endif
#ifndef IDFMT_DIRECTMAPPED
#define IDFMT_DIRECTMAPPED	(2)
#endif

enum {
	ASOT_PORT = 1,
	ASOT_IOREQUEST
};

enum {
	TDRIMAGE_ERROR = 1,
	TDRIMAGE_WARNING
};

/* Provided by os3support.c — not in NDK 3.2 */
int64 GetFileSize(BPTR fh);
void FreeSysObject(ULONG type, APTR obj);

struct Node *GetHead(struct List *list);
struct Node *GetPred(struct Node *node);
struct Node *GetSucc(struct Node *node);

uint32 GetAttrs(Object *obj, Tag tag1, ...);
ULONG RefreshSetGadgetAttrs(struct Gadget *g, struct Window *w, struct Requester *r, Tag tag1, ...);
ULONG RefreshSetGadgetAttrsA(struct Gadget *g, struct Window *w, struct Requester *r, struct TagItem *tags);

char *ASPrintf(const char *fmt, ...);
char *strlwr(char *str);

/* PosixLib exposes __socket_select; NetSurf calls waitselect() */
int waitselect(int nfds, void *readfds, void *writefds, void *exceptfds,
	void *timeout, unsigned long *sigmask);

struct utsname;
int uname(struct utsname *buf);

/* Softfloat/PosixLib may declare this; provide if the libc lacks it */
float ceilf(float x);

#endif /* !__amigaos4__ */
#endif /* AMIGA_OS3SUPPORT_H_ */
