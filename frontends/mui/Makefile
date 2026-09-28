# ----------------------------------------------------------------------------
# Tsunami MUI frontend setup
# ----------------------------------------------------------------------------

CFLAGS += -std=c99 -DnsTsunami -D__m68k__ \
	-DPATH_MAX=1024 -fomit-frame-pointer -m68020-60 -msoft-float

$(eval $(call feature_enabled,AMIGA_ICON,-DWITH_AMIGA_ICON,,Amiga icon))
$(eval $(call feature_enabled,AMIGA_DATATYPES,-DWITH_AMIGA_DATATYPES,,DataTypes))
$(eval $(call feature_enabled,AMIHTTP,-DWITH_AMIHTTP,,amihttp.library))
$(eval $(call feature_enabled,AMISSL,-DWITH_AMISSL -DWITH_OPENSSL -D__NO_NET_API -D__NO_NETINCLUDE_ERRNO -I$(GCCSDK_INSTALL_ENV)/netinclude,-lamisslauto,AmiSSL))

CFLAGS += -I$(GCCSDK_INSTALL_ENV)/include
CFLAGS += $(shell $(PKG_CONFIG) --cflags tre)

ifeq ($(NETSURF_USE_CURL),YES)
  LDFLAGS += $(shell $(PKG_CONFIG) --static --libs libcurl)
endif
ifeq ($(NETSURF_USE_AMIHTTP),YES)
  CFLAGS += -I../AmiHTTP/SDK/Include_H
endif

LDFLAGS += $(shell $(PKG_CONFIG) --libs tre)
LDFLAGS += -L$(GCCSDK_INSTALL_ENV)/lib
LDFLAGS += -lpbl -liconv -lamiga -lm

ifeq ($(NETSURF_USE_OPENSSL),YES)
  LDFLAGS += $(shell $(PKG_CONFIG) --static --libs openssl)
endif

COMMON_WARNFLAGS += -Wno-cast-align

# Resource search path compiled into the binary
TSUNAMI_RESOURCES_DIR := $(FRONTEND_RESOURCES_DIR)
CFLAGS += -DTSUNAMI_RESPATH=\"$(TSUNAMI_RESOURCES_DIR)\"

EXETARGET := Tsunami

MESSAGES_FILTER=tsu
MESSAGES_TARGET=$(TSUNAMI_RESOURCES_DIR)

# ----------------------------------------------------------------------------
# Source file setup
# ----------------------------------------------------------------------------

S_FRONTEND := main.c gui.c browser.c throbber.c schedule.c bitmap.c layout.c \
	filetype.c fetch.c plotters.c libs.c misc.c version.c \
	corewindow.c file.c search.c hotlist.c history.c history_local.c \
	cookies.c clipboard.c gui_options.c pageinfo.c

SOURCES = $(S_COMMON) $(S_IMAGE) $(S_BROWSER) $(S_FRONTEND)

# testament.h is generated into OBJROOT for the $VER string
frontends/mui/version.c: testament $(OBJROOT)/testament.h

# ----------------------------------------------------------------------------
# Install / package
# ----------------------------------------------------------------------------

install-mui:

package-mui:
