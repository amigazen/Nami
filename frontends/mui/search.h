/*
 * Copyright 2008-2009 Chris Young <chris@unsatisfactorysoftware.co.uk>
 * Copyright 2026 amigazen project
 *
 * Forked from frontends/amiga/search.h for Tsunami.
 */

#ifndef MUI_SEARCH_H
#define MUI_SEARCH_H

struct gui_search_table;
struct gui_window;
struct find_window;

extern struct gui_search_table *amiga_search_table;

void ami_search_open(struct gui_window *gwin);
void ami_search_close(void);
void ami_search_fini(void);
void ami_search_close_if_gw(struct gui_window *gw);
struct gui_window *ami_search_get_gwin(struct find_window *fw);

#endif /* MUI_SEARCH_H */
