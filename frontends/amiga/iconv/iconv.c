/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2025 amigazen project
 *
 * Lightweight iconv for NetSurf AmigaOS3 / vbcc (no codesets / iconv.library):
 * - Hand-coded UTF-8 / UTF-16 (bullet fonts need UTF-8→UTF-16).
 * - locale.library via OpenLocale when LocaleBase is set.
 * - Windows-1252 (HTML5 / hubbub default), ISO-8859-1, ASCII, UTF-8.
 * - Unknown → UTF-8 falls back to Latin-1 so simple pages still parse.
 * - Recognise //IGNORE //TRANSLIT suffixes from NetSurf's utf8 helpers.
 */

#include "iconv.h"

#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <errno.h>

#include <proto/exec.h>
#include <proto/locale.h>
#include <libraries/locale.h>

/* Internal conversion descriptor */
typedef struct {
	const char *from_code;
	const char *to_code;
	int conversion_type;
	struct Locale *locale;
} iconv_desc_t;

#define CONV_IDENTITY         0
#define CONV_ASCII_TO_UTF8    1
#define CONV_UTF8_TO_ASCII    2
#define CONV_LATIN1_TO_UTF8   3
#define CONV_UTF8_TO_LATIN1   4
#define CONV_LATIN1_TO_ASCII  5
#define CONV_ASCII_TO_LATIN1  6
#define CONV_LOCALE_TO_UTF8   7
#define CONV_UTF8_TO_LOCALE   8
#define CONV_LOCALE_TO_ASCII  9
#define CONV_ASCII_TO_LOCALE 10
#define CONV_CP1252_TO_UTF8  11
#define CONV_UTF8_TO_CP1252  12
#define CONV_UTF8_TO_UTF16BE 13
#define CONV_UTF8_TO_UTF16LE 14
#define CONV_UTF16BE_TO_UTF8 15
#define CONV_UTF16LE_TO_UTF8 16

/*
 * Windows-1252 C1 range (0x80–0x9F).  Undefined slots keep the Latin-1
 * codepoint so conversion never fails on common HTML documents.
 */
static const ULONG cp1252_c1[32] = {
	0x20AC, 0x0081, 0x201A, 0x0192, 0x201E, 0x2026, 0x2020, 0x2021,
	0x02C6, 0x2030, 0x0160, 0x2039, 0x0152, 0x008D, 0x017D, 0x008F,
	0x0090, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2013, 0x2014,
	0x02DC, 0x2122, 0x0161, 0x203A, 0x0153, 0x009D, 0x017E, 0x0178
};

/*
 * Copy encoding name into buf without //IGNORE / //TRANSLIT suffixes
 * (NetSurf's utf8_to_enc appends //IGNORE).
 */
static void strip_iconv_suffix(const char *encoding, char *buf, size_t buflen)
{
	size_t i;
	size_t n;

	if (encoding == NULL || buflen == 0) {
		if (buflen > 0) {
			buf[0] = '\0';
		}
		return;
	}

	n = strlen(encoding);
	for (i = 0; i < n; i++) {
		if (encoding[i] == '/' && encoding[i + 1] == '/') {
			n = i;
			break;
		}
	}
	if (n >= buflen) {
		n = buflen - 1;
	}
	memcpy(buf, encoding, n);
	buf[n] = '\0';
}

/* Map aliases NetSurf / IANA may use onto the small set we implement. */
static const char *canonical_encoding(const char *encoding, char *buf, size_t buflen)
{
	strip_iconv_suffix(encoding, buf, buflen);
	if (buf[0] == '\0') {
		return NULL;
	}

	if (strcasecmp(buf, "UTF-8") == 0 || strcasecmp(buf, "UTF8") == 0) {
		return "UTF-8";
	}
	/*
	 * Bullet font paths call utf8_to_enc(..., "UTF-16", ...). Amiga is
	 * big-endian so bare UTF-16 / UCS-2 mean BE (native UWORD order).
	 */
	if (strcasecmp(buf, "UTF-16") == 0 ||
	    strcasecmp(buf, "UTF16") == 0 ||
	    strcasecmp(buf, "UTF-16BE") == 0 ||
	    strcasecmp(buf, "UTF16BE") == 0 ||
	    strcasecmp(buf, "UCS-2") == 0 ||
	    strcasecmp(buf, "UCS2") == 0 ||
	    strcasecmp(buf, "UCS-2BE") == 0) {
		return "UTF-16BE";
	}
	if (strcasecmp(buf, "UTF-16LE") == 0 ||
	    strcasecmp(buf, "UTF16LE") == 0 ||
	    strcasecmp(buf, "UCS-2LE") == 0) {
		return "UTF-16LE";
	}
	if (strcasecmp(buf, "ASCII") == 0 || strcasecmp(buf, "US-ASCII") == 0) {
		return "ASCII";
	}
	if (strcasecmp(buf, "ISO-8859-1") == 0 ||
	    strcasecmp(buf, "ISO8859-1") == 0 ||
	    strcasecmp(buf, "ISO_8859-1") == 0 ||
	    strcasecmp(buf, "LATIN1") == 0 ||
	    strcasecmp(buf, "LATIN-1") == 0 ||
	    strcasecmp(buf, "CP819") == 0) {
		return "ISO-8859-1";
	}
	/*
	 * HTML5 / hubbub default when no charset is declared (welcome.html,
	 * many legacy pages). Also accept common alias spellings.
	 */
	if (strcasecmp(buf, "WINDOWS-1252") == 0 ||
	    strcasecmp(buf, "WINDOWS1252") == 0 ||
	    strcasecmp(buf, "CP1252") == 0 ||
	    strcasecmp(buf, "CP-1252") == 0 ||
	    strcasecmp(buf, "MS-ANSI") == 0) {
		return "WINDOWS-1252";
	}
	/* Western ISO-8859 variants: treat as Latin-1 for web text. */
	if (strncasecmp(buf, "ISO-8859-", 9) == 0 ||
	    strncasecmp(buf, "ISO8859-", 8) == 0 ||
	    strncasecmp(buf, "ISO_8859-", 9) == 0 ||
	    strcasecmp(buf, "LATIN9") == 0 ||
	    strcasecmp(buf, "ISO-8859-15") == 0) {
		return "ISO-8859-1";
	}
	if (strcasecmp(buf, "LOCALE") == 0) {
		return "LOCALE";
	}

	/* Unknown name — keep stripped form for identity / fallback paths */
	return buf;
}

static int is_valid_locale_char(struct Locale *locale, ULONG character)
{
	if (locale == NULL) {
		return (character <= 0xFF) ? 1 : 0;
	}
	return IsPrint(locale, character) ? 1 : 0;
}

/* Encode UCS-4 code point to UTF-8; returns bytes written or 0 on error. */
static int utf8_encode(ULONG cp, char *out, size_t outleft)
{
	if (cp <= 0x7FU) {
		if (outleft < 1) {
			return -1;
		}
		out[0] = (char)cp;
		return 1;
	}
	if (cp <= 0x7FFU) {
		if (outleft < 2) {
			return -1;
		}
		out[0] = (char)(0xC0U | (cp >> 6));
		out[1] = (char)(0x80U | (cp & 0x3FU));
		return 2;
	}
	if (cp <= 0xFFFFU) {
		if (outleft < 3) {
			return -1;
		}
		out[0] = (char)(0xE0U | (cp >> 12));
		out[1] = (char)(0x80U | ((cp >> 6) & 0x3FU));
		out[2] = (char)(0x80U | (cp & 0x3FU));
		return 3;
	}
	if (cp <= 0x10FFFFU) {
		if (outleft < 4) {
			return -1;
		}
		out[0] = (char)(0xF0U | (cp >> 18));
		out[1] = (char)(0x80U | ((cp >> 12) & 0x3FU));
		out[2] = (char)(0x80U | ((cp >> 6) & 0x3FU));
		out[3] = (char)(0x80U | (cp & 0x3FU));
		return 4;
	}
	return 0;
}

/*
 * Decode one UTF-8 sequence. Returns byte length, or 0 for EILSEQ,
 * or -1 if input is incomplete (EINVAL).
 */
static int utf8_decode(const char *in, size_t inleft, ULONG *cp_out)
{
	unsigned char c0;
	unsigned char c1;
	unsigned char c2;
	unsigned char c3;
	ULONG cp;

	if (inleft == 0) {
		return -1;
	}

	c0 = (unsigned char)in[0];
	if (c0 < 0x80U) {
		*cp_out = c0;
		return 1;
	}
	if ((c0 & 0xE0U) == 0xC0U) {
		if (inleft < 2) {
			return -1;
		}
		c1 = (unsigned char)in[1];
		if ((c1 & 0xC0U) != 0x80U || c0 < 0xC2U) {
			return 0;
		}
		cp = ((ULONG)(c0 & 0x1FU) << 6) | (ULONG)(c1 & 0x3FU);
		*cp_out = cp;
		return 2;
	}
	if ((c0 & 0xF0U) == 0xE0U) {
		if (inleft < 3) {
			return -1;
		}
		c1 = (unsigned char)in[1];
		c2 = (unsigned char)in[2];
		if ((c1 & 0xC0U) != 0x80U || (c2 & 0xC0U) != 0x80U) {
			return 0;
		}
		cp = ((ULONG)(c0 & 0x0FU) << 12) |
		     ((ULONG)(c1 & 0x3FU) << 6) |
		     (ULONG)(c2 & 0x3FU);
		if (cp < 0x800U || (cp >= 0xD800U && cp <= 0xDFFFU)) {
			return 0;
		}
		*cp_out = cp;
		return 3;
	}
	if ((c0 & 0xF8U) == 0xF0U) {
		if (inleft < 4) {
			return -1;
		}
		c1 = (unsigned char)in[1];
		c2 = (unsigned char)in[2];
		c3 = (unsigned char)in[3];
		if ((c1 & 0xC0U) != 0x80U || (c2 & 0xC0U) != 0x80U ||
		    (c3 & 0xC0U) != 0x80U || c0 > 0xF4U) {
			return 0;
		}
		cp = ((ULONG)(c0 & 0x07U) << 18) |
		     ((ULONG)(c1 & 0x3FU) << 12) |
		     ((ULONG)(c2 & 0x3FU) << 6) |
		     (ULONG)(c3 & 0x3FU);
		if (cp < 0x10000U || cp > 0x10FFFFU) {
			return 0;
		}
		*cp_out = cp;
		return 4;
	}
	return 0;
}

/* Encode UCS-4 to UTF-16; returns bytes written, -1 if E2BIG, 0 if EILSEQ. */
static int utf16_encode(ULONG cp, char *out, size_t outleft, int le)
{
	ULONG w1;
	ULONG w2;

	if (cp > 0x10FFFFUL || (cp >= 0xD800UL && cp <= 0xDFFFUL)) {
		return 0;
	}
	if (cp < 0x10000UL) {
		if (outleft < 2) {
			return -1;
		}
		if (le) {
			out[0] = (char)(cp & 0xFFUL);
			out[1] = (char)((cp >> 8) & 0xFFUL);
		} else {
			out[0] = (char)((cp >> 8) & 0xFFUL);
			out[1] = (char)(cp & 0xFFUL);
		}
		return 2;
	}
	if (outleft < 4) {
		return -1;
	}
	cp -= 0x10000UL;
	w1 = 0xD800UL | (cp >> 10);
	w2 = 0xDC00UL | (cp & 0x3FFUL);
	if (le) {
		out[0] = (char)(w1 & 0xFFUL);
		out[1] = (char)((w1 >> 8) & 0xFFUL);
		out[2] = (char)(w2 & 0xFFUL);
		out[3] = (char)((w2 >> 8) & 0xFFUL);
	} else {
		out[0] = (char)((w1 >> 8) & 0xFFUL);
		out[1] = (char)(w1 & 0xFFUL);
		out[2] = (char)((w2 >> 8) & 0xFFUL);
		out[3] = (char)(w2 & 0xFFUL);
	}
	return 4;
}

/*
 * Decode one UTF-16 code unit sequence. Returns byte length, 0 EILSEQ, -1 EINVAL.
 */
static int utf16_decode(const char *in, size_t inleft, ULONG *cp_out, int le)
{
	ULONG w1;
	ULONG w2;

	if (inleft < 2) {
		return -1;
	}
	if (le) {
		w1 = (ULONG)(unsigned char)in[0] |
		     ((ULONG)(unsigned char)in[1] << 8);
	} else {
		w1 = ((ULONG)(unsigned char)in[0] << 8) |
		     (ULONG)(unsigned char)in[1];
	}
	if (w1 < 0xD800UL || w1 > 0xDFFFUL) {
		*cp_out = w1;
		return 2;
	}
	if (w1 > 0xDBFFUL) {
		return 0;
	}
	if (inleft < 4) {
		return -1;
	}
	if (le) {
		w2 = (ULONG)(unsigned char)in[2] |
		     ((ULONG)(unsigned char)in[3] << 8);
	} else {
		w2 = ((ULONG)(unsigned char)in[2] << 8) |
		     (ULONG)(unsigned char)in[3];
	}
	if (w2 < 0xDC00UL || w2 > 0xDFFFUL) {
		return 0;
	}
	*cp_out = 0x10000UL + (((w1 & 0x3FFUL) << 10) | (w2 & 0x3FFUL));
	return 4;
}

static int get_conversion_type(const char *from, const char *to)
{
	char from_buf[64];
	char to_buf[64];
	const char *from_norm;
	const char *to_norm;

	from_norm = canonical_encoding(from, from_buf, sizeof(from_buf));
	to_norm = canonical_encoding(to, to_buf, sizeof(to_buf));

	if (from_norm == NULL || to_norm == NULL) {
		return -1;
	}

	if (strcasecmp(from_norm, to_norm) == 0) {
		return CONV_IDENTITY;
	}

	if (strcasecmp(from_norm, "ASCII") == 0 && strcasecmp(to_norm, "UTF-8") == 0) {
		return CONV_ASCII_TO_UTF8;
	}
	if (strcasecmp(from_norm, "UTF-8") == 0 && strcasecmp(to_norm, "ASCII") == 0) {
		return CONV_UTF8_TO_ASCII;
	}
	if (strcasecmp(from_norm, "ASCII") == 0 && strcasecmp(to_norm, "ISO-8859-1") == 0) {
		return CONV_ASCII_TO_LATIN1;
	}
	if (strcasecmp(from_norm, "ISO-8859-1") == 0 && strcasecmp(to_norm, "ASCII") == 0) {
		return CONV_LATIN1_TO_ASCII;
	}
	if (strcasecmp(from_norm, "ISO-8859-1") == 0 && strcasecmp(to_norm, "UTF-8") == 0) {
		return CONV_LATIN1_TO_UTF8;
	}
	if (strcasecmp(from_norm, "UTF-8") == 0 && strcasecmp(to_norm, "ISO-8859-1") == 0) {
		return CONV_UTF8_TO_LATIN1;
	}
	if (strcasecmp(from_norm, "LOCALE") == 0 && strcasecmp(to_norm, "UTF-8") == 0) {
		return CONV_LOCALE_TO_UTF8;
	}
	if (strcasecmp(from_norm, "UTF-8") == 0 && strcasecmp(to_norm, "LOCALE") == 0) {
		return CONV_UTF8_TO_LOCALE;
	}
	if (strcasecmp(from_norm, "LOCALE") == 0 && strcasecmp(to_norm, "ASCII") == 0) {
		return CONV_LOCALE_TO_ASCII;
	}
	if (strcasecmp(from_norm, "ASCII") == 0 && strcasecmp(to_norm, "LOCALE") == 0) {
		return CONV_ASCII_TO_LOCALE;
	}

	if (strcasecmp(from_norm, "WINDOWS-1252") == 0 &&
	    strcasecmp(to_norm, "UTF-8") == 0) {
		return CONV_CP1252_TO_UTF8;
	}
	if (strcasecmp(from_norm, "UTF-8") == 0 &&
	    strcasecmp(to_norm, "WINDOWS-1252") == 0) {
		return CONV_UTF8_TO_CP1252;
	}
	if (strcasecmp(from_norm, "WINDOWS-1252") == 0 &&
	    strcasecmp(to_norm, "ISO-8859-1") == 0) {
		/* Lossy but safe for ASCII/Latin-1 subset pages. */
		return CONV_IDENTITY;
	}
	if (strcasecmp(from_norm, "ISO-8859-1") == 0 &&
	    strcasecmp(to_norm, "WINDOWS-1252") == 0) {
		return CONV_IDENTITY;
	}
	if (strcasecmp(from_norm, "WINDOWS-1252") == 0 &&
	    strcasecmp(to_norm, "ASCII") == 0) {
		return CONV_LATIN1_TO_ASCII;
	}
	if (strcasecmp(from_norm, "ASCII") == 0 &&
	    strcasecmp(to_norm, "WINDOWS-1252") == 0) {
		return CONV_ASCII_TO_LATIN1;
	}

	/* Required by bullet.library glyph paths (utf8_to_enc → UTF-16). */
	if (strcasecmp(from_norm, "UTF-8") == 0 &&
	    strcasecmp(to_norm, "UTF-16BE") == 0) {
		return CONV_UTF8_TO_UTF16BE;
	}
	if (strcasecmp(from_norm, "UTF-8") == 0 &&
	    strcasecmp(to_norm, "UTF-16LE") == 0) {
		return CONV_UTF8_TO_UTF16LE;
	}
	if (strcasecmp(from_norm, "UTF-16BE") == 0 &&
	    strcasecmp(to_norm, "UTF-8") == 0) {
		return CONV_UTF16BE_TO_UTF8;
	}
	if (strcasecmp(from_norm, "UTF-16LE") == 0 &&
	    strcasecmp(to_norm, "UTF-8") == 0) {
		return CONV_UTF16LE_TO_UTF8;
	}

	/*
	 * Best-effort: unknown → UTF-8 as Latin-1 so charset-labelled pages
	 * still parse. Do NOT map UTF-8 → unknown (that broke UTF-16).
	 */
	if (strcasecmp(to_norm, "UTF-8") == 0) {
		return CONV_LATIN1_TO_UTF8;
	}

	return -1;
}

iconv_t iconv_open(const char *tocode, const char *fromcode)
{
	iconv_desc_t *desc;
	int conv_type;
	struct Locale *locale;

	locale = NULL;

	if (tocode == NULL || fromcode == NULL) {
		errno = EINVAL;
		return (iconv_t)-1;
	}

	conv_type = get_conversion_type(fromcode, tocode);
	if (conv_type < 0) {
		errno = EINVAL;
		return (iconv_t)-1;
	}

	/*
	 * Locale-aware modes need OpenLocale. NetSurf already opened
	 * locale.library into LocaleBase; do not OpenLibrary again.
	 */
	if (conv_type >= CONV_LOCALE_TO_UTF8) {
		if (LocaleBase != NULL) {
			locale = OpenLocale(NULL);
		}
		if (locale == NULL) {
			if (conv_type == CONV_LOCALE_TO_UTF8) {
				conv_type = CONV_LATIN1_TO_UTF8;
			} else if (conv_type == CONV_UTF8_TO_LOCALE) {
				conv_type = CONV_UTF8_TO_LATIN1;
			} else if (conv_type == CONV_LOCALE_TO_ASCII) {
				conv_type = CONV_LATIN1_TO_ASCII;
			} else if (conv_type == CONV_ASCII_TO_LOCALE) {
				conv_type = CONV_ASCII_TO_LATIN1;
			}
		}
	}

	desc = (iconv_desc_t *)malloc(sizeof(iconv_desc_t));
	if (desc == NULL) {
		if (locale != NULL) {
			CloseLocale(locale);
		}
		errno = ENOMEM;
		return (iconv_t)-1;
	}

	desc->from_code = fromcode;
	desc->to_code = tocode;
	desc->conversion_type = conv_type;
	desc->locale = locale;

	return (iconv_t)desc;
}

size_t iconv(iconv_t cd, const char **inbuf, size_t *inbytesleft,
	     char **outbuf, size_t *outbytesleft)
{
	iconv_desc_t *desc = (iconv_desc_t *)cd;
	const char *inptr;
	char *outptr;
	size_t inleft;
	size_t outleft;
	size_t converted;

	converted = 0;

	if (desc == NULL) {
		errno = EBADF;
		return (size_t)-1;
	}

	if (inbuf == NULL || *inbuf == NULL) {
		return 0;
	}

	inptr = *inbuf;
	outptr = *outbuf;
	inleft = *inbytesleft;
	outleft = *outbytesleft;

	switch (desc->conversion_type) {
	case CONV_IDENTITY:
		{
			size_t copy_len;

			copy_len = (inleft < outleft) ? inleft : outleft;
			memcpy(outptr, inptr, copy_len);
			inptr += copy_len;
			outptr += copy_len;
			inleft -= copy_len;
			outleft -= copy_len;
			converted = copy_len;
		}
		break;

	case CONV_ASCII_TO_UTF8:
	case CONV_ASCII_TO_LATIN1:
	case CONV_ASCII_TO_LOCALE:
		while (inleft > 0 && outleft > 0) {
			unsigned char c;

			c = (unsigned char)*inptr;
			if (c >= 0x80U) {
				errno = EILSEQ;
				return (size_t)-1;
			}
			if (desc->conversion_type == CONV_ASCII_TO_LOCALE &&
			    desc->locale != NULL &&
			    !is_valid_locale_char(desc->locale, c)) {
				errno = EILSEQ;
				return (size_t)-1;
			}
			*outptr = (char)c;
			inptr++;
			outptr++;
			inleft--;
			outleft--;
			converted++;
		}
		break;

	case CONV_UTF8_TO_ASCII:
		while (inleft > 0 && outleft > 0) {
			ULONG cp;
			int n;

			n = utf8_decode(inptr, inleft, &cp);
			if (n < 0) {
				errno = EINVAL;
				return (size_t)-1;
			}
			if (n == 0 || cp >= 0x80U) {
				errno = EILSEQ;
				return (size_t)-1;
			}
			*outptr = (char)cp;
			inptr += n;
			outptr++;
			inleft -= (size_t)n;
			outleft--;
			converted++;
		}
		break;

	case CONV_LATIN1_TO_UTF8:
	case CONV_LOCALE_TO_UTF8:
		while (inleft > 0 && outleft > 0) {
			unsigned char c;
			int n;

			c = (unsigned char)*inptr;
			if (desc->conversion_type == CONV_LOCALE_TO_UTF8 &&
			    desc->locale != NULL &&
			    !is_valid_locale_char(desc->locale, c)) {
				errno = EILSEQ;
				return (size_t)-1;
			}
			n = utf8_encode((ULONG)c, outptr, outleft);
			if (n < 0) {
				errno = E2BIG;
				break;
			}
			if (n == 0) {
				errno = EILSEQ;
				return (size_t)-1;
			}
			inptr++;
			outptr += n;
			inleft--;
			outleft -= (size_t)n;
			converted++;
		}
		break;

	case CONV_UTF8_TO_LATIN1:
	case CONV_UTF8_TO_LOCALE:
		while (inleft > 0 && outleft > 0) {
			ULONG cp;
			int n;

			n = utf8_decode(inptr, inleft, &cp);
			if (n < 0) {
				errno = EINVAL;
				return (size_t)-1;
			}
			if (n == 0 || cp > 0xFFU) {
				errno = EILSEQ;
				return (size_t)-1;
			}
			if (desc->conversion_type == CONV_UTF8_TO_LOCALE &&
			    desc->locale != NULL &&
			    !is_valid_locale_char(desc->locale, cp)) {
				errno = EILSEQ;
				return (size_t)-1;
			}
			*outptr = (char)cp;
			inptr += n;
			outptr++;
			inleft -= (size_t)n;
			outleft--;
			converted++;
		}
		break;

	case CONV_LATIN1_TO_ASCII:
		while (inleft > 0 && outleft > 0) {
			unsigned char c;

			c = (unsigned char)*inptr;
			if (c >= 0x80U) {
				errno = EILSEQ;
				return (size_t)-1;
			}
			*outptr = (char)c;
			inptr++;
			outptr++;
			inleft--;
			outleft--;
			converted++;
		}
		break;

	case CONV_LOCALE_TO_ASCII:
		while (inleft > 0 && outleft > 0) {
			unsigned char c;

			c = (unsigned char)*inptr;
			if (desc->locale != NULL &&
			    !is_valid_locale_char(desc->locale, c)) {
				errno = EILSEQ;
				return (size_t)-1;
			}
			if (c < 0x80U) {
				*outptr = (char)c;
			} else {
				*outptr = '?';
			}
			inptr++;
			outptr++;
			inleft--;
			outleft--;
			converted++;
		}
		break;

	case CONV_CP1252_TO_UTF8:
		while (inleft > 0 && outleft > 0) {
			unsigned char c;
			ULONG cp;
			int n;

			c = (unsigned char)*inptr;
			if (c >= 0x80U && c <= 0x9FU) {
				cp = cp1252_c1[c - 0x80U];
			} else {
				cp = (ULONG)c;
			}
			n = utf8_encode(cp, outptr, outleft);
			if (n < 0) {
				errno = E2BIG;
				break;
			}
			if (n == 0) {
				errno = EILSEQ;
				return (size_t)-1;
			}
			inptr++;
			outptr += n;
			inleft--;
			outleft -= (size_t)n;
			converted++;
		}
		break;

	case CONV_UTF8_TO_CP1252:
		while (inleft > 0 && outleft > 0) {
			ULONG cp;
			int n;
			unsigned char outc;
			int found;
			int i;

			n = utf8_decode(inptr, inleft, &cp);
			if (n < 0) {
				errno = EINVAL;
				return (size_t)-1;
			}
			if (n == 0) {
				errno = EILSEQ;
				return (size_t)-1;
			}
			if (cp <= 0xFFU && (cp < 0x80U || cp > 0x9FU)) {
				outc = (unsigned char)cp;
			} else {
				found = 0;
				outc = '?';
				for (i = 0; i < 32; i++) {
					if (cp1252_c1[i] == cp) {
						outc = (unsigned char)(0x80 + i);
						found = 1;
						break;
					}
				}
				if (found == 0 && cp <= 0xFFU) {
					outc = (unsigned char)cp;
				}
			}
			*outptr = (char)outc;
			inptr += n;
			outptr++;
			inleft -= (size_t)n;
			outleft--;
			converted++;
		}
		break;

	case CONV_UTF8_TO_UTF16BE:
	case CONV_UTF8_TO_UTF16LE:
		while (inleft > 0) {
			ULONG cp;
			int n;
			int w;
			int le;

			le = (desc->conversion_type == CONV_UTF8_TO_UTF16LE);
			n = utf8_decode(inptr, inleft, &cp);
			if (n < 0) {
				errno = EINVAL;
				return (size_t)-1;
			}
			if (n == 0) {
				errno = EILSEQ;
				return (size_t)-1;
			}
			w = utf16_encode(cp, outptr, outleft, le);
			if (w < 0) {
				errno = E2BIG;
				break;
			}
			if (w == 0) {
				errno = EILSEQ;
				return (size_t)-1;
			}
			inptr += n;
			outptr += w;
			inleft -= (size_t)n;
			outleft -= (size_t)w;
			converted++;
		}
		break;

	case CONV_UTF16BE_TO_UTF8:
	case CONV_UTF16LE_TO_UTF8:
		while (inleft > 0 && outleft > 0) {
			ULONG cp;
			int n;
			int w;
			int le;

			le = (desc->conversion_type == CONV_UTF16LE_TO_UTF8);
			n = utf16_decode(inptr, inleft, &cp, le);
			if (n < 0) {
				errno = EINVAL;
				return (size_t)-1;
			}
			if (n == 0) {
				errno = EILSEQ;
				return (size_t)-1;
			}
			w = utf8_encode(cp, outptr, outleft);
			if (w < 0) {
				errno = E2BIG;
				break;
			}
			if (w == 0) {
				errno = EILSEQ;
				return (size_t)-1;
			}
			inptr += n;
			outptr += w;
			inleft -= (size_t)n;
			outleft -= (size_t)w;
			converted++;
		}
		break;

	default:
		errno = EINVAL;
		return (size_t)-1;
	}

	*inbuf = inptr;
	*outbuf = outptr;
	*inbytesleft = inleft;
	*outbytesleft = outleft;

	return converted;
}

int iconv_close(iconv_t cd)
{
	iconv_desc_t *desc = (iconv_desc_t *)cd;

	if (desc == NULL) {
		errno = EBADF;
		return -1;
	}

	if (desc->locale != NULL) {
		CloseLocale(desc->locale);
	}

	free(desc);
	return 0;
}
