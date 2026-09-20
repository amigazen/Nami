/*
 * Copyright 2026 AmigaZen / Nami contributors
 *
 * Private structures for the quickjs.library NetSurf binding.
 */

#ifndef NETSURF_JS_QUICKJS_AMIGA_PRIVATE_H_
#define NETSURF_JS_QUICKJS_AMIGA_PRIVATE_H_

#include "quickjs.h"

#include "javascript/js.h"

struct qjs_listener;
struct qjs_timer;
struct qjs_ctime;
struct qjs_node_memo;

struct jsheap {
	JSRuntime *rt;
	JSClassID node_class;
	int node_class_ready;
	JSClassID event_class;
	int event_class_ready;
	JSClassID list_class;
	int list_class_ready;
	JSClassID map_class;
	int map_class_ready;
	JSClassID loc_class;
	int loc_class_ready;
	JSClassID token_class;
	int token_class_ready;
	int timeout;
	int pending_destroy;
	int class_failed;
	unsigned int live_threads;
	/* Thread that owns node memo entries — finalizer unlinks via this
	 * so the JS opaque stays a plain dom_node* (no calloc wrapper). */
	struct jsthread *memo_thread;
};

struct jsthread {
	jsheap *heap;
	JSContext *ctx;
	void *win_priv; /* browser_window * */
	void *doc_priv; /* html_content * */
	struct qjs_listener *listeners;
	struct qjs_timer *timers;
	struct qjs_ctime *ctimes;
	/* Memoised JS wrappers keyed by dom_node* so node === node works. */
	struct qjs_node_memo *node_memo;
	int console_group;
	int next_timer;
	int pending_destroy;
	int closed;
	/* Location/navigator/timers not installed yet — finish in js_exec.
	 * Never ami_schedule from bind: that WaitIO's under the content
	 * path and deadlocks the whole OS when fetch reschedules. */
	int win_rest_pending;
};

#endif /* NETSURF_JS_QUICKJS_AMIGA_PRIVATE_H_ */
