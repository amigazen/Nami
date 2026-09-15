/*
 * SPDX-License-Identifier: BSD-2-Clause
 * 
 * Copyright (c) 2025 amigazen project
 */

#ifndef _ICONV_H
#define _ICONV_H

#include <stddef.h>
#include <errno.h>

/* Define iconv_t as an opaque pointer type */
typedef void *iconv_t;

/* Get size_t declaration */
#include <stddef.h>

/* Get errno declaration and values */
#include <errno.h>

#ifndef EILSEQ
#define EILSEQ ENOENT
#endif
#ifndef E2BIG
#define E2BIG ENOMEM
#endif
#ifndef EBADF
#define EBADF EINVAL
#endif

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Allocates descriptor for code conversion from encoding 'fromcode' to
 * encoding 'tocode'. Returns (iconv_t)-1 on error.
 */
iconv_t iconv_open(const char *tocode, const char *fromcode);

/*
 * Converts, using conversion descriptor 'cd', at most '*inbytesleft' bytes
 * starting at '*inbuf', writing at most '*outbytesleft' bytes starting at
 * '*outbuf'.
 * 
 * Decrements '*inbytesleft' and increments '*inbuf' by the same amount.
 * Decrements '*outbytesleft' and increments '*outbuf' by the same amount.
 * 
 * Returns the number of non-reversible conversions performed, or (size_t)-1
 * on error (with errno set appropriately).
 */
size_t iconv(iconv_t cd, const char **inbuf, size_t *inbytesleft,
             char **outbuf, size_t *outbytesleft);

/*
 * Frees resources allocated for conversion descriptor 'cd'.
 * Returns 0 on success, -1 on error.
 */
int iconv_close(iconv_t cd);

#ifdef __cplusplus
}
#endif

#endif /* _ICONV_H */

