/*
 * hubbub_stubs.c - classic hubbub_* C API routed through hubbub.library LVOs
 *
 * Used by the Nami binary (links stubs instead of static hubbub.lib).
 * Keep in sync with /libhubbub/amiga/stubs/hubbub_stubs.c
 */

#define __USE_SYSBASE

#include <exec/types.h>
#include <proto/exec.h>

#include <hubbub/parser.h>
#include <hubbub/errors.h>

#include "hubbub_stub_open.h"

extern struct Library *HubbubBase;

#define HSTUB_GUARD() \
	do { \
		if (!hubbub_stub_ensure_open()) { \
			return HUBBUB_BADPARM; \
		} \
	} while (0)

#define HSTUB_GUARD_PTR() \
	do { \
		if (!hubbub_stub_ensure_open()) { \
			return (const char *)0; \
		} \
	} while (0)

static ULONG __lvo_hubbub_parser_create(
	__reg("a6") struct Library *,
	__reg("a0") const char *enc,
	__reg("d0") ULONG fix_enc,
	__reg("a1") hubbub_parser **parser) = "\tjsr\t-30(a6)";

static ULONG __lvo_hubbub_parser_destroy(
	__reg("a6") struct Library *,
	__reg("a0") hubbub_parser *parser) = "\tjsr\t-36(a6)";

static ULONG __lvo_hubbub_parser_setopt(
	__reg("a6") struct Library *,
	__reg("a0") hubbub_parser *parser,
	__reg("d0") ULONG type,
	__reg("a1") hubbub_parser_optparams *params) = "\tjsr\t-42(a6)";

static ULONG __lvo_hubbub_parser_parse_chunk(
	__reg("a6") struct Library *,
	__reg("a0") hubbub_parser *parser,
	__reg("a1") const uint8_t *data,
	__reg("d0") ULONG len) = "\tjsr\t-48(a6)";

static ULONG __lvo_hubbub_parser_insert_chunk(
	__reg("a6") struct Library *,
	__reg("a0") hubbub_parser *parser,
	__reg("a1") const uint8_t *data,
	__reg("d0") ULONG len) = "\tjsr\t-54(a6)";

static ULONG __lvo_hubbub_parser_completed(
	__reg("a6") struct Library *,
	__reg("a0") hubbub_parser *parser) = "\tjsr\t-60(a6)";

static APTR __lvo_hubbub_parser_read_charset(
	__reg("a6") struct Library *,
	__reg("a0") hubbub_parser *parser,
	__reg("a1") hubbub_charset_source *source) = "\tjsr\t-66(a6)";

static APTR __lvo_hubbub_error_to_string(
	__reg("a6") struct Library *,
	__reg("d0") ULONG error) = "\tjsr\t-72(a6)";

hubbub_error hubbub_parser_create(const char *enc, bool fix_enc,
	hubbub_parser **parser)
{
	HSTUB_GUARD();
	return (hubbub_error)__lvo_hubbub_parser_create(HubbubBase, enc,
		fix_enc ? 1UL : 0UL, parser);
}

hubbub_error hubbub_parser_destroy(hubbub_parser *parser)
{
	HSTUB_GUARD();
	return (hubbub_error)__lvo_hubbub_parser_destroy(HubbubBase, parser);
}

hubbub_error hubbub_parser_setopt(hubbub_parser *parser,
	hubbub_parser_opttype type, hubbub_parser_optparams *params)
{
	HSTUB_GUARD();
	return (hubbub_error)__lvo_hubbub_parser_setopt(HubbubBase, parser,
		(ULONG)type, params);
}

hubbub_error hubbub_parser_parse_chunk(hubbub_parser *parser,
	const uint8_t *data, size_t len)
{
	HSTUB_GUARD();
	return (hubbub_error)__lvo_hubbub_parser_parse_chunk(HubbubBase,
		parser, data, (ULONG)len);
}

hubbub_error hubbub_parser_insert_chunk(hubbub_parser *parser,
	const uint8_t *data, size_t len)
{
	HSTUB_GUARD();
	return (hubbub_error)__lvo_hubbub_parser_insert_chunk(HubbubBase,
		parser, data, (ULONG)len);
}

hubbub_error hubbub_parser_completed(hubbub_parser *parser)
{
	HSTUB_GUARD();
	return (hubbub_error)__lvo_hubbub_parser_completed(HubbubBase, parser);
}

const char *hubbub_parser_read_charset(hubbub_parser *parser,
	hubbub_charset_source *source)
{
	HSTUB_GUARD_PTR();
	return (const char *)__lvo_hubbub_parser_read_charset(HubbubBase,
		parser, source);
}

const char *hubbub_error_to_string(hubbub_error error)
{
	HSTUB_GUARD_PTR();
	return (const char *)__lvo_hubbub_error_to_string(HubbubBase,
		(ULONG)error);
}
