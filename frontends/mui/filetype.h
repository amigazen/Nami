/*
 * Copyright 2026 amigazen project
 *
 * This file is part of NetSurf / Tsunami (MUI frontend).
 */

#ifndef MUI_FILETYPE_H
#define MUI_FILETYPE_H

void tsunami_fetch_filetype_init(const char *mimefile);
void tsunami_fetch_filetype_fin(void);
const char *tsunami_fetch_filetype(const char *unix_path);

#endif /* MUI_FILETYPE_H */
