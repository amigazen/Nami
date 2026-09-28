/*
 * Copyright 2008-2009 Chris Young <chris@unsatisfactorysoftware.co.uk>
 * Copyright 2026 amigazen project
 */

#ifndef MUI_CLIPBOARD_H
#define MUI_CLIPBOARD_H

#include <stdbool.h>

struct bitmap;
struct hlcache_handle;
struct gui_window;
struct gui_clipboard_table;

extern struct gui_clipboard_table *amiga_clipboard_table;

void gui_start_selection(struct gui_window *g);
void ami_clipboard_init(void);
void ami_clipboard_free(void);
void ami_drag_selection(struct gui_window *g);
bool ami_easy_clipboard(const char *text);
bool ami_easy_clipboard_bitmap(struct bitmap *bitmap);

#endif /* MUI_CLIPBOARD_H */
