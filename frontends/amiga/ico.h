/*
 * Copyright 2026 AmigaZen / Kitsune
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

/** \file
 * Built-in Windows ICO decoder for favicons (no libnsbmp / icon.datatype).
 */

#ifndef AMIGA_ICO_H
#define AMIGA_ICO_H

#include "utils/errors.h"

nserror amiga_ico_init(void);

#endif
