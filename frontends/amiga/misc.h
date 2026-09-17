/*
 * Copyright 2010 Chris Young <chris@unsatisfactorysoftware.co.uk>
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

#ifndef AMIGA_MISC_H
#define AMIGA_MISC_H

#include <exec/types.h>

#include "utils/errors.h"

extern struct gui_file_table *amiga_file_table;
struct Window;
struct nsurl;

/** Image style for ami_misc_requester (maps to REQIMAGE_* / TDRIMAGE_*). */
enum {
	AMI_REQ_IMAGE_INFO = 0,
	AMI_REQ_IMAGE_WARNING,
	AMI_REQ_IMAGE_ERROR,
	AMI_REQ_IMAGE_QUESTION
};

/**
 * Modal info/query requester.
 * Prefer requester.class when open; else TimedDosRequester (OS4) or
 * EasyRequest (OS3). Button numbering matches EasyRequest (1..n-1, 0=last).
 *
 * @param win reference window (may be NULL; then screen is used)
 * @param title window title (system charset)
 * @param body body text (system charset)
 * @param gadgets gadget labels "Ok|Cancel" style (system charset)
 * @param image AMI_REQ_IMAGE_*
 * @return selected button number
 */
LONG ami_misc_requester(struct Window *win,
		const char *title, const char *body, const char *gadgets,
		ULONG image);

/**
 * Like ami_misc_requester with optional timeout (OS4 requester.class /
 * TimedDosRequester). timeout_secs 0 means no timeout. Returns -1 on timeout.
 */
LONG ami_misc_requester_ex(struct Window *win,
		const char *title, const char *body, const char *gadgets,
		ULONG image, LONG timeout_secs, BOOL inactive);

/**
 * Warn the user of an event.
 *
 * \param[in] warning A warning looked up in the message translation table
 * \param[in] detail Additional text to be displayed or NULL.
 * \return NSERROR_OK on success or error code if there was a
 *           faliure displaying the message to the user.
 */
nserror amiga_warn_user(const char *warning, const char *detail);
char *translate_escape_chars(const char *s);
void ami_misc_fatal_error(const char *message);
int32 amiga_warn_user_multi(const char *body,
	const char *opt1, const char *opt2, struct Window *win);

/**
 * Convert a user-supplied address string to nsurl.
 * Handles http(s) URLs and Amiga Vol:path / relative files without
 * mistaking volume names for URI schemes.
 */
nserror ami_string_to_nsurl(const char *s, struct nsurl **url_out);

#endif

