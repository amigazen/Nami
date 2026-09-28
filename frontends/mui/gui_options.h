/*
 * Copyright 2009 Chris Young <chris@unsatisfactorysoftware.co.uk>
 * Copyright 2026 amigazen project
 */

#ifndef MUI_GUI_OPTIONS_H
#define MUI_GUI_OPTIONS_H

void ami_gui_opts_open(void);
/** Dispose prefs window (call from gui fini). */
void ami_gui_opts_fini(void);
struct List *ami_gui_opts_websearch(int *idx);
void ami_gui_opts_websearch_free(struct List *websearchlist);

#endif /* MUI_GUI_OPTIONS_H */
