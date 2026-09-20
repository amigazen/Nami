/*
 * Copyright 2026 AmigaZen / Nami contributors
 *
 * Window, Location, Navigator and Console. Timers go through the
 * frontend scheduler (guit->misc->schedule), same idea as duktape's
 * window_alloc_new_callback. Minimum delay is 10ms so a 0 timeout
 * cannot spin the Amiga scheduler.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#ifdef __VBCC__
#ifndef JS_PRINTF_FORMAT
#define JS_PRINTF_FORMAT
#define JS_PRINTF_FORMAT_ATTR(format_param, dots_param)
#endif
#ifndef __attribute__
#define __attribute__(x)
#endif
#endif

#include "quickjs.h"

#include <dom/dom.h>

#include "utils/log.h"
#include "utils/nsurl.h"
#include "utils/useragent.h"
#include "utils/errors.h"
#include "netsurf/browser_window.h"
#include "netsurf/console.h"
/* gui_table.h only forward-declares gui_misc_table; schedule is in misc.h. */
#include "netsurf/misc.h"
#include "content/llcache.h"
#include "html/private.h"
#include "desktop/gui_internal.h"

#include <nsutils/time.h>

#include "private.h"
#include "bind_priv.h"

struct qjs_timer {
	struct qjs_timer *next;
	jsthread *thread;
	int id;
	int ms;
	int repeating;
	JSValue func;
	char *code;
};

struct qjs_ctime {
	struct qjs_ctime *next;
	char *name;
	uint64_t ms;
};

static void
qjs_loc_finalizer(JSRuntime *rt, JSValueConst val)
{
	jsheap *heap;
	nsurl *url;

	heap = (jsheap *)JS_GetRuntimeOpaque(rt);
	if (heap == NULL || !heap->loc_class_ready) {
		return;
	}
	url = (nsurl *)JS_GetOpaque(val, heap->loc_class);
	if (url != NULL) {
		nsurl_unref(url);
	}
}

static JSClassDef qjs_loc_class = {
	"Location",
	qjs_loc_finalizer,
	NULL,
	NULL,
	NULL
};

static nsurl *
qjs_loc_url(JSContext *ctx, JSValueConst this_val)
{
	jsthread *thread;

	thread = qjs_thread_from_ctx(ctx);
	if (thread == NULL || thread->heap == NULL ||
			!thread->heap->loc_class_ready) {
		return NULL;
	}
	return (nsurl *)JS_GetOpaque(this_val, thread->heap->loc_class);
}

static JSValue
js_loc_part(JSContext *ctx, JSValueConst this_val, int magic)
{
	nsurl *url;
	char *s;
	size_t n;
	JSValue out;

	url = qjs_loc_url(ctx, this_val);
	if (url == NULL) {
		return JS_NewStringLen(ctx, "", 0);
	}
	s = NULL;
	n = 0;
	if (nsurl_get(url, (nsurl_component)magic, &s, &n) != NSERROR_OK ||
			s == NULL) {
		return JS_NewStringLen(ctx, "", 0);
	}
	out = JS_NewStringLen(ctx, s, n);
	free(s);
	return out;
}

static void
qjs_go(JSContext *ctx, JSValueConst this_val, const char *rel,
		enum browser_window_nav_flags flags)
{
	jsthread *thread;
	nsurl *base;
	nsurl *joined;
	struct browser_window *bw;

	thread = qjs_thread_from_ctx(ctx);
	base = qjs_loc_url(ctx, this_val);
	if (thread == NULL || base == NULL || rel == NULL) {
		return;
	}
	bw = (struct browser_window *)thread->win_priv;
	if (bw == NULL) {
		return;
	}
	joined = NULL;
	if (nsurl_join(base, rel, &joined) != NSERROR_OK || joined == NULL) {
		return;
	}
	browser_window_navigate(bw, joined, NULL, flags, NULL, NULL, NULL);
	nsurl_unref(joined);
}

static JSValue
js_loc_href_set(JSContext *ctx, JSValueConst this_val, JSValueConst val,
		int magic)
{
	const char *s;

	(void)magic;
	s = JS_ToCString(ctx, val);
	if (s == NULL) {
		return JS_EXCEPTION;
	}
	qjs_go(ctx, this_val, s, BW_NAVIGATE_HISTORY);
	JS_FreeCString(ctx, s);
	return JS_UNDEFINED;
}

static JSValue
js_loc_assign(JSContext *ctx, JSValueConst this_val,
		int argc, JSValueConst *argv)
{
	const char *s;

	if (argc < 1) {
		return JS_UNDEFINED;
	}
	s = JS_ToCString(ctx, argv[0]);
	if (s == NULL) {
		return JS_EXCEPTION;
	}
	qjs_go(ctx, this_val, s, BW_NAVIGATE_HISTORY);
	JS_FreeCString(ctx, s);
	return JS_UNDEFINED;
}

static JSValue
js_loc_replace(JSContext *ctx, JSValueConst this_val,
		int argc, JSValueConst *argv)
{
	const char *s;

	if (argc < 1) {
		return JS_UNDEFINED;
	}
	s = JS_ToCString(ctx, argv[0]);
	if (s == NULL) {
		return JS_EXCEPTION;
	}
	qjs_go(ctx, this_val, s, BW_NAVIGATE_NONE);
	JS_FreeCString(ctx, s);
	return JS_UNDEFINED;
}

static JSValue
js_loc_reload(JSContext *ctx, JSValueConst this_val,
		int argc, JSValueConst *argv)
{
	jsthread *thread;
	struct browser_window *bw;

	(void)this_val;
	(void)argc;
	(void)argv;
	thread = qjs_thread_from_ctx(ctx);
	if (thread == NULL) {
		return JS_UNDEFINED;
	}
	bw = (struct browser_window *)thread->win_priv;
	if (bw != NULL) {
		browser_window_reload(bw, false);
	}
	return JS_UNDEFINED;
}

static JSValue
js_loc_href_get(JSContext *ctx, JSValueConst this_val)
{
	return js_loc_part(ctx, this_val, (int)NSURL_COMPLETE);
}

static JSValue
js_loc_protocol_get(JSContext *ctx, JSValueConst this_val)
{
	nsurl *url;
	char *s;
	size_t n;
	char *with_colon;
	JSValue out;

	/* Location.protocol is scheme plus a trailing colon. */
	url = qjs_loc_url(ctx, this_val);
	if (url == NULL) {
		return JS_NewStringLen(ctx, "", 0);
	}
	s = NULL;
	n = 0;
	if (nsurl_get(url, NSURL_SCHEME, &s, &n) != NSERROR_OK || s == NULL) {
		return JS_NewStringLen(ctx, "", 0);
	}
	with_colon = malloc(n + 2);
	if (with_colon == NULL) {
		free(s);
		return JS_EXCEPTION;
	}
	memcpy(with_colon, s, n);
	with_colon[n] = ':';
	with_colon[n + 1] = '\0';
	free(s);
	out = JS_NewStringLen(ctx, with_colon, n + 1);
	free(with_colon);
	return out;
}

static JSValue
js_loc_href_set_plain(JSContext *ctx, JSValueConst this_val,
		JSValueConst val)
{
	return js_loc_href_set(ctx, this_val, val, 0);
}

static const JSCFunctionListEntry qjs_loc_funcs[] = {
	/* Non-magic setter: JS_CFUNC_setter_magic through the LVO path
	 * locked up hard while installing Location. */
	JS_CGETSET_DEF("href", js_loc_href_get, js_loc_href_set_plain),
	JS_CGETSET_MAGIC_DEF("origin", js_loc_part, NULL,
			NSURL_SCHEME | NSURL_HOST | NSURL_PORT),
	JS_CGETSET_DEF("protocol", js_loc_protocol_get, NULL),
	JS_CGETSET_MAGIC_DEF("username", js_loc_part, NULL, NSURL_USERNAME),
	JS_CGETSET_MAGIC_DEF("password", js_loc_part, NULL, NSURL_PASSWORD),
	JS_CGETSET_MAGIC_DEF("host", js_loc_part, NULL, NSURL_HOST),
	JS_CGETSET_MAGIC_DEF("hostname", js_loc_part, NULL, NSURL_HOST),
	JS_CGETSET_MAGIC_DEF("port", js_loc_part, NULL, NSURL_PORT),
	JS_CGETSET_MAGIC_DEF("pathname", js_loc_part, NULL, NSURL_PATH),
	JS_CGETSET_MAGIC_DEF("search", js_loc_part, NULL, NSURL_QUERY),
	JS_CGETSET_MAGIC_DEF("hash", js_loc_part, NULL, NSURL_FRAGMENT),
	JS_CFUNC_DEF("assign", 1, js_loc_assign),
	JS_CFUNC_DEF("replace", 1, js_loc_replace),
	JS_CFUNC_DEF("reload", 0, js_loc_reload),
};

static JSValue
js_nav_str(JSContext *ctx, JSValueConst this_val, int magic)
{
	const char *s;

	(void)this_val;
	s = "";
	if (magic == 0) {
		s = "Mozilla";
	} else if (magic == 1) {
		s = "Netscape";
	} else if (magic == 2) {
		s = "3.4";
	} else if (magic == 3) {
		s = "Gecko";
	} else if (magic == 4) {
		s = "20100101";
	} else if (magic == 5) {
		s = user_agent_string();
		if (s == NULL) {
			s = "";
		}
	}
	return JS_NewString(ctx, s);
}

static JSValue
js_nav_bool(JSContext *ctx, JSValueConst this_val, int magic)
{
	(void)this_val;
	/* 0 cookieEnabled, 1 javaEnabled, 2 onLine */
	if (magic == 2) {
		return JS_TRUE;
	}
	return JS_FALSE;
}

static JSValue
js_taint(JSContext *ctx, JSValueConst this_val,
		int argc, JSValueConst *argv)
{
	(void)this_val;
	(void)argc;
	(void)argv;
	return JS_FALSE;
}

static const JSCFunctionListEntry qjs_nav_funcs[] = {
	JS_CGETSET_MAGIC_DEF("appCodeName", js_nav_str, NULL, 0),
	JS_CGETSET_MAGIC_DEF("appName", js_nav_str, NULL, 1),
	JS_CGETSET_MAGIC_DEF("appVersion", js_nav_str, NULL, 2),
	JS_CGETSET_MAGIC_DEF("product", js_nav_str, NULL, 3),
	JS_CGETSET_MAGIC_DEF("productSub", js_nav_str, NULL, 4),
	JS_CGETSET_MAGIC_DEF("userAgent", js_nav_str, NULL, 5),
	JS_CGETSET_MAGIC_DEF("platform", js_nav_str, NULL, 9),
	JS_CGETSET_MAGIC_DEF("vendor", js_nav_str, NULL, 9),
	JS_CGETSET_MAGIC_DEF("vendorSub", js_nav_str, NULL, 9),
	JS_CGETSET_MAGIC_DEF("cookieEnabled", js_nav_bool, NULL, 0),
	JS_CGETSET_MAGIC_DEF("javaEnabled", js_nav_bool, NULL, 1),
	JS_CGETSET_MAGIC_DEF("onLine", js_nav_bool, NULL, 2),
	JS_CFUNC_DEF("taintEnabled", 0, js_taint),
};

static void
qjs_console_write(JSContext *ctx, browser_window_console_flags flags,
		int argc, JSValueConst *argv)
{
	jsthread *thread;
	struct browser_window *bw;
	char buf[1024];
	size_t used;
	int i;
	const char *s;
	size_t n;
	size_t k;
	unsigned int g;

	thread = qjs_thread_from_ctx(ctx);
	if (thread == NULL) {
		return;
	}
	bw = (struct browser_window *)thread->win_priv;
	used = 0;
	for (g = 0; g < (unsigned int)thread->console_group && used + 1 < sizeof(buf); g++) {
		buf[used++] = ' ';
	}
	for (i = 0; i < argc && used + 1 < sizeof(buf); i++) {
		s = JS_ToCStringLen(ctx, &n, argv[i]);
		if (s == NULL) {
			continue;
		}
		if (i > 0 && used + 1 < sizeof(buf)) {
			buf[used++] = ' ';
		}
		for (k = 0; k < n && used + 1 < sizeof(buf); k++) {
			buf[used++] = s[k];
		}
		JS_FreeCString(ctx, s);
	}
	buf[used] = '\0';
	NSLOG(netsurf, INFO, "console: %s", buf);
	if (bw != NULL) {
		browser_window_console_log(bw, BW_CS_SCRIPT_CONSOLE,
				buf, used, flags);
	}
}

static JSValue
js_con_log(JSContext *ctx, JSValueConst this_val,
		int argc, JSValueConst *argv, int magic)
{
	browser_window_console_flags flags;

	(void)this_val;
	flags = BW_CS_FLAG_LEVEL_LOG;
	if (magic == 1) {
		flags = BW_CS_FLAG_LEVEL_INFO;
	} else if (magic == 2) {
		flags = BW_CS_FLAG_LEVEL_WARN;
	} else if (magic == 3) {
		flags = BW_CS_FLAG_LEVEL_ERROR;
	} else if (magic == 4) {
		flags = BW_CS_FLAG_LEVEL_DEBUG;
	}
	qjs_console_write(ctx, flags, argc, argv);
	return JS_UNDEFINED;
}

static JSValue
js_con_log0(JSContext *ctx, JSValueConst this_val,
		int argc, JSValueConst *argv)
{
	return js_con_log(ctx, this_val, argc, argv, 0);
}

static JSValue
js_con_info(JSContext *ctx, JSValueConst this_val,
		int argc, JSValueConst *argv)
{
	return js_con_log(ctx, this_val, argc, argv, 1);
}

static JSValue
js_con_warn(JSContext *ctx, JSValueConst this_val,
		int argc, JSValueConst *argv)
{
	return js_con_log(ctx, this_val, argc, argv, 2);
}

static JSValue
js_con_error(JSContext *ctx, JSValueConst this_val,
		int argc, JSValueConst *argv)
{
	return js_con_log(ctx, this_val, argc, argv, 3);
}

static JSValue
js_con_debug(JSContext *ctx, JSValueConst this_val,
		int argc, JSValueConst *argv)
{
	return js_con_log(ctx, this_val, argc, argv, 4);
}

static JSValue
js_con_group(JSContext *ctx, JSValueConst this_val,
		int argc, JSValueConst *argv)
{
	jsthread *thread;

	(void)this_val;
	(void)argc;
	(void)argv;
	thread = qjs_thread_from_ctx(ctx);
	if (thread != NULL) {
		thread->console_group++;
	}
	return JS_UNDEFINED;
}

static JSValue
js_con_group_end(JSContext *ctx, JSValueConst this_val,
		int argc, JSValueConst *argv)
{
	jsthread *thread;

	(void)this_val;
	(void)argc;
	(void)argv;
	thread = qjs_thread_from_ctx(ctx);
	if (thread != NULL && thread->console_group > 0) {
		thread->console_group--;
	}
	return JS_UNDEFINED;
}

static JSValue
js_con_time(JSContext *ctx, JSValueConst this_val,
		int argc, JSValueConst *argv)
{
	jsthread *thread;
	const char *name;
	struct qjs_ctime *ct;
	uint64_t now;

	(void)this_val;
	thread = qjs_thread_from_ctx(ctx);
	if (thread == NULL || argc < 1) {
		return JS_UNDEFINED;
	}
	name = JS_ToCString(ctx, argv[0]);
	if (name == NULL) {
		return JS_EXCEPTION;
	}
	now = 0;
	if (nsu_getmonotonic_ms(&now) != NSUERROR_OK) {
		JS_FreeCString(ctx, name);
		return JS_UNDEFINED;
	}
	ct = malloc(sizeof(*ct));
	if (ct == NULL) {
		JS_FreeCString(ctx, name);
		return JS_UNDEFINED;
	}
	ct->name = strdup(name);
	JS_FreeCString(ctx, name);
	if (ct->name == NULL) {
		free(ct);
		return JS_UNDEFINED;
	}
	ct->ms = now;
	ct->next = thread->ctimes;
	thread->ctimes = ct;
	return JS_UNDEFINED;
}

static JSValue
js_con_time_end(JSContext *ctx, JSValueConst this_val,
		int argc, JSValueConst *argv)
{
	jsthread *thread;
	const char *name;
	struct qjs_ctime *ct;
	struct qjs_ctime **pp;
	uint64_t now;
	char msg[128];
	JSValue one;

	(void)this_val;
	thread = qjs_thread_from_ctx(ctx);
	if (thread == NULL || argc < 1) {
		return JS_UNDEFINED;
	}
	name = JS_ToCString(ctx, argv[0]);
	if (name == NULL) {
		return JS_EXCEPTION;
	}
	now = 0;
	nsu_getmonotonic_ms(&now);
	pp = &thread->ctimes;
	while (*pp != NULL) {
		if (strcmp((*pp)->name, name) == 0) {
			ct = *pp;
			*pp = ct->next;
			snprintf(msg, sizeof(msg), "%s: %lu ms", name,
					(unsigned long)(now - ct->ms));
			free(ct->name);
			free(ct);
			JS_FreeCString(ctx, name);
			one = JS_NewString(ctx, msg);
			qjs_console_write(ctx, BW_CS_FLAG_LEVEL_INFO, 1, &one);
			JS_FreeValue(ctx, one);
			return JS_UNDEFINED;
		}
		pp = &(*pp)->next;
	}
	JS_FreeCString(ctx, name);
	return JS_UNDEFINED;
}

static const JSCFunctionListEntry qjs_con_funcs[] = {
	/* Plain CFUNC only — JS_CFUNC_generic_magic locked up on install. */
	JS_CFUNC_DEF("log", 0, js_con_log0),
	JS_CFUNC_DEF("info", 0, js_con_info),
	JS_CFUNC_DEF("warn", 0, js_con_warn),
	JS_CFUNC_DEF("error", 0, js_con_error),
	JS_CFUNC_DEF("debug", 0, js_con_debug),
	JS_CFUNC_DEF("dir", 0, js_con_info),
	JS_CFUNC_DEF("trace", 0, js_con_info),
	JS_CFUNC_DEF("group", 0, js_con_group),
	JS_CFUNC_DEF("groupCollapsed", 0, js_con_group),
	JS_CFUNC_DEF("groupEnd", 0, js_con_group_end),
	JS_CFUNC_DEF("time", 1, js_con_time),
	JS_CFUNC_DEF("timeEnd", 1, js_con_time_end),
};

static void qjs_timer_fire(void *pw);

static struct qjs_timer *
qjs_timer_find(jsthread *thread, int id)
{
	struct qjs_timer *tm;

	for (tm = thread->timers; tm != NULL; tm = tm->next) {
		if (tm->id == id) {
			return tm;
		}
	}
	return NULL;
}

static void
qjs_timer_unlink(jsthread *thread, struct qjs_timer *tm, int cancel)
{
	struct qjs_timer **pp;

	if (cancel && guit != NULL && guit->misc != NULL &&
			guit->misc->schedule != NULL) {
		guit->misc->schedule(-1, qjs_timer_fire, tm);
	}
	pp = &thread->timers;
	while (*pp != NULL) {
		if (*pp == tm) {
			*pp = tm->next;
			break;
		}
		pp = &(*pp)->next;
	}
	if (thread->ctx != NULL) {
		JS_FreeValue(thread->ctx, tm->func);
	}
	free(tm->code);
	free(tm);
}

static void
qjs_timer_fire(void *pw)
{
	struct qjs_timer *tm;
	jsthread *thread;
	JSValue ret;
	int again;
	int id;
	int ms;

	tm = (struct qjs_timer *)pw;
	thread = tm->thread;
	if (thread == NULL || thread->closed || thread->ctx == NULL) {
		return;
	}
	/* clearTimeout inside the callback frees tm. Keep copies. */
	again = tm->repeating;
	id = tm->id;
	ms = tm->ms;
	if (tm->code != NULL) {
		ret = JS_Eval(thread->ctx, tm->code, strlen(tm->code),
				"<timeout>", JS_EVAL_TYPE_GLOBAL);
	} else {
		ret = JS_Call(thread->ctx, tm->func, JS_UNDEFINED, 0, NULL);
	}
	if (JS_IsException(ret)) {
		JSValue ev;
		const char *msg;

		ev = JS_GetException(thread->ctx);
		msg = JS_ToCString(thread->ctx, ev);
		if (msg != NULL) {
			NSLOG(netsurf, WARNING, "timer exception: %s", msg);
			JS_FreeCString(thread->ctx, msg);
		}
		JS_FreeValue(thread->ctx, ev);
	}
	JS_FreeValue(thread->ctx, ret);

	tm = qjs_timer_find(thread, id);
	if (tm == NULL) {
		return;
	}
	if (again && guit != NULL && guit->misc != NULL &&
			guit->misc->schedule != NULL) {
		guit->misc->schedule(ms, qjs_timer_fire, tm);
	} else if (!again) {
		qjs_timer_unlink(thread, tm, 0);
	}
}

static JSValue
js_set_timer(JSContext *ctx, JSValueConst this_val,
		int argc, JSValueConst *argv, int magic)
{
	jsthread *thread;
	struct qjs_timer *tm;
	int32_t ms;
	const char *code;

	(void)this_val;
	thread = qjs_thread_from_ctx(ctx);
	if (thread == NULL || thread->closed || argc < 1) {
		return JS_NewInt32(ctx, 0);
	}
	if (guit == NULL || guit->misc == NULL || guit->misc->schedule == NULL) {
		return JS_NewInt32(ctx, 0);
	}

	ms = 10;
	if (argc > 1) {
		if (JS_ToInt32(ctx, &ms, argv[1]) < 0) {
			return JS_EXCEPTION;
		}
	}
	if (ms < 10) {
		ms = 10;
	}

	tm = calloc(1, sizeof(*tm));
	if (tm == NULL) {
		return JS_NewInt32(ctx, 0);
	}
	tm->thread = thread;
	tm->ms = (int)ms;
	tm->repeating = magic ? 1 : 0;
	tm->func = JS_UNDEFINED;
	tm->code = NULL;
	thread->next_timer++;
	if (thread->next_timer <= 0) {
		thread->next_timer = 1;
	}
	tm->id = thread->next_timer;

	if (JS_IsFunction(ctx, argv[0])) {
		tm->func = JS_DupValue(ctx, argv[0]);
	} else {
		code = JS_ToCString(ctx, argv[0]);
		if (code == NULL) {
			free(tm);
			return JS_EXCEPTION;
		}
		tm->code = strdup(code);
		JS_FreeCString(ctx, code);
		if (tm->code == NULL) {
			free(tm);
			return JS_NewInt32(ctx, 0);
		}
	}

	tm->next = thread->timers;
	thread->timers = tm;
	guit->misc->schedule(tm->ms, qjs_timer_fire, tm);
	return JS_NewInt32(ctx, tm->id);
}

static JSValue
js_clear_timer(JSContext *ctx, JSValueConst this_val,
		int argc, JSValueConst *argv)
{
	jsthread *thread;
	struct qjs_timer *tm;
	int32_t id;

	(void)this_val;
	thread = qjs_thread_from_ctx(ctx);
	if (thread == NULL || argc < 1) {
		return JS_UNDEFINED;
	}
	id = 0;
	if (JS_ToInt32(ctx, &id, argv[0]) < 0) {
		return JS_EXCEPTION;
	}
	tm = qjs_timer_find(thread, (int)id);
	if (tm != NULL) {
		qjs_timer_unlink(thread, tm, 1);
	}
	return JS_UNDEFINED;
}

static JSValue
js_alert(JSContext *ctx, JSValueConst this_val,
		int argc, JSValueConst *argv)
{
	(void)this_val;
	if (argc < 1) {
		NSLOG(netsurf, INFO, "JS ALERT");
		return JS_UNDEFINED;
	}
	qjs_console_write(ctx, BW_CS_FLAG_LEVEL_WARN, 1, argv);
	return JS_UNDEFINED;
}

static JSValue
js_name_get(JSContext *ctx, JSValueConst this_val)
{
	jsthread *thread;
	struct browser_window *bw;
	const char *name;

	(void)this_val;
	thread = qjs_thread_from_ctx(ctx);
	if (thread == NULL) {
		return JS_NewStringLen(ctx, "", 0);
	}
	bw = (struct browser_window *)thread->win_priv;
	name = NULL;
	if (bw == NULL ||
			browser_window_get_name(bw, &name) != NSERROR_OK ||
			name == NULL) {
		return JS_NewStringLen(ctx, "", 0);
	}
	return JS_NewString(ctx, name);
}

static JSValue
js_name_set(JSContext *ctx, JSValueConst this_val, JSValueConst val)
{
	jsthread *thread;
	struct browser_window *bw;
	const char *name;

	(void)this_val;
	thread = qjs_thread_from_ctx(ctx);
	if (thread == NULL) {
		return JS_UNDEFINED;
	}
	bw = (struct browser_window *)thread->win_priv;
	name = JS_ToCString(ctx, val);
	if (name != NULL && bw != NULL) {
		browser_window_set_name(bw, name);
	}
	if (name != NULL) {
		JS_FreeCString(ctx, name);
	}
	return JS_UNDEFINED;
}

static JSValue
js_set_timeout(JSContext *ctx, JSValueConst this_val,
		int argc, JSValueConst *argv)
{
	return js_set_timer(ctx, this_val, argc, argv, 0);
}

static JSValue
js_set_interval(JSContext *ctx, JSValueConst this_val,
		int argc, JSValueConst *argv)
{
	return js_set_timer(ctx, this_val, argc, argv, 1);
}

static const JSCFunctionListEntry qjs_win_funcs[] = {
	JS_CFUNC_DEF("alert", 1, js_alert),
	JS_CFUNC_DEF("setTimeout", 2, js_set_timeout),
	JS_CFUNC_DEF("setInterval", 2, js_set_interval),
	JS_CFUNC_DEF("clearTimeout", 1, js_clear_timer),
	JS_CFUNC_DEF("clearInterval", 1, js_clear_timer),
	JS_CGETSET_DEF("name", js_name_get, js_name_set),
};

static JSValue
qjs_make_location(JSContext *ctx, jsthread *thread)
{
	html_content *html;
	nsurl *url;
	JSValue obj;

	html = (html_content *)thread->doc_priv;
	if (html == NULL || html->base.llcache == NULL) {
		return JS_NULL;
	}
	url = llcache_handle_get_url(html->base.llcache);
	if (url == NULL) {
		return JS_NULL;
	}
	obj = JS_NewObjectClass(ctx, (int)thread->heap->loc_class);
	if (JS_IsException(obj)) {
		return obj;
	}
	nsurl_ref(url);
	JS_SetOpaque(obj, url);
	return obj;
}

nserror
qjs_bind_win_prepare(jsheap *heap)
{
	if (heap == NULL || heap->rt == NULL) {
		return NSERROR_BAD_PARAMETER;
	}
	if (heap->loc_class_ready) {
		return NSERROR_OK;
	}
	qjs_claim_class_id(heap->rt, &heap->loc_class);
	if (qjs_new_class(heap->rt, heap->loc_class, &qjs_loc_class) != 0) {
		NSLOG(netsurf, WARNING, "JS_NewClass Location failed id=%lu",
				(unsigned long)heap->loc_class);
		return NSERROR_NOMEM;
	}
	NSLOG(netsurf, INFO, "JS_NewClass Location id=%lu",
			(unsigned long)heap->loc_class);
	heap->loc_class_ready = 1;
	return NSERROR_OK;
}

/* Location / navigator / window timers / self. Called from js_exec.
 * Never ami_schedule from bind — WaitIO under content bind deadlocks OS3. */
void
qjs_bind_win_finish(jsthread *thread)
{
	JSContext *ctx;
	JSValue global;
	JSValue nav;
	JSValue loc;
	JSValue fn;
	JSValue console;

	if (thread == NULL || !thread->win_rest_pending) {
		return;
	}
	thread->win_rest_pending = 0;
	if (thread->closed || thread->ctx == NULL || thread->heap == NULL) {
		return;
	}

	ctx = thread->ctx;
	NSLOG(netsurf, INFO, "bind_win rest begin");
	global = JS_GetGlobalObject(ctx);

	if (thread->heap->loc_class_ready) {
		JSValue proto;

		NSLOG(netsurf, INFO, "bind_win location");
		proto = JS_NewObject(ctx);
		JS_SetPropertyFunctionList(ctx, proto, qjs_loc_funcs,
				(int)(sizeof(qjs_loc_funcs) /
						sizeof(qjs_loc_funcs[0])));
		JS_SetClassProto(ctx, thread->heap->loc_class, proto);
		loc = qjs_make_location(ctx, thread);
		JS_SetPropertyStr(ctx, global, "location", loc);
	}

	NSLOG(netsurf, INFO, "bind_win navigator");
	nav = JS_NewObject(ctx);
	JS_SetPropertyFunctionList(ctx, nav, qjs_nav_funcs,
			(int)(sizeof(qjs_nav_funcs) / sizeof(qjs_nav_funcs[0])));
	JS_SetPropertyStr(ctx, global, "navigator", nav);

	/* Install onto global with NewCFunction + SetPropertyStr — a bulk
	 * SetPropertyFunctionList on global locked up (likely the "name"
	 * CGETSET colliding with an existing global). Skip window.name. */
	NSLOG(netsurf, INFO, "bind_win timers");
	fn = JS_NewCFunction(ctx, js_alert, "alert", 1);
	JS_SetPropertyStr(ctx, global, "alert", fn);
	fn = JS_NewCFunction(ctx, js_set_timeout, "setTimeout", 2);
	JS_SetPropertyStr(ctx, global, "setTimeout", fn);
	fn = JS_NewCFunction(ctx, js_set_interval, "setInterval", 2);
	JS_SetPropertyStr(ctx, global, "setInterval", fn);
	fn = JS_NewCFunction(ctx, js_clear_timer, "clearTimeout", 1);
	JS_SetPropertyStr(ctx, global, "clearTimeout", fn);
	fn = JS_NewCFunction(ctx, js_clear_timer, "clearInterval", 1);
	JS_SetPropertyStr(ctx, global, "clearInterval", fn);
	(void)qjs_win_funcs;
	NSLOG(netsurf, INFO, "bind_win timers done");

	/* console.log is already installed in bind.c. Add the rest with
	 * NewCFunction — SetPropertyFunctionList on console locked up the
	 * same way MAGIC did earlier. */
	NSLOG(netsurf, INFO, "bind_win console extras");
	console = JS_GetPropertyStr(ctx, global, "console");
	if (!JS_IsUndefined(console) && !JS_IsNull(console) &&
			!JS_IsException(console)) {
		fn = JS_NewCFunction(ctx, js_con_info, "info", 0);
		JS_SetPropertyStr(ctx, console, "info", fn);
		fn = JS_NewCFunction(ctx, js_con_warn, "warn", 0);
		JS_SetPropertyStr(ctx, console, "warn", fn);
		fn = JS_NewCFunction(ctx, js_con_error, "error", 0);
		JS_SetPropertyStr(ctx, console, "error", fn);
		fn = JS_NewCFunction(ctx, js_con_debug, "debug", 0);
		JS_SetPropertyStr(ctx, console, "debug", fn);
	}
	JS_FreeValue(ctx, console);
	(void)qjs_con_funcs;
	NSLOG(netsurf, INFO, "bind_win console extras done");

	NSLOG(netsurf, INFO, "bind_win self/parent/top");
	JS_SetPropertyStr(ctx, global, "self", JS_DupValue(ctx, global));
	JS_SetPropertyStr(ctx, global, "parent", JS_DupValue(ctx, global));
	JS_SetPropertyStr(ctx, global, "top", JS_DupValue(ctx, global));
	JS_SetPropertyStr(ctx, global, "closed", JS_NewBool(ctx, 0));

	JS_FreeValue(ctx, global);
	NSLOG(netsurf, INFO, "bind_win rest done");
}

void
qjs_bind_win_install(JSContext *ctx, JSValue global, JSValue console)
{
	jsthread *thread;

	(void)global;
	(void)console;
	thread = qjs_thread_from_ctx(ctx);
	NSLOG(netsurf, INFO, "bind_win install (console only, rest pending)");

	/* console.log is already on the object from bind.c. Location /
	 * navigator / timers wait for the first js_exec. */
	(void)thread;
}

void
qjs_bind_dom_teardown(jsthread *thread)
{
	struct qjs_ctime *ct;
	struct qjs_ctime *next;

	if (thread == NULL) {
		return;
	}
	thread->win_rest_pending = 0;
	while (thread->timers != NULL) {
		qjs_timer_unlink(thread, thread->timers, 1);
	}
	ct = thread->ctimes;
	thread->ctimes = NULL;
	while (ct != NULL) {
		next = ct->next;
		free(ct->name);
		free(ct);
		ct = next;
	}
}
