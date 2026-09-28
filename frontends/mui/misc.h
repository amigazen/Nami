/*
 * Copyright 2026 amigazen project
 *
 * This file is part of NetSurf / Tsunami (MUI frontend).
 */

#ifndef MUI_MISC_H
#define MUI_MISC_H

#include "netsurf/misc.h"
#include "utils/file.h"

struct Screen;

extern struct gui_misc_table *tsunami_misc_table;

/**
 * File ops: POSIX defaults plus Amiga Vol:path <-> file:/// conversion.
 */
struct gui_file_table *tsunami_file_table_get(void);

/**
 * Remember the screen used by the open browser window (font AA / DPI).
 */
void tsunami_set_screen(struct Screen *scr);

/**
 * Screen pointer for Amiga font/plotter code (ami_gui_get_screen).
 */
struct Screen *ami_gui_get_screen(void);

void tsunami_quit(void);
int tsunami_quitting(void);

#endif /* MUI_MISC_H */
