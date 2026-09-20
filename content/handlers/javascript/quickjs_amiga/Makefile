#
# NetSurf javascript source file inclusion — Amiga quickjs.library LVO client
#
# Included by javascript/Makefile when NETSURF_JS_ENGINE=quickjs_amiga
#
# The engine lives in LIBS:quickjs.library (nea-quickjs). This directory
# provides the js.h implementation only.
#
# The Amiga OS3 vbcc build (make -f VMakefile) links the LVO bridge from
# nea-quickjs. That is the supported configuration: VBCC calling convention
# matches the bridge trampolines. Do not use this engine with a gcc Amiga
# link without a matching bridge.
#

S_JAVASCRIPT += content.c quickjs_amiga/js.c quickjs_amiga/bind.c quickjs_amiga/bind_node.c quickjs_amiga/bind_list.c quickjs_amiga/bind_win.c quickjs_amiga/dom_sync.c
