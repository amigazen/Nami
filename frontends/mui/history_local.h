/*
 * Copyright 2017 Chris Young <chris@unsatisfactorysoftware.co.uk>
 * Copyright 2026 amigazen project
 */

#ifndef MUI_HISTORY_LOCAL_H
#define MUI_HISTORY_LOCAL_H

#include "utils/errors.h"

struct gui_window;
struct ami_history_local_window;

nserror ami_history_local_present(struct gui_window *gw);
nserror ami_history_local_destroy(struct ami_history_local_window *history_local_win);
void ami_history_local_close(void);
void ami_history_local_close_if_gw(struct gui_window *gw);

#endif /* MUI_HISTORY_LOCAL_H */
