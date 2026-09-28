# Tsunami MUI frontend objects for vbcc OS3. Included by VMakefile.
# Core objects come from frontends/amiga/vobjects.mak (OBJS minus AMIGA_FE_OBJS).
# Shared Amiga image/font/plotter objects are re-linked after the FE filter.
# Menu/feature modules are forked under frontends/mui/ — never link Amiga GUI .o.

TSUNAMI_FE_OBJS = \
	build/vbcc-os3/frontends_mui_main.o \
	build/vbcc-os3/frontends_mui_gui.o \
	build/vbcc-os3/frontends_mui_browser.o \
	build/vbcc-os3/frontends_mui_throbber.o \
	build/vbcc-os3/frontends_mui_schedule.o \
	build/vbcc-os3/frontends_mui_filetype.o \
	build/vbcc-os3/frontends_mui_fetch.o \
	build/vbcc-os3/frontends_mui_theme.o \
	build/vbcc-os3/frontends_mui_libs.o \
	build/vbcc-os3/frontends_mui_misc.o \
	build/vbcc-os3/frontends_mui_version.o \
	build/vbcc-os3/frontends_mui_font_stubs.o \
	build/vbcc-os3/frontends_mui_ami_stubs.o \
	build/vbcc-os3/frontends_mui_corewindow.o \
	build/vbcc-os3/frontends_mui_file.o \
	build/vbcc-os3/frontends_mui_search.o \
	build/vbcc-os3/frontends_mui_hotlist.o \
	build/vbcc-os3/frontends_mui_history.o \
	build/vbcc-os3/frontends_mui_history_local.o \
	build/vbcc-os3/frontends_mui_cookies.o \
	build/vbcc-os3/frontends_mui_clipboard.o \
	build/vbcc-os3/frontends_mui_gui_options.o \
	build/vbcc-os3/frontends_mui_pageinfo.o \
	build/vbcc-os3/frontends_mui_iconv.o \
	build/vbcc-os3/frontends_mui_iconv_fallback.o \
	build/vbcc-os3/frontends_amiga_plotters.o \
	build/vbcc-os3/frontends_amiga_font.o \
	build/vbcc-os3/frontends_amiga_font_diskfont.o \
	build/vbcc-os3/frontends_amiga_font_ttengine.o \
	build/vbcc-os3/frontends_amiga_font_bullet.o \
	build/vbcc-os3/frontends_amiga_font_cache.o \
	build/vbcc-os3/frontends_amiga_utf8.o \
	build/vbcc-os3/frontends_amiga_bitmap.o \
	build/vbcc-os3/frontends_amiga_memory.o \
	build/vbcc-os3/frontends_amiga_object.o \
	build/vbcc-os3/frontends_amiga_filetype.o \
	build/vbcc-os3/frontends_amiga_rtg.o \
	build/vbcc-os3/frontends_amiga_datatypes.o \
	build/vbcc-os3/frontends_amiga_dt_picture.o \
	build/vbcc-os3/frontends_amiga_dt_sound.o

$(TSUNAMI_FE_OBJS): build/vbcc-os3/stamp

build/vbcc-os3/frontends_mui_main.o: frontends/mui/main.c
	vc $(VCFLAGS_TSUNAMI) -c -o build/vbcc-os3/frontends_mui_main.o frontends/mui/main.c

build/vbcc-os3/frontends_mui_gui.o: frontends/mui/gui.c
	vc $(VCFLAGS_TSUNAMI) -c -o build/vbcc-os3/frontends_mui_gui.o frontends/mui/gui.c

build/vbcc-os3/frontends_mui_browser.o: frontends/mui/browser.c
	vc $(VCFLAGS_TSUNAMI) -c -o build/vbcc-os3/frontends_mui_browser.o frontends/mui/browser.c

build/vbcc-os3/frontends_mui_throbber.o: frontends/mui/throbber.c
	vc $(VCFLAGS_TSUNAMI) -c -o build/vbcc-os3/frontends_mui_throbber.o frontends/mui/throbber.c

build/vbcc-os3/frontends_mui_schedule.o: frontends/mui/schedule.c
	vc $(VCFLAGS_TSUNAMI) -c -o build/vbcc-os3/frontends_mui_schedule.o frontends/mui/schedule.c

build/vbcc-os3/frontends_mui_filetype.o: frontends/mui/filetype.c
	vc $(VCFLAGS_TSUNAMI) -c -o build/vbcc-os3/frontends_mui_filetype.o frontends/mui/filetype.c

build/vbcc-os3/frontends_mui_fetch.o: frontends/mui/fetch.c
	vc $(VCFLAGS_TSUNAMI) -c -o build/vbcc-os3/frontends_mui_fetch.o frontends/mui/fetch.c

build/vbcc-os3/frontends_mui_theme.o: frontends/mui/theme.c
	vc $(VCFLAGS_TSUNAMI) -c -o build/vbcc-os3/frontends_mui_theme.o frontends/mui/theme.c

build/vbcc-os3/frontends_mui_libs.o: frontends/mui/libs.c
	vc $(VCFLAGS_TSUNAMI) -c -o build/vbcc-os3/frontends_mui_libs.o frontends/mui/libs.c

build/vbcc-os3/frontends_mui_misc.o: frontends/mui/misc.c
	vc $(VCFLAGS_TSUNAMI) -c -o build/vbcc-os3/frontends_mui_misc.o frontends/mui/misc.c

build/vbcc-os3/frontends_mui_version.o: frontends/mui/version.c
	vc $(VCFLAGS_TSUNAMI) -c -o build/vbcc-os3/frontends_mui_version.o frontends/mui/version.c

build/vbcc-os3/frontends_mui_font_stubs.o: frontends/mui/font_stubs.c
	vc $(VCFLAGS_TSUNAMI) -c -o build/vbcc-os3/frontends_mui_font_stubs.o frontends/mui/font_stubs.c

build/vbcc-os3/frontends_mui_ami_stubs.o: frontends/mui/ami_stubs.c
	vc $(VCFLAGS_TSUNAMI) -c -o build/vbcc-os3/frontends_mui_ami_stubs.o frontends/mui/ami_stubs.c

build/vbcc-os3/frontends_mui_corewindow.o: frontends/mui/corewindow.c
	vc $(VCFLAGS_TSUNAMI) -c -o build/vbcc-os3/frontends_mui_corewindow.o frontends/mui/corewindow.c

build/vbcc-os3/frontends_mui_file.o: frontends/mui/file.c
	vc $(VCFLAGS_TSUNAMI) -c -o build/vbcc-os3/frontends_mui_file.o frontends/mui/file.c

build/vbcc-os3/frontends_mui_search.o: frontends/mui/search.c
	vc $(VCFLAGS_TSUNAMI) -c -o build/vbcc-os3/frontends_mui_search.o frontends/mui/search.c

build/vbcc-os3/frontends_mui_hotlist.o: frontends/mui/hotlist.c
	vc $(VCFLAGS_TSUNAMI) -c -o build/vbcc-os3/frontends_mui_hotlist.o frontends/mui/hotlist.c

build/vbcc-os3/frontends_mui_history.o: frontends/mui/history.c
	vc $(VCFLAGS_TSUNAMI) -c -o build/vbcc-os3/frontends_mui_history.o frontends/mui/history.c

build/vbcc-os3/frontends_mui_history_local.o: frontends/mui/history_local.c
	vc $(VCFLAGS_TSUNAMI) -c -o build/vbcc-os3/frontends_mui_history_local.o frontends/mui/history_local.c

build/vbcc-os3/frontends_mui_cookies.o: frontends/mui/cookies.c
	vc $(VCFLAGS_TSUNAMI) -c -o build/vbcc-os3/frontends_mui_cookies.o frontends/mui/cookies.c

build/vbcc-os3/frontends_mui_clipboard.o: frontends/mui/clipboard.c
	vc $(VCFLAGS_TSUNAMI) -c -o build/vbcc-os3/frontends_mui_clipboard.o frontends/mui/clipboard.c

build/vbcc-os3/frontends_mui_gui_options.o: frontends/mui/gui_options.c
	vc $(VCFLAGS_TSUNAMI) -c -o build/vbcc-os3/frontends_mui_gui_options.o frontends/mui/gui_options.c

build/vbcc-os3/frontends_mui_pageinfo.o: frontends/mui/pageinfo.c
	vc $(VCFLAGS_TSUNAMI) -c -o build/vbcc-os3/frontends_mui_pageinfo.o frontends/mui/pageinfo.c

build/vbcc-os3/frontends_mui_iconv.o: frontends/mui/iconv.c
	vc $(VCFLAGS_TSUNAMI) -c -o build/vbcc-os3/frontends_mui_iconv.o frontends/mui/iconv.c

# Same AmigaOS3 lightweight converter, symbols renamed so mui/iconv.c can
# export the POSIX iconv_* entry points and dispatch to iconv.library first.
build/vbcc-os3/frontends_mui_iconv_fallback.o: frontends/amiga/iconv/iconv.c
	vc $(VCFLAGS_TSUNAMI) \
		-Diconv_open=tsunami_fb_iconv_open \
		-Diconv=tsunami_fb_iconv \
		-Diconv_close=tsunami_fb_iconv_close \
		-c -o build/vbcc-os3/frontends_mui_iconv_fallback.o \
		frontends/amiga/iconv/iconv.c
