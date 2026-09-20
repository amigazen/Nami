/*
 * Copyright 2026 AmigaZen / Nami contributors
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

/**
 * \file
 * JavaScript engine binding using Amiga quickjs.library (LVO client).
 *
 * Does not compile QuickJS into NetSurf. Calls go through the nea-quickjs
 * assembly bridge after OpenLibrary("quickjs.library").
 */

#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

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

#include "utils/errors.h"
#include "utils/log.h"
#include "utils/nsoption.h"

#include "javascript/js.h"
#include "javascript/content.h"

#include "private.h"
#include "bind.h"
#include "bind_priv.h"

/* Default heap cap for classic Amiga (bytes). Full DOM bind after
 * NewContext needs more than the old 2MB demo budget. */
#ifndef NETSURF_QJS_HEAP_LIMIT
#define NETSURF_QJS_HEAP_LIMIT (8 * 1024 * 1024)
#endif

/* Provided by nea-quickjs library/vbcc/quickjs_bridge.c */
extern int quickjs_bridge_init(void);
extern void quickjs_bridge_cleanup(void);

static int qjs_library_ok = 0;
static int qjs_library_tried = 0;
static int qjs_live_contexts = 0;
static int qjs_shutdown = 0;

/*
 * netsurf_exit calls js_finalise before hlcache destroys HTML contents.
 * Those contents still own JSContext objects. CloseLibrary must wait
 * until the last context and runtime have been freed, or destroythread
 * jumps into a closed library and locks up.
 */
static void qjs_close_library_if_idle(void)
{
	if (!qjs_shutdown || !qjs_library_ok) {
		return;
	}
	if (qjs_live_contexts != 0) {
		NSLOG(netsurf, INFO,
		      "defer CloseLibrary, contexts=%d", qjs_live_contexts);
		return;
	}

	NSLOG(netsurf, INFO, "CloseLibrary quickjs.library");
	quickjs_bridge_cleanup();
	qjs_library_ok = 0;
	qjs_library_tried = 0;
}

/*
 * OpenLibrary("quickjs.library") runs CustomLibInit (math, timer.device,
 * memory pool, net probe). Open on first real script context only.
 */
static int qjs_open_library(void)
{
	if (qjs_library_ok) {
		return 0;
	}
	if (qjs_library_tried) {
		return -1;
	}

	qjs_library_tried = 1;
	NSLOG(netsurf, INFO, "OpenLibrary quickjs.library");
	if (quickjs_bridge_init() != 0) {
		NSLOG(netsurf, WARNING,
		      "quickjs.library not available; disabling JavaScript");
		nsoption_set_bool(enable_javascript, false);
		return -1;
	}

	qjs_library_ok = 1;
	NSLOG(netsurf, INFO, "OpenLibrary quickjs.library ok");
	return 0;
}

void js_initialise(void)
{
	javascript_init();
	NSLOG(netsurf, INFO,
	      "JavaScript handler ready (quickjs.library deferred, option %s)",
	      nsoption_bool(enable_javascript) ? "on" : "off");
}

void js_finalise(void)
{
	NSLOG(netsurf, INFO, "js_finalise (library_ok=%d contexts=%d)",
			qjs_library_ok, qjs_live_contexts);
	qjs_shutdown = 1;
	qjs_close_library_if_idle();
	NSLOG(netsurf, INFO, "js_finalise done");
}

/*
 * NetSurf calls js_newheap for every window. Always allocate the heap
 * shell so toggling Enable JavaScript and reloading can still create a
 * thread. The QuickJS runtime stays deferred until js_newthread.
 */
nserror js_newheap(int timeout, jsheap **heap_out)
{
	jsheap *heap;

	*heap_out = NULL;

	heap = calloc(1, sizeof(*heap));
	if (heap == NULL) {
		return NSERROR_NOMEM;
	}

	heap->timeout = timeout;
	*heap_out = heap;
	return NSERROR_OK;
}

static nserror qjs_ensure_runtime(jsheap *heap)
{
	if (heap->rt != NULL) {
		return NSERROR_OK;
	}

	if (qjs_open_library() != 0) {
		return NSERROR_NOT_IMPLEMENTED;
	}

	NSLOG(netsurf, INFO, "JS_NewRuntime");
	heap->rt = JS_NewRuntime();
	if (heap->rt == NULL) {
		NSLOG(netsurf, WARNING, "JS_NewRuntime failed");
		return NSERROR_NOMEM;
	}

	JS_SetMemoryLimit(heap->rt, (size_t)NETSURF_QJS_HEAP_LIMIT);
	NSLOG(netsurf, INFO, "JS_NewRuntime ok");
	return NSERROR_OK;
}

static void qjs_destroyheap(jsheap *heap)
{
	assert(heap != NULL);
	assert(heap->live_threads == 0);

	NSLOG(netsurf, INFO, "JS_FreeRuntime %p", (void *)heap->rt);
	if (heap->rt != NULL) {
		JS_SetRuntimeOpaque(heap->rt, NULL);
		JS_FreeRuntime(heap->rt);
		heap->rt = NULL;
		NSLOG(netsurf, INFO, "JS_FreeRuntime done");
	}
	free(heap);
}

void js_destroyheap(jsheap *heap)
{
	if (heap == NULL) {
		return;
	}

	NSLOG(netsurf, INFO, "js_destroyheap live_threads=%u",
			heap->live_threads);
	heap->pending_destroy = 1;
	if (heap->live_threads == 0) {
		qjs_destroyheap(heap);
	}
}

nserror js_newthread(jsheap *heap, void *win_priv, void *doc_priv,
		jsthread **thread_out)
{
	jsthread *thread;

	*thread_out = NULL;

	if (heap == NULL || heap->pending_destroy) {
		return NSERROR_BAD_PARAMETER;
	}

	if (!nsoption_bool(enable_javascript)) {
		return NSERROR_NOT_IMPLEMENTED;
	}

	if (qjs_ensure_runtime(heap) != NSERROR_OK) {
		return NSERROR_NOMEM;
	}

	/* Classes before any context. A failed bind used to NewContext
	 * once per script and free it again; that storm is what died on
	 * exit inside JS_FreeRuntime. */
	if (heap->class_failed) {
		return NSERROR_NOMEM;
	}
	NSLOG(netsurf, INFO, "QuickJS class prepare");
	if (qjs_bind_prepare_runtime(heap) != NSERROR_OK) {
		NSLOG(netsurf, WARNING, "QuickJS class prepare failed");
		return NSERROR_NOMEM;
	}
	NSLOG(netsurf, INFO, "QuickJS class prepare ok");

	thread = calloc(1, sizeof(*thread));
	if (thread == NULL) {
		return NSERROR_NOMEM;
	}

	/* Full JS_NewContext pulls Date/RegExp/Proxy/TypedArray/Promise
	 * and friends through one LVO on the caller's stack — that is what
	 * locked up on "new window". Build a lean context instead. */
	NSLOG(netsurf, INFO, "JS_NewContextRaw");
	thread->ctx = JS_NewContextRaw(heap->rt);
	if (thread->ctx == NULL) {
		free(thread);
		NSLOG(netsurf, WARNING, "JS_NewContextRaw failed");
		return NSERROR_NOMEM;
	}
	if (JS_AddIntrinsicBaseObjects(thread->ctx) ||
			JS_AddIntrinsicEval(thread->ctx) ||
			JS_AddIntrinsicJSON(thread->ctx)) {
		JS_FreeContext(thread->ctx);
		free(thread);
		NSLOG(netsurf, WARNING, "QuickJS intrinsices failed");
		return NSERROR_NOMEM;
	}
	NSLOG(netsurf, INFO, "JS_NewContextRaw ok");

	thread->heap = heap;
	thread->win_priv = win_priv;
	thread->doc_priv = doc_priv;
	heap->memo_thread = thread;

	NSLOG(netsurf, INFO, "qjs_bind_install");
	if (qjs_bind_install(thread) != NSERROR_OK) {
		NSLOG(netsurf, WARNING, "qjs_bind_install failed");
		heap->memo_thread = NULL;
		JS_FreeContext(thread->ctx);
		free(thread);
		return NSERROR_NOMEM;
	}

	heap->live_threads++;
	qjs_live_contexts++;
	*thread_out = thread;
	return NSERROR_OK;
}

nserror js_closethread(jsthread *thread)
{
	if (thread == NULL) {
		return NSERROR_OK;
	}

	NSLOG(netsurf, INFO, "js_closethread");
	thread->closed = 1;
	return NSERROR_OK;
}

void js_destroythread(jsthread *thread)
{
	jsheap *heap;

	if (thread == NULL) {
		return;
	}

	NSLOG(netsurf, INFO, "js_destroythread");
	heap = thread->heap;

	/* Free JSValues and libdom listeners while the context lives. */
	if (qjs_library_ok) {
		qjs_bind_teardown(thread);
	}

	if (thread->ctx != NULL && qjs_library_ok) {
		NSLOG(netsurf, INFO, "JS_FreeContext");
		JS_FreeContext(thread->ctx);
		NSLOG(netsurf, INFO, "JS_FreeContext done");
		thread->ctx = NULL;
	}
	thread->ctx = NULL;

	/* Weak memo entries should be gone via finalizers; sweep leftovers. */
	qjs_node_memo_clear(thread);

	free(thread);

	if (qjs_live_contexts > 0) {
		qjs_live_contexts--;
	}

	if (heap != NULL) {
		if (heap->live_threads > 0) {
			heap->live_threads--;
		}
		if (heap->pending_destroy && heap->live_threads == 0) {
			qjs_destroyheap(heap);
		}
	}

	qjs_close_library_if_idle();
}

static void qjs_log_exception(JSContext *ctx)
{
	JSValue exc;
	const char *msg;

	exc = JS_GetException(ctx);
	msg = JS_ToCString(ctx, exc);
	if (msg != NULL) {
		NSLOG(netsurf, WARNING, "JavaScript exception: %s", msg);
		JS_FreeCString(ctx, msg);
	} else {
		NSLOG(netsurf, WARNING, "JavaScript exception (unprintable)");
	}
	JS_FreeValue(ctx, exc);
}

bool js_exec(jsthread *thread, const uint8_t *txt, size_t txtlen,
		const char *name)
{
	JSValue result;
	const char *filename;
	bool ok;

	if (thread == NULL) {
		NSLOG(netsurf, WARNING, "js_exec skipped: no thread");
		return false;
	}
	if (thread->ctx == NULL || thread->pending_destroy || thread->closed) {
		NSLOG(netsurf, WARNING,
				"js_exec skipped: ctx=%p pending=%d closed=%d",
				(void *)thread->ctx, thread->pending_destroy,
				thread->closed);
		return false;
	}

	if (txt == NULL || txtlen == 0) {
		NSLOG(netsurf, WARNING, "js_exec skipped: empty script");
		return false;
	}

	NSLOG(netsurf, INFO, "js_exec begin %lu bytes pending_win=%d",
			(unsigned long)txtlen, thread->win_rest_pending);

	/* Finish location/navigator/timers here — never via ami_schedule
	 * from bind (that deadlocks OS3 when fetch WaitIO's). */
	qjs_bind_win_finish(thread);

	if (name != NULL) {
		filename = name;
	} else {
		filename = "<script>";
	}

	NSLOG(netsurf, INFO, "JS eval %lu bytes from %s",
			(unsigned long)txtlen, filename);

	result = JS_Eval(thread->ctx, (const char *)txt, txtlen, filename,
			JS_EVAL_TYPE_GLOBAL);

	if (JS_IsException(result)) {
		qjs_log_exception(thread->ctx);
		JS_FreeValue(thread->ctx, result);
		return false;
	}

	ok = true;
	JS_FreeValue(thread->ctx, result);
	return ok;
}

bool js_fire_event(jsthread *thread, const char *type,
		struct dom_document *doc, struct dom_node *target)
{
	/*
	 * Click and most DOM events are delivered via libdom listeners
	 * registered in addEventListener. load-on-window is swallowed
	 * quietly until a Window handler binding exists.
	 */
	if (thread == NULL || type == NULL) {
		return true;
	}
	(void)doc;
	(void)target;
	return true;
}

bool js_dom_event_add_listener(jsthread *thread,
		struct dom_document *document,
		struct dom_node *node,
		struct dom_string *event_type_dom,
		void *js_funcval)
{
	(void)thread;
	(void)document;
	(void)node;
	(void)event_type_dom;
	(void)js_funcval;
	return false;
}

void js_handle_new_element(jsthread *thread, struct dom_element *node)
{
	if (thread == NULL || node == NULL) {
		return;
	}
}

void js_event_cleanup(jsthread *thread, struct dom_event *evt)
{
	if (thread == NULL || evt == NULL) {
		return;
	}
}
