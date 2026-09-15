#ifndef _REGEX_H_
#define _REGEX_H_

/*
 * Static Henry Spencer POSIX regex (vendored under frontends/amiga/regex/).
 * Linked into NetSurf — no regex.library for now.
 */

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef size_t regoff_t;

typedef struct {
	int re_magic;
	size_t re_nsub;		/* number of parenthesized subexpressions */
	const char *re_endp;	/* end pointer for REG_PEND */
	struct re_guts *re_g;	/* none of your business :-) */
} regex_t;

typedef struct {
	regoff_t rm_so;		/* start of match */
	regoff_t rm_eo;		/* end of match */
} regmatch_t;

/* regcomp() flags */
#define REG_BASIC	0000
#define REG_EXTENDED	0001
#define REG_ICASE	0002
#define REG_NOSUB	0004
#define REG_NEWLINE	0010
#define REG_NOSPEC	0020
#define REG_PEND	0040
#define REG_DUMP	0200

/* regerror() codes */
#define REG_OKAY	0
#define REG_NOMATCH	1
#define REG_BADPAT	2
#define REG_ECOLLATE	3
#define REG_ECTYPE	4
#define REG_EESCAPE	5
#define REG_ESUBREG	6
#define REG_EBRACK	7
#define REG_EPAREN	8
#define REG_EBRACE	9
#define REG_BADBR	10
#define REG_ERANGE	11
#define REG_ESPACE	12
#define REG_BADRPT	13
#define REG_EMPTY	14
#define REG_ASSERT	15
#define REG_INVARG	16
#define REG_ATOI	255
#define REG_ITOA	0400

/* regexec() flags */
#define REG_NOTBOL	00001
#define REG_NOTEOL	00002
#define REG_STARTEND	00004
#define REG_TRACE	00400
#define REG_LARGE	01000
#define REG_BACKR	02000

int regcomp(regex_t *preg, const char *pattern, int cflags);
size_t regerror(int errcode, const regex_t *preg, char *errbuf,
	size_t errbuf_size);
int regexec(const regex_t *preg, const char *string, size_t nmatch,
	regmatch_t pmatch[], int eflags);
void regfree(regex_t *preg);

#ifdef __cplusplus
}
#endif

#endif /* !_REGEX_H_ */
