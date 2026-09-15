#ifndef CLIB_REGEX_PROTOS_H
#define CLIB_REGEX_PROTOS_H
/*
 * clib prototypes for Alfonso Ranieri regex.library
 */

#ifndef _REGEX_H_
#include <regex.h>
#endif

int regcomp(regex_t *preg, const char *pattern, int cflags);
size_t regerror(int errcode, const regex_t *preg, char *errbuf,
	size_t errbuf_size);
int regexec(const regex_t *preg, const char *string, size_t nmatch,
	regmatch_t pmatch[], int eflags);
void regfree(regex_t *preg);

#endif /* CLIB_REGEX_PROTOS_H */
