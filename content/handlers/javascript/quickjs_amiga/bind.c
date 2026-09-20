/*
 * Copyright 2026 AmigaZen / Nami contributors
 *
 * Hand-written QuickJS bindings: window, document, Element, console.
 * Enough for a click → textContent welcome-page demo.
 */

#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>
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

#include <dom/dom.h>

#include "utils/log.h"
#include "netsurf/browser_window.h"
#include "netsurf/console.h"
#include "html/private.h"

#include "private.h"
#include "bind.h"
#include "bind_priv.h"
#include "dom_sync.h"

struct qjs_listener {
	struct qjs_listener *next;
	dom_node *node;
	dom_string *type;
	JSValue func;
	dom_event_listener *dom_listen;
	int dom_registered;
};

/* One JS object per live DOM node pointer (same idea as dukky NODE_MAGIC).
 * The memo is weak: it does not JS_DupValue. The object finalizer unlinks.
 * Opaque stays a plain dom_node* — a calloc'd wrapper broke script eval
 * on the LVO path (no JS eval after bind). */
struct qjs_node_memo {
	struct qjs_node_memo *next;
	dom_node *node;
	JSValue obj;
};

/* Client trampoline for JS_NewClass expects (rt, class_def, class_id)
 * on the stack while quickjs.h declares (rt, class_id, class_def). */
int
qjs_new_class(JSRuntime *rt, JSClassID class_id, const JSClassDef *def)
{
	return JS_NewClass(rt, (JSClassID)(uintptr_t)def,
			(const JSClassDef *)(uintptr_t)class_id);
}

static void
qjs_memo_unlink(jsthread *thread, dom_node *node)
{
	struct qjs_node_memo **pp;
	struct qjs_node_memo *m;

	if (thread == NULL || node == NULL) {
		return;
	}
	pp = &thread->node_memo;
	while (*pp != NULL) {
		m = *pp;
		if (m->node == node) {
			*pp = m->next;
			free(m);
			return;
		}
		pp = &(*pp)->next;
	}
}

void
qjs_node_memo_clear(jsthread *thread)
{
	struct qjs_node_memo *m;
	struct qjs_node_memo *next;

	/* After FreeContext only — free leftover weak entries. Never
	 * JS_FreeValue here; that raced FreeContext and crashed on exit. */
	if (thread == NULL) {
		return;
	}
	if (thread->heap != NULL && thread->heap->memo_thread == thread) {
		thread->heap->memo_thread = NULL;
	}
	m = thread->node_memo;
	thread->node_memo = NULL;
	while (m != NULL) {
		next = m->next;
		free(m);
		m = next;
	}
}

/*
 * quickjs.library's js_class_id_alloc can come back as 0. JS_NewClassID
 * then hands out 0 (free-slot sentinel) and the next id collides with
 * JS_CLASS_OBJECT. Seed above the builtin enum and skip live slots.
 * Stay well under 256 so class_proto stays modest.
 */
static JSClassID qjs_class_seq = 64;

void
qjs_claim_class_id(JSRuntime *rt, JSClassID *id)
{
	JSClassID n;

	if (*id != 0) {
		JS_NewClassID(rt, id);
		return;
	}

	n = 0;
	JS_NewClassID(rt, &n);
	if (n == 0 || JS_IsRegisteredClass(rt, n)) {
		n = qjs_class_seq;
		while (n < 512 && JS_IsRegisteredClass(rt, n)) {
			n++;
		}
		qjs_class_seq = (JSClassID)(n + 1);
	}
	*id = n;
	JS_NewClassID(rt, id);
}

static void
qjs_node_finalizer_rt(JSRuntime *rt, JSValueConst val)
{
	jsheap *heap;
	dom_node *node;

	heap = (jsheap *)JS_GetRuntimeOpaque(rt);
	if (heap == NULL || !heap->node_class_ready) {
		return;
	}
	node = (dom_node *)JS_GetOpaque(val, heap->node_class);
	if (node == NULL) {
		return;
	}
	JS_SetOpaque(val, NULL);
	qjs_memo_unlink(heap->memo_thread, node);
	dom_node_unref(node);
}

static JSClassDef qjs_node_class_live = {
	"HTMLElement",
	qjs_node_finalizer_rt,
	NULL,
	NULL,
	NULL
};

jsthread *
qjs_thread_from_ctx(JSContext *ctx)
{
	return (jsthread *)JS_GetContextOpaque(ctx);
}

JSValue
qjs_push_node(JSContext *ctx, dom_node *node)
{
	jsthread *thread;
	jsheap *heap;
	struct qjs_node_memo *m;
	JSValue obj;

	if (node == NULL) {
		return JS_NULL;
	}

	thread = qjs_thread_from_ctx(ctx);
	if (thread == NULL || thread->heap == NULL) {
		return JS_NULL;
	}
	heap = thread->heap;
	if (!heap->node_class_ready) {
		return JS_NULL;
	}
	heap->memo_thread = thread;

	for (m = thread->node_memo; m != NULL; m = m->next) {
		if (m->node == node) {
			return JS_DupValue(ctx, m->obj);
		}
	}

	obj = JS_NewObjectClass(ctx, heap->node_class);
	if (JS_IsException(obj)) {
		return obj;
	}

	dom_node_ref(node);
	if (JS_SetOpaque(obj, node) != 0) {
		/* LVO status can be unreliable; opaque may still be set. */
		NSLOG(netsurf, WARNING, "JS_SetOpaque node status != 0");
	}

	m = calloc(1, sizeof(*m));
	if (m != NULL) {
		m->node = node;
		m->obj = obj; /* weak — finalizer unlinks */
		m->next = thread->node_memo;
		thread->node_memo = m;
	}
	return obj;
}

dom_node *
qjs_node_from_this(JSContext *ctx, JSValueConst this_val)
{
	jsthread *thread;
	jsheap *heap;

	thread = qjs_thread_from_ctx(ctx);
	if (thread == NULL || thread->heap == NULL) {
		return NULL;
	}
	heap = thread->heap;
	if (!heap->node_class_ready) {
		return NULL;
	}
	return (dom_node *)JS_GetOpaque2(ctx, this_val, heap->node_class);
}

static JSValue
js_console_log(JSContext *ctx, JSValueConst this_val,
		int argc, JSValueConst *argv)
{
	jsthread *thread;
	struct browser_window *bw;
	int i;
	const char *s;
	size_t len;

	(void)this_val;
	thread = qjs_thread_from_ctx(ctx);
	if (thread == NULL) {
		return JS_UNDEFINED;
	}
	bw = (struct browser_window *)thread->win_priv;

	for (i = 0; i < argc; i++) {
		s = JS_ToCStringLen(ctx, &len, argv[i]);
		if (s == NULL) {
			continue;
		}
		NSLOG(netsurf, INFO, "console.log: %s", s);
		if (bw != NULL) {
			browser_window_console_log(bw, BW_CS_SCRIPT_CONSOLE,
					s, len, BW_CS_FLAG_LEVEL_LOG);
		}
		JS_FreeCString(ctx, s);
	}
	return JS_UNDEFINED;
}

static JSValue
js_document_getElementById(JSContext *ctx, JSValueConst this_val,
		int argc, JSValueConst *argv)
{
	jsthread *thread;
	html_content *html;
	dom_string *id_dom;
	dom_element *element;
	dom_exception exc;
	const char *text;
	size_t text_len;
	JSValue result;

	(void)this_val;
	if (argc < 1) {
		return JS_NULL;
	}

	thread = qjs_thread_from_ctx(ctx);
	if (thread == NULL) {
		return JS_NULL;
	}
	html = (html_content *)thread->doc_priv;
	if (html == NULL || html->document == NULL) {
		return JS_NULL;
	}

	text = JS_ToCStringLen(ctx, &text_len, argv[0]);
	if (text == NULL) {
		return JS_EXCEPTION;
	}

	exc = dom_string_create((const uint8_t *)text, text_len, &id_dom);
	JS_FreeCString(ctx, text);
	if (exc != DOM_NO_ERR) {
		return JS_NULL;
	}

	exc = dom_document_get_element_by_id(html->document, id_dom, &element);
	dom_string_unref(id_dom);
	if (exc != DOM_NO_ERR || element == NULL) {
		return JS_NULL;
	}

	result = qjs_push_node(ctx, (dom_node *)element);
	dom_node_unref(element);
	return result;
}

static JSValue
js_element_get_textContent(JSContext *ctx, JSValueConst this_val)
{
	dom_node *node;
	dom_exception exc;
	dom_string *content;
	JSValue result;

	node = qjs_node_from_this(ctx, this_val);
	if (node == NULL) {
		return JS_NULL;
	}

	exc = dom_node_get_text_content(node, &content);
	if (exc != DOM_NO_ERR || content == NULL) {
		return JS_NULL;
	}

	result = JS_NewStringLen(ctx, dom_string_data(content),
			dom_string_length(content));
	dom_string_unref(content);
	return result;
}

static JSValue
js_element_set_textContent(JSContext *ctx, JSValueConst this_val,
		JSValueConst val)
{
	dom_node *node;
	dom_node *child;
	dom_node *spare;
	dom_exception exc;
	dom_string *content;
	const char *text;
	size_t text_len;
	jsthread *thread;
	html_content *html;

	node = qjs_node_from_this(ctx, this_val);
	if (node == NULL) {
		return JS_UNDEFINED;
	}

	text = JS_ToCStringLen(ctx, &text_len, val);
	if (text == NULL) {
		return JS_EXCEPTION;
	}

	/* Spec: empty string removes all children and does not create a
	 * Text node. libdom's set_text_content always appends a Text, which
	 * left a phantom child and broke NodeList.item(0) === element. */
	if (text_len == 0) {
		JS_FreeCString(ctx, text);
		child = NULL;
		while (dom_node_get_first_child(node, &child) == DOM_NO_ERR &&
				child != NULL) {
			spare = NULL;
			dom_node_remove_child(node, child, &spare);
			dom_node_unref(child);
			if (spare != NULL) {
				dom_node_unref(spare);
			}
			child = NULL;
		}
	} else {
		exc = dom_string_create((const uint8_t *)text, text_len, &content);
		JS_FreeCString(ctx, text);
		if (exc != DOM_NO_ERR) {
			return JS_UNDEFINED;
		}
		exc = dom_node_set_text_content(node, content);
		dom_string_unref(content);
		if (exc != DOM_NO_ERR) {
			return JS_UNDEFINED;
		}
	}

	thread = qjs_thread_from_ctx(ctx);
	if (thread != NULL) {
		html = (html_content *)thread->doc_priv;
		qjs_dom_sync_text(html, node);
	}

	NSLOG(netsurf, INFO, "textContent set on node %p", (void *)node);
	return JS_UNDEFINED;
}

static int
qjs_node_is_or_ancestor_of(dom_node *want, dom_node *target)
{
	dom_node *n;
	dom_node *parent;
	dom_exception exc;
	int owned;

	if (want == NULL || target == NULL) {
		return 0;
	}

	n = target;
	owned = 0;
	while (n != NULL) {
		if (n == want) {
			if (owned) {
				dom_node_unref(n);
			}
			return 1;
		}
		parent = NULL;
		exc = dom_node_get_parent_node(n, &parent);
		if (owned) {
			dom_node_unref(n);
		}
		if (exc != DOM_NO_ERR) {
			return 0;
		}
		n = parent;
		owned = 1;
	}
	return 0;
}

static void
qjs_dom_event_callback(dom_event *evt, void *pw)
{
	jsthread *thread = (jsthread *)pw;
	dom_string *name;
	dom_event_target *targ;
	dom_exception exc;
	struct qjs_listener *lis;
	JSValue ret;
	JSValue this_obj;
	JSValue ev;
	JSValue argv[1];

	if (thread == NULL || thread->ctx == NULL || thread->closed ||
			thread->pending_destroy) {
		return;
	}

	exc = dom_event_get_type(evt, &name);
	if (exc != DOM_NO_ERR) {
		return;
	}

	/* Match against the event target (and ancestors), not currentTarget.
	 * Libdom listeners are hung on the document so every click reaches
	 * us even when the box tree points at a text node or <html>. */
	exc = dom_event_get_target(evt, &targ);
	if (exc != DOM_NO_ERR) {
		dom_string_unref(name);
		return;
	}

	NSLOG(netsurf, INFO, "qjs event '%*s' target %p",
			(int)dom_string_length(name),
			dom_string_data(name), (void *)targ);

	ev = qjs_push_event(thread->ctx, evt);
	argv[0] = ev;

	for (lis = thread->listeners; lis != NULL; lis = lis->next) {
		if (dom_string_isequal(lis->type, name) == false) {
			continue;
		}
		if (!qjs_node_is_or_ancestor_of(lis->node,
				(dom_node *)targ)) {
			continue;
		}

		this_obj = qjs_push_node(thread->ctx, lis->node);
		ret = JS_Call(thread->ctx, lis->func, this_obj, 1, argv);
		JS_FreeValue(thread->ctx, this_obj);
		if (JS_IsException(ret)) {
			JSValue exc_v;
			const char *msg;

			exc_v = JS_GetException(thread->ctx);
			msg = JS_ToCString(thread->ctx, exc_v);
			if (msg != NULL) {
				NSLOG(netsurf, WARNING,
						"listener exception: %s", msg);
				JS_FreeCString(thread->ctx, msg);
			}
			JS_FreeValue(thread->ctx, exc_v);
		}
		JS_FreeValue(thread->ctx, ret);
	}

	JS_FreeValue(thread->ctx, ev);
	dom_node_unref(targ);
	dom_string_unref(name);
}

static JSValue
js_element_addEventListener(JSContext *ctx, JSValueConst this_val,
		int argc, JSValueConst *argv)
{
	jsthread *thread;
	html_content *html;
	dom_node *node;
	dom_node *doc_node;
	dom_string *type_dom;
	dom_event_listener *listen;
	dom_exception exc;
	struct qjs_listener *lis;
	struct qjs_listener *scan;
	const char *type;
	size_t type_len;
	int already;

	if (argc < 2) {
		return JS_UNDEFINED;
	}

	node = qjs_node_from_this(ctx, this_val);
	if (node == NULL) {
		return JS_UNDEFINED;
	}

	if (!JS_IsFunction(ctx, argv[1])) {
		return JS_UNDEFINED;
	}

	thread = qjs_thread_from_ctx(ctx);
	if (thread == NULL || thread->closed) {
		return JS_UNDEFINED;
	}
	html = (html_content *)thread->doc_priv;
	if (html == NULL || html->document == NULL) {
		return JS_UNDEFINED;
	}
	doc_node = (dom_node *)html->document;

	type = JS_ToCStringLen(ctx, &type_len, argv[0]);
	if (type == NULL) {
		return JS_EXCEPTION;
	}

	exc = dom_string_create((const uint8_t *)type, type_len, &type_dom);
	JS_FreeCString(ctx, type);
	if (exc != DOM_NO_ERR) {
		return JS_UNDEFINED;
	}

	lis = calloc(1, sizeof(*lis));
	if (lis == NULL) {
		dom_string_unref(type_dom);
		return JS_EXCEPTION;
	}

	lis->node = dom_node_ref(node);
	lis->type = type_dom;
	lis->func = JS_DupValue(ctx, argv[1]);
	lis->next = thread->listeners;
	thread->listeners = lis;

	/* One libdom listener per event type on the document — bubbles
	 * from any box/text node reach us. */
	already = 0;
	for (scan = lis->next; scan != NULL; scan = scan->next) {
		if (dom_string_isequal(scan->type, type_dom) &&
				scan->dom_registered) {
			already = 1;
			break;
		}
	}

	if (!already) {
		exc = dom_event_listener_create(qjs_dom_event_callback,
				thread, &listen);
		if (exc == DOM_NO_ERR) {
			exc = dom_event_target_add_event_listener(
					doc_node, type_dom, listen, false);
			if (exc == DOM_NO_ERR) {
				lis->dom_listen = listen;
				lis->dom_registered = 1;
				NSLOG(netsurf, INFO,
						"addEventListener %*s on %p (doc)",
						(int)dom_string_length(type_dom),
						dom_string_data(type_dom),
						(void *)node);
			} else {
				dom_event_listener_unref(listen);
			}
		}
	} else {
		NSLOG(netsurf, INFO, "addEventListener %*s on %p (shared)",
				(int)dom_string_length(type_dom),
				dom_string_data(type_dom), (void *)node);
	}

	return JS_UNDEFINED;
}

static JSValue
js_element_removeEventListener(JSContext *ctx, JSValueConst this_val,
		int argc, JSValueConst *argv)
{
	jsthread *thread;
	html_content *html;
	dom_node *node;
	dom_node *doc_node;
	dom_string *type_dom;
	const char *type;
	size_t type_len;
	struct qjs_listener **pp;
	struct qjs_listener *lis;
	struct qjs_listener *scan;
	int moved;

	if (argc < 2) {
		return JS_UNDEFINED;
	}
	thread = qjs_thread_from_ctx(ctx);
	node = qjs_node_from_this(ctx, this_val);
	if (thread == NULL || node == NULL) {
		return JS_UNDEFINED;
	}
	html = (html_content *)thread->doc_priv;
	doc_node = (html != NULL && html->document != NULL) ?
			(dom_node *)html->document : NULL;
	type = JS_ToCStringLen(ctx, &type_len, argv[0]);
	if (type == NULL) {
		return JS_EXCEPTION;
	}
	type_dom = NULL;
	if (dom_string_create((const uint8_t *)type, type_len, &type_dom) !=
			DOM_NO_ERR) {
		JS_FreeCString(ctx, type);
		return JS_UNDEFINED;
	}
	JS_FreeCString(ctx, type);

	pp = &thread->listeners;
	while (*pp != NULL) {
		lis = *pp;
		if (lis->node != node ||
				dom_string_isequal(lis->type, type_dom) == false ||
				JS_IsSameValue(ctx, lis->func, argv[1]) == false) {
			pp = &lis->next;
			continue;
		}
		*pp = lis->next;
		moved = 0;
		if (lis->dom_registered && lis->dom_listen != NULL) {
			for (scan = thread->listeners; scan != NULL;
					scan = scan->next) {
				if (dom_string_isequal(scan->type, type_dom)) {
					scan->dom_listen = lis->dom_listen;
					scan->dom_registered = 1;
					moved = 1;
					break;
				}
			}
			if (!moved && doc_node != NULL) {
				dom_event_target_remove_event_listener(
						doc_node, type_dom,
						lis->dom_listen, false);
				dom_event_listener_unref(lis->dom_listen);
			}
		}
		JS_FreeValue(ctx, lis->func);
		dom_string_unref(lis->type);
		dom_node_unref(lis->node);
		free(lis);
		break;
	}
	dom_string_unref(type_dom);
	return JS_UNDEFINED;
}

static const JSCFunctionListEntry qjs_element_proto_funcs[] = {
	JS_CGETSET_DEF("textContent", js_element_get_textContent,
			js_element_set_textContent),
	JS_CFUNC_DEF("addEventListener", 2, js_element_addEventListener),
	JS_CFUNC_DEF("removeEventListener", 2, js_element_removeEventListener),
};

static const JSCFunctionListEntry qjs_document_funcs[] = {
	JS_CFUNC_DEF("getElementById", 1, js_document_getElementById),
};

static const JSCFunctionListEntry qjs_console_funcs[] = {
	JS_CFUNC_DEF("log", 0, js_console_log),
};

static nserror
qjs_ensure_node_class(jsheap *heap)
{
	if (heap->node_class_ready) {
		return NSERROR_OK;
	}

	qjs_claim_class_id(heap->rt, &heap->node_class);
	if (qjs_new_class(heap->rt, heap->node_class,
			&qjs_node_class_live) != 0) {
		NSLOG(netsurf, WARNING, "JS_NewClass HTMLElement failed id=%lu",
				(unsigned long)heap->node_class);
		return NSERROR_NOMEM;
	}

	JS_SetRuntimeOpaque(heap->rt, heap);
	heap->node_class_ready = 1;
	return NSERROR_OK;
}

nserror
qjs_bind_prepare_runtime(jsheap *heap)
{
	if (heap == NULL || heap->rt == NULL) {
		return NSERROR_BAD_PARAMETER;
	}
	if (heap->class_failed) {
		return NSERROR_NOMEM;
	}
	if (qjs_ensure_node_class(heap) != NSERROR_OK ||
			qjs_bind_dom_prepare(heap) != NSERROR_OK ||
			qjs_bind_win_prepare(heap) != NSERROR_OK) {
		heap->class_failed = 1;
		return NSERROR_NOMEM;
	}
	return NSERROR_OK;
}

nserror qjs_bind_install(jsthread *thread)
{
	JSContext *ctx;
	JSValue global;
	JSValue proto;
	JSValue document;
	JSValue console;
	html_content *html;

	if (thread == NULL || thread->ctx == NULL || thread->heap == NULL) {
		return NSERROR_BAD_PARAMETER;
	}

	NSLOG(netsurf, INFO, "bind_install begin (sizeof list entry=%lu)",
			(unsigned long)sizeof(JSCFunctionListEntry));

	if (qjs_ensure_node_class(thread->heap) != NSERROR_OK) {
		return NSERROR_NOMEM;
	}
	if (qjs_bind_dom_prepare(thread->heap) != NSERROR_OK) {
		return NSERROR_NOMEM;
	}

	ctx = thread->ctx;
	JS_SetContextOpaque(ctx, thread);

	NSLOG(netsurf, INFO, "bind_install proto");
	proto = JS_NewObject(ctx);
	JS_SetPropertyFunctionList(ctx, proto, qjs_element_proto_funcs,
			(int)(sizeof(qjs_element_proto_funcs) /
					sizeof(qjs_element_proto_funcs[0])));
	/* Proto is consumed by SetClassProto. Attach methods first. */
	document = JS_UNDEFINED;
	NSLOG(netsurf, INFO, "bind_install node");
	qjs_bind_node_install(ctx, proto, document);
	NSLOG(netsurf, INFO, "bind_install list");
	qjs_bind_list_install(ctx, proto);
	NSLOG(netsurf, INFO, "bind_install SetClassProto id=%lu",
			(unsigned long)thread->heap->node_class);
	JS_SetClassProto(ctx, thread->heap->node_class, proto);

	global = JS_GetGlobalObject(ctx);

	html = (html_content *)thread->doc_priv;
	if (html != NULL && html->document != NULL) {
		NSLOG(netsurf, INFO, "bind_install document");
		document = qjs_push_node(ctx, (dom_node *)html->document);
		if (!JS_IsException(document) && !JS_IsNull(document)) {
			JS_SetPropertyFunctionList(ctx, document, qjs_document_funcs,
					(int)(sizeof(qjs_document_funcs) /
							sizeof(qjs_document_funcs[0])));
			JS_SetPropertyStr(ctx, global, "document", document);
		} else {
			NSLOG(netsurf, WARNING, "bind_install document push failed");
			JS_FreeValue(ctx, document);
		}
	}

	NSLOG(netsurf, INFO, "bind_install window");
	console = JS_NewObject(ctx);
	NSLOG(netsurf, INFO, "bind_install console object");
	JS_SetPropertyFunctionList(ctx, console, qjs_console_funcs,
			(int)(sizeof(qjs_console_funcs) /
					sizeof(qjs_console_funcs[0])));
	qjs_bind_win_install(ctx, global, console);
	/* Set pending here (not in bind_win.o) so a stale win object cannot
	 * write win_rest_pending onto closed after jsthread layout changes. */
	thread->win_rest_pending = 1;
	JS_SetPropertyStr(ctx, global, "console", console);

	/* window === globalThis (SetPropertyStr takes the DupValue). */
	JS_SetPropertyStr(ctx, global, "window", JS_DupValue(ctx, global));

	JS_FreeValue(ctx, global);

	NSLOG(netsurf, INFO, "QuickJS DOM bindings installed");
	return NSERROR_OK;
}

void qjs_bind_teardown(jsthread *thread)
{
	html_content *html;
	dom_node *doc_node;
	struct qjs_listener *lis;
	struct qjs_listener *next;

	if (thread == NULL) {
		return;
	}

	qjs_bind_dom_teardown(thread);

	html = (html_content *)thread->doc_priv;
	doc_node = (html != NULL && html->document != NULL) ?
			(dom_node *)html->document : NULL;

	lis = thread->listeners;
	thread->listeners = NULL;

	while (lis != NULL) {
		next = lis->next;
		if (lis->dom_registered && lis->dom_listen != NULL &&
				doc_node != NULL && lis->type != NULL) {
			dom_event_target_remove_event_listener(
					doc_node, lis->type,
					lis->dom_listen, false);
			dom_event_listener_unref(lis->dom_listen);
		}
		if (thread->ctx != NULL) {
			JS_FreeValue(thread->ctx, lis->func);
		}
		if (lis->type != NULL) {
			dom_string_unref(lis->type);
		}
		if (lis->node != NULL) {
			dom_node_unref(lis->node);
		}
		free(lis);
		lis = next;
	}

	if (thread->ctx != NULL) {
		JS_SetContextOpaque(thread->ctx, NULL);
	}
}
