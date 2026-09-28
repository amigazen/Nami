/*
 * Copyright 2026 amigazen project
 *
 * This file is part of NetSurf / Tsunami (MUI frontend).
 */

#ifndef MUI_BITMAP_H
#define MUI_BITMAP_H

#include "netsurf/bitmap.h"

extern struct gui_bitmap_table *tsunami_bitmap_table;

unsigned char *bitmap_get_buffer(void *bitmap);
size_t bitmap_get_rowstride(void *bitmap);
int bitmap_get_width(void *bitmap);
int bitmap_get_height(void *bitmap);

#endif /* MUI_BITMAP_H */
