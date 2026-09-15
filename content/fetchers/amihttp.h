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

/**
 * \file
 * HTTP/HTTPS fetch via amihttp.library (registration).
 */

#ifndef NETSURF_CONTENT_FETCHERS_AMIHTTP_H
#define NETSURF_CONTENT_FETCHERS_AMIHTTP_H

#include "utils/errors.h"

#ifdef WITH_AMIHTTP

/**
 * Register amihttp scheme handlers for http and https.
 *
 * \return NSERROR_OK on success, or an error code on failure.
 */
nserror fetch_amihttp_register(void);

/**
 * Exec signal bit used for amihttp transaction completion notify.
 * OR (1UL << bit) into the main Wait()/WaitSelect() mask; -1 if unused.
 */
BYTE fetch_amihttp_signal(void);

#endif /* WITH_AMIHTTP */

#endif
