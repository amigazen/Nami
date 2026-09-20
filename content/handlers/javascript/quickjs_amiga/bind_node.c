/*
 * Copyright 2026 AmigaZen / Nami contributors
 *
 * Node, Element and Document methods. Ported from the duktape .bnd
 * files that have real C bodies.
 *
 * Structural edits (appendChild, innerHTML) update libdom. The box
 * tree is only refreshed when the node already has a text box
 * (qjs_dom_sync_text). Inserting new elements does not rebuild boxes.
 */

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
#include <dom/core/attr.h>
#include <dom/html/html_document.h>
#include <dom/html/html_element.h>
#include <dom/html/html_input_element.h>
#include <dom/html/html_text_area_element.h>
#include <dom/html/html_select_element.h>
#include <dom/html/html_anchor_element.h>
#include <dom/html/html_image_element.h>
#include <dom/html/html_form_element.h>
#include <dom/core/implementation.h>
#include <dom/core/document_type.h>
#include <dom/bindings/hubbub/parser.h>

#include "utils/log.h"
#include "utils/corestrings.h"
#include "content/llcache.h"
#include "content/urldb.h"
#include "html/private.h"

#include "private.h"
#include "bind_priv.h"
#include "dom_sync.h"

static void
qjs_sync(JSContext *ctx, dom_node *node)
{
	jsthread *thread;

	thread = qjs_thread_from_ctx(ctx);
	if (thread == NULL) {
		return;
	}
	qjs_dom_sync_text((html_content *)thread->doc_priv, node);
}

static dom_string *
qjs_arg_str(JSContext *ctx, JSValueConst v)
{
	const char *s;
	size_t n;
	dom_string *out;

	s = JS_ToCStringLen(ctx, &n, v);
	if (s == NULL) {
		JSValue ex;

		/* Caller returns null; do not leave a pending exception. */
		ex = JS_GetException(ctx);
		JS_FreeValue(ctx, ex);
		return NULL;
	}
	out = NULL;
	if (dom_string_create((const uint8_t *)s, n, &out) != DOM_NO_ERR) {
		out = NULL;
	}
	JS_FreeCString(ctx, s);
	return out;
}

static JSValue
qjs_from_dom(JSContext *ctx, dom_string *s)
{
	JSValue v;

	if (s == NULL) {
		return JS_NULL;
	}
	v = JS_NewStringLen(ctx, (const char *)dom_string_data(s),
			dom_string_length(s));
	dom_string_unref(s);
	return v;
}

static int
qjs_name_is(dom_node *node, dom_string *expect)
{
	dom_string *name;
	int ok;

	name = NULL;
	if (dom_node_get_node_name(node, &name) != DOM_NO_ERR || name == NULL) {
		return 0;
	}
	ok = dom_string_isequal(name, expect) ? 1 : 0;
	dom_string_unref(name);
	return ok;
}

static int
qjs_name_ascii(dom_node *node, const char *tag)
{
	dom_string *name;
	const uint8_t *data;
	size_t len;
	size_t i;
	size_t tlen;
	int ok;

	name = NULL;
	if (dom_node_get_node_name(node, &name) != DOM_NO_ERR || name == NULL) {
		return 0;
	}
	data = dom_string_data(name);
	len = dom_string_length(name);
	tlen = strlen(tag);
	ok = 1;
	if (len != tlen) {
		ok = 0;
	}
	for (i = 0; ok && i < len; i++) {
		char c;
		char d;

		c = (char)data[i];
		d = tag[i];
		if (c >= 'a' && c <= 'z') {
			c = (char)(c - ('a' - 'A'));
		}
		if (d >= 'a' && d <= 'z') {
			d = (char)(d - ('a' - 'A'));
		}
		if (c != d) {
			ok = 0;
		}
	}
	dom_string_unref(name);
	return ok;
}

static int
qjs_node_type(dom_node *node, dom_node_type *t)
{
	*t = DOM_NODE_TYPE_COUNT;
	if (node == NULL) {
		return 0;
	}
	return dom_node_get_node_type(node, t) == DOM_NO_ERR;
}

static JSValue
js_node_type(JSContext *ctx, JSValueConst this_val)
{
	dom_node *node;
	dom_node_type t;

	node = qjs_node_from_this(ctx, this_val);
	if (!qjs_node_type(node, &t)) {
		return JS_NewInt32(ctx, 0);
	}
	return JS_NewInt32(ctx, (int32_t)t);
}

static JSValue
js_node_name(JSContext *ctx, JSValueConst this_val)
{
	dom_node *node;
	dom_string *name;

	node = qjs_node_from_this(ctx, this_val);
	if (node == NULL) {
		return JS_NULL;
	}
	name = NULL;
	if (dom_node_get_node_name(node, &name) != DOM_NO_ERR) {
		return JS_NULL;
	}
	return qjs_from_dom(ctx, name);
}

static JSValue
js_node_base(JSContext *ctx, JSValueConst this_val)
{
	dom_node *node;
	dom_string *base;

	node = qjs_node_from_this(ctx, this_val);
	if (node == NULL) {
		return JS_NULL;
	}
	base = NULL;
	if (dom_node_get_base(node, &base) != DOM_NO_ERR) {
		return JS_NULL;
	}
	return qjs_from_dom(ctx, base);
}

static JSValue
js_node_owner(JSContext *ctx, JSValueConst this_val)
{
	dom_node *node;
	dom_node *doc;
	JSValue out;

	node = qjs_node_from_this(ctx, this_val);
	if (node == NULL) {
		return JS_NULL;
	}
	doc = NULL;
	if (dom_node_get_owner_document(node, &doc) != DOM_NO_ERR) {
		return JS_NULL;
	}
	out = qjs_push_node(ctx, doc);
	if (doc != NULL) {
		dom_node_unref(doc);
	}
	return out;
}

static JSValue
js_rel(JSContext *ctx, JSValueConst this_val, int which)
{
	dom_node *node;
	dom_node *rel;
	dom_exception exc;
	JSValue out;

	node = qjs_node_from_this(ctx, this_val);
	if (node == NULL) {
		return JS_NULL;
	}
	rel = NULL;
	exc = DOM_NO_ERR;
	if (which == 0) {
		exc = dom_node_get_first_child(node, &rel);
	} else if (which == 1) {
		exc = dom_node_get_last_child(node, &rel);
	} else if (which == 2) {
		exc = dom_node_get_previous_sibling(node, &rel);
	} else if (which == 3) {
		exc = dom_node_get_next_sibling(node, &rel);
	} else {
		exc = dom_node_get_parent_node(node, &rel);
	}
	if (exc != DOM_NO_ERR) {
		return JS_NULL;
	}
	out = qjs_push_node(ctx, rel);
	if (rel != NULL) {
		dom_node_unref(rel);
	}
	return out;
}

static JSValue
js_first(JSContext *ctx, JSValueConst this_val)
{
	return js_rel(ctx, this_val, 0);
}

static JSValue
js_last(JSContext *ctx, JSValueConst this_val)
{
	return js_rel(ctx, this_val, 1);
}

static JSValue
js_prev(JSContext *ctx, JSValueConst this_val)
{
	return js_rel(ctx, this_val, 2);
}

static JSValue
js_next(JSContext *ctx, JSValueConst this_val)
{
	return js_rel(ctx, this_val, 3);
}

static JSValue
js_parent(JSContext *ctx, JSValueConst this_val)
{
	return js_rel(ctx, this_val, 4);
}

static JSValue
js_parent_el(JSContext *ctx, JSValueConst this_val)
{
	dom_node *node;
	dom_node *rel;
	dom_node_type t;
	JSValue out;

	node = qjs_node_from_this(ctx, this_val);
	if (node == NULL) {
		return JS_NULL;
	}
	rel = NULL;
	if (dom_node_get_parent_node(node, &rel) != DOM_NO_ERR || rel == NULL) {
		return JS_NULL;
	}
	t = DOM_NODE_TYPE_COUNT;
	if (dom_node_get_node_type(rel, &t) != DOM_NO_ERR ||
			t != DOM_ELEMENT_NODE) {
		dom_node_unref(rel);
		return JS_NULL;
	}
	out = qjs_push_node(ctx, rel);
	dom_node_unref(rel);
	return out;
}

static JSValue
js_has_child(JSContext *ctx, JSValueConst this_val,
		int argc, JSValueConst *argv)
{
	dom_node *node;
	bool res;

	(void)argc;
	(void)argv;
	node = qjs_node_from_this(ctx, this_val);
	res = false;
	if (node == NULL ||
			dom_node_has_child_nodes(node, &res) != DOM_NO_ERR) {
		return JS_FALSE;
	}
	return JS_NewBool(ctx, res);
}

static JSValue
js_child_nodes(JSContext *ctx, JSValueConst this_val)
{
	dom_node *node;
	dom_nodelist *list;
	JSValue out;

	node = qjs_node_from_this(ctx, this_val);
	if (node == NULL) {
		return JS_NULL;
	}
	/* Do not cache on the JS object — a snapshot goes stale after
	 * appendChild/removeChild and breaks NodeList.item === child. */
	list = NULL;
	if (dom_node_get_child_nodes(node, &list) != DOM_NO_ERR || list == NULL) {
		return JS_NULL;
	}
	out = qjs_push_nodelist(ctx, list);
	dom_nodelist_unref(list);
	return out;
}

static JSValue
js_node_value_get(JSContext *ctx, JSValueConst this_val)
{
	dom_node *node;
	dom_string *content;

	node = qjs_node_from_this(ctx, this_val);
	if (node == NULL) {
		return JS_NULL;
	}
	content = NULL;
	if (dom_node_get_node_value(node, &content) != DOM_NO_ERR) {
		return JS_NULL;
	}
	return qjs_from_dom(ctx, content);
}

static JSValue
js_node_value_set(JSContext *ctx, JSValueConst this_val, JSValueConst val)
{
	dom_node *node;
	dom_string *content;

	node = qjs_node_from_this(ctx, this_val);
	if (node == NULL) {
		return JS_UNDEFINED;
	}
	content = qjs_arg_str(ctx, val);
	if (content == NULL) {
		return JS_EXCEPTION;
	}
	dom_node_set_node_value(node, content);
	dom_string_unref(content);
	qjs_sync(ctx, node);
	return JS_UNDEFINED;
}

static JSValue
js_normalize(JSContext *ctx, JSValueConst this_val,
		int argc, JSValueConst *argv)
{
	dom_node *node;

	(void)argc;
	(void)argv;
	node = qjs_node_from_this(ctx, this_val);
	if (node != NULL) {
		dom_node_normalize(node);
	}
	return JS_UNDEFINED;
}

static JSValue
js_clone(JSContext *ctx, JSValueConst this_val,
		int argc, JSValueConst *argv)
{
	dom_node *node;
	dom_node *clone;
	int deep;
	JSValue out;

	node = qjs_node_from_this(ctx, this_val);
	if (node == NULL) {
		return JS_NULL;
	}
	deep = 0;
	if (argc > 0) {
		deep = JS_ToBool(ctx, argv[0]);
		if (deep < 0) {
			return JS_EXCEPTION;
		}
	}
	clone = NULL;
	if (dom_node_clone_node(node, deep != 0, &clone) != DOM_NO_ERR) {
		return JS_NULL;
	}
	out = qjs_push_node(ctx, clone);
	if (clone != NULL) {
		dom_node_unref(clone);
	}
	return out;
}

static JSValue
js_equal(JSContext *ctx, JSValueConst this_val,
		int argc, JSValueConst *argv)
{
	dom_node *node;
	dom_node *other;
	bool result;

	if (argc < 1) {
		return JS_FALSE;
	}
	node = qjs_node_from_this(ctx, this_val);
	other = qjs_node_from_this(ctx, argv[0]);
	result = false;
	if (node == NULL || other == NULL ||
			dom_node_is_equal(node, other, &result) != DOM_NO_ERR) {
		return JS_FALSE;
	}
	return JS_NewBool(ctx, result);
}

static JSValue
js_compare(JSContext *ctx, JSValueConst this_val,
		int argc, JSValueConst *argv)
{
	dom_node *node;
	dom_node *other;
	uint16_t ret;

	if (argc < 1) {
		return JS_NewInt32(ctx, 0);
	}
	node = qjs_node_from_this(ctx, this_val);
	other = qjs_node_from_this(ctx, argv[0]);
	ret = 0;
	if (node == NULL || other == NULL ||
			dom_node_compare_document_position(node, other, &ret) !=
			DOM_NO_ERR) {
		return JS_NewInt32(ctx, 0);
	}
	return JS_NewInt32(ctx, (int32_t)ret);
}

static JSValue
js_contains(JSContext *ctx, JSValueConst this_val,
		int argc, JSValueConst *argv)
{
	dom_node *node;
	dom_node *other;
	dom_node *n;
	dom_node *parent;
	dom_exception exc;
	int owned;

	(void)ctx;
	if (argc < 1) {
		return JS_FALSE;
	}
	node = qjs_node_from_this(ctx, this_val);
	other = qjs_node_from_this(ctx, argv[0]);
	if (node == NULL || other == NULL) {
		return JS_FALSE;
	}
	/* libdom's compare_document_position is a stub (NOT_SUPPORTED).
	 * Walk parents for inclusive descendant. */
	if (node == other) {
		return JS_TRUE;
	}
	n = other;
	owned = 0;
	while (n != NULL) {
		parent = NULL;
		exc = dom_node_get_parent_node(n, &parent);
		if (owned) {
			dom_node_unref(n);
		}
		if (exc != DOM_NO_ERR || parent == NULL) {
			return JS_FALSE;
		}
		if (parent == node) {
			dom_node_unref(parent);
			return JS_TRUE;
		}
		n = parent;
		owned = 1;
	}
	return JS_FALSE;
}

static JSValue
js_lookup_prefix(JSContext *ctx, JSValueConst this_val,
		int argc, JSValueConst *argv)
{
	dom_node *node;
	dom_string *ns;
	dom_string *pfx;

	if (argc < 1) {
		return JS_NULL;
	}
	node = qjs_node_from_this(ctx, this_val);
	if (node == NULL) {
		return JS_NULL;
	}
	ns = qjs_arg_str(ctx, argv[0]);
	if (ns == NULL) {
		return JS_NULL;
	}
	pfx = NULL;
	if (dom_node_lookup_prefix(node, ns, &pfx) != DOM_NO_ERR) {
		dom_string_unref(ns);
		return JS_NULL;
	}
	dom_string_unref(ns);
	return qjs_from_dom(ctx, pfx);
}

static JSValue
js_lookup_ns(JSContext *ctx, JSValueConst this_val,
		int argc, JSValueConst *argv)
{
	dom_node *node;
	dom_string *pfx;
	dom_string *ns;

	if (argc < 1) {
		return JS_NULL;
	}
	node = qjs_node_from_this(ctx, this_val);
	if (node == NULL) {
		return JS_NULL;
	}
	pfx = qjs_arg_str(ctx, argv[0]);
	if (pfx == NULL) {
		return JS_NULL;
	}
	ns = NULL;
	if (dom_node_lookup_namespace(node, pfx, &ns) != DOM_NO_ERR) {
		dom_string_unref(pfx);
		return JS_NULL;
	}
	dom_string_unref(pfx);
	return qjs_from_dom(ctx, ns);
}

static JSValue
js_default_ns(JSContext *ctx, JSValueConst this_val,
		int argc, JSValueConst *argv)
{
	dom_node *node;
	dom_string *ns;
	bool ret;

	if (argc < 1) {
		return JS_FALSE;
	}
	node = qjs_node_from_this(ctx, this_val);
	if (node == NULL) {
		return JS_FALSE;
	}
	ns = qjs_arg_str(ctx, argv[0]);
	if (ns == NULL) {
		return JS_FALSE;
	}
	ret = false;
	if (dom_node_is_default_namespace(node, ns, &ret) != DOM_NO_ERR) {
		dom_string_unref(ns);
		return JS_FALSE;
	}
	dom_string_unref(ns);
	return JS_NewBool(ctx, ret);
}

static JSValue
js_insert(JSContext *ctx, JSValueConst this_val,
		int argc, JSValueConst *argv)
{
	dom_node *node;
	dom_node *child;
	dom_node *before;
	dom_node *spare;
	JSValue out;

	if (argc < 1) {
		return JS_NULL;
	}
	node = qjs_node_from_this(ctx, this_val);
	child = qjs_node_from_this(ctx, argv[0]);
	before = NULL;
	if (argc > 1 && !JS_IsNull(argv[1]) && !JS_IsUndefined(argv[1])) {
		before = qjs_node_from_this(ctx, argv[1]);
	}
	if (node == NULL || child == NULL) {
		return JS_NULL;
	}
	spare = NULL;
	if (dom_node_insert_before(node, child, before, &spare) != DOM_NO_ERR) {
		return JS_NULL;
	}
	out = qjs_push_node(ctx, spare);
	if (spare != NULL) {
		dom_node_unref(spare);
	}
	qjs_sync(ctx, node);
	return out;
}

static JSValue
js_append(JSContext *ctx, JSValueConst this_val,
		int argc, JSValueConst *argv)
{
	dom_node *node;
	dom_node *child;
	dom_node *spare;
	JSValue out;

	if (argc < 1) {
		return JS_NULL;
	}
	node = qjs_node_from_this(ctx, this_val);
	child = qjs_node_from_this(ctx, argv[0]);
	if (node == NULL || child == NULL) {
		return JS_NULL;
	}
	spare = NULL;
	if (dom_node_append_child(node, child, &spare) != DOM_NO_ERR) {
		return JS_NULL;
	}
	out = qjs_push_node(ctx, spare);
	if (spare != NULL) {
		dom_node_unref(spare);
	}
	qjs_sync(ctx, node);
	return out;
}

static JSValue
js_replace(JSContext *ctx, JSValueConst this_val,
		int argc, JSValueConst *argv)
{
	dom_node *node;
	dom_node *child;
	dom_node *old;
	dom_node *spare;
	JSValue out;

	if (argc < 2) {
		return JS_NULL;
	}
	node = qjs_node_from_this(ctx, this_val);
	child = qjs_node_from_this(ctx, argv[0]);
	old = qjs_node_from_this(ctx, argv[1]);
	if (node == NULL || child == NULL || old == NULL) {
		return JS_NULL;
	}
	spare = NULL;
	if (dom_node_replace_child(node, child, old, &spare) != DOM_NO_ERR) {
		return JS_NULL;
	}
	out = qjs_push_node(ctx, spare);
	if (spare != NULL) {
		dom_node_unref(spare);
	}
	qjs_sync(ctx, node);
	return out;
}

static JSValue
js_remove(JSContext *ctx, JSValueConst this_val,
		int argc, JSValueConst *argv)
{
	dom_node *node;
	dom_node *child;
	dom_node *spare;
	JSValue out;

	if (argc < 1) {
		return JS_NULL;
	}
	node = qjs_node_from_this(ctx, this_val);
	child = qjs_node_from_this(ctx, argv[0]);
	if (node == NULL || child == NULL) {
		return JS_NULL;
	}
	spare = NULL;
	if (dom_node_remove_child(node, child, &spare) != DOM_NO_ERR) {
		return JS_NULL;
	}
	out = qjs_push_node(ctx, spare);
	if (spare != NULL) {
		dom_node_unref(spare);
	}
	qjs_sync(ctx, node);
	return out;
}

static dom_node *
qjs_skip_element(dom_node *n, int forward)
{
	dom_node *step;
	dom_node_type t;

	while (n != NULL) {
		t = DOM_NODE_TYPE_COUNT;
		if (dom_node_get_node_type(n, &t) == DOM_NO_ERR &&
				t == DOM_ELEMENT_NODE) {
			return n;
		}
		step = NULL;
		if (forward) {
			dom_node_get_next_sibling(n, &step);
		} else {
			dom_node_get_previous_sibling(n, &step);
		}
		dom_node_unref(n);
		n = step;
	}
	return NULL;
}

static JSValue
js_elem_end(JSContext *ctx, JSValueConst this_val, int which)
{
	dom_node *node;
	dom_node *el;
	dom_exception exc;
	JSValue out;

	node = qjs_node_from_this(ctx, this_val);
	if (node == NULL) {
		return JS_NULL;
	}
	el = NULL;
	exc = DOM_NO_ERR;
	if (which == 0) {
		exc = dom_node_get_first_child(node, &el);
	} else if (which == 1) {
		exc = dom_node_get_last_child(node, &el);
	} else if (which == 2) {
		exc = dom_node_get_previous_sibling(node, &el);
	} else {
		exc = dom_node_get_next_sibling(node, &el);
	}
	if (exc != DOM_NO_ERR) {
		return JS_NULL;
	}
	el = qjs_skip_element(el, which == 0 || which == 3);
	out = qjs_push_node(ctx, el);
	if (el != NULL) {
		dom_node_unref(el);
	}
	return out;
}

static JSValue
js_first_el(JSContext *ctx, JSValueConst this_val)
{
	return js_elem_end(ctx, this_val, 0);
}

static JSValue
js_last_el(JSContext *ctx, JSValueConst this_val)
{
	return js_elem_end(ctx, this_val, 1);
}

static JSValue
js_prev_el(JSContext *ctx, JSValueConst this_val)
{
	return js_elem_end(ctx, this_val, 2);
}

static JSValue
js_next_el(JSContext *ctx, JSValueConst this_val)
{
	return js_elem_end(ctx, this_val, 3);
}

static JSValue
js_child_el_count(JSContext *ctx, JSValueConst this_val)
{
	dom_node *node;
	dom_node *el;
	dom_node *step;
	dom_node_type t;
	int count;

	node = qjs_node_from_this(ctx, this_val);
	if (node == NULL) {
		return JS_NewInt32(ctx, 0);
	}
	el = NULL;
	if (dom_node_get_first_child(node, &el) != DOM_NO_ERR) {
		return JS_NewInt32(ctx, 0);
	}
	count = 0;
	while (el != NULL) {
		t = DOM_NODE_TYPE_COUNT;
		if (dom_node_get_node_type(el, &t) == DOM_NO_ERR &&
				t == DOM_ELEMENT_NODE) {
			count++;
		}
		step = NULL;
		if (dom_node_get_next_sibling(el, &step) != DOM_NO_ERR) {
			dom_node_unref(el);
			break;
		}
		dom_node_unref(el);
		el = step;
	}
	return JS_NewInt32(ctx, count);
}

static JSValue
js_by_tag(JSContext *ctx, JSValueConst this_val,
		int argc, JSValueConst *argv)
{
	dom_node *node;
	dom_node_type t;
	dom_string *tag;
	dom_nodelist *list;
	JSValue out;

	if (argc < 1) {
		return JS_NULL;
	}
	node = qjs_node_from_this(ctx, this_val);
	if (node == NULL || !qjs_node_type(node, &t)) {
		return JS_NULL;
	}
	tag = qjs_arg_str(ctx, argv[0]);
	if (tag == NULL) {
		return JS_NULL;
	}
	list = NULL;
	if (t == DOM_DOCUMENT_NODE) {
		dom_document_get_elements_by_tag_name(node, tag, &list);
	} else {
		dom_element_get_elements_by_tag_name(node, tag, &list);
	}
	dom_string_unref(tag);
	out = qjs_push_nodelist(ctx, list);
	if (list != NULL) {
		dom_nodelist_unref(list);
	}
	return out;
}

static JSValue
js_attr_named(JSContext *ctx, dom_node *node, dom_string *name, int empty)
{
	dom_string *val;

	val = NULL;
	if (node == NULL ||
			dom_element_get_attribute(node, name, &val) != DOM_NO_ERR) {
		if (empty) {
			return JS_NewStringLen(ctx, "", 0);
		}
		return JS_NULL;
	}
	if (val == NULL) {
		if (empty) {
			return JS_NewStringLen(ctx, "", 0);
		}
		return JS_NULL;
	}
	return qjs_from_dom(ctx, val);
}

static JSValue
js_id_get(JSContext *ctx, JSValueConst this_val)
{
	return js_attr_named(ctx, qjs_node_from_this(ctx, this_val),
			corestring_dom_id, 1);
}

static JSValue
js_id_set(JSContext *ctx, JSValueConst this_val, JSValueConst val)
{
	dom_node *node;
	dom_string *s;

	node = qjs_node_from_this(ctx, this_val);
	s = qjs_arg_str(ctx, val);
	if (node != NULL && s != NULL) {
		dom_element_set_attribute(node, corestring_dom_id, s);
		dom_string_unref(s);
	}
	return JS_UNDEFINED;
}

static JSValue
js_class_get(JSContext *ctx, JSValueConst this_val)
{
	return js_attr_named(ctx, qjs_node_from_this(ctx, this_val),
			corestring_dom_class, 1);
}

static JSValue
js_class_set(JSContext *ctx, JSValueConst this_val, JSValueConst val)
{
	dom_node *node;
	dom_string *s;

	node = qjs_node_from_this(ctx, this_val);
	s = qjs_arg_str(ctx, val);
	if (node != NULL && s != NULL) {
		dom_element_set_attribute(node, corestring_dom_class, s);
		dom_string_unref(s);
	}
	return JS_UNDEFINED;
}

static JSValue
js_get_attr(JSContext *ctx, JSValueConst this_val,
		int argc, JSValueConst *argv)
{
	dom_string *name;

	if (argc < 1) {
		return JS_NULL;
	}
	name = qjs_arg_str(ctx, argv[0]);
	if (name == NULL) {
		return JS_NULL;
	}
	{
		JSValue out;

		out = js_attr_named(ctx, qjs_node_from_this(ctx, this_val),
				name, 0);
		dom_string_unref(name);
		return out;
	}
}

static JSValue
js_set_attr(JSContext *ctx, JSValueConst this_val,
		int argc, JSValueConst *argv)
{
	dom_node *node;
	dom_string *name;
	dom_string *value;

	if (argc < 2) {
		return JS_UNDEFINED;
	}
	node = qjs_node_from_this(ctx, this_val);
	name = qjs_arg_str(ctx, argv[0]);
	value = qjs_arg_str(ctx, argv[1]);
	if (node != NULL && name != NULL && value != NULL) {
		dom_element_set_attribute(node, name, value);
	}
	if (name != NULL) {
		dom_string_unref(name);
	}
	if (value != NULL) {
		dom_string_unref(value);
	}
	return JS_UNDEFINED;
}

static JSValue
js_has_attr(JSContext *ctx, JSValueConst this_val,
		int argc, JSValueConst *argv)
{
	dom_node *node;
	dom_string *name;
	bool res;

	if (argc < 1) {
		return JS_FALSE;
	}
	node = qjs_node_from_this(ctx, this_val);
	name = qjs_arg_str(ctx, argv[0]);
	res = false;
	if (node != NULL && name != NULL) {
		dom_element_has_attribute(node, name, &res);
	}
	if (name != NULL) {
		dom_string_unref(name);
	}
	return JS_NewBool(ctx, res);
}

static JSValue
js_remove_attr(JSContext *ctx, JSValueConst this_val,
		int argc, JSValueConst *argv)
{
	dom_node *node;
	dom_string *name;

	if (argc < 1) {
		return JS_UNDEFINED;
	}
	node = qjs_node_from_this(ctx, this_val);
	name = qjs_arg_str(ctx, argv[0]);
	if (node != NULL && name != NULL) {
		dom_element_remove_attribute(node, name);
	}
	if (name != NULL) {
		dom_string_unref(name);
	}
	return JS_UNDEFINED;
}

static JSValue
js_class_list(JSContext *ctx, JSValueConst this_val)
{
	dom_node *node;
	dom_node_type t;
	dom_tokenlist *tokens;
	JSValue out;

	node = qjs_node_from_this(ctx, this_val);
	if (!qjs_node_type(node, &t) || t != DOM_ELEMENT_NODE) {
		return JS_NULL;
	}
	tokens = NULL;
	if (dom_tokenlist_create((dom_element *)node, corestring_dom_class,
			&tokens) != DOM_NO_ERR) {
		return JS_NULL;
	}
	out = qjs_push_tokenlist(ctx, tokens);
	dom_tokenlist_unref(tokens);
	return out;
}

static JSValue
js_attributes(JSContext *ctx, JSValueConst this_val)
{
	dom_node *node;
	dom_namednodemap *map;
	JSValue out;

	node = qjs_node_from_this(ctx, this_val);
	if (node == NULL) {
		return JS_NULL;
	}
	map = NULL;
	if (dom_node_get_attributes(node, &map) != DOM_NO_ERR) {
		return JS_NULL;
	}
	out = qjs_push_namemap(ctx, map);
	if (map != NULL) {
		dom_namednodemap_unref(map);
	}
	return out;
}

static int
qjs_html_buf_grow(char **buf, size_t *len, size_t *cap, size_t need)
{
	char *nbuf;
	size_t ncap;

	if (*len + need + 1 <= *cap) {
		return 0;
	}
	ncap = *cap ? *cap : 64;
	while (ncap < *len + need + 1) {
		ncap *= 2;
	}
	nbuf = realloc(*buf, ncap);
	if (nbuf == NULL) {
		return -1;
	}
	*buf = nbuf;
	*cap = ncap;
	return 0;
}

static int
qjs_html_buf_append(char **buf, size_t *len, size_t *cap,
		const char *s, size_t n)
{
	if (s == NULL || n == 0) {
		return 0;
	}
	if (qjs_html_buf_grow(buf, len, cap, n) != 0) {
		return -1;
	}
	memcpy(*buf + *len, s, n);
	*len += n;
	(*buf)[*len] = '\0';
	return 0;
}

static int
qjs_html_serialize_node(dom_node *node, char **buf, size_t *len, size_t *cap);

static int
qjs_html_serialize_children(dom_node *parent, char **buf, size_t *len,
		size_t *cap)
{
	dom_node *child;
	dom_node *next;
	dom_exception exc;

	child = NULL;
	exc = dom_node_get_first_child(parent, &child);
	while (exc == DOM_NO_ERR && child != NULL) {
		if (qjs_html_serialize_node(child, buf, len, cap) != 0) {
			dom_node_unref(child);
			return -1;
		}
		next = NULL;
		exc = dom_node_get_next_sibling(child, &next);
		dom_node_unref(child);
		child = next;
	}
	return 0;
}

static int
qjs_html_serialize_node(dom_node *node, char **buf, size_t *len, size_t *cap)
{
	dom_node_type t;
	dom_string *name;
	dom_string *val;
	dom_namednodemap *attrs;
	dom_attr *attr;
	uint32_t i;
	uint32_t n;
	dom_exception exc;

	t = DOM_NODE_TYPE_COUNT;
	if (dom_node_get_node_type(node, &t) != DOM_NO_ERR) {
		return -1;
	}

	if (t == DOM_TEXT_NODE || t == DOM_CDATA_SECTION_NODE) {
		val = NULL;
		if (dom_node_get_text_content(node, &val) != DOM_NO_ERR ||
				val == NULL) {
			return 0;
		}
		if (qjs_html_buf_append(buf, len, cap, dom_string_data(val),
				dom_string_length(val)) != 0) {
			dom_string_unref(val);
			return -1;
		}
		dom_string_unref(val);
		return 0;
	}

	if (t != DOM_ELEMENT_NODE) {
		return 0;
	}

	name = NULL;
	if (dom_node_get_node_name(node, &name) != DOM_NO_ERR || name == NULL) {
		return -1;
	}
	if (qjs_html_buf_append(buf, len, cap, "<", 1) != 0 ||
			qjs_html_buf_append(buf, len, cap, dom_string_data(name),
					dom_string_length(name)) != 0) {
		dom_string_unref(name);
		return -1;
	}

	attrs = NULL;
	exc = dom_node_get_attributes(node, &attrs);
	if (exc == DOM_NO_ERR && attrs != NULL) {
		n = 0;
		dom_namednodemap_get_length(attrs, &n);
		for (i = 0; i < n; i++) {
			attr = NULL;
			if (dom_namednodemap_item(attrs, i,
					(dom_node **)&attr) != DOM_NO_ERR ||
					attr == NULL) {
				continue;
			}
			val = NULL;
			if (dom_attr_get_name(attr, &val) == DOM_NO_ERR &&
					val != NULL) {
				qjs_html_buf_append(buf, len, cap, " ", 1);
				qjs_html_buf_append(buf, len, cap,
						dom_string_data(val),
						dom_string_length(val));
				dom_string_unref(val);
			}
			val = NULL;
			if (dom_attr_get_value(attr, &val) == DOM_NO_ERR &&
					val != NULL) {
				qjs_html_buf_append(buf, len, cap, "=\"", 2);
				qjs_html_buf_append(buf, len, cap,
						dom_string_data(val),
						dom_string_length(val));
				qjs_html_buf_append(buf, len, cap, "\"", 1);
				dom_string_unref(val);
			}
			dom_node_unref((dom_node *)attr);
		}
		dom_namednodemap_unref(attrs);
	}

	if (qjs_html_buf_append(buf, len, cap, ">", 1) != 0) {
		dom_string_unref(name);
		return -1;
	}
	if (qjs_html_serialize_children(node, buf, len, cap) != 0) {
		dom_string_unref(name);
		return -1;
	}
	if (qjs_html_buf_append(buf, len, cap, "</", 2) != 0 ||
			qjs_html_buf_append(buf, len, cap, dom_string_data(name),
					dom_string_length(name)) != 0 ||
			qjs_html_buf_append(buf, len, cap, ">", 1) != 0) {
		dom_string_unref(name);
		return -1;
	}
	dom_string_unref(name);
	return 0;
}

static JSValue
js_inner_get(JSContext *ctx, JSValueConst this_val)
{
	dom_node *node;
	char *buf;
	size_t len;
	size_t cap;
	JSValue out;

	node = qjs_node_from_this(ctx, this_val);
	if (node == NULL) {
		return JS_NewStringLen(ctx, "", 0);
	}

	buf = NULL;
	len = 0;
	cap = 0;
	if (qjs_html_serialize_children(node, &buf, &len, &cap) != 0) {
		free(buf);
		return JS_NewStringLen(ctx, "", 0);
	}
	if (buf == NULL) {
		return JS_NewStringLen(ctx, "", 0);
	}
	out = JS_NewStringLen(ctx, buf, len);
	free(buf);
	return out;
}

static JSValue
js_inner_set(JSContext *ctx, JSValueConst this_val, JSValueConst val)
{
	dom_node *node;
	const char *s;
	size_t size;
	dom_hubbub_parser_params parse_params;
	dom_hubbub_error error;
	dom_hubbub_parser *parser;
	dom_document *doc;
	dom_document_fragment *fragment;
	dom_node *child;
	dom_node *html;
	dom_node *body;
	dom_nodelist *bodies;
	dom_node *cref;
	dom_exception exc;

	node = qjs_node_from_this(ctx, this_val);
	if (node == NULL) {
		return JS_UNDEFINED;
	}
	s = JS_ToCStringLen(ctx, &size, val);
	if (s == NULL) {
		return JS_EXCEPTION;
	}

	parser = NULL;
	doc = NULL;
	fragment = NULL;
	child = NULL;
	html = NULL;
	body = NULL;
	bodies = NULL;
	cref = NULL;

	exc = dom_node_get_owner_document(node, (dom_node **)&doc);
	if (exc != DOM_NO_ERR || doc == NULL) {
		goto out;
	}

	parse_params.enc = "UTF-8";
	parse_params.fix_enc = true;
	parse_params.enable_script = false;
	parse_params.script = NULL;
	parse_params.msg = NULL;
	parse_params.ctx = NULL;
	parse_params.daf = NULL;

	error = dom_hubbub_fragment_parser_create(&parse_params, doc,
			&parser, &fragment);
	if (error != DOM_HUBBUB_OK) {
		goto out;
	}
	error = dom_hubbub_parser_parse_chunk(parser, (const uint8_t *)s, size);
	if (error != DOM_HUBBUB_OK) {
		goto out;
	}
	error = dom_hubbub_parser_completed(parser);
	if (error != DOM_HUBBUB_OK) {
		goto out;
	}

	exc = dom_node_get_first_child(node, &child);
	while (exc == DOM_NO_ERR && child != NULL) {
		exc = dom_node_remove_child(node, child, &cref);
		dom_node_unref(child);
		child = NULL;
		if (cref != NULL) {
			dom_node_unref(cref);
			cref = NULL;
		}
		if (exc != DOM_NO_ERR) {
			goto out;
		}
		exc = dom_node_get_first_child(node, &child);
	}

	exc = dom_node_get_first_child(fragment, &html);
	if (exc != DOM_NO_ERR || html == NULL) {
		goto out;
	}
	exc = dom_element_get_elements_by_tag_name(html, corestring_dom_BODY,
			&bodies);
	if (exc != DOM_NO_ERR || bodies == NULL) {
		goto out;
	}
	exc = dom_nodelist_item(bodies, 0, &body);
	if (exc != DOM_NO_ERR || body == NULL) {
		goto out;
	}

	exc = dom_node_get_first_child(body, &child);
	while (exc == DOM_NO_ERR && child != NULL) {
		exc = dom_node_remove_child(body, child, &cref);
		if (cref != NULL) {
			dom_node_unref(cref);
			cref = NULL;
		}
		if (exc != DOM_NO_ERR) {
			goto out;
		}
		exc = dom_node_append_child(node, child, &cref);
		dom_node_unref(child);
		child = NULL;
		if (cref != NULL) {
			dom_node_unref(cref);
			cref = NULL;
		}
		if (exc != DOM_NO_ERR) {
			goto out;
		}
		exc = dom_node_get_first_child(body, &child);
	}
	qjs_sync(ctx, node);

out:
	JS_FreeCString(ctx, s);
	if (parser != NULL) {
		dom_hubbub_parser_destroy(parser);
	}
	if (doc != NULL) {
		dom_node_unref(doc);
	}
	if (fragment != NULL) {
		dom_node_unref(fragment);
	}
	if (child != NULL) {
		dom_node_unref(child);
	}
	if (html != NULL) {
		dom_node_unref(html);
	}
	if (bodies != NULL) {
		dom_nodelist_unref(bodies);
	}
	if (body != NULL) {
		dom_node_unref(body);
	}
	return JS_UNDEFINED;
}

static JSValue
js_style(JSContext *ctx, JSValueConst this_val)
{
	(void)this_val;
	/* Empty object so feature tests that read element.style do not loop. */
	return JS_NewObject(ctx);
}

static int
qjs_control_kind(dom_node *node)
{
	if (qjs_name_is(node, corestring_dom_INPUT)) {
		return 1;
	}
	if (qjs_name_is(node, corestring_dom_TEXTAREA)) {
		return 2;
	}
	if (qjs_name_is(node, corestring_dom_SELECT)) {
		return 3;
	}
	return 0;
}

static JSValue
js_value_get(JSContext *ctx, JSValueConst this_val)
{
	dom_node *node;
	dom_string *val;
	int kind;

	node = qjs_node_from_this(ctx, this_val);
	if (node == NULL) {
		return JS_NewStringLen(ctx, "", 0);
	}
	kind = qjs_control_kind(node);
	val = NULL;
	if (kind == 1) {
		dom_html_input_element_get_value(
				(dom_html_input_element *)node, &val);
	} else if (kind == 2) {
		dom_html_text_area_element_get_value(
				(dom_html_text_area_element *)node, &val);
	} else if (kind == 3) {
		dom_html_select_element_get_value(
				(dom_html_select_element *)node, &val);
	} else {
		return js_attr_named(ctx, node, corestring_dom_value, 1);
	}
	if (val == NULL) {
		return JS_NewStringLen(ctx, "", 0);
	}
	return qjs_from_dom(ctx, val);
}

static JSValue
js_value_set(JSContext *ctx, JSValueConst this_val, JSValueConst val)
{
	dom_node *node;
	dom_string *s;
	int kind;

	node = qjs_node_from_this(ctx, this_val);
	if (node == NULL) {
		return JS_UNDEFINED;
	}
	s = qjs_arg_str(ctx, val);
	if (s == NULL) {
		return JS_EXCEPTION;
	}
	kind = qjs_control_kind(node);
	if (kind == 1) {
		dom_html_input_element_set_value(
				(dom_html_input_element *)node, s);
	} else if (kind == 2) {
		dom_html_text_area_element_set_value(
				(dom_html_text_area_element *)node, s);
	} else if (kind == 3) {
		dom_html_select_element_set_value(
				(dom_html_select_element *)node, s);
	} else {
		dom_element_set_attribute(node, corestring_dom_value, s);
	}
	dom_string_unref(s);
	return JS_UNDEFINED;
}

static JSValue
js_checked_get(JSContext *ctx, JSValueConst this_val)
{
	dom_node *node;
	bool bit;

	node = qjs_node_from_this(ctx, this_val);
	bit = false;
	if (node != NULL && qjs_name_is(node, corestring_dom_INPUT)) {
		dom_html_input_element_get_checked(
				(dom_html_input_element *)node, &bit);
	}
	return JS_NewBool(ctx, bit);
}

static JSValue
js_checked_set(JSContext *ctx, JSValueConst this_val, JSValueConst val)
{
	dom_node *node;
	int bit;

	node = qjs_node_from_this(ctx, this_val);
	bit = JS_ToBool(ctx, val);
	if (bit < 0) {
		return JS_EXCEPTION;
	}
	if (node != NULL && qjs_name_is(node, corestring_dom_INPUT)) {
		dom_html_input_element_set_checked(
				(dom_html_input_element *)node, bit != 0);
	}
	return JS_UNDEFINED;
}

static JSValue
js_disabled_get(JSContext *ctx, JSValueConst this_val)
{
	dom_node *node;
	bool bit;

	node = qjs_node_from_this(ctx, this_val);
	bit = false;
	if (node != NULL && qjs_name_is(node, corestring_dom_INPUT)) {
		dom_html_input_element_get_disabled(
				(dom_html_input_element *)node, &bit);
	}
	return JS_NewBool(ctx, bit);
}

static JSValue
js_disabled_set(JSContext *ctx, JSValueConst this_val, JSValueConst val)
{
	dom_node *node;
	int bit;

	node = qjs_node_from_this(ctx, this_val);
	bit = JS_ToBool(ctx, val);
	if (bit < 0) {
		return JS_EXCEPTION;
	}
	if (node != NULL && qjs_name_is(node, corestring_dom_INPUT)) {
		dom_html_input_element_set_disabled(
				(dom_html_input_element *)node, bit != 0);
	}
	return JS_UNDEFINED;
}

static JSValue
js_type_get(JSContext *ctx, JSValueConst this_val)
{
	dom_node *node;
	dom_string *t;

	node = qjs_node_from_this(ctx, this_val);
	if (node == NULL || !qjs_name_is(node, corestring_dom_INPUT)) {
		return JS_NewStringLen(ctx, "", 0);
	}
	t = NULL;
	if (dom_html_input_element_get_type(
			(dom_html_input_element *)node, &t) != DOM_NO_ERR ||
			t == NULL) {
		return JS_NewStringLen(ctx, "", 0);
	}
	return qjs_from_dom(ctx, t);
}

static JSValue
js_href_get(JSContext *ctx, JSValueConst this_val)
{
	dom_node *node;
	dom_string *href;

	node = qjs_node_from_this(ctx, this_val);
	if (node == NULL) {
		return JS_NewStringLen(ctx, "", 0);
	}
	if (qjs_name_ascii(node, "A")) {
		href = NULL;
		if (dom_html_anchor_element_get_href(
				(dom_html_anchor_element *)node, &href) !=
				DOM_NO_ERR || href == NULL) {
			return JS_NewStringLen(ctx, "", 0);
		}
		return qjs_from_dom(ctx, href);
	}
	return js_attr_named(ctx, node, corestring_dom_href, 1);
}

static JSValue
js_href_set(JSContext *ctx, JSValueConst this_val, JSValueConst val)
{
	dom_node *node;
	dom_string *s;

	node = qjs_node_from_this(ctx, this_val);
	s = qjs_arg_str(ctx, val);
	if (node == NULL || s == NULL) {
		if (s != NULL) {
			dom_string_unref(s);
		}
		return JS_UNDEFINED;
	}
	if (qjs_name_ascii(node, "A")) {
		dom_html_anchor_element_set_href(
				(dom_html_anchor_element *)node, s);
	} else {
		dom_element_set_attribute(node, corestring_dom_href, s);
	}
	dom_string_unref(s);
	return JS_UNDEFINED;
}

static JSValue
js_src_get(JSContext *ctx, JSValueConst this_val)
{
	dom_node *node;
	dom_string *src;

	node = qjs_node_from_this(ctx, this_val);
	if (node == NULL) {
		return JS_NewStringLen(ctx, "", 0);
	}
	if (qjs_name_ascii(node, "IMG")) {
		src = NULL;
		if (dom_html_image_element_get_src(
				(dom_html_image_element *)node, &src) !=
				DOM_NO_ERR || src == NULL) {
			return JS_NewStringLen(ctx, "", 0);
		}
		return qjs_from_dom(ctx, src);
	}
	return js_attr_named(ctx, node, corestring_dom_src, 1);
}

static JSValue
js_src_set(JSContext *ctx, JSValueConst this_val, JSValueConst val)
{
	dom_node *node;
	dom_string *s;

	node = qjs_node_from_this(ctx, this_val);
	s = qjs_arg_str(ctx, val);
	if (node == NULL || s == NULL) {
		if (s != NULL) {
			dom_string_unref(s);
		}
		return JS_UNDEFINED;
	}
	if (qjs_name_ascii(node, "IMG")) {
		dom_html_image_element_set_src(
				(dom_html_image_element *)node, s);
	} else {
		dom_element_set_attribute(node, corestring_dom_src, s);
	}
	dom_string_unref(s);
	return JS_UNDEFINED;
}

static JSValue
js_submit(JSContext *ctx, JSValueConst this_val,
		int argc, JSValueConst *argv)
{
	dom_node *node;

	(void)argc;
	(void)argv;
	node = qjs_node_from_this(ctx, this_val);
	if (node != NULL && qjs_name_ascii(node, "FORM")) {
		dom_html_form_element_submit((dom_html_form_element *)node);
	}
	return JS_UNDEFINED;
}

static JSValue
js_focus(JSContext *ctx, JSValueConst this_val,
		int argc, JSValueConst *argv)
{
	dom_node *node;

	(void)argc;
	(void)argv;
	node = qjs_node_from_this(ctx, this_val);
	if (node != NULL && qjs_name_is(node, corestring_dom_INPUT)) {
		dom_html_input_element_focus((dom_html_input_element *)node);
	}
	return JS_UNDEFINED;
}

static JSValue
js_blur(JSContext *ctx, JSValueConst this_val,
		int argc, JSValueConst *argv)
{
	dom_node *node;

	(void)argc;
	(void)argv;
	node = qjs_node_from_this(ctx, this_val);
	if (node != NULL && qjs_name_is(node, corestring_dom_INPUT)) {
		dom_html_input_element_blur((dom_html_input_element *)node);
	}
	return JS_UNDEFINED;
}

static JSValue
js_select(JSContext *ctx, JSValueConst this_val,
		int argc, JSValueConst *argv)
{
	dom_node *node;

	(void)argc;
	(void)argv;
	node = qjs_node_from_this(ctx, this_val);
	if (node != NULL && qjs_name_is(node, corestring_dom_INPUT)) {
		dom_html_input_element_select((dom_html_input_element *)node);
	}
	return JS_UNDEFINED;
}

static JSValue
js_click(JSContext *ctx, JSValueConst this_val,
		int argc, JSValueConst *argv)
{
	dom_node *node;

	(void)argc;
	(void)argv;
	(void)ctx;
	node = qjs_node_from_this(ctx, this_val);
	if (node == NULL) {
		return JS_UNDEFINED;
	}
	/* INPUT uses the HTML click helper; everything else gets a real
	 * DOM click so addEventListener handlers run (welcome smoke). */
	if (qjs_name_is(node, corestring_dom_INPUT)) {
		dom_html_input_element_click((dom_html_input_element *)node);
	} else {
		fire_generic_dom_event(corestring_dom_click, node, true, true);
	}
	return JS_UNDEFINED;
}

static html_content *
qjs_html_of(JSContext *ctx, dom_node *node)
{
	html_content *html;
	dom_exception err;

	html = NULL;
	err = dom_node_get_user_data(node,
			corestring_dom___ns_key_html_content_data,
			(void **)&html);
	if (err != DOM_NO_ERR) {
		return NULL;
	}
	if (html == NULL) {
		jsthread *thread;

		thread = qjs_thread_from_ctx(ctx);
		if (thread != NULL) {
			html = (html_content *)thread->doc_priv;
		}
	}
	return html;
}

static JSValue
js_doc_write(JSContext *ctx, JSValueConst this_val,
		int argc, JSValueConst *argv)
{
	dom_node *node;
	html_content *html;
	int i;
	const char *text;
	size_t len;

	node = qjs_node_from_this(ctx, this_val);
	if (node == NULL) {
		return JS_UNDEFINED;
	}
	html = qjs_html_of(ctx, node);
	if (html == NULL || html->parser == NULL) {
		return JS_UNDEFINED;
	}
	for (i = 0; i < argc; i++) {
		text = JS_ToCStringLen(ctx, &len, argv[i]);
		if (text == NULL) {
			continue;
		}
		dom_hubbub_parser_insert_chunk(html->parser,
				(uint8_t *)text, len);
		JS_FreeCString(ctx, text);
	}
	return JS_UNDEFINED;
}

static JSValue
js_doc_writeln(JSContext *ctx, JSValueConst this_val,
		int argc, JSValueConst *argv)
{
	dom_node *node;
	html_content *html;

	js_doc_write(ctx, this_val, argc, argv);
	node = qjs_node_from_this(ctx, this_val);
	html = NULL;
	if (node != NULL) {
		html = qjs_html_of(ctx, node);
	}
	if (html != NULL && html->parser != NULL) {
		dom_hubbub_parser_insert_chunk(html->parser,
				(const uint8_t *)"\n", 1);
	}
	return JS_UNDEFINED;
}

static JSValue
js_create_text(JSContext *ctx, JSValueConst this_val,
		int argc, JSValueConst *argv)
{
	dom_node *node;
	dom_string *text;
	dom_node *neu;
	JSValue out;

	if (argc < 1) {
		return JS_NULL;
	}
	node = qjs_node_from_this(ctx, this_val);
	text = qjs_arg_str(ctx, argv[0]);
	if (node == NULL || text == NULL) {
		if (text != NULL) {
			dom_string_unref(text);
		}
		return JS_NULL;
	}
	neu = NULL;
	if (dom_document_create_text_node(node, text, &neu) != DOM_NO_ERR) {
		dom_string_unref(text);
		return JS_NULL;
	}
	dom_string_unref(text);
	out = qjs_push_node(ctx, neu);
	if (neu != NULL) {
		dom_node_unref(neu);
	}
	return out;
}

static JSValue
js_create_el(JSContext *ctx, JSValueConst this_val,
		int argc, JSValueConst *argv)
{
	dom_node *node;
	dom_string *text;
	dom_string *ns;
	dom_node *neu;
	JSValue out;

	if (argc < 1) {
		return JS_NULL;
	}
	node = qjs_node_from_this(ctx, this_val);
	if (node == NULL) {
		return JS_NULL;
	}
	if (argc > 1) {
		ns = qjs_arg_str(ctx, argv[0]);
		text = qjs_arg_str(ctx, argv[1]);
	} else {
		ns = NULL;
		dom_string_ref(corestring_dom_html_namespace);
		ns = corestring_dom_html_namespace;
		text = qjs_arg_str(ctx, argv[0]);
	}
	if (ns == NULL || text == NULL) {
		if (ns != NULL) {
			dom_string_unref(ns);
		}
		if (text != NULL) {
			dom_string_unref(text);
		}
		return JS_NULL;
	}
	neu = NULL;
	if (dom_document_create_element_ns(node, ns, text, &neu) != DOM_NO_ERR) {
		dom_string_unref(ns);
		dom_string_unref(text);
		return JS_NULL;
	}
	dom_string_unref(ns);
	dom_string_unref(text);
	out = qjs_push_node(ctx, neu);
	if (neu != NULL) {
		dom_node_unref(neu);
	}
	return out;
}

static JSValue
js_create_frag(JSContext *ctx, JSValueConst this_val,
		int argc, JSValueConst *argv)
{
	dom_node *node;
	dom_document_fragment *frag;
	JSValue out;

	(void)argc;
	(void)argv;
	node = qjs_node_from_this(ctx, this_val);
	if (node == NULL) {
		return JS_NULL;
	}
	frag = NULL;
	if (dom_document_create_document_fragment(node, &frag) != DOM_NO_ERR) {
		return JS_NULL;
	}
	out = qjs_push_node(ctx, (dom_node *)frag);
	if (frag != NULL) {
		dom_node_unref(frag);
	}
	return out;
}

static JSValue
js_tag_first(JSContext *ctx, JSValueConst this_val, dom_string *tag)
{
	dom_node *node;
	dom_nodelist *nodes;
	dom_node *item;
	JSValue out;

	node = qjs_node_from_this(ctx, this_val);
	if (node == NULL) {
		return JS_NULL;
	}
	nodes = NULL;
	if (dom_document_get_elements_by_tag_name(node, tag, &nodes) !=
			DOM_NO_ERR || nodes == NULL) {
		return JS_NULL;
	}
	item = NULL;
	if (dom_nodelist_item(nodes, 0, &item) != DOM_NO_ERR) {
		dom_nodelist_unref(nodes);
		return JS_NULL;
	}
	dom_nodelist_unref(nodes);
	out = qjs_push_node(ctx, item);
	if (item != NULL) {
		dom_node_unref(item);
	}
	return out;
}

static JSValue
js_head(JSContext *ctx, JSValueConst this_val)
{
	return js_tag_first(ctx, this_val, corestring_dom_HEAD);
}

static JSValue
js_body(JSContext *ctx, JSValueConst this_val)
{
	return js_tag_first(ctx, this_val, corestring_dom_BODY);
}

static JSValue
js_doc_el(JSContext *ctx, JSValueConst this_val)
{
	dom_node *node;
	dom_element *el;
	JSValue out;

	node = qjs_node_from_this(ctx, this_val);
	if (node == NULL) {
		return JS_NULL;
	}
	el = NULL;
	if (dom_document_get_document_element(node, &el) != DOM_NO_ERR) {
		return JS_NULL;
	}
	out = qjs_push_node(ctx, (dom_node *)el);
	if (el != NULL) {
		dom_node_unref(el);
	}
	return out;
}

static JSValue
js_doc_loc(JSContext *ctx, JSValueConst this_val)
{
	JSValue global;
	JSValue loc;

	(void)this_val;
	global = JS_GetGlobalObject(ctx);
	loc = JS_GetPropertyStr(ctx, global, "location");
	JS_FreeValue(ctx, global);
	return loc;
}

static JSValue
js_cookie_get(JSContext *ctx, JSValueConst this_val)
{
	dom_node *node;
	html_content *html;
	char *cookie;
	JSValue out;

	node = qjs_node_from_this(ctx, this_val);
	html = NULL;
	if (node != NULL) {
		html = qjs_html_of(ctx, node);
	}
	if (html == NULL || html->base.llcache == NULL) {
		return JS_NewStringLen(ctx, "", 0);
	}
	cookie = urldb_get_cookie(llcache_handle_get_url(html->base.llcache),
			false);
	if (cookie == NULL) {
		return JS_NewStringLen(ctx, "", 0);
	}
	out = JS_NewString(ctx, cookie);
	free(cookie);
	return out;
}

static JSValue
js_cookie_set(JSContext *ctx, JSValueConst this_val, JSValueConst val)
{
	dom_node *node;
	html_content *html;
	const char *cookie;

	node = qjs_node_from_this(ctx, this_val);
	html = NULL;
	if (node != NULL) {
		html = qjs_html_of(ctx, node);
	}
	cookie = JS_ToCString(ctx, val);
	if (cookie != NULL && html != NULL && html->base.llcache != NULL) {
		urldb_set_cookie(cookie,
				llcache_handle_get_url(html->base.llcache), NULL);
	}
	if (cookie != NULL) {
		JS_FreeCString(ctx, cookie);
	}
	return JS_UNDEFINED;
}

static JSValue
js_title_get(JSContext *ctx, JSValueConst this_val)
{
	dom_node *node;
	dom_string *title;

	node = qjs_node_from_this(ctx, this_val);
	if (node == NULL) {
		return JS_NewStringLen(ctx, "", 0);
	}
	title = NULL;
	if (dom_html_document_get_title(node, &title) != DOM_NO_ERR ||
			title == NULL) {
		return JS_NewStringLen(ctx, "", 0);
	}
	return qjs_from_dom(ctx, title);
}

static JSValue
js_title_set(JSContext *ctx, JSValueConst this_val, JSValueConst val)
{
	dom_node *node;
	dom_string *title;

	node = qjs_node_from_this(ctx, this_val);
	title = qjs_arg_str(ctx, val);
	if (node != NULL && title != NULL) {
		dom_html_document_set_title(node, title);
		dom_string_unref(title);
	}
	return JS_UNDEFINED;
}

static JSValue
js_create_event(JSContext *ctx, JSValueConst this_val,
		int argc, JSValueConst *argv)
{
	dom_event *evt;

	(void)this_val;
	(void)argc;
	(void)argv;
	evt = NULL;
	if (dom_event_create(&evt) != DOM_NO_ERR || evt == NULL) {
		return JS_NULL;
	}
	dom_event_set_is_trusted(evt, false);
	{
		JSValue out;

		out = qjs_push_event(ctx, evt);
		dom_event_unref(evt);
		return out;
	}
}

static JSValue
js_has_feature(JSContext *ctx, JSValueConst this_val,
		int argc, JSValueConst *argv)
{
	(void)this_val;
	(void)argc;
	(void)argv;
	return JS_TRUE;
}

static JSValue
js_create_html_doc(JSContext *ctx, JSValueConst this_val,
		int argc, JSValueConst *argv)
{
	const char *text;
	size_t text_len;
	dom_document *doc;
	dom_document *ret;
	dom_document_type *doctype;
	dom_html_element *html;
	dom_html_element *head;
	dom_html_element *title;
	dom_html_element *body;
	dom_node *spare;
	dom_string *text_str;
	dom_exception exc;
	JSValue out;

	(void)this_val;
	text = "";
	text_len = 0;
	if (argc > 0) {
		text = JS_ToCStringLen(ctx, &text_len, argv[0]);
		if (text == NULL) {
			return JS_EXCEPTION;
		}
	}
	doc = NULL;
	ret = NULL;
	doctype = NULL;
	html = NULL;
	head = NULL;
	title = NULL;
	body = NULL;
	spare = NULL;
	text_str = NULL;
	out = JS_NULL;

	exc = dom_string_create((const uint8_t *)text, text_len, &text_str);
	if (argc > 0) {
		JS_FreeCString(ctx, text);
	}
	if (exc != DOM_NO_ERR) {
		goto done;
	}
	exc = dom_implementation_create_document(DOM_IMPLEMENTATION_HTML,
			NULL, NULL, NULL, NULL, NULL, &doc);
	if (exc != DOM_NO_ERR) {
		goto done;
	}
	exc = dom_implementation_create_document_type("html", NULL, NULL,
			&doctype);
	if (exc != DOM_NO_ERR) {
		goto done;
	}
	exc = dom_node_append_child(doc, (dom_node *)doctype, &spare);
	if (exc != DOM_NO_ERR) {
		goto done;
	}
	if (spare != NULL) {
		dom_node_unref(spare);
		spare = NULL;
	}
	exc = dom_document_create_element_ns(doc, corestring_dom_html_namespace,
			corestring_dom_HTML, (dom_node **)&html);
	if (exc != DOM_NO_ERR) {
		goto done;
	}
	exc = dom_document_create_element_ns(doc, corestring_dom_html_namespace,
			corestring_dom_HEAD, (dom_node **)&head);
	if (exc != DOM_NO_ERR) {
		goto done;
	}
	exc = dom_document_create_element_ns(doc, corestring_dom_html_namespace,
			corestring_dom_TITLE, (dom_node **)&title);
	if (exc != DOM_NO_ERR) {
		goto done;
	}
	exc = dom_document_create_element_ns(doc, corestring_dom_html_namespace,
			corestring_dom_BODY, (dom_node **)&body);
	if (exc != DOM_NO_ERR) {
		goto done;
	}
	exc = dom_node_set_text_content((dom_node *)title, text_str);
	if (exc != DOM_NO_ERR) {
		goto done;
	}
	exc = dom_node_append_child((dom_node *)head, (dom_node *)title, &spare);
	if (spare != NULL) {
		dom_node_unref(spare);
		spare = NULL;
	}
	if (exc != DOM_NO_ERR) {
		goto done;
	}
	exc = dom_node_append_child((dom_node *)html, (dom_node *)head, &spare);
	if (spare != NULL) {
		dom_node_unref(spare);
		spare = NULL;
	}
	if (exc != DOM_NO_ERR) {
		goto done;
	}
	exc = dom_node_append_child((dom_node *)html, (dom_node *)body, &spare);
	if (spare != NULL) {
		dom_node_unref(spare);
		spare = NULL;
	}
	if (exc != DOM_NO_ERR) {
		goto done;
	}
	exc = dom_node_append_child(doc, (dom_node *)html, &spare);
	if (spare != NULL) {
		dom_node_unref(spare);
		spare = NULL;
	}
	if (exc != DOM_NO_ERR) {
		goto done;
	}
	ret = doc;
	doc = NULL;

done:
	if (text_str != NULL) {
		dom_string_unref(text_str);
	}
	if (doc != NULL) {
		dom_node_unref(doc);
	}
	if (html != NULL) {
		dom_node_unref(html);
	}
	if (head != NULL) {
		dom_node_unref(head);
	}
	if (title != NULL) {
		dom_node_unref(title);
	}
	if (body != NULL) {
		dom_node_unref(body);
	}
	if (doctype != NULL) {
		dom_node_unref(doctype);
	}
	if (ret != NULL) {
		out = qjs_push_node(ctx, (dom_node *)ret);
		dom_node_unref(ret);
	}
	return out;
}

static const JSCFunctionListEntry qjs_impl_funcs[] = {
	JS_CFUNC_DEF("hasFeature", 0, js_has_feature),
	JS_CFUNC_DEF("createHTMLDocument", 1, js_create_html_doc),
};

static JSValue
js_implementation(JSContext *ctx, JSValueConst this_val)
{
	JSValue impl;

	(void)this_val;
	impl = JS_NewObject(ctx);
	JS_SetPropertyFunctionList(ctx, impl, qjs_impl_funcs,
			(int)(sizeof(qjs_impl_funcs) / sizeof(qjs_impl_funcs[0])));
	return impl;
}

static const JSCFunctionListEntry qjs_node_proto_funcs[] = {
	JS_CGETSET_DEF("nodeType", js_node_type, NULL),
	JS_CGETSET_DEF("nodeName", js_node_name, NULL),
	JS_CGETSET_DEF("tagName", js_node_name, NULL),
	JS_CGETSET_DEF("baseURI", js_node_base, NULL),
	JS_CGETSET_DEF("ownerDocument", js_node_owner, NULL),
	JS_CGETSET_DEF("parentNode", js_parent, NULL),
	JS_CGETSET_DEF("parentElement", js_parent_el, NULL),
	JS_CGETSET_DEF("childNodes", js_child_nodes, NULL),
	JS_CGETSET_DEF("firstChild", js_first, NULL),
	JS_CGETSET_DEF("lastChild", js_last, NULL),
	JS_CGETSET_DEF("previousSibling", js_prev, NULL),
	JS_CGETSET_DEF("nextSibling", js_next, NULL),
	JS_CGETSET_DEF("nodeValue", js_node_value_get, js_node_value_set),
	JS_CFUNC_DEF("hasChildNodes", 0, js_has_child),
	JS_CFUNC_DEF("normalize", 0, js_normalize),
	JS_CFUNC_DEF("cloneNode", 1, js_clone),
	JS_CFUNC_DEF("isEqualNode", 1, js_equal),
	JS_CFUNC_DEF("compareDocumentPosition", 1, js_compare),
	JS_CFUNC_DEF("contains", 1, js_contains),
	JS_CFUNC_DEF("lookupPrefix", 1, js_lookup_prefix),
	JS_CFUNC_DEF("lookupNamespaceURI", 1, js_lookup_ns),
	JS_CFUNC_DEF("isDefaultNamespace", 1, js_default_ns),
	JS_CFUNC_DEF("insertBefore", 2, js_insert),
	JS_CFUNC_DEF("appendChild", 1, js_append),
	JS_CFUNC_DEF("replaceChild", 2, js_replace),
	JS_CFUNC_DEF("removeChild", 1, js_remove),
	JS_CGETSET_DEF("firstElementChild", js_first_el, NULL),
	JS_CGETSET_DEF("lastElementChild", js_last_el, NULL),
	JS_CGETSET_DEF("previousElementSibling", js_prev_el, NULL),
	JS_CGETSET_DEF("nextElementSibling", js_next_el, NULL),
	JS_CGETSET_DEF("childElementCount", js_child_el_count, NULL),
	JS_CFUNC_DEF("getElementsByTagName", 1, js_by_tag),
	JS_CGETSET_DEF("id", js_id_get, js_id_set),
	JS_CGETSET_DEF("className", js_class_get, js_class_set),
	JS_CGETSET_DEF("classList", js_class_list, NULL),
	JS_CFUNC_DEF("getAttribute", 1, js_get_attr),
	JS_CFUNC_DEF("setAttribute", 2, js_set_attr),
	JS_CFUNC_DEF("hasAttribute", 1, js_has_attr),
	JS_CFUNC_DEF("removeAttribute", 1, js_remove_attr),
	JS_CGETSET_DEF("attributes", js_attributes, NULL),
	JS_CGETSET_DEF("innerHTML", js_inner_get, js_inner_set),
	JS_CGETSET_DEF("style", js_style, NULL),
	JS_CGETSET_DEF("value", js_value_get, js_value_set),
	JS_CGETSET_DEF("checked", js_checked_get, js_checked_set),
	JS_CGETSET_DEF("disabled", js_disabled_get, js_disabled_set),
	JS_CGETSET_DEF("type", js_type_get, NULL),
	JS_CGETSET_DEF("href", js_href_get, js_href_set),
	JS_CGETSET_DEF("src", js_src_get, js_src_set),
	JS_CFUNC_DEF("submit", 0, js_submit),
	JS_CFUNC_DEF("focus", 0, js_focus),
	JS_CFUNC_DEF("blur", 0, js_blur),
	JS_CFUNC_DEF("select", 0, js_select),
	JS_CFUNC_DEF("click", 0, js_click),
	JS_CFUNC_DEF("write", 0, js_doc_write),
	JS_CFUNC_DEF("writeln", 0, js_doc_writeln),
	JS_CFUNC_DEF("createTextNode", 1, js_create_text),
	JS_CFUNC_DEF("createElement", 1, js_create_el),
	JS_CFUNC_DEF("createElementNS", 2, js_create_el),
	JS_CFUNC_DEF("createDocumentFragment", 0, js_create_frag),
	JS_CGETSET_DEF("head", js_head, NULL),
	JS_CGETSET_DEF("body", js_body, NULL),
	JS_CGETSET_DEF("documentElement", js_doc_el, NULL),
	JS_CGETSET_DEF("location", js_doc_loc, NULL),
	JS_CGETSET_DEF("cookie", js_cookie_get, js_cookie_set),
	JS_CGETSET_DEF("title", js_title_get, js_title_set),
	JS_CFUNC_DEF("createEvent", 1, js_create_event),
	JS_CGETSET_DEF("implementation", js_implementation, NULL),
};

static void
qjs_set_node_const(JSContext *ctx, JSValue obj, const char *name, int32_t v)
{
	JS_SetPropertyStr(ctx, obj, name, JS_NewInt32(ctx, v));
}

void
qjs_bind_node_install(JSContext *ctx, JSValue proto, JSValue document)
{
	JSValue global;
	JSValue ctor;

	(void)document;
	NSLOG(netsurf, INFO, "bind_node SetPropertyFunctionList n=%d",
			(int)(sizeof(qjs_node_proto_funcs) /
					sizeof(qjs_node_proto_funcs[0])));
	JS_SetPropertyFunctionList(ctx, proto, qjs_node_proto_funcs,
			(int)(sizeof(qjs_node_proto_funcs) /
					sizeof(qjs_node_proto_funcs[0])));
	NSLOG(netsurf, INFO, "bind_node proto methods done");

	qjs_set_node_const(ctx, proto, "ELEMENT_NODE", DOM_ELEMENT_NODE);
	qjs_set_node_const(ctx, proto, "ATTRIBUTE_NODE", DOM_ATTRIBUTE_NODE);
	qjs_set_node_const(ctx, proto, "TEXT_NODE", DOM_TEXT_NODE);
	qjs_set_node_const(ctx, proto, "COMMENT_NODE", DOM_COMMENT_NODE);
	qjs_set_node_const(ctx, proto, "DOCUMENT_NODE", DOM_DOCUMENT_NODE);
	qjs_set_node_const(ctx, proto, "DOCUMENT_FRAGMENT_NODE",
			DOM_DOCUMENT_FRAGMENT_NODE);

	global = JS_GetGlobalObject(ctx);
	ctor = JS_NewObject(ctx);
	qjs_set_node_const(ctx, ctor, "ELEMENT_NODE", DOM_ELEMENT_NODE);
	qjs_set_node_const(ctx, ctor, "ATTRIBUTE_NODE", DOM_ATTRIBUTE_NODE);
	qjs_set_node_const(ctx, ctor, "TEXT_NODE", DOM_TEXT_NODE);
	qjs_set_node_const(ctx, ctor, "CDATA_SECTION_NODE",
			DOM_CDATA_SECTION_NODE);
	qjs_set_node_const(ctx, ctor, "COMMENT_NODE", DOM_COMMENT_NODE);
	qjs_set_node_const(ctx, ctor, "DOCUMENT_NODE", DOM_DOCUMENT_NODE);
	qjs_set_node_const(ctx, ctor, "DOCUMENT_TYPE_NODE",
			DOM_DOCUMENT_TYPE_NODE);
	qjs_set_node_const(ctx, ctor, "DOCUMENT_FRAGMENT_NODE",
			DOM_DOCUMENT_FRAGMENT_NODE);
	JS_SetPropertyStr(ctx, global, "Node", ctor);
	JS_FreeValue(ctx, global);
}
