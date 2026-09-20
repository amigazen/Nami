/*
 * Copyright 2026 AmigaZen / Nami contributors
 *
 * Shared helpers for the QuickJS DOM bindings.
 * The implemented surface matches NetSurf's duktape/*.bnd methods that
 * have real C bodies (Node, Element, Document, Window, Event, lists,
 * Location, Navigator, Console). Empty WebIDL attribute stubs, canvas
 * and CSSOM are not ported: duktape has no implementation for those.
 */

#ifndef NETSURF_JS_QUICKJS_AMIGA_BIND_PRIV_H_
#define NETSURF_JS_QUICKJS_AMIGA_BIND_PRIV_H_

#include "private.h"

struct dom_node;
struct dom_event;
struct dom_nodelist;
struct dom_namednodemap;
struct dom_tokenlist;

jsthread *qjs_thread_from_ctx(JSContext *ctx);
void qjs_claim_class_id(JSRuntime *rt, JSClassID *id);
int qjs_new_class(JSRuntime *rt, JSClassID class_id, const JSClassDef *def);
nserror qjs_bind_win_prepare(jsheap *heap);
JSValue qjs_push_node(JSContext *ctx, struct dom_node *node);
void qjs_node_memo_clear(jsthread *thread);
struct dom_node *qjs_node_from_this(JSContext *ctx, JSValueConst this_val);

JSValue qjs_push_event(JSContext *ctx, struct dom_event *evt);
JSValue qjs_push_nodelist(JSContext *ctx, struct dom_nodelist *list);
JSValue qjs_push_namemap(JSContext *ctx, struct dom_namednodemap *map);
JSValue qjs_push_tokenlist(JSContext *ctx, struct dom_tokenlist *list);

nserror qjs_bind_dom_prepare(jsheap *heap);
void qjs_bind_list_install(JSContext *ctx, JSValue node_proto);
void qjs_bind_node_install(JSContext *ctx, JSValue proto, JSValue document);
void qjs_bind_win_install(JSContext *ctx, JSValue global, JSValue console);
void qjs_bind_win_finish(jsthread *thread);
void qjs_bind_dom_teardown(jsthread *thread);

#endif /* NETSURF_JS_QUICKJS_AMIGA_BIND_PRIV_H_ */
