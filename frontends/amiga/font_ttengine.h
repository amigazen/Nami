/*
 * Copyright 2026 amigazen project
 *
 * This file is part of NetSurf, http://www.netsurf-browser.org/
 *
 * NetSurf is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * NetSurf is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef AMIGA_FONT_TTENGINE_H
#define AMIGA_FONT_TTENGINE_H

#include <stdbool.h>

/**
 * Initialise the optional ttengine.library font backend.
 *
 * Requires TTEngineBase already open. Sets ami_nsfont on success.
 *
 * \return true if the backend is ready, false if ttengine is unavailable.
 */
bool ami_font_ttengine_init(void);

void ami_font_ttengine_fini(void);

/** true when ttengine.library was opened successfully. */
bool ami_font_ttengine_available(void);

#endif
