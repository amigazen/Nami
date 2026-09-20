/*
 * Copyright 2026 AmigaZen / Nami contributors
 *
 * Event, NodeList, NamedNodeMap and DOMTokenList classes.
 * Indexed access (list[0]) is an exotic own-property so prototype
 * methods (item, length) still resolve.
 */

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

#include <dom/dom.h>

#include "utils/log.h"

#include "private.h"
#include "bind_priv.h"

static void
qjs_event_finalizer(JSRuntime *rt, JSValueConst val)
{
	jsheap *heap;
	dom_event *evt;

	heap = (jsheap *)JS_GetRuntimeOpaque(rt);
	if (heap == NULL || !heap->event_class_ready) {
		return;
	}
	evt = (dom_event *)JS_GetOpaque(val, heap->event_class);
	if (evt != NULL) {
		dom_event_unref(evt);
	}
}

static void
qjs_list_finalizer(JSRuntime *rt, JSValueConst val)
{
	jsheap *heap;
	dom_nodelist *list;

	heap = (jsheap *)JS_GetRuntimeOpaque(rt);
	if (heap == NULL || !heap->list_class_ready) {
		return;
	}
	list = (dom_nodelist *)JS_GetOpaque(val, heap->list_class);
	if (list != NULL) {
		dom_nodelist_unref(list);
	}
}

static void
qjs_map_finalizer(JSRuntime *rt, JSValueConst val)
{
	jsheap *heap;
	dom_namednodemap *map;

	heap = (jsheap *)JS_GetRuntimeOpaque(rt);
	if (heap == NULL || !heap->map_class_ready) {
		return;
	}
	map = (dom_namednodemap *)JS_GetOpaque(val, heap->map_class);
	if (map != NULL) {
		dom_namednodemap_unref(map);
	}
}

static void
qjs_token_finalizer(JSRuntime *rt, JSValueConst val)
{
	jsheap *heap;
	dom_tokenlist *tokens;

	heap = (jsheap *)JS_GetRuntimeOpaque(rt);
	if (heap == NULL || !heap->token_class_ready) {
		return;
	}
	tokens = (dom_tokenlist *)JS_GetOpaque(val, heap->token_class);
	if (tokens != NULL) {
		dom_tokenlist_unref(tokens);
	}
}

/* Return 1 and fill desc when prop is a decimal index into the list. */
static int
qjs_index_atom(JSContext *ctx, JSAtom prop, uint32_t *idx)
{
	const char *s;
	char *end;
	unsigned long n;

	s = JS_AtomToCString(ctx, prop);
	if (s == NULL) {
		return -1;
	}
	if (s[0] < '0' || s[0] > '9') {
		JS_FreeCString(ctx, s);
		return 0;
	}
	n = strtoul(s, &end, 10);
	if (end == s || *end != '\0') {
		JS_FreeCString(ctx, s);
		return 0;
	}
	JS_FreeCString(ctx, s);
	*idx = (uint32_t)n;
	return 1;
}

static int
qjs_list_get_own(JSContext *ctx, JSPropertyDescriptor *desc,
		JSValueConst obj, JSAtom prop)
{
	jsthread *thread;
	dom_nodelist *list;
	dom_node *node;
	uint32_t idx;
	int is_idx;

	is_idx = qjs_index_atom(ctx, prop, &idx);
	if (is_idx <= 0) {
		return is_idx;
	}

	thread = qjs_thread_from_ctx(ctx);
	if (thread == NULL || thread->heap == NULL ||
			!thread->heap->list_class_ready) {
		return 0;
	}
	list = (dom_nodelist *)JS_GetOpaque(obj, thread->heap->list_class);
	if (list == NULL) {
		return 0;
	}
	node = NULL;
	if (dom_nodelist_item(list, idx, &node) != DOM_NO_ERR || node == NULL) {
		if (node != NULL) {
			dom_node_unref(node);
		}
		return 0;
	}
	if (desc != NULL) {
		desc->flags = JS_PROP_ENUMERABLE;
		desc->value = qjs_push_node(ctx, node);
		desc->getter = JS_UNDEFINED;
		desc->setter = JS_UNDEFINED;
	}
	dom_node_unref(node);
	return 1;
}

static int
qjs_map_get_own(JSContext *ctx, JSPropertyDescriptor *desc,
		JSValueConst obj, JSAtom prop)
{
	jsthread *thread;
	dom_namednodemap *map;
	dom_node *node;
	uint32_t idx;
	int is_idx;

	is_idx = qjs_index_atom(ctx, prop, &idx);
	if (is_idx <= 0) {
		return is_idx;
	}

	thread = qjs_thread_from_ctx(ctx);
	if (thread == NULL || thread->heap == NULL ||
			!thread->heap->map_class_ready) {
		return 0;
	}
	map = (dom_namednodemap *)JS_GetOpaque(obj, thread->heap->map_class);
	if (map == NULL) {
		return 0;
	}
	node = NULL;
	if (dom_namednodemap_item(map, idx, &node) != DOM_NO_ERR ||
			node == NULL) {
		if (node != NULL) {
			dom_node_unref(node);
		}
		return 0;
	}
	if (desc != NULL) {
		desc->flags = JS_PROP_ENUMERABLE;
		desc->value = qjs_push_node(ctx, node);
		desc->getter = JS_UNDEFINED;
		desc->setter = JS_UNDEFINED;
	}
	dom_node_unref(node);
	return 1;
}

static int
qjs_token_get_own(JSContext *ctx, JSPropertyDescriptor *desc,
		JSValueConst obj, JSAtom prop)
{
	jsthread *thread;
	dom_tokenlist *tokens;
	dom_string *value;
	uint32_t idx;
	int is_idx;

	is_idx = qjs_index_atom(ctx, prop, &idx);
	if (is_idx <= 0) {
		return is_idx;
	}

	thread = qjs_thread_from_ctx(ctx);
	if (thread == NULL || thread->heap == NULL ||
			!thread->heap->token_class_ready) {
		return 0;
	}
	tokens = (dom_tokenlist *)JS_GetOpaque(obj, thread->heap->token_class);
	if (tokens == NULL) {
		return 0;
	}
	value = NULL;
	if (dom_tokenlist_item(tokens, idx, &value) != DOM_NO_ERR ||
			value == NULL) {
		if (value != NULL) {
			dom_string_unref(value);
		}
		return 0;
	}
	if (desc != NULL) {
		desc->flags = JS_PROP_ENUMERABLE;
		desc->value = JS_NewStringLen(ctx,
				(const char *)dom_string_data(value),
				dom_string_length(value));
		desc->getter = JS_UNDEFINED;
		desc->setter = JS_UNDEFINED;
	}
	dom_string_unref(value);
	return 1;
}

static JSClassExoticMethods qjs_list_exotic = {
	qjs_list_get_own,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL
};

static JSClassExoticMethods qjs_map_exotic = {
	qjs_map_get_own,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL
};

static JSClassExoticMethods qjs_token_exotic = {
	qjs_token_get_own,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL
};

static JSClassDef qjs_event_class = {
	"Event",
	qjs_event_finalizer,
	NULL,
	NULL,
	NULL
};

static JSClassDef qjs_list_class = {
	"NodeList",
	qjs_list_finalizer,
	NULL,
	NULL,
	&qjs_list_exotic
};

static JSClassDef qjs_map_class = {
	"NamedNodeMap",
	qjs_map_finalizer,
	NULL,
	NULL,
	&qjs_map_exotic
};

static JSClassDef qjs_token_class = {
	"DOMTokenList",
	qjs_token_finalizer,
	NULL,
	NULL,
	&qjs_token_exotic
};

static JSValue
qjs_wrap(JSContext *ctx, JSClassID class_id, void *opaque, int do_ref,
		void (*refn)(void *))
{
	JSValue obj;

	if (opaque == NULL) {
		return JS_NULL;
	}
	obj = JS_NewObjectClass(ctx, (int)class_id);
	if (JS_IsException(obj)) {
		return obj;
	}
	if (do_ref && refn != NULL) {
		refn(opaque);
	}
	JS_SetOpaque(obj, opaque);
	return obj;
}

static void
qjs_event_ref_v(void *p)
{
	dom_event_ref((dom_event *)p);
}

static void
qjs_list_ref_v(void *p)
{
	dom_nodelist_ref((dom_nodelist *)p);
}

static void
qjs_map_ref_v(void *p)
{
	dom_namednodemap_ref((dom_namednodemap *)p);
}

static void
qjs_token_ref_v(void *p)
{
	dom_tokenlist_ref((dom_tokenlist *)p);
}

JSValue
qjs_push_event(JSContext *ctx, struct dom_event *evt)
{
	jsthread *thread;

	thread = qjs_thread_from_ctx(ctx);
	if (thread == NULL || thread->heap == NULL ||
			!thread->heap->event_class_ready) {
		return JS_NULL;
	}
	return qjs_wrap(ctx, thread->heap->event_class, evt, 1, qjs_event_ref_v);
}

JSValue
qjs_push_nodelist(JSContext *ctx, struct dom_nodelist *list)
{
	jsthread *thread;

	thread = qjs_thread_from_ctx(ctx);
	if (thread == NULL || thread->heap == NULL ||
			!thread->heap->list_class_ready) {
		return JS_NULL;
	}
	return qjs_wrap(ctx, thread->heap->list_class, list, 1, qjs_list_ref_v);
}

JSValue
qjs_push_namemap(JSContext *ctx, struct dom_namednodemap *map)
{
	jsthread *thread;

	thread = qjs_thread_from_ctx(ctx);
	if (thread == NULL || thread->heap == NULL ||
			!thread->heap->map_class_ready) {
		return JS_NULL;
	}
	return qjs_wrap(ctx, thread->heap->map_class, map, 1, qjs_map_ref_v);
}

JSValue
qjs_push_tokenlist(JSContext *ctx, struct dom_tokenlist *list)
{
	jsthread *thread;

	thread = qjs_thread_from_ctx(ctx);
	if (thread == NULL || thread->heap == NULL ||
			!thread->heap->token_class_ready) {
		return JS_NULL;
	}
	return qjs_wrap(ctx, thread->heap->token_class, list, 1,
			qjs_token_ref_v);
}

static dom_event *
qjs_event_of(JSContext *ctx, JSValueConst val)
{
	jsthread *thread;

	thread = qjs_thread_from_ctx(ctx);
	if (thread == NULL || thread->heap == NULL ||
			!thread->heap->event_class_ready) {
		return NULL;
	}
	return (dom_event *)JS_GetOpaque(val, thread->heap->event_class);
}

static JSValue
js_event_type(JSContext *ctx, JSValueConst this_val)
{
	dom_event *evt;
	dom_string *type;
	JSValue out;

	evt = qjs_event_of(ctx, this_val);
	if (evt == NULL) {
		return JS_NewStringLen(ctx, "", 0);
	}
	type = NULL;
	if (dom_event_get_type(evt, &type) != DOM_NO_ERR || type == NULL) {
		return JS_NewStringLen(ctx, "", 0);
	}
	out = JS_NewStringLen(ctx, (const char *)dom_string_data(type),
			dom_string_length(type));
	dom_string_unref(type);
	return out;
}

static JSValue
js_event_target(JSContext *ctx, JSValueConst this_val)
{
	dom_event *evt;
	dom_event_target *targ;
	JSValue out;

	evt = qjs_event_of(ctx, this_val);
	if (evt == NULL) {
		return JS_NULL;
	}
	targ = NULL;
	if (dom_event_get_target(evt, &targ) != DOM_NO_ERR) {
		return JS_NULL;
	}
	out = qjs_push_node(ctx, (dom_node *)targ);
	if (targ != NULL) {
		dom_node_unref(targ);
	}
	return out;
}

static JSValue
js_event_current(JSContext *ctx, JSValueConst this_val)
{
	dom_event *evt;
	dom_event_target *targ;
	JSValue out;

	evt = qjs_event_of(ctx, this_val);
	if (evt == NULL) {
		return JS_NULL;
	}
	targ = NULL;
	if (dom_event_get_current_target(evt, &targ) != DOM_NO_ERR) {
		return JS_NULL;
	}
	out = qjs_push_node(ctx, (dom_node *)targ);
	if (targ != NULL) {
		dom_node_unref(targ);
	}
	return out;
}

static JSValue
js_event_phase(JSContext *ctx, JSValueConst this_val)
{
	dom_event *evt;
	dom_event_flow_phase phase;

	evt = qjs_event_of(ctx, this_val);
	if (evt == NULL) {
		return JS_NewInt32(ctx, 0);
	}
	if (dom_event_get_event_phase(evt, &phase) != DOM_NO_ERR) {
		return JS_NewInt32(ctx, 0);
	}
	return JS_NewInt32(ctx, (int32_t)phase);
}

static JSValue
js_event_bool(JSContext *ctx, JSValueConst this_val, int magic)
{
	dom_event *evt;
	bool bit;
	dom_exception exc;

	evt = qjs_event_of(ctx, this_val);
	if (evt == NULL) {
		return JS_NewBool(ctx, 0);
	}
	bit = false;
	exc = DOM_NO_ERR;
	if (magic == 0) {
		exc = dom_event_get_bubbles(evt, &bit);
	} else if (magic == 1) {
		exc = dom_event_get_cancelable(evt, &bit);
	} else if (magic == 2) {
		exc = dom_event_is_default_prevented(evt, &bit);
	} else {
		exc = dom_event_get_is_trusted(evt, &bit);
	}
	if (exc != DOM_NO_ERR) {
		return JS_NewBool(ctx, 0);
	}
	return JS_NewBool(ctx, bit);
}

static JSValue
js_event_stop(JSContext *ctx, JSValueConst this_val,
		int argc, JSValueConst *argv)
{
	dom_event *evt;

	(void)argc;
	(void)argv;
	evt = qjs_event_of(ctx, this_val);
	if (evt != NULL) {
		dom_event_stop_propagation(evt);
	}
	return JS_UNDEFINED;
}

static JSValue
js_event_stop_imm(JSContext *ctx, JSValueConst this_val,
		int argc, JSValueConst *argv)
{
	dom_event *evt;

	(void)argc;
	(void)argv;
	evt = qjs_event_of(ctx, this_val);
	if (evt != NULL) {
		dom_event_stop_immediate_propagation(evt);
	}
	return JS_UNDEFINED;
}

static JSValue
js_event_prevent(JSContext *ctx, JSValueConst this_val,
		int argc, JSValueConst *argv)
{
	dom_event *evt;

	(void)argc;
	(void)argv;
	evt = qjs_event_of(ctx, this_val);
	if (evt != NULL) {
		dom_event_prevent_default(evt);
	}
	return JS_UNDEFINED;
}

static JSValue
js_event_init(JSContext *ctx, JSValueConst this_val,
		int argc, JSValueConst *argv)
{
	dom_event *evt;
	const char *text;
	size_t len;
	dom_string *type;
	int bubbles;
	int cancel;

	evt = qjs_event_of(ctx, this_val);
	if (evt == NULL || argc < 1) {
		return JS_UNDEFINED;
	}
	text = JS_ToCStringLen(ctx, &len, argv[0]);
	if (text == NULL) {
		return JS_EXCEPTION;
	}
	type = NULL;
	if (dom_string_create((const uint8_t *)text, len, &type) != DOM_NO_ERR) {
		JS_FreeCString(ctx, text);
		return JS_UNDEFINED;
	}
	JS_FreeCString(ctx, text);
	bubbles = 0;
	cancel = 0;
	if (argc > 1) {
		bubbles = JS_ToBool(ctx, argv[1]);
	}
	if (argc > 2) {
		cancel = JS_ToBool(ctx, argv[2]);
	}
	dom_event_init(evt, type, bubbles > 0, cancel > 0);
	dom_string_unref(type);
	return JS_UNDEFINED;
}

static JSValue
js_node_dispatchEvent(JSContext *ctx, JSValueConst this_val,
		int argc, JSValueConst *argv)
{
	dom_node *node;
	dom_event *evt;
	bool in_dispatch;
	bool ready;
	bool success;

	if (argc < 1) {
		return JS_FALSE;
	}
	node = qjs_node_from_this(ctx, this_val);
	evt = qjs_event_of(ctx, argv[0]);
	if (node == NULL || evt == NULL) {
		return JS_FALSE;
	}
	in_dispatch = false;
	ready = false;
	if (dom_event_in_dispatch(evt, &in_dispatch) != DOM_NO_ERR ||
			in_dispatch) {
		return JS_FALSE;
	}
	if (dom_event_is_initialised(evt, &ready) != DOM_NO_ERR || !ready) {
		return JS_FALSE;
	}
	dom_event_set_is_trusted(evt, false);
	success = false;
	if (dom_event_target_dispatch_event(node, evt, &success) != DOM_NO_ERR) {
		return JS_FALSE;
	}
	return JS_NewBool(ctx, success);
}

static JSValue
js_list_length(JSContext *ctx, JSValueConst this_val)
{
	jsthread *thread;
	dom_nodelist *list;
	uint32_t len;

	thread = qjs_thread_from_ctx(ctx);
	if (thread == NULL || !thread->heap->list_class_ready) {
		return JS_NewInt32(ctx, 0);
	}
	list = (dom_nodelist *)JS_GetOpaque(this_val, thread->heap->list_class);
	len = 0;
	if (list != NULL) {
		dom_nodelist_get_length(list, &len);
	}
	return JS_NewUint32(ctx, len);
}

static JSValue
js_list_item(JSContext *ctx, JSValueConst this_val,
		int argc, JSValueConst *argv)
{
	jsthread *thread;
	dom_nodelist *list;
	dom_node *node;
	uint32_t idx;
	JSValue out;

	if (argc < 1) {
		return JS_NULL;
	}
	thread = qjs_thread_from_ctx(ctx);
	if (thread == NULL || !thread->heap->list_class_ready) {
		return JS_NULL;
	}
	list = (dom_nodelist *)JS_GetOpaque(this_val, thread->heap->list_class);
	if (list == NULL) {
		return JS_NULL;
	}
	idx = 0;
	if (JS_ToUint32(ctx, &idx, argv[0]) < 0) {
		return JS_EXCEPTION;
	}
	node = NULL;
	if (dom_nodelist_item(list, idx, &node) != DOM_NO_ERR) {
		return JS_NULL;
	}
	out = qjs_push_node(ctx, node);
	if (node != NULL) {
		dom_node_unref(node);
	}
	return out;
}

static JSValue
js_map_length(JSContext *ctx, JSValueConst this_val)
{
	jsthread *thread;
	dom_namednodemap *map;
	uint32_t len;

	thread = qjs_thread_from_ctx(ctx);
	if (thread == NULL || !thread->heap->map_class_ready) {
		return JS_NewInt32(ctx, 0);
	}
	map = (dom_namednodemap *)JS_GetOpaque(this_val, thread->heap->map_class);
	len = 0;
	if (map != NULL) {
		dom_namednodemap_get_length(map, &len);
	}
	return JS_NewUint32(ctx, len);
}

static JSValue
js_map_item(JSContext *ctx, JSValueConst this_val,
		int argc, JSValueConst *argv)
{
	jsthread *thread;
	dom_namednodemap *map;
	dom_node *node;
	uint32_t idx;
	JSValue out;

	if (argc < 1) {
		return JS_NULL;
	}
	thread = qjs_thread_from_ctx(ctx);
	if (thread == NULL || !thread->heap->map_class_ready) {
		return JS_NULL;
	}
	map = (dom_namednodemap *)JS_GetOpaque(this_val, thread->heap->map_class);
	if (map == NULL) {
		return JS_NULL;
	}
	idx = 0;
	if (JS_ToUint32(ctx, &idx, argv[0]) < 0) {
		return JS_EXCEPTION;
	}
	node = NULL;
	if (dom_namednodemap_item(map, idx, &node) != DOM_NO_ERR) {
		return JS_NULL;
	}
	out = qjs_push_node(ctx, node);
	if (node != NULL) {
		dom_node_unref(node);
	}
	return out;
}

static JSValue
js_map_named(JSContext *ctx, JSValueConst this_val,
		int argc, JSValueConst *argv)
{
	jsthread *thread;
	dom_namednodemap *map;
	const char *s;
	size_t n;
	dom_string *name;
	dom_node *attr;
	JSValue out;

	if (argc < 1) {
		return JS_NULL;
	}
	thread = qjs_thread_from_ctx(ctx);
	if (thread == NULL || !thread->heap->map_class_ready) {
		return JS_NULL;
	}
	map = (dom_namednodemap *)JS_GetOpaque(this_val, thread->heap->map_class);
	if (map == NULL) {
		return JS_NULL;
	}
	s = JS_ToCStringLen(ctx, &n, argv[0]);
	if (s == NULL) {
		return JS_EXCEPTION;
	}
	name = NULL;
	if (dom_string_create((const uint8_t *)s, n, &name) != DOM_NO_ERR) {
		JS_FreeCString(ctx, s);
		return JS_NULL;
	}
	JS_FreeCString(ctx, s);
	attr = NULL;
	if (dom_namednodemap_get_named_item(map, name, &attr) != DOM_NO_ERR) {
		dom_string_unref(name);
		return JS_NULL;
	}
	dom_string_unref(name);
	out = qjs_push_node(ctx, attr);
	if (attr != NULL) {
		dom_node_unref(attr);
	}
	return out;
}

static JSValue
js_token_length(JSContext *ctx, JSValueConst this_val)
{
	jsthread *thread;
	dom_tokenlist *tokens;
	uint32_t len;

	thread = qjs_thread_from_ctx(ctx);
	if (thread == NULL || !thread->heap->token_class_ready) {
		return JS_NewInt32(ctx, 0);
	}
	tokens = (dom_tokenlist *)JS_GetOpaque(this_val,
			thread->heap->token_class);
	len = 0;
	if (tokens != NULL) {
		dom_tokenlist_get_length(tokens, &len);
	}
	return JS_NewUint32(ctx, len);
}

static JSValue
js_token_item(JSContext *ctx, JSValueConst this_val,
		int argc, JSValueConst *argv)
{
	jsthread *thread;
	dom_tokenlist *tokens;
	dom_string *value;
	uint32_t idx;
	JSValue out;

	if (argc < 1) {
		return JS_NULL;
	}
	thread = qjs_thread_from_ctx(ctx);
	if (thread == NULL || !thread->heap->token_class_ready) {
		return JS_NULL;
	}
	tokens = (dom_tokenlist *)JS_GetOpaque(this_val,
			thread->heap->token_class);
	if (tokens == NULL) {
		return JS_NULL;
	}
	idx = 0;
	if (JS_ToUint32(ctx, &idx, argv[0]) < 0) {
		return JS_EXCEPTION;
	}
	value = NULL;
	if (dom_tokenlist_item(tokens, idx, &value) != DOM_NO_ERR ||
			value == NULL) {
		return JS_NULL;
	}
	out = JS_NewStringLen(ctx, (const char *)dom_string_data(value),
			dom_string_length(value));
	dom_string_unref(value);
	return out;
}

static dom_tokenlist *
qjs_tokens_of(JSContext *ctx, JSValueConst this_val)
{
	jsthread *thread;

	thread = qjs_thread_from_ctx(ctx);
	if (thread == NULL || thread->heap == NULL ||
			!thread->heap->token_class_ready) {
		return NULL;
	}
	return (dom_tokenlist *)JS_GetOpaque(this_val, thread->heap->token_class);
}

static JSValue
js_token_add(JSContext *ctx, JSValueConst this_val,
		int argc, JSValueConst *argv)
{
	dom_tokenlist *tokens;
	int i;
	const char *s;
	size_t n;
	dom_string *value;

	tokens = qjs_tokens_of(ctx, this_val);
	if (tokens == NULL) {
		return JS_UNDEFINED;
	}
	for (i = 0; i < argc; i++) {
		s = JS_ToCStringLen(ctx, &n, argv[i]);
		if (s == NULL) {
			return JS_EXCEPTION;
		}
		value = NULL;
		if (dom_string_create_interned((const uint8_t *)s, n,
				&value) == DOM_NO_ERR) {
			dom_tokenlist_add(tokens, value);
			dom_string_unref(value);
		}
		JS_FreeCString(ctx, s);
	}
	return JS_UNDEFINED;
}

static JSValue
js_token_remove(JSContext *ctx, JSValueConst this_val,
		int argc, JSValueConst *argv)
{
	dom_tokenlist *tokens;
	int i;
	const char *s;
	size_t n;
	dom_string *value;

	tokens = qjs_tokens_of(ctx, this_val);
	if (tokens == NULL) {
		return JS_UNDEFINED;
	}
	for (i = 0; i < argc; i++) {
		s = JS_ToCStringLen(ctx, &n, argv[i]);
		if (s == NULL) {
			return JS_EXCEPTION;
		}
		value = NULL;
		if (dom_string_create_interned((const uint8_t *)s, n,
				&value) == DOM_NO_ERR) {
			dom_tokenlist_remove(tokens, value);
			dom_string_unref(value);
		}
		JS_FreeCString(ctx, s);
	}
	return JS_UNDEFINED;
}

static JSValue
js_token_contains(JSContext *ctx, JSValueConst this_val,
		int argc, JSValueConst *argv)
{
	dom_tokenlist *tokens;
	const char *s;
	size_t n;
	dom_string *value;
	bool present;

	if (argc < 1) {
		return JS_FALSE;
	}
	tokens = qjs_tokens_of(ctx, this_val);
	if (tokens == NULL) {
		return JS_FALSE;
	}
	s = JS_ToCStringLen(ctx, &n, argv[0]);
	if (s == NULL) {
		return JS_EXCEPTION;
	}
	value = NULL;
	present = false;
	if (dom_string_create_interned((const uint8_t *)s, n, &value) ==
			DOM_NO_ERR) {
		dom_tokenlist_contains(tokens, value, &present);
		dom_string_unref(value);
	}
	JS_FreeCString(ctx, s);
	return JS_NewBool(ctx, present);
}

/* toggle(token) flips. toggle(token, force) adds when force is true. */
static JSValue
js_token_toggle(JSContext *ctx, JSValueConst this_val,
		int argc, JSValueConst *argv)
{
	dom_tokenlist *tokens;
	const char *s;
	size_t n;
	dom_string *value;
	bool present;
	int force;
	int has_force;

	if (argc < 1) {
		return JS_FALSE;
	}
	tokens = qjs_tokens_of(ctx, this_val);
	if (tokens == NULL) {
		return JS_FALSE;
	}
	s = JS_ToCStringLen(ctx, &n, argv[0]);
	if (s == NULL) {
		return JS_EXCEPTION;
	}
	value = NULL;
	if (dom_string_create_interned((const uint8_t *)s, n, &value) !=
			DOM_NO_ERR) {
		JS_FreeCString(ctx, s);
		return JS_FALSE;
	}
	JS_FreeCString(ctx, s);
	present = false;
	if (dom_tokenlist_contains(tokens, value, &present) != DOM_NO_ERR) {
		dom_string_unref(value);
		return JS_FALSE;
	}
	has_force = (argc > 1);
	force = 0;
	if (has_force) {
		force = JS_ToBool(ctx, argv[1]);
		if (force < 0) {
			dom_string_unref(value);
			return JS_EXCEPTION;
		}
	}
	if (has_force) {
		if (force) {
			dom_tokenlist_add(tokens, value);
			present = true;
		} else {
			dom_tokenlist_remove(tokens, value);
			present = false;
		}
	} else if (present) {
		dom_tokenlist_remove(tokens, value);
		present = false;
	} else {
		dom_tokenlist_add(tokens, value);
		present = true;
	}
	dom_string_unref(value);
	return JS_NewBool(ctx, present);
}

static JSValue
js_token_value_get(JSContext *ctx, JSValueConst this_val)
{
	dom_tokenlist *tokens;
	dom_string *value;
	JSValue out;

	tokens = qjs_tokens_of(ctx, this_val);
	if (tokens == NULL) {
		return JS_NewStringLen(ctx, "", 0);
	}
	value = NULL;
	if (dom_tokenlist_get_value(tokens, &value) != DOM_NO_ERR ||
			value == NULL) {
		return JS_NewStringLen(ctx, "", 0);
	}
	out = JS_NewStringLen(ctx, (const char *)dom_string_data(value),
			dom_string_length(value));
	dom_string_unref(value);
	return out;
}

static JSValue
js_token_value_set(JSContext *ctx, JSValueConst this_val, JSValueConst val)
{
	dom_tokenlist *tokens;
	const char *s;
	size_t n;
	dom_string *value;

	tokens = qjs_tokens_of(ctx, this_val);
	if (tokens == NULL) {
		return JS_UNDEFINED;
	}
	s = JS_ToCStringLen(ctx, &n, val);
	if (s == NULL) {
		return JS_EXCEPTION;
	}
	value = NULL;
	if (dom_string_create_interned((const uint8_t *)s, n, &value) ==
			DOM_NO_ERR) {
		dom_tokenlist_set_value(tokens, value);
		dom_string_unref(value);
	}
	JS_FreeCString(ctx, s);
	return JS_UNDEFINED;
}

static const JSCFunctionListEntry qjs_event_funcs[] = {
	JS_CGETSET_DEF("type", js_event_type, NULL),
	JS_CGETSET_DEF("target", js_event_target, NULL),
	JS_CGETSET_DEF("currentTarget", js_event_current, NULL),
	JS_CGETSET_DEF("eventPhase", js_event_phase, NULL),
	JS_CGETSET_MAGIC_DEF("bubbles", js_event_bool, NULL, 0),
	JS_CGETSET_MAGIC_DEF("cancelable", js_event_bool, NULL, 1),
	JS_CGETSET_MAGIC_DEF("defaultPrevented", js_event_bool, NULL, 2),
	JS_CGETSET_MAGIC_DEF("isTrusted", js_event_bool, NULL, 3),
	JS_CFUNC_DEF("stopPropagation", 0, js_event_stop),
	JS_CFUNC_DEF("stopImmediatePropagation", 0, js_event_stop_imm),
	JS_CFUNC_DEF("preventDefault", 0, js_event_prevent),
	JS_CFUNC_DEF("initEvent", 3, js_event_init),
};

static const JSCFunctionListEntry qjs_list_funcs[] = {
	JS_CGETSET_DEF("length", js_list_length, NULL),
	JS_CFUNC_DEF("item", 1, js_list_item),
};

static const JSCFunctionListEntry qjs_map_funcs[] = {
	JS_CGETSET_DEF("length", js_map_length, NULL),
	JS_CFUNC_DEF("item", 1, js_map_item),
	JS_CFUNC_DEF("getNamedItem", 1, js_map_named),
};

static const JSCFunctionListEntry qjs_token_funcs[] = {
	JS_CGETSET_DEF("length", js_token_length, NULL),
	JS_CGETSET_DEF("value", js_token_value_get, js_token_value_set),
	JS_CFUNC_DEF("item", 1, js_token_item),
	JS_CFUNC_DEF("add", 0, js_token_add),
	JS_CFUNC_DEF("remove", 0, js_token_remove),
	JS_CFUNC_DEF("contains", 1, js_token_contains),
	JS_CFUNC_DEF("toggle", 1, js_token_toggle),
};

static const JSCFunctionListEntry qjs_target_funcs[] = {
	JS_CFUNC_DEF("dispatchEvent", 1, js_node_dispatchEvent),
};

static int
qjs_one_class(jsheap *heap, JSClassID *id, int *ready, const JSClassDef *def)
{
	if (*ready) {
		return 0;
	}
	qjs_claim_class_id(heap->rt, id);
	if (qjs_new_class(heap->rt, *id, def) != 0) {
		NSLOG(netsurf, WARNING, "JS_NewClass %s failed id=%lu",
				def->class_name, (unsigned long)*id);
		return -1;
	}
	NSLOG(netsurf, INFO, "JS_NewClass %s id=%lu",
			def->class_name, (unsigned long)*id);
	*ready = 1;
	return 0;
}

nserror
qjs_bind_dom_prepare(jsheap *heap)
{
	if (heap == NULL || heap->rt == NULL) {
		return NSERROR_BAD_PARAMETER;
	}
	if (qjs_one_class(heap, &heap->event_class, &heap->event_class_ready,
			&qjs_event_class) != 0) {
		return NSERROR_NOMEM;
	}
	if (qjs_one_class(heap, &heap->list_class, &heap->list_class_ready,
			&qjs_list_class) != 0) {
		return NSERROR_NOMEM;
	}
	if (qjs_one_class(heap, &heap->map_class, &heap->map_class_ready,
			&qjs_map_class) != 0) {
		return NSERROR_NOMEM;
	}
	if (qjs_one_class(heap, &heap->token_class, &heap->token_class_ready,
			&qjs_token_class) != 0) {
		return NSERROR_NOMEM;
	}
	return NSERROR_OK;
}

static void
qjs_set_proto(JSContext *ctx, JSClassID id,
		const JSCFunctionListEntry *tab, int n)
{
	JSValue proto;

	proto = JS_NewObject(ctx);
	JS_SetPropertyFunctionList(ctx, proto, tab, n);
	JS_SetClassProto(ctx, id, proto);
}

void
qjs_bind_list_install(JSContext *ctx, JSValue node_proto)
{
	jsthread *thread;
	JSValue evproto;

	thread = qjs_thread_from_ctx(ctx);
	if (thread == NULL || thread->heap == NULL) {
		return;
	}

	if (thread->heap->event_class_ready) {
		qjs_set_proto(ctx, thread->heap->event_class, qjs_event_funcs,
				(int)(sizeof(qjs_event_funcs) /
						sizeof(qjs_event_funcs[0])));
		evproto = JS_GetClassProto(ctx, thread->heap->event_class);
		JS_SetPropertyStr(ctx, evproto, "CAPTURING_PHASE",
				JS_NewInt32(ctx, DOM_CAPTURING_PHASE));
		JS_SetPropertyStr(ctx, evproto, "AT_TARGET",
				JS_NewInt32(ctx, DOM_AT_TARGET));
		JS_SetPropertyStr(ctx, evproto, "BUBBLING_PHASE",
				JS_NewInt32(ctx, DOM_BUBBLING_PHASE));
		JS_FreeValue(ctx, evproto);
	}
	if (thread->heap->list_class_ready) {
		qjs_set_proto(ctx, thread->heap->list_class, qjs_list_funcs,
				(int)(sizeof(qjs_list_funcs) /
						sizeof(qjs_list_funcs[0])));
	}
	if (thread->heap->map_class_ready) {
		qjs_set_proto(ctx, thread->heap->map_class, qjs_map_funcs,
				(int)(sizeof(qjs_map_funcs) /
						sizeof(qjs_map_funcs[0])));
	}
	if (thread->heap->token_class_ready) {
		qjs_set_proto(ctx, thread->heap->token_class, qjs_token_funcs,
				(int)(sizeof(qjs_token_funcs) /
						sizeof(qjs_token_funcs[0])));
	}

	JS_SetPropertyFunctionList(ctx, node_proto, qjs_target_funcs,
			(int)(sizeof(qjs_target_funcs) /
					sizeof(qjs_target_funcs[0])));
}
