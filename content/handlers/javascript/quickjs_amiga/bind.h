/*
 * Copyright 2026 AmigaZen / Nami contributors
 *
 * Minimal Window / Document / Element / Console bindings for QuickJS.
 */

#ifndef NETSURF_JS_QUICKJS_AMIGA_BIND_H_
#define NETSURF_JS_QUICKJS_AMIGA_BIND_H_

#include "utils/errors.h"
#include "javascript/js.h"

/**
 * Register DOM classes on the runtime. Must run before JS_NewContext:
 * a live context makes JS_NewClass grow that context's class_proto table.
 */
nserror qjs_bind_prepare_runtime(jsheap *heap);

/**
 * Install window/document/console globals on a new JSContext.
 */
nserror qjs_bind_install(jsthread *thread);

/**
 * Drop listeners and opaque refs while the context is still alive.
 */
void qjs_bind_teardown(jsthread *thread);

#endif /* NETSURF_JS_QUICKJS_AMIGA_BIND_H_ */
