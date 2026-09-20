# Plain compile rules for vbcc OS3. Included by VMakefile.
OBJS = \
	build/vbcc-os3/content_content.o \
	build/vbcc-os3/content_content_factory.o \
	build/vbcc-os3/content_fetch.o \
	build/vbcc-os3/content_hlcache.o \
	build/vbcc-os3/content_llcache.o \
	build/vbcc-os3/content_mimesniff.o \
	build/vbcc-os3/content_textsearch.o \
	build/vbcc-os3/content_urldb.o \
	build/vbcc-os3/content_no_backing_store.o \
	build/vbcc-os3/content_fs_backing_store.o \
	build/vbcc-os3/content_handlers_css_css.o \
	build/vbcc-os3/content_handlers_css_dump.o \
	build/vbcc-os3/content_handlers_css_internal.o \
	build/vbcc-os3/content_handlers_css_hints.o \
	build/vbcc-os3/content_handlers_css_select.o \
	build/vbcc-os3/content_handlers_javascript_quickjs_amiga_js.o \
	build/vbcc-os3/content_handlers_javascript_quickjs_amiga_bind.o \
	build/vbcc-os3/content_handlers_javascript_quickjs_amiga_bind_node.o \
	build/vbcc-os3/content_handlers_javascript_quickjs_amiga_bind_list.o \
	build/vbcc-os3/content_handlers_javascript_quickjs_amiga_bind_win.o \
	build/vbcc-os3/content_handlers_javascript_quickjs_amiga_dom_sync.o \
	build/vbcc-os3/content_handlers_javascript_content.o \
	build/vbcc-os3/content_handlers_javascript_fetcher.o \
	build/vbcc-os3/qjs_bridge_quickjs_bridge.o \
	build/vbcc-os3/qjs_bridge_a6.o \
	build/vbcc-os3/qjs_bridge_dpvs.o \
	build/vbcc-os3/qjs_bridge_asm.o \
	build/vbcc-os3/qjs_bridge_asm_batch1.o \
	build/vbcc-os3/qjs_bridge_asm_batch2.o \
	build/vbcc-os3/qjs_bridge_asm_libc.o \
	build/vbcc-os3/content_handlers_html_box_construct.o \
	build/vbcc-os3/content_handlers_html_box_inspect.o \
	build/vbcc-os3/content_handlers_html_box_manipulate.o \
	build/vbcc-os3/content_handlers_html_box_normalise.o \
	build/vbcc-os3/content_handlers_html_box_special.o \
	build/vbcc-os3/content_handlers_html_box_textarea.o \
	build/vbcc-os3/content_handlers_html_css.o \
	build/vbcc-os3/content_handlers_html_css_fetcher.o \
	build/vbcc-os3/content_handlers_html_dom_event.o \
	build/vbcc-os3/content_handlers_html_font.o \
	build/vbcc-os3/content_handlers_html_form.o \
	build/vbcc-os3/content_handlers_html_forms.o \
	build/vbcc-os3/content_handlers_html_html.o \
	build/vbcc-os3/content_handlers_html_imagemap.o \
	build/vbcc-os3/content_handlers_html_interaction.o \
	build/vbcc-os3/content_handlers_html_layout.o \
	build/vbcc-os3/content_handlers_html_layout_flex.o \
	build/vbcc-os3/content_handlers_html_object.o \
	build/vbcc-os3/content_handlers_html_redraw.o \
	build/vbcc-os3/content_handlers_html_redraw_border.o \
	build/vbcc-os3/content_handlers_html_script.o \
	build/vbcc-os3/content_handlers_html_table.o \
	build/vbcc-os3/content_handlers_html_textselection.o \
	build/vbcc-os3/content_handlers_text_textplain.o \
	build/vbcc-os3/content_fetchers_data.o \
	build/vbcc-os3/content_fetchers_amihttp.o \
	build/vbcc-os3/content_fetchers_resource.o \
	build/vbcc-os3/content_fetchers_about_about.o \
	build/vbcc-os3/content_fetchers_about_blank.o \
	build/vbcc-os3/content_fetchers_about_certificate.o \
	build/vbcc-os3/content_fetchers_about_chart.o \
	build/vbcc-os3/content_fetchers_about_choices.o \
	build/vbcc-os3/content_fetchers_about_config.o \
	build/vbcc-os3/content_fetchers_about_imagecache.o \
	build/vbcc-os3/content_fetchers_about_nscolours.o \
	build/vbcc-os3/content_fetchers_about_query.o \
	build/vbcc-os3/content_fetchers_about_query_auth.o \
	build/vbcc-os3/content_fetchers_about_query_fetcherror.o \
	build/vbcc-os3/content_fetchers_about_query_privacy.o \
	build/vbcc-os3/content_fetchers_about_query_timeout.o \
	build/vbcc-os3/content_fetchers_about_testament.o \
	build/vbcc-os3/content_fetchers_about_websearch.o \
	build/vbcc-os3/content_fetchers_file_dirlist.o \
	build/vbcc-os3/content_fetchers_file_file.o \
	build/vbcc-os3/utils_bloom.o \
	build/vbcc-os3/utils_corestrings.o \
	build/vbcc-os3/utils_file.o \
	build/vbcc-os3/utils_filepath.o \
	build/vbcc-os3/utils_hashmap.o \
	build/vbcc-os3/utils_hashtable.o \
	build/vbcc-os3/utils_idna.o \
	build/vbcc-os3/utils_libdom.o \
	build/vbcc-os3/utils_log.o \
	build/vbcc-os3/utils_messages.o \
	build/vbcc-os3/utils_nscolour.o \
	build/vbcc-os3/utils_nsoption.o \
	build/vbcc-os3/utils_punycode.o \
	build/vbcc-os3/utils_ssl_certs.o \
	build/vbcc-os3/utils_talloc.o \
	build/vbcc-os3/utils_time.o \
	build/vbcc-os3/utils_url.o \
	build/vbcc-os3/utils_useragent.o \
	build/vbcc-os3/utils_utf8.o \
	build/vbcc-os3/utils_utils.o \
	build/vbcc-os3/utils_http_challenge.o \
	build/vbcc-os3/utils_http_generics.o \
	build/vbcc-os3/utils_http_primitives.o \
	build/vbcc-os3/utils_http_parameter.o \
	build/vbcc-os3/utils_http_cache-control.o \
	build/vbcc-os3/utils_http_content-disposition.o \
	build/vbcc-os3/utils_http_content-type.o \
	build/vbcc-os3/utils_http_strict-transport-security.o \
	build/vbcc-os3/utils_http_www-authenticate.o \
	build/vbcc-os3/utils_nsurl_nsurl.o \
	build/vbcc-os3/utils_nsurl_parse.o \
	build/vbcc-os3/desktop_cookie_manager.o \
	build/vbcc-os3/desktop_knockout.o \
	build/vbcc-os3/desktop_hotlist.o \
	build/vbcc-os3/desktop_mouse.o \
	build/vbcc-os3/desktop_plot_style.o \
	build/vbcc-os3/desktop_print.o \
	build/vbcc-os3/desktop_search.o \
	build/vbcc-os3/desktop_searchweb.o \
	build/vbcc-os3/desktop_scrollbar.o \
	build/vbcc-os3/desktop_textarea.o \
	build/vbcc-os3/desktop_version.o \
	build/vbcc-os3/desktop_system_colour.o \
	build/vbcc-os3/desktop_local_history.o \
	build/vbcc-os3/desktop_global_history.o \
	build/vbcc-os3/desktop_treeview.o \
	build/vbcc-os3/desktop_page-info.o \
	build/vbcc-os3/content_handlers_image_image.o \
	build/vbcc-os3/content_handlers_image_image_cache.o \
	build/vbcc-os3/desktop_bitmap.o \
	build/vbcc-os3/desktop_browser.o \
	build/vbcc-os3/desktop_browser_window.o \
	build/vbcc-os3/desktop_browser_history.o \
	build/vbcc-os3/desktop_download.o \
	build/vbcc-os3/desktop_frames.o \
	build/vbcc-os3/desktop_netsurf.o \
	build/vbcc-os3/desktop_cw_helper.o \
	build/vbcc-os3/desktop_save_complete.o \
	build/vbcc-os3/desktop_save_text.o \
	build/vbcc-os3/desktop_selection.o \
	build/vbcc-os3/desktop_textinput.o \
	build/vbcc-os3/desktop_gui_factory.o \
	build/vbcc-os3/desktop_save_pdf.o \
	build/vbcc-os3/desktop_font_haru.o \
	build/vbcc-os3/frontends_amiga_gui.o \
	build/vbcc-os3/frontends_amiga_history.o \
	build/vbcc-os3/frontends_amiga_hotlist.o \
	build/vbcc-os3/frontends_amiga_schedule.o \
	build/vbcc-os3/frontends_amiga_file.o \
	build/vbcc-os3/frontends_amiga_misc.o \
	build/vbcc-os3/frontends_amiga_bitmap.o \
	build/vbcc-os3/frontends_amiga_font.o \
	build/vbcc-os3/frontends_amiga_filetype.o \
	build/vbcc-os3/frontends_amiga_utf8.o \
	build/vbcc-os3/frontends_amiga_iconv_iconv.o \
	build/vbcc-os3/frontends_amiga_memory.o \
	build/vbcc-os3/frontends_amiga_plotters.o \
	build/vbcc-os3/frontends_amiga_object.o \
	build/vbcc-os3/frontends_amiga_menu.o \
	build/vbcc-os3/frontends_amiga_save_pdf.o \
	build/vbcc-os3/frontends_amiga_arexx.o \
	build/vbcc-os3/frontends_amiga_version.o \
	build/vbcc-os3/frontends_amiga_cookies.o \
	build/vbcc-os3/frontends_amiga_ctxmenu.o \
	build/vbcc-os3/frontends_amiga_clipboard.o \
	build/vbcc-os3/frontends_amiga_help.o \
	build/vbcc-os3/frontends_amiga_font_scan.o \
	build/vbcc-os3/frontends_amiga_launch.o \
	build/vbcc-os3/frontends_amiga_search.o \
	build/vbcc-os3/frontends_amiga_history_local.o \
	build/vbcc-os3/frontends_amiga_download.o \
	build/vbcc-os3/frontends_amiga_iff_dr2d.o \
	build/vbcc-os3/frontends_amiga_gui_options.o \
	build/vbcc-os3/frontends_amiga_print.o \
	build/vbcc-os3/frontends_amiga_theme.o \
	build/vbcc-os3/frontends_amiga_drag.o \
	build/vbcc-os3/frontends_amiga_icon.o \
	build/vbcc-os3/frontends_amiga_ico.o \
	build/vbcc-os3/frontends_amiga_libs.o \
	build/vbcc-os3/frontends_amiga_datatypes.o \
	build/vbcc-os3/frontends_amiga_dt_picture.o \
	build/vbcc-os3/frontends_amiga_dt_anim.o \
	build/vbcc-os3/frontends_amiga_dt_sound.o \
	build/vbcc-os3/frontends_amiga_plugin_hack.o \
	build/vbcc-os3/frontends_amiga_stringview_stringview.o \
	build/vbcc-os3/frontends_amiga_stringview_urlhistory.o \
	build/vbcc-os3/frontends_amiga_rtg.o \
	build/vbcc-os3/frontends_amiga_agclass_amigaguide_class.o \
	build/vbcc-os3/frontends_amiga_os3support.o \
	build/vbcc-os3/frontends_amiga_font_diskfont.o \
	build/vbcc-os3/frontends_amiga_font_ttengine.o \
	build/vbcc-os3/frontends_amiga_selectmenu.o \
	build/vbcc-os3/frontends_amiga_hash_xxhash.o \
	build/vbcc-os3/frontends_amiga_font_cache.o \
	build/vbcc-os3/frontends_amiga_font_bullet.o \
	build/vbcc-os3/frontends_amiga_nsoption.o \
	build/vbcc-os3/frontends_amiga_corewindow.o \
	build/vbcc-os3/frontends_amiga_gui_menu.o \
	build/vbcc-os3/frontends_amiga_pageinfo.o

build/vbcc-os3/content_content.o: content/content.c
	vc $(VCFLAGS) -c -o build/vbcc-os3/content_content.o content/content.c

build/vbcc-os3/content_content_factory.o: content/content_factory.c
	vc $(VCFLAGS) -c -o build/vbcc-os3/content_content_factory.o content/content_factory.c

build/vbcc-os3/content_fetch.o: content/fetch.c
	vc $(VCFLAGS) -c -o build/vbcc-os3/content_fetch.o content/fetch.c

build/vbcc-os3/content_hlcache.o: content/hlcache.c
	vc $(VCFLAGS) -c -o build/vbcc-os3/content_hlcache.o content/hlcache.c

build/vbcc-os3/content_llcache.o: content/llcache.c
	vc $(VCFLAGS) -c -o build/vbcc-os3/content_llcache.o content/llcache.c

build/vbcc-os3/content_mimesniff.o: content/mimesniff.c
	vc $(VCFLAGS) -c -o build/vbcc-os3/content_mimesniff.o content/mimesniff.c

build/vbcc-os3/content_textsearch.o: content/textsearch.c
	vc $(VCFLAGS) -c -o build/vbcc-os3/content_textsearch.o content/textsearch.c

build/vbcc-os3/content_urldb.o: content/urldb.c
	vc $(VCFLAGS) -c -o build/vbcc-os3/content_urldb.o content/urldb.c

build/vbcc-os3/content_no_backing_store.o: content/no_backing_store.c
	vc $(VCFLAGS) -c -o build/vbcc-os3/content_no_backing_store.o content/no_backing_store.c

build/vbcc-os3/content_fs_backing_store.o: content/fs_backing_store.c
	vc $(VCFLAGS) -c -o build/vbcc-os3/content_fs_backing_store.o content/fs_backing_store.c

build/vbcc-os3/content_handlers_css_css.o: content/handlers/css/css.c
	vc $(VCFLAGS) -c -o build/vbcc-os3/content_handlers_css_css.o content/handlers/css/css.c

build/vbcc-os3/content_handlers_css_dump.o: content/handlers/css/dump.c
	vc $(VCFLAGS) -c -o build/vbcc-os3/content_handlers_css_dump.o content/handlers/css/dump.c

build/vbcc-os3/content_handlers_css_internal.o: content/handlers/css/internal.c
	vc $(VCFLAGS) -c -o build/vbcc-os3/content_handlers_css_internal.o content/handlers/css/internal.c

build/vbcc-os3/content_handlers_css_hints.o: content/handlers/css/hints.c
	vc $(VCFLAGS) -c -o build/vbcc-os3/content_handlers_css_hints.o content/handlers/css/hints.c

build/vbcc-os3/content_handlers_css_select.o: content/handlers/css/select.c
	vc $(VCFLAGS) -c -o build/vbcc-os3/content_handlers_css_select.o content/handlers/css/select.c

QJS_PRIV = content/handlers/javascript/quickjs_amiga/private.h \
	content/handlers/javascript/quickjs_amiga/bind_priv.h \
	content/handlers/javascript/quickjs_amiga/bind.h

build/vbcc-os3/content_handlers_javascript_quickjs_amiga_js.o: content/handlers/javascript/quickjs_amiga/js.c $(QJS_PRIV)
	vc $(VCFLAGS) $(QJSCFLAGS) -c -o build/vbcc-os3/content_handlers_javascript_quickjs_amiga_js.o content/handlers/javascript/quickjs_amiga/js.c

build/vbcc-os3/content_handlers_javascript_quickjs_amiga_bind.o: content/handlers/javascript/quickjs_amiga/bind.c $(QJS_PRIV)
	vc $(VCFLAGS) $(QJSCFLAGS) -c -o build/vbcc-os3/content_handlers_javascript_quickjs_amiga_bind.o content/handlers/javascript/quickjs_amiga/bind.c

build/vbcc-os3/content_handlers_javascript_quickjs_amiga_bind_node.o: content/handlers/javascript/quickjs_amiga/bind_node.c $(QJS_PRIV)
	vc $(VCFLAGS) $(QJSCFLAGS) -c -o build/vbcc-os3/content_handlers_javascript_quickjs_amiga_bind_node.o content/handlers/javascript/quickjs_amiga/bind_node.c

build/vbcc-os3/content_handlers_javascript_quickjs_amiga_bind_list.o: content/handlers/javascript/quickjs_amiga/bind_list.c $(QJS_PRIV)
	vc $(VCFLAGS) $(QJSCFLAGS) -c -o build/vbcc-os3/content_handlers_javascript_quickjs_amiga_bind_list.o content/handlers/javascript/quickjs_amiga/bind_list.c

build/vbcc-os3/content_handlers_javascript_quickjs_amiga_bind_win.o: content/handlers/javascript/quickjs_amiga/bind_win.c $(QJS_PRIV)
	vc $(VCFLAGS) $(QJSCFLAGS) -c -o build/vbcc-os3/content_handlers_javascript_quickjs_amiga_bind_win.o content/handlers/javascript/quickjs_amiga/bind_win.c

build/vbcc-os3/content_handlers_javascript_quickjs_amiga_dom_sync.o: content/handlers/javascript/quickjs_amiga/dom_sync.c $(QJS_PRIV)
	vc $(VCFLAGS) $(QJSCFLAGS) -c -o build/vbcc-os3/content_handlers_javascript_quickjs_amiga_dom_sync.o content/handlers/javascript/quickjs_amiga/dom_sync.c

build/vbcc-os3/content_handlers_javascript_content.o: content/handlers/javascript/content.c
	vc $(VCFLAGS) -c -o build/vbcc-os3/content_handlers_javascript_content.o content/handlers/javascript/content.c

build/vbcc-os3/content_handlers_javascript_fetcher.o: content/handlers/javascript/fetcher.c
	vc $(VCFLAGS) -c -o build/vbcc-os3/content_handlers_javascript_fetcher.o content/handlers/javascript/fetcher.c

# nea-quickjs LVO client bridge (engine stays in LIBS:quickjs.library)
build/vbcc-os3/qjs_bridge_quickjs_bridge.o: $(QJSVBCC)/quickjs_bridge.c
	vc $(VCFLAGS) $(QJSCFLAGS) -c -o build/vbcc-os3/qjs_bridge_quickjs_bridge.o $(QJSVBCC)/quickjs_bridge.c

build/vbcc-os3/qjs_bridge_a6.o: $(QJSVBCC)/bridge_a6.s
	$(VASM) -Fhunk -nowarn=62 -m68020 -o build/vbcc-os3/qjs_bridge_a6.o $(QJSVBCC)/bridge_a6.s

build/vbcc-os3/qjs_bridge_dpvs.o: $(QJSVBCC)/bridge_dpvs.s
	$(VASM) -Fhunk -nowarn=62 -m68020 -o build/vbcc-os3/qjs_bridge_dpvs.o $(QJSVBCC)/bridge_dpvs.s

build/vbcc-os3/qjs_bridge_asm.o: $(QJSVBCC)/bridge_asm.s
	$(VASM) -Fhunk -nowarn=62 -m68020 -o build/vbcc-os3/qjs_bridge_asm.o $(QJSVBCC)/bridge_asm.s

build/vbcc-os3/qjs_bridge_asm_batch1.o: $(QJSVBCC)/bridge_asm_batch1.s
	$(VASM) -Fhunk -nowarn=62 -m68020 -o build/vbcc-os3/qjs_bridge_asm_batch1.o $(QJSVBCC)/bridge_asm_batch1.s

build/vbcc-os3/qjs_bridge_asm_batch2.o: $(QJSVBCC)/bridge_asm_batch2.s
	$(VASM) -Fhunk -nowarn=62 -m68020 -o build/vbcc-os3/qjs_bridge_asm_batch2.o $(QJSVBCC)/bridge_asm_batch2.s

build/vbcc-os3/qjs_bridge_asm_libc.o: $(QJSVBCC)/bridge_asm_libc.s
	$(VASM) -Fhunk -nowarn=62 -m68020 -o build/vbcc-os3/qjs_bridge_asm_libc.o $(QJSVBCC)/bridge_asm_libc.s

build/vbcc-os3/content_handlers_html_box_construct.o: content/handlers/html/box_construct.c content/handlers/html/private.h
	vc $(VCFLAGS) -c -o build/vbcc-os3/content_handlers_html_box_construct.o content/handlers/html/box_construct.c

build/vbcc-os3/content_handlers_html_box_inspect.o: content/handlers/html/box_inspect.c content/handlers/html/private.h
	vc $(VCFLAGS) -c -o build/vbcc-os3/content_handlers_html_box_inspect.o content/handlers/html/box_inspect.c

build/vbcc-os3/content_handlers_html_box_manipulate.o: content/handlers/html/box_manipulate.c content/handlers/html/private.h
	vc $(VCFLAGS) -c -o build/vbcc-os3/content_handlers_html_box_manipulate.o content/handlers/html/box_manipulate.c

build/vbcc-os3/content_handlers_html_box_normalise.o: content/handlers/html/box_normalise.c content/handlers/html/private.h
	vc $(VCFLAGS) -c -o build/vbcc-os3/content_handlers_html_box_normalise.o content/handlers/html/box_normalise.c

build/vbcc-os3/content_handlers_html_box_special.o: content/handlers/html/box_special.c content/handlers/html/private.h
	vc $(VCFLAGS) -c -o build/vbcc-os3/content_handlers_html_box_special.o content/handlers/html/box_special.c

build/vbcc-os3/content_handlers_html_box_textarea.o: content/handlers/html/box_textarea.c content/handlers/html/private.h
	vc $(VCFLAGS) -c -o build/vbcc-os3/content_handlers_html_box_textarea.o content/handlers/html/box_textarea.c

build/vbcc-os3/content_handlers_html_css.o: content/handlers/html/css.c content/handlers/html/private.h content/handlers/html/html.h
	vc $(VCFLAGS) -c -o build/vbcc-os3/content_handlers_html_css.o content/handlers/html/css.c

build/vbcc-os3/content_handlers_html_css_fetcher.o: content/handlers/html/css_fetcher.c content/handlers/html/private.h
	vc $(VCFLAGS) -c -o build/vbcc-os3/content_handlers_html_css_fetcher.o content/handlers/html/css_fetcher.c

build/vbcc-os3/content_handlers_html_dom_event.o: content/handlers/html/dom_event.c content/handlers/html/private.h
	vc $(VCFLAGS) -c -o build/vbcc-os3/content_handlers_html_dom_event.o content/handlers/html/dom_event.c

build/vbcc-os3/content_handlers_html_font.o: content/handlers/html/font.c content/handlers/html/private.h
	vc $(VCFLAGS) -c -o build/vbcc-os3/content_handlers_html_font.o content/handlers/html/font.c

build/vbcc-os3/content_handlers_html_form.o: content/handlers/html/form.c content/handlers/html/private.h
	vc $(VCFLAGS) -c -o build/vbcc-os3/content_handlers_html_form.o content/handlers/html/form.c

build/vbcc-os3/content_handlers_html_forms.o: content/handlers/html/forms.c content/handlers/html/private.h
	vc $(VCFLAGS) -c -o build/vbcc-os3/content_handlers_html_forms.o content/handlers/html/forms.c

build/vbcc-os3/content_handlers_html_html.o: content/handlers/html/html.c content/handlers/html/private.h content/handlers/html/html.h
	vc $(VCFLAGS) -c -o build/vbcc-os3/content_handlers_html_html.o content/handlers/html/html.c

build/vbcc-os3/content_handlers_html_imagemap.o: content/handlers/html/imagemap.c content/handlers/html/private.h
	vc $(VCFLAGS) -c -o build/vbcc-os3/content_handlers_html_imagemap.o content/handlers/html/imagemap.c

build/vbcc-os3/content_handlers_html_interaction.o: content/handlers/html/interaction.c content/handlers/html/private.h
	vc $(VCFLAGS) -c -o build/vbcc-os3/content_handlers_html_interaction.o content/handlers/html/interaction.c

build/vbcc-os3/content_handlers_html_layout.o: content/handlers/html/layout.c content/handlers/html/private.h
	vc $(VCFLAGS) -c -o build/vbcc-os3/content_handlers_html_layout.o content/handlers/html/layout.c

build/vbcc-os3/content_handlers_html_layout_flex.o: content/handlers/html/layout_flex.c content/handlers/html/private.h
	vc $(VCFLAGS) -c -o build/vbcc-os3/content_handlers_html_layout_flex.o content/handlers/html/layout_flex.c

build/vbcc-os3/content_handlers_html_object.o: content/handlers/html/object.c content/handlers/html/private.h
	vc $(VCFLAGS) -c -o build/vbcc-os3/content_handlers_html_object.o content/handlers/html/object.c

build/vbcc-os3/content_handlers_html_redraw.o: content/handlers/html/redraw.c content/handlers/html/private.h
	vc $(VCFLAGS) -c -o build/vbcc-os3/content_handlers_html_redraw.o content/handlers/html/redraw.c

build/vbcc-os3/content_handlers_html_redraw_border.o: content/handlers/html/redraw_border.c content/handlers/html/private.h
	vc $(VCFLAGS) -c -o build/vbcc-os3/content_handlers_html_redraw_border.o content/handlers/html/redraw_border.c

build/vbcc-os3/content_handlers_html_script.o: content/handlers/html/script.c content/handlers/html/private.h
	vc $(VCFLAGS) -c -o build/vbcc-os3/content_handlers_html_script.o content/handlers/html/script.c

build/vbcc-os3/content_handlers_html_table.o: content/handlers/html/table.c content/handlers/html/private.h
	vc $(VCFLAGS) -c -o build/vbcc-os3/content_handlers_html_table.o content/handlers/html/table.c

build/vbcc-os3/content_handlers_html_textselection.o: content/handlers/html/textselection.c content/handlers/html/private.h
	vc $(VCFLAGS) -c -o build/vbcc-os3/content_handlers_html_textselection.o content/handlers/html/textselection.c

build/vbcc-os3/content_handlers_text_textplain.o: content/handlers/text/textplain.c
	vc $(VCFLAGS) -c -o build/vbcc-os3/content_handlers_text_textplain.o content/handlers/text/textplain.c

build/vbcc-os3/content_fetchers_data.o: content/fetchers/data.c
	vc $(VCFLAGS) -c -o build/vbcc-os3/content_fetchers_data.o content/fetchers/data.c

build/vbcc-os3/content_fetchers_amihttp.o: content/fetchers/amihttp.c
	vc $(VCFLAGS) -c -o build/vbcc-os3/content_fetchers_amihttp.o content/fetchers/amihttp.c

build/vbcc-os3/content_fetchers_resource.o: content/fetchers/resource.c
	vc $(VCFLAGS) -c -o build/vbcc-os3/content_fetchers_resource.o content/fetchers/resource.c

build/vbcc-os3/content_fetchers_about_about.o: content/fetchers/about/about.c
	vc $(VCFLAGS) -c -o build/vbcc-os3/content_fetchers_about_about.o content/fetchers/about/about.c

build/vbcc-os3/content_fetchers_about_blank.o: content/fetchers/about/blank.c
	vc $(VCFLAGS) -c -o build/vbcc-os3/content_fetchers_about_blank.o content/fetchers/about/blank.c

build/vbcc-os3/content_fetchers_about_certificate.o: content/fetchers/about/certificate.c
	vc $(VCFLAGS) -c -o build/vbcc-os3/content_fetchers_about_certificate.o content/fetchers/about/certificate.c

build/vbcc-os3/content_fetchers_about_chart.o: content/fetchers/about/chart.c
	vc $(VCFLAGS) -c -o build/vbcc-os3/content_fetchers_about_chart.o content/fetchers/about/chart.c

build/vbcc-os3/content_fetchers_about_choices.o: content/fetchers/about/choices.c
	vc $(VCFLAGS) -c -o build/vbcc-os3/content_fetchers_about_choices.o content/fetchers/about/choices.c

build/vbcc-os3/content_fetchers_about_config.o: content/fetchers/about/config.c
	vc $(VCFLAGS) -c -o build/vbcc-os3/content_fetchers_about_config.o content/fetchers/about/config.c

build/vbcc-os3/content_fetchers_about_imagecache.o: content/fetchers/about/imagecache.c
	vc $(VCFLAGS) -c -o build/vbcc-os3/content_fetchers_about_imagecache.o content/fetchers/about/imagecache.c

build/vbcc-os3/content_fetchers_about_nscolours.o: content/fetchers/about/nscolours.c
	vc $(VCFLAGS) -c -o build/vbcc-os3/content_fetchers_about_nscolours.o content/fetchers/about/nscolours.c

build/vbcc-os3/content_fetchers_about_query.o: content/fetchers/about/query.c
	vc $(VCFLAGS) -c -o build/vbcc-os3/content_fetchers_about_query.o content/fetchers/about/query.c

build/vbcc-os3/content_fetchers_about_query_auth.o: content/fetchers/about/query_auth.c
	vc $(VCFLAGS) -c -o build/vbcc-os3/content_fetchers_about_query_auth.o content/fetchers/about/query_auth.c

build/vbcc-os3/content_fetchers_about_query_fetcherror.o: content/fetchers/about/query_fetcherror.c
	vc $(VCFLAGS) -c -o build/vbcc-os3/content_fetchers_about_query_fetcherror.o content/fetchers/about/query_fetcherror.c

build/vbcc-os3/content_fetchers_about_query_privacy.o: content/fetchers/about/query_privacy.c
	vc $(VCFLAGS) -c -o build/vbcc-os3/content_fetchers_about_query_privacy.o content/fetchers/about/query_privacy.c

build/vbcc-os3/content_fetchers_about_query_timeout.o: content/fetchers/about/query_timeout.c
	vc $(VCFLAGS) -c -o build/vbcc-os3/content_fetchers_about_query_timeout.o content/fetchers/about/query_timeout.c

build/vbcc-os3/content_fetchers_about_testament.o: content/fetchers/about/testament.c
	vc $(VCFLAGS) -c -o build/vbcc-os3/content_fetchers_about_testament.o content/fetchers/about/testament.c

build/vbcc-os3/content_fetchers_about_websearch.o: content/fetchers/about/websearch.c
	vc $(VCFLAGS) -c -o build/vbcc-os3/content_fetchers_about_websearch.o content/fetchers/about/websearch.c

build/vbcc-os3/content_fetchers_file_dirlist.o: content/fetchers/file/dirlist.c
	vc $(VCFLAGS) -c -o build/vbcc-os3/content_fetchers_file_dirlist.o content/fetchers/file/dirlist.c

build/vbcc-os3/content_fetchers_file_file.o: content/fetchers/file/file.c
	vc $(VCFLAGS) -c -o build/vbcc-os3/content_fetchers_file_file.o content/fetchers/file/file.c

build/vbcc-os3/utils_bloom.o: utils/bloom.c
	vc $(VCFLAGS) -c -o build/vbcc-os3/utils_bloom.o utils/bloom.c

build/vbcc-os3/utils_corestrings.o: utils/corestrings.c
	vc $(VCFLAGS) -c -o build/vbcc-os3/utils_corestrings.o utils/corestrings.c

build/vbcc-os3/utils_file.o: utils/file.c
	vc $(VCFLAGS) -c -o build/vbcc-os3/utils_file.o utils/file.c

build/vbcc-os3/utils_filepath.o: utils/filepath.c
	vc $(VCFLAGS) -c -o build/vbcc-os3/utils_filepath.o utils/filepath.c

build/vbcc-os3/utils_hashmap.o: utils/hashmap.c
	vc $(VCFLAGS) -c -o build/vbcc-os3/utils_hashmap.o utils/hashmap.c

build/vbcc-os3/utils_hashtable.o: utils/hashtable.c
	vc $(VCFLAGS) -c -o build/vbcc-os3/utils_hashtable.o utils/hashtable.c

build/vbcc-os3/utils_idna.o: utils/idna.c
	vc $(VCFLAGS) -c -o build/vbcc-os3/utils_idna.o utils/idna.c

build/vbcc-os3/utils_libdom.o: utils/libdom.c
	vc $(VCFLAGS) -c -o build/vbcc-os3/utils_libdom.o utils/libdom.c

build/vbcc-os3/utils_log.o: utils/log.c
	vc $(VCFLAGS) -c -o build/vbcc-os3/utils_log.o utils/log.c

build/vbcc-os3/utils_messages.o: utils/messages.c
	vc $(VCFLAGS) -c -o build/vbcc-os3/utils_messages.o utils/messages.c

build/vbcc-os3/utils_nscolour.o: utils/nscolour.c
	vc $(VCFLAGS) -c -o build/vbcc-os3/utils_nscolour.o utils/nscolour.c

build/vbcc-os3/utils_nsoption.o: utils/nsoption.c
	vc $(VCFLAGS) -c -o build/vbcc-os3/utils_nsoption.o utils/nsoption.c

build/vbcc-os3/utils_punycode.o: utils/punycode.c
	vc $(VCFLAGS) -c -o build/vbcc-os3/utils_punycode.o utils/punycode.c

build/vbcc-os3/utils_ssl_certs.o: utils/ssl_certs.c
	vc $(VCFLAGS) -c -o build/vbcc-os3/utils_ssl_certs.o utils/ssl_certs.c

build/vbcc-os3/utils_talloc.o: utils/talloc.c
	vc $(VCFLAGS) -c -o build/vbcc-os3/utils_talloc.o utils/talloc.c

build/vbcc-os3/utils_time.o: utils/time.c
	vc $(VCFLAGS) -c -o build/vbcc-os3/utils_time.o utils/time.c

build/vbcc-os3/utils_url.o: utils/url.c
	vc $(VCFLAGS) -c -o build/vbcc-os3/utils_url.o utils/url.c

build/vbcc-os3/utils_useragent.o: utils/useragent.c
	vc $(VCFLAGS) -c -o build/vbcc-os3/utils_useragent.o utils/useragent.c

build/vbcc-os3/utils_utf8.o: utils/utf8.c
	vc $(VCFLAGS) -c -o build/vbcc-os3/utils_utf8.o utils/utf8.c

build/vbcc-os3/utils_utils.o: utils/utils.c
	vc $(VCFLAGS) -c -o build/vbcc-os3/utils_utils.o utils/utils.c

build/vbcc-os3/utils_http_challenge.o: utils/http/challenge.c
	vc $(VCFLAGS) -c -o build/vbcc-os3/utils_http_challenge.o utils/http/challenge.c

build/vbcc-os3/utils_http_generics.o: utils/http/generics.c
	vc $(VCFLAGS) -c -o build/vbcc-os3/utils_http_generics.o utils/http/generics.c

build/vbcc-os3/utils_http_primitives.o: utils/http/primitives.c
	vc $(VCFLAGS) -c -o build/vbcc-os3/utils_http_primitives.o utils/http/primitives.c

build/vbcc-os3/utils_http_parameter.o: utils/http/parameter.c
	vc $(VCFLAGS) -c -o build/vbcc-os3/utils_http_parameter.o utils/http/parameter.c

build/vbcc-os3/utils_http_cache-control.o: utils/http/cache-control.c
	vc $(VCFLAGS) -c -o build/vbcc-os3/utils_http_cache-control.o utils/http/cache-control.c

build/vbcc-os3/utils_http_content-disposition.o: utils/http/content-disposition.c
	vc $(VCFLAGS) -c -o build/vbcc-os3/utils_http_content-disposition.o utils/http/content-disposition.c

build/vbcc-os3/utils_http_content-type.o: utils/http/content-type.c
	vc $(VCFLAGS) -c -o build/vbcc-os3/utils_http_content-type.o utils/http/content-type.c

build/vbcc-os3/utils_http_strict-transport-security.o: utils/http/strict-transport-security.c
	vc $(VCFLAGS) -c -o build/vbcc-os3/utils_http_strict-transport-security.o utils/http/strict-transport-security.c

build/vbcc-os3/utils_http_www-authenticate.o: utils/http/www-authenticate.c
	vc $(VCFLAGS) -c -o build/vbcc-os3/utils_http_www-authenticate.o utils/http/www-authenticate.c

build/vbcc-os3/utils_nsurl_nsurl.o: utils/nsurl/nsurl.c
	vc $(VCFLAGS) -c -o build/vbcc-os3/utils_nsurl_nsurl.o utils/nsurl/nsurl.c

build/vbcc-os3/utils_nsurl_parse.o: utils/nsurl/parse.c
	vc $(VCFLAGS) -c -o build/vbcc-os3/utils_nsurl_parse.o utils/nsurl/parse.c

build/vbcc-os3/desktop_cookie_manager.o: desktop/cookie_manager.c
	vc $(VCFLAGS) -c -o build/vbcc-os3/desktop_cookie_manager.o desktop/cookie_manager.c

build/vbcc-os3/desktop_knockout.o: desktop/knockout.c
	vc $(VCFLAGS) -c -o build/vbcc-os3/desktop_knockout.o desktop/knockout.c

build/vbcc-os3/desktop_hotlist.o: desktop/hotlist.c
	vc $(VCFLAGS) -c -o build/vbcc-os3/desktop_hotlist.o desktop/hotlist.c

build/vbcc-os3/desktop_mouse.o: desktop/mouse.c
	vc $(VCFLAGS) -c -o build/vbcc-os3/desktop_mouse.o desktop/mouse.c

build/vbcc-os3/desktop_plot_style.o: desktop/plot_style.c
	vc $(VCFLAGS) -c -o build/vbcc-os3/desktop_plot_style.o desktop/plot_style.c

build/vbcc-os3/desktop_print.o: desktop/print.c
	vc $(VCFLAGS) -c -o build/vbcc-os3/desktop_print.o desktop/print.c

build/vbcc-os3/desktop_search.o: desktop/search.c
	vc $(VCFLAGS) -c -o build/vbcc-os3/desktop_search.o desktop/search.c

build/vbcc-os3/desktop_searchweb.o: desktop/searchweb.c
	vc $(VCFLAGS) -c -o build/vbcc-os3/desktop_searchweb.o desktop/searchweb.c

build/vbcc-os3/desktop_scrollbar.o: desktop/scrollbar.c
	vc $(VCFLAGS) -c -o build/vbcc-os3/desktop_scrollbar.o desktop/scrollbar.c

build/vbcc-os3/desktop_textarea.o: desktop/textarea.c
	vc $(VCFLAGS) -c -o build/vbcc-os3/desktop_textarea.o desktop/textarea.c

build/vbcc-os3/desktop_version.o: desktop/version.c
	vc $(VCFLAGS) -c -o build/vbcc-os3/desktop_version.o desktop/version.c

build/vbcc-os3/desktop_system_colour.o: desktop/system_colour.c
	vc $(VCFLAGS) -c -o build/vbcc-os3/desktop_system_colour.o desktop/system_colour.c

build/vbcc-os3/desktop_local_history.o: desktop/local_history.c
	vc $(VCFLAGS) -c -o build/vbcc-os3/desktop_local_history.o desktop/local_history.c

build/vbcc-os3/desktop_global_history.o: desktop/global_history.c
	vc $(VCFLAGS) -c -o build/vbcc-os3/desktop_global_history.o desktop/global_history.c

build/vbcc-os3/desktop_treeview.o: desktop/treeview.c
	vc $(VCFLAGS) -c -o build/vbcc-os3/desktop_treeview.o desktop/treeview.c

build/vbcc-os3/desktop_page-info.o: desktop/page-info.c
	vc $(VCFLAGS) -c -o build/vbcc-os3/desktop_page-info.o desktop/page-info.c

build/vbcc-os3/content_handlers_image_image.o: content/handlers/image/image.c
	vc $(VCFLAGS) -c -o build/vbcc-os3/content_handlers_image_image.o content/handlers/image/image.c

build/vbcc-os3/content_handlers_image_image_cache.o: content/handlers/image/image_cache.c
	vc $(VCFLAGS) -c -o build/vbcc-os3/content_handlers_image_image_cache.o content/handlers/image/image_cache.c

build/vbcc-os3/desktop_bitmap.o: desktop/bitmap.c
	vc $(VCFLAGS) -c -o build/vbcc-os3/desktop_bitmap.o desktop/bitmap.c

build/vbcc-os3/desktop_browser.o: desktop/browser.c
	vc $(VCFLAGS) -c -o build/vbcc-os3/desktop_browser.o desktop/browser.c

build/vbcc-os3/desktop_browser_window.o: desktop/browser_window.c
	vc $(VCFLAGS) -c -o build/vbcc-os3/desktop_browser_window.o desktop/browser_window.c

build/vbcc-os3/desktop_browser_history.o: desktop/browser_history.c
	vc $(VCFLAGS) -c -o build/vbcc-os3/desktop_browser_history.o desktop/browser_history.c

build/vbcc-os3/desktop_download.o: desktop/download.c
	vc $(VCFLAGS) -c -o build/vbcc-os3/desktop_download.o desktop/download.c

build/vbcc-os3/desktop_frames.o: desktop/frames.c
	vc $(VCFLAGS) -c -o build/vbcc-os3/desktop_frames.o desktop/frames.c

build/vbcc-os3/desktop_netsurf.o: desktop/netsurf.c
	vc $(VCFLAGS) -c -o build/vbcc-os3/desktop_netsurf.o desktop/netsurf.c

build/vbcc-os3/desktop_cw_helper.o: desktop/cw_helper.c
	vc $(VCFLAGS) -c -o build/vbcc-os3/desktop_cw_helper.o desktop/cw_helper.c

build/vbcc-os3/desktop_save_complete.o: desktop/save_complete.c frontends/amiga/include/regex.h
	vc $(VCFLAGS) -c -o build/vbcc-os3/desktop_save_complete.o desktop/save_complete.c

build/vbcc-os3/desktop_save_text.o: desktop/save_text.c
	vc $(VCFLAGS) -c -o build/vbcc-os3/desktop_save_text.o desktop/save_text.c

build/vbcc-os3/desktop_selection.o: desktop/selection.c
	vc $(VCFLAGS) -c -o build/vbcc-os3/desktop_selection.o desktop/selection.c

build/vbcc-os3/desktop_textinput.o: desktop/textinput.c
	vc $(VCFLAGS) -c -o build/vbcc-os3/desktop_textinput.o desktop/textinput.c

build/vbcc-os3/desktop_gui_factory.o: desktop/gui_factory.c
	vc $(VCFLAGS) -c -o build/vbcc-os3/desktop_gui_factory.o desktop/gui_factory.c

build/vbcc-os3/desktop_save_pdf.o: desktop/save_pdf.c
	vc $(VCFLAGS) -c -o build/vbcc-os3/desktop_save_pdf.o desktop/save_pdf.c

build/vbcc-os3/desktop_font_haru.o: desktop/font_haru.c
	vc $(VCFLAGS) -c -o build/vbcc-os3/desktop_font_haru.o desktop/font_haru.c

build/vbcc-os3/frontends_amiga_gui.o: frontends/amiga/gui.c
	vc $(VCFLAGS) -c -o build/vbcc-os3/frontends_amiga_gui.o frontends/amiga/gui.c

build/vbcc-os3/frontends_amiga_history.o: frontends/amiga/history.c
	vc $(VCFLAGS) -c -o build/vbcc-os3/frontends_amiga_history.o frontends/amiga/history.c

build/vbcc-os3/frontends_amiga_hotlist.o: frontends/amiga/hotlist.c
	vc $(VCFLAGS) -c -o build/vbcc-os3/frontends_amiga_hotlist.o frontends/amiga/hotlist.c

build/vbcc-os3/frontends_amiga_schedule.o: frontends/amiga/schedule.c
	vc $(VCFLAGS) -c -o build/vbcc-os3/frontends_amiga_schedule.o frontends/amiga/schedule.c

build/vbcc-os3/frontends_amiga_file.o: frontends/amiga/file.c
	vc $(VCFLAGS) -c -o build/vbcc-os3/frontends_amiga_file.o frontends/amiga/file.c

build/vbcc-os3/frontends_amiga_misc.o: frontends/amiga/misc.c
	vc $(VCFLAGS) -c -o build/vbcc-os3/frontends_amiga_misc.o frontends/amiga/misc.c

build/vbcc-os3/frontends_amiga_bitmap.o: frontends/amiga/bitmap.c
	vc $(VCFLAGS) -c -o build/vbcc-os3/frontends_amiga_bitmap.o frontends/amiga/bitmap.c

build/vbcc-os3/frontends_amiga_font.o: frontends/amiga/font.c
	vc $(VCFLAGS) -c -o build/vbcc-os3/frontends_amiga_font.o frontends/amiga/font.c

build/vbcc-os3/frontends_amiga_filetype.o: frontends/amiga/filetype.c
	vc $(VCFLAGS) -c -o build/vbcc-os3/frontends_amiga_filetype.o frontends/amiga/filetype.c

build/vbcc-os3/frontends_amiga_utf8.o: frontends/amiga/utf8.c
	vc $(VCFLAGS) -c -o build/vbcc-os3/frontends_amiga_utf8.o frontends/amiga/utf8.c

build/vbcc-os3/frontends_amiga_iconv_iconv.o: frontends/amiga/iconv/iconv.c
	vc $(VCFLAGS) -c -o build/vbcc-os3/frontends_amiga_iconv_iconv.o frontends/amiga/iconv/iconv.c

build/vbcc-os3/frontends_amiga_memory.o: frontends/amiga/memory.c
	vc $(VCFLAGS) -c -o build/vbcc-os3/frontends_amiga_memory.o frontends/amiga/memory.c

build/vbcc-os3/frontends_amiga_plotters.o: frontends/amiga/plotters.c
	vc $(VCFLAGS) -c -o build/vbcc-os3/frontends_amiga_plotters.o frontends/amiga/plotters.c

build/vbcc-os3/frontends_amiga_object.o: frontends/amiga/object.c
	vc $(VCFLAGS) -c -o build/vbcc-os3/frontends_amiga_object.o frontends/amiga/object.c

build/vbcc-os3/frontends_amiga_menu.o: frontends/amiga/menu.c
	vc $(VCFLAGS) -c -o build/vbcc-os3/frontends_amiga_menu.o frontends/amiga/menu.c

build/vbcc-os3/frontends_amiga_save_pdf.o: frontends/amiga/save_pdf.c
	vc $(VCFLAGS) -c -o build/vbcc-os3/frontends_amiga_save_pdf.o frontends/amiga/save_pdf.c

build/vbcc-os3/frontends_amiga_arexx.o: frontends/amiga/arexx.c
	vc $(VCFLAGS) -c -o build/vbcc-os3/frontends_amiga_arexx.o frontends/amiga/arexx.c

build/vbcc-os3/frontends_amiga_version.o: frontends/amiga/version.c
	vc $(VCFLAGS) -c -o build/vbcc-os3/frontends_amiga_version.o frontends/amiga/version.c

build/vbcc-os3/frontends_amiga_cookies.o: frontends/amiga/cookies.c
	vc $(VCFLAGS) -c -o build/vbcc-os3/frontends_amiga_cookies.o frontends/amiga/cookies.c

build/vbcc-os3/frontends_amiga_ctxmenu.o: frontends/amiga/ctxmenu.c
	vc $(VCFLAGS) -c -o build/vbcc-os3/frontends_amiga_ctxmenu.o frontends/amiga/ctxmenu.c

build/vbcc-os3/frontends_amiga_clipboard.o: frontends/amiga/clipboard.c
	vc $(VCFLAGS) -c -o build/vbcc-os3/frontends_amiga_clipboard.o frontends/amiga/clipboard.c

build/vbcc-os3/frontends_amiga_help.o: frontends/amiga/help.c
	vc $(VCFLAGS) -c -o build/vbcc-os3/frontends_amiga_help.o frontends/amiga/help.c

build/vbcc-os3/frontends_amiga_font_scan.o: frontends/amiga/font_scan.c
	vc $(VCFLAGS) -c -o build/vbcc-os3/frontends_amiga_font_scan.o frontends/amiga/font_scan.c

build/vbcc-os3/frontends_amiga_launch.o: frontends/amiga/launch.c
	vc $(VCFLAGS) -c -o build/vbcc-os3/frontends_amiga_launch.o frontends/amiga/launch.c

build/vbcc-os3/frontends_amiga_search.o: frontends/amiga/search.c
	vc $(VCFLAGS) -c -o build/vbcc-os3/frontends_amiga_search.o frontends/amiga/search.c

build/vbcc-os3/frontends_amiga_history_local.o: frontends/amiga/history_local.c
	vc $(VCFLAGS) -c -o build/vbcc-os3/frontends_amiga_history_local.o frontends/amiga/history_local.c

build/vbcc-os3/frontends_amiga_download.o: frontends/amiga/download.c
	vc $(VCFLAGS) -c -o build/vbcc-os3/frontends_amiga_download.o frontends/amiga/download.c

build/vbcc-os3/frontends_amiga_iff_dr2d.o: frontends/amiga/iff_dr2d.c
	vc $(VCFLAGS) -c -o build/vbcc-os3/frontends_amiga_iff_dr2d.o frontends/amiga/iff_dr2d.c

build/vbcc-os3/frontends_amiga_gui_options.o: frontends/amiga/gui_options.c
	vc $(VCFLAGS) -c -o build/vbcc-os3/frontends_amiga_gui_options.o frontends/amiga/gui_options.c

build/vbcc-os3/frontends_amiga_print.o: frontends/amiga/print.c
	vc $(VCFLAGS) -c -o build/vbcc-os3/frontends_amiga_print.o frontends/amiga/print.c

build/vbcc-os3/frontends_amiga_theme.o: frontends/amiga/theme.c
	vc $(VCFLAGS) -c -o build/vbcc-os3/frontends_amiga_theme.o frontends/amiga/theme.c

build/vbcc-os3/frontends_amiga_drag.o: frontends/amiga/drag.c
	vc $(VCFLAGS) -c -o build/vbcc-os3/frontends_amiga_drag.o frontends/amiga/drag.c

build/vbcc-os3/frontends_amiga_icon.o: frontends/amiga/icon.c
	vc $(VCFLAGS) -c -o build/vbcc-os3/frontends_amiga_icon.o frontends/amiga/icon.c

build/vbcc-os3/frontends_amiga_ico.o: frontends/amiga/ico.c
	vc $(VCFLAGS) -c -o build/vbcc-os3/frontends_amiga_ico.o frontends/amiga/ico.c

build/vbcc-os3/frontends_amiga_libs.o: frontends/amiga/libs.c
	vc $(VCFLAGS) -c -o build/vbcc-os3/frontends_amiga_libs.o frontends/amiga/libs.c

build/vbcc-os3/frontends_amiga_datatypes.o: frontends/amiga/datatypes.c
	vc $(VCFLAGS) -c -o build/vbcc-os3/frontends_amiga_datatypes.o frontends/amiga/datatypes.c

build/vbcc-os3/frontends_amiga_dt_picture.o: frontends/amiga/dt_picture.c
	vc $(VCFLAGS) -c -o build/vbcc-os3/frontends_amiga_dt_picture.o frontends/amiga/dt_picture.c

build/vbcc-os3/frontends_amiga_dt_anim.o: frontends/amiga/dt_anim.c
	vc $(VCFLAGS) -c -o build/vbcc-os3/frontends_amiga_dt_anim.o frontends/amiga/dt_anim.c

build/vbcc-os3/frontends_amiga_dt_sound.o: frontends/amiga/dt_sound.c
	vc $(VCFLAGS) -c -o build/vbcc-os3/frontends_amiga_dt_sound.o frontends/amiga/dt_sound.c

build/vbcc-os3/frontends_amiga_plugin_hack.o: frontends/amiga/plugin_hack.c
	vc $(VCFLAGS) -c -o build/vbcc-os3/frontends_amiga_plugin_hack.o frontends/amiga/plugin_hack.c

build/vbcc-os3/frontends_amiga_stringview_stringview.o: frontends/amiga/stringview/stringview.c
	vc $(VCFLAGS) -c -o build/vbcc-os3/frontends_amiga_stringview_stringview.o frontends/amiga/stringview/stringview.c

build/vbcc-os3/frontends_amiga_stringview_urlhistory.o: frontends/amiga/stringview/urlhistory.c
	vc $(VCFLAGS) -c -o build/vbcc-os3/frontends_amiga_stringview_urlhistory.o frontends/amiga/stringview/urlhistory.c

build/vbcc-os3/frontends_amiga_rtg.o: frontends/amiga/rtg.c
	vc $(VCFLAGS) -c -o build/vbcc-os3/frontends_amiga_rtg.o frontends/amiga/rtg.c

build/vbcc-os3/frontends_amiga_agclass_amigaguide_class.o: frontends/amiga/agclass/amigaguide_class.c
	vc $(VCFLAGS) -c -o build/vbcc-os3/frontends_amiga_agclass_amigaguide_class.o frontends/amiga/agclass/amigaguide_class.c

build/vbcc-os3/frontends_amiga_os3support.o: frontends/amiga/os3support.c
	vc $(VCFLAGS) -c -o build/vbcc-os3/frontends_amiga_os3support.o frontends/amiga/os3support.c

build/vbcc-os3/frontends_amiga_font_diskfont.o: frontends/amiga/font_diskfont.c
	vc $(VCFLAGS) -c -o build/vbcc-os3/frontends_amiga_font_diskfont.o frontends/amiga/font_diskfont.c

build/vbcc-os3/frontends_amiga_font_ttengine.o: frontends/amiga/font_ttengine.c
	vc $(VCFLAGS) -c -o build/vbcc-os3/frontends_amiga_font_ttengine.o frontends/amiga/font_ttengine.c

build/vbcc-os3/frontends_amiga_selectmenu.o: frontends/amiga/selectmenu.c
	vc $(VCFLAGS) -c -o build/vbcc-os3/frontends_amiga_selectmenu.o frontends/amiga/selectmenu.c

build/vbcc-os3/frontends_amiga_hash_xxhash.o: frontends/amiga/hash/xxhash.c
	vc $(VCFLAGS) -c -o build/vbcc-os3/frontends_amiga_hash_xxhash.o frontends/amiga/hash/xxhash.c

build/vbcc-os3/frontends_amiga_font_cache.o: frontends/amiga/font_cache.c
	vc $(VCFLAGS) -c -o build/vbcc-os3/frontends_amiga_font_cache.o frontends/amiga/font_cache.c

build/vbcc-os3/frontends_amiga_font_bullet.o: frontends/amiga/font_bullet.c
	vc $(VCFLAGS) -c -o build/vbcc-os3/frontends_amiga_font_bullet.o frontends/amiga/font_bullet.c

build/vbcc-os3/frontends_amiga_nsoption.o: frontends/amiga/nsoption.c
	vc $(VCFLAGS) -c -o build/vbcc-os3/frontends_amiga_nsoption.o frontends/amiga/nsoption.c

build/vbcc-os3/frontends_amiga_corewindow.o: frontends/amiga/corewindow.c
	vc $(VCFLAGS) -c -o build/vbcc-os3/frontends_amiga_corewindow.o frontends/amiga/corewindow.c

build/vbcc-os3/frontends_amiga_gui_menu.o: frontends/amiga/gui_menu.c
	vc $(VCFLAGS) -c -o build/vbcc-os3/frontends_amiga_gui_menu.o frontends/amiga/gui_menu.c

build/vbcc-os3/frontends_amiga_pageinfo.o: frontends/amiga/pageinfo.c
	vc $(VCFLAGS) -c -o build/vbcc-os3/frontends_amiga_pageinfo.o frontends/amiga/pageinfo.c

