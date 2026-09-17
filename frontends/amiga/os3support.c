/*
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
 * Compatibility functions for AmigaOS 3
 */

#ifndef __amigaos4__
#include "os3support.h"

#include <inttypes.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/dos.h>
#include <proto/utility.h>

#include <intuition/gadgetclass.h>

#include "utils/log.h"

#define SUCCESS (TRUE)
#define FAILURE (FALSE)
#define NO      !

/* Utility */
struct FormatContext
{
	STRPTR	Index;
	LONG	Size;
	BOOL	Overflow;
};

STATIC VOID ASM
StuffChar(
	REG(a3, struct FormatContext *	Context),
	REG(d0, UBYTE Char))
{
	/* Is there still room? */
	if(Context->Size > 0)
	{
		(*Context->Index) = Char;

		Context->Index++;
		Context->Size--;

		/* Is there only a single character left? */
		if(Context->Size == 1)
		{
			/* Provide null-termination. */
			(*Context->Index) = '\0';

			/* Don't store any further characters. */
			Context->Size = 0;
		}
	}
	else
	{
		Context->Overflow = TRUE;
	}
}

BOOL
VSPrintfN(
	LONG			MaxLen,
	STRPTR			Buffer,
	const STRPTR	FormatString,
	const va_list	VarArgs)
{
	BOOL result = FAILURE;

	/* format a text, but place only up to MaxLen
	 * characters in the output buffer (including
	 * the terminating NUL)
	 */

	if (Buffer == NULL || FormatString == NULL) return(result);

	if(MaxLen > 1)
	{
		struct FormatContext Context;

		Context.Index		= Buffer;
		Context.Size		= MaxLen;
		Context.Overflow	= FALSE;

		RawDoFmt(FormatString,(APTR)VarArgs,(VOID (*)())StuffChar,(APTR)&Context);

		if(NO Context.Overflow)
			result = SUCCESS;
	}

	return(result);
}

BOOL
SPrintfN(
	LONG			MaxLen,
	STRPTR			Buffer,
	const STRPTR	FormatString,
					...)
{
	va_list VarArgs;
	BOOL result = FAILURE;

	/* format a text, varargs version */

	if (Buffer == NULL && FormatString == NULL) return result;

	va_start(VarArgs,FormatString);
	result = VSPrintfN(MaxLen,Buffer,FormatString,VarArgs);
	va_end(VarArgs);

	return(result);
}

char *ASPrintf(const char *fmt, ...)
{
  int r;
  va_list ap;
  static char buffer[2048];
  char *rbuf;
  
  va_start(ap, fmt);
  r = VSPrintfN(2048, buffer, (const STRPTR)fmt, ap);
  va_end(ap);

	r = strlen(buffer);
	/* Prefer Fast — ASPrintf strings are not graphics buffers */
	rbuf = AllocVec(r+1, MEMF_PUBLIC | MEMF_FAST | MEMF_CLEAR);
	if (rbuf == NULL) {
		rbuf = AllocVec(r+1, MEMF_PUBLIC | MEMF_CLEAR);
	}
	if (rbuf != NULL)
	{
		strncpy(rbuf, buffer, r);
	}
	return rbuf;
}

/* C */
char *strlwr(char *str)
{
  size_t i;
  size_t len = strlen(str);

  for(i=0; i<len; i++)
  str[i] = tolower((unsigned char)str[i]);

  return str;
}

char *strsep(char **s1, const char *s2)
{
	char *const p1 = *s1;

	if (p1 != NULL) {
		*s1 = strpbrk(p1, s2);
		if (*s1 != NULL) {
			*(*s1)++ = '\0';
		}
	}
	return p1;
}

long long int strtoll(const char *nptr, char **endptr, int base)
{
	return (long long int)strtol(nptr, endptr, base);
}

/* Diskfont OutlineFont / ESetInfo come from NDK3.2R4 diskfont.library */

/* DOS */
int64 GetFileSize(BPTR fh)
{
	int32 size = 0;
	struct FileInfoBlock *fib = AllocDosObject(DOS_FIB, NULL);
	if(fib == NULL) return 0;

	ExamineFH(fh, fib);
	size = fib->fib_Size;

	FreeDosObject(DOS_FIB, fib);
	return (int64)size;
}

void FreeSysObject(ULONG type, APTR obj)
{
	switch(type) {
		case ASOT_PORT:
			DeleteMsgPort(obj);
		break;
		case ASOT_IOREQUEST:
			DeleteIORequest(obj);
		break;
	}
}


/* Exec */
struct Node *GetHead(struct List *list)
{
	struct Node *res = NULL;

	if ((NULL != list) && (NULL != list->lh_Head->ln_Succ))
	{
		res = list->lh_Head;
	}
	return res;
}

struct Node *GetPred(struct Node *node)
{
	if (node->ln_Pred->ln_Pred == NULL) return NULL;
	return node->ln_Pred;
}

struct Node *GetSucc(struct Node *node)
{
	if (node->ln_Succ->ln_Succ == NULL) return NULL;
	return node->ln_Succ;
}


/* Intuition */
uint32 GetAttrs(Object *obj, Tag tag1, ...)
{
	va_list ap;
	Tag tag = tag1;
	ULONG data = 0;
	int i = 0;

	va_start(ap, tag1);

	while(tag != TAG_DONE) {
		data = va_arg(ap, ULONG);
		i += GetAttr(tag, obj, (void *)data);
		tag = va_arg(ap, Tag);
	}
	va_end(ap);

	return i;
}

ULONG RefreshSetGadgetAttrsA(struct Gadget *g, struct Window *w, struct Requester *r, struct TagItem *tags)
{
	ULONG retval;
	BOOL changedisabled = FALSE;
	BOOL disabled;
	struct TagItem *ti;

	if (w) {
		if ((ti = FindTagItem(GA_Disabled,tags)) && (ti->ti_Data != FALSE)) {
			changedisabled = TRUE;
 			disabled = g->Flags & GFLG_DISABLED;
 		}
 	}
	retval = SetGadgetAttrsA(g,w,r,tags);
	if (w && (retval || (changedisabled && disabled != (g->Flags & GFLG_DISABLED)))) {
		RefreshGList(g,w,r,1);
		retval = 1;
	}
	return retval;
}

ULONG RefreshSetGadgetAttrs(struct Gadget *g, struct Window *w, struct Requester *r, Tag tag1, ...)
{
	return RefreshSetGadgetAttrsA(g,w,r,(struct TagItem *) &tag1);
}

APTR NewObject(struct IClass * classPtr, CONST_STRPTR classID, ULONG tagList, ...)
{
	return NewObjectA(classPtr, classID, (const struct TagItem *) &tagList);
}

/* PosixLib internal; maps to bsdsocket WaitSelect when available */
extern int __socket_select(int nfds, void *rd, void *wr, void *ex,
	void *timeout, unsigned long *sigmask);

int waitselect(int nfds, void *readfds, void *writefds, void *exceptfds,
	void *timeout, unsigned long *sigmask)
{
	return __socket_select(nfds, readfds, writefds, exceptfds, timeout, sigmask);
}

#include "utils/utsname.h"

int uname(struct utsname *buf)
{
	if (buf == NULL)
		return -1;
	strcpy(buf->sysname, "AmigaOS");
	strcpy(buf->nodename, "amiga");
	strcpy(buf->release, "3");
	strcpy(buf->version, "3.2");
	strcpy(buf->machine, "m68k");
	return 0;
}

float ceilf(float x)
{
	long i;

	i = (long)x;
	if ((float)i == x)
		return x;
	if (x > 0.0f)
		return (float)(i + 1);
	return (float)i;
}

#endif

