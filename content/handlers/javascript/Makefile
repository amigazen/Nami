#
# NetSurf javascript source file inclusion
#
# Included by Makefile
#

S_JAVASCRIPT_BINDING:=

# Prefer NETSURF_JS_ENGINE when set. NETSURF_USE_DUKTAPE remains a shim.
ifeq ($(NETSURF_JS_ENGINE),)
  ifeq ($(NETSURF_USE_DUKTAPE),YES)
    NETSURF_JS_ENGINE := duktape
  else
    NETSURF_JS_ENGINE := none
  endif
endif

ifeq ($(NETSURF_JS_ENGINE),duktape)
# Duktape (compiled into the binary)
include content/handlers/javascript/duktape/Makefile
else ifeq ($(NETSURF_JS_ENGINE),quickjs_amiga)
# Amiga quickjs.library LVO client (nea-quickjs)
include content/handlers/javascript/quickjs_amiga/Makefile
else
# None
include content/handlers/javascript/none/Makefile
endif

# Fetcher for javascript scheme is always required
S_JAVASCRIPT += fetcher.c
