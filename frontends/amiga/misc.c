/*
 * Copyright 2008-2010 Chris Young <chris@unsatisfactorysoftware.co.uk>
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

#include "amiga/os3support.h"

#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include <proto/dos.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/utility.h>

#include <classes/requester.h>
#include <intuition/classusr.h>

#include "utils/utils.h"
#include "utils/corestrings.h"
#include "utils/log.h"
#include "utils/file.h"
#include "utils/messages.h"
#include "utils/nsurl.h"
#include "utils/url.h"

#include "netsurf/window.h"

#include "amiga/gui.h"
#include "amiga/libs.h"
#include "amiga/misc.h"
#include "amiga/utf8.h"

#ifdef __amigaos4__
extern struct ClassLibrary *RequesterBase;
#else
extern struct Library *RequesterBase;
#endif

/**
 * Map AMI_REQ_IMAGE_* to requester.class REQIMAGE_* (V47+).
 */
static ULONG ami_misc_req_image_class(ULONG image)
{
	switch(image) {
	case AMI_REQ_IMAGE_WARNING:
		return REQIMAGE_WARNING;
	case AMI_REQ_IMAGE_ERROR:
		return REQIMAGE_ERROR;
	case AMI_REQ_IMAGE_QUESTION:
		return REQIMAGE_QUESTION;
	case AMI_REQ_IMAGE_INFO:
	default:
		return REQIMAGE_INFO;
	}
}

/**
 * Map AMI_REQ_IMAGE_* to TimedDosRequester TDRIMAGE_* (OS4 fallback).
 */
static ULONG ami_misc_req_image_tdr(ULONG image)
{
	switch(image) {
	case AMI_REQ_IMAGE_WARNING:
		return TDRIMAGE_WARNING;
	case AMI_REQ_IMAGE_ERROR:
		return TDRIMAGE_ERROR;
#ifdef __amigaos4__
	case AMI_REQ_IMAGE_QUESTION:
		return TDRIMAGE_QUESTION;
	case AMI_REQ_IMAGE_INFO:
	default:
		return TDRIMAGE_INFO;
#else
	case AMI_REQ_IMAGE_QUESTION:
		return TDRIMAGE_WARNING;
	case AMI_REQ_IMAGE_INFO:
	default:
		return TDRIMAGE_WARNING;
#endif
	}
}

/**
 * Try requester.class RM_OPENREQ. Returns -2 if the class is unavailable
 * or NewObject failed so the caller can fall back.
 */
static LONG ami_misc_requester_via_class(struct Window *win,
		const char *title, const char *body, const char *gadgets,
		ULONG image, LONG timeout_secs, BOOL inactive)
{
	Object *reqobj;
	struct orRequest reqmsg;
	struct TagItem tags[12];
	ULONG n;
	LONG ret;
	struct Screen *scr;
	ULONG libver;

	if(RequesterClass == NULL)
		return -2;

	reqobj = NewObject(RequesterClass, NULL, TAG_DONE);
	if(reqobj == NULL)
		return -2;

	n = 0;
	tags[n].ti_Tag = REQ_Type;
	tags[n].ti_Data = REQTYPE_INFO;
	n++;
	tags[n].ti_Tag = REQ_TitleText;
	tags[n].ti_Data = (ULONG)title;
	n++;
	tags[n].ti_Tag = REQ_BodyText;
	tags[n].ti_Data = (ULONG)body;
	n++;
	tags[n].ti_Tag = REQ_GadgetText;
	tags[n].ti_Data = (ULONG)gadgets;
	n++;

	libver = 0;
	if(RequesterBase != NULL)
		libver = ((struct Library *)RequesterBase)->lib_Version;
	if(libver >= 47) {
		tags[n].ti_Tag = REQ_Image;
		tags[n].ti_Data = ami_misc_req_image_class(image);
		n++;
	}

#ifdef __amigaos4__
	/* Timeout / inactive are OS4-only requester.class tags */
	if(timeout_secs > 0) {
		tags[n].ti_Tag = REQ_TimeOutSecs;
		tags[n].ti_Data = (ULONG)timeout_secs;
		n++;
		tags[n].ti_Tag = REQ_Inactive;
		tags[n].ti_Data = inactive ? TRUE : FALSE;
		n++;
	}
#else
	(void)timeout_secs;
	(void)inactive;
#endif

	tags[n].ti_Tag = TAG_DONE;
	tags[n].ti_Data = 0;

	scr = NULL;
	if(win == NULL)
		scr = ami_gui_get_screen();

	reqmsg.MethodID = RM_OPENREQ;
	reqmsg.or_Attrs = tags;
	reqmsg.or_Window = win;
	reqmsg.or_Screen = scr;

	ret = (LONG)IDoMethodA(reqobj, (Msg)&reqmsg);
	DisposeObject(reqobj);
	return ret;
}

/* exported interface documented in amiga/misc.h */
LONG ami_misc_requester(struct Window *win,
		const char *title, const char *body, const char *gadgets,
		ULONG image)
{
	return ami_misc_requester_ex(win, title, body, gadgets, image, 0, FALSE);
}

/* exported interface documented in amiga/misc.h */
LONG ami_misc_requester_ex(struct Window *win,
		const char *title, const char *body, const char *gadgets,
		ULONG image, LONG timeout_secs, BOOL inactive)
{
	LONG ret;
#ifndef __amigaos4__
	struct EasyStruct easyreq;
#endif

	if(title == NULL)
		title = "Nami";
	if(body == NULL)
		body = "";
	if(gadgets == NULL)
		gadgets = "Ok";

	ret = ami_misc_requester_via_class(win, title, body, gadgets,
			image, timeout_secs, inactive);
	if(ret != -2)
		return ret;

#ifdef __amigaos4__
	if(timeout_secs > 0) {
		ret = TimedDosRequesterTags(
			TDR_TitleString, title,
			TDR_FormatString, body,
			TDR_GadgetString, gadgets,
			TDR_ImageType, ami_misc_req_image_tdr(image),
			TDR_Window, win,
			TDR_Timeout, timeout_secs,
			TDR_Inactive, inactive,
			TAG_DONE);
	} else {
		ret = TimedDosRequesterTags(
			TDR_TitleString, title,
			TDR_FormatString, body,
			TDR_GadgetString, gadgets,
			TDR_ImageType, ami_misc_req_image_tdr(image),
			TDR_Window, win,
			TAG_DONE);
	}
#else
	(void)timeout_secs;
	(void)inactive;
	(void)image;
	easyreq.es_StructSize = sizeof(struct EasyStruct);
	easyreq.es_Flags = 0;
	easyreq.es_Title = (char *)title;
	easyreq.es_TextFormat = (char *)body;
	easyreq.es_GadgetFormat = (char *)gadgets;
	ret = EasyRequest(win, &easyreq, NULL);
#endif
	return ret;
}

static LONG ami_misc_req(const char *message, ULONG image)
{
	LONG ret;
	struct gui_window *cur_gw;
	char *local_msg;
	char *local_title;
	char *local_ok;

	cur_gw = ami_gui_get_active_gw();

	/* Requesters expect the system charset, not UTF-8. */
	local_msg = ami_utf8_easy(message != NULL ? message : "");
	local_title = ami_utf8_easy(messages_get("NetSurf"));
	local_ok = ami_utf8_easy(messages_get("OK"));

	NSLOG(netsurf, INFO, "%s", local_msg != NULL ? local_msg : message);

	ret = ami_misc_requester(
			cur_gw ? ami_gui_get_window(cur_gw) : NULL,
			local_title != NULL ? local_title : messages_get("NetSurf"),
			local_msg != NULL ? local_msg : message,
			local_ok != NULL ? local_ok : messages_get("OK"),
			image);

	if(local_msg != NULL)
		free(local_msg);
	if(local_title != NULL)
		free(local_title);
	if(local_ok != NULL)
		free(local_ok);
	return ret;
}

void ami_misc_fatal_error(const char *message)
{
	ami_misc_req(message, AMI_REQ_IMAGE_ERROR);
}

/* exported interface documented in amiga/misc.h */
nserror amiga_warn_user(const char *warning, const char *detail)
{
	STRPTR bodytext;
	const char *msg;

	/*
	 * Modal NoMemory requesters lock the UI for minutes while the
	 * machine is already out of RAM (amigans OOM path). Log only.
	 */
	if (warning != NULL && strcmp(warning, "NoMemory") == 0) {
		NSLOG(netsurf, WARNING, "NoMemory (non-modal)%s%s",
		      (detail != NULL && detail[0] != '\0') ? ": " : "",
		      detail != NULL ? detail : "");
		return NSERROR_NOMEM;
	}

	/* messages_get is UTF-8; ami_misc_req converts once for intuition. */
	msg = messages_get(warning);
	bodytext = ASPrintf("%s\n%s",
			msg != NULL ? msg : warning,
			detail != NULL ? detail : "");

	ami_misc_req(bodytext != NULL ? (const char *)bodytext : warning,
			AMI_REQ_IMAGE_WARNING);

	if(bodytext != NULL)
		FreeVec(bodytext);

	return NSERROR_OK;
}

int32 amiga_warn_user_multi(const char *body, const char *opt1, const char *opt2, struct Window *win)
{
	int32 res;
	char *local_text;
	char *local_g1;
	char *local_g2;
	char *local_title;
	char *local_gadgets;

	res = 0;
	local_text = ami_utf8_easy(body);
	local_g1 = ami_utf8_easy(messages_get(opt1));
	local_g2 = ami_utf8_easy(messages_get(opt2));
	local_title = ami_utf8_easy(messages_get("NetSurf"));
	local_gadgets = ASPrintf("%s|%s",
			local_g1 != NULL ? local_g1 : "",
			local_g2 != NULL ? local_g2 : "");

	if(local_g1 != NULL)
		free(local_g1);
	if(local_g2 != NULL)
		free(local_g2);

	res = ami_misc_requester(win,
			local_title != NULL ? local_title : messages_get("NetSurf"),
			local_text != NULL ? local_text : body,
			local_gadgets != NULL ? local_gadgets : "Ok|Cancel",
			AMI_REQ_IMAGE_WARNING);

	if(local_text != NULL)
		free(local_text);
	if(local_title != NULL)
		free(local_title);
	if(local_gadgets != NULL)
		FreeVec(local_gadgets);

	return res;
}

/**
 * Create a path from a nsurl using amiga file handling.
 *
 * @param[in] url The url to encode.
 * @param[out] path_out A string containing the result path which should
 *                      be freed by the caller.
 * @return NSERROR_OK and the path is written to \a path or error code
 *         on faliure.
 */
static nserror amiga_nsurl_to_path(struct nsurl *url, char **path_out)
{
	lwc_string *urlpath;
	size_t path_len;
	char *path;
	bool match;
	lwc_string *scheme;
	nserror res;
	char *colon;
	char *slash;

	if ((url == NULL) || (path_out == NULL)) {
		return NSERROR_BAD_PARAMETER;
	}

	scheme = nsurl_get_component(url, NSURL_SCHEME);

	if (lwc_string_caseless_isequal(scheme, corestring_lwc_file,
					&match) != lwc_error_ok)
	{
		return NSERROR_BAD_PARAMETER;
	}
	lwc_string_unref(scheme);
	if (match == false) {
		return NSERROR_BAD_PARAMETER;
	}

	urlpath = nsurl_get_component(url, NSURL_PATH);
	if (urlpath == NULL) {
		return NSERROR_BAD_PARAMETER;
	}

	res = url_unescape(lwc_string_data(urlpath) + 1, 0, &path_len, &path);
	lwc_string_unref(urlpath);
	if (res != NSERROR_OK) {
		return res;
	}

	colon = strchr(path, ':');
	if(colon == NULL) {
		slash = strchr(path, '/');
		if(slash) {
			*slash = ':';
		} else {
			char *tmp_path = malloc(path_len + 2);
			if(tmp_path == NULL) return NSERROR_NOMEM;

			strncpy(tmp_path, path, path_len);
			free(path);

			path = tmp_path;
			path[path_len] = ':';
			path[path_len + 1] = '\0';
		}
	}

	*path_out = path;

	return NSERROR_OK;
}

/**
 * Create a nsurl from a path using amiga file handling.
 *
 * Keep Amiga assign/volume names (e.g. PROGDIR:Resources/...).  Resolving via
 * DevNameFromLock to a host volume path produced file:///AmigaZen/... URLs that
 * then failed inconsistently in the file fetcher / error pages.
 */
static nserror amiga_path_to_nsurl(const char *path, struct nsurl **url_out)
{
	char *colon = NULL;
	char *r = NULL;
	char newpath[1024];
	nserror ret;

	if (path == NULL) {
		return NSERROR_BAD_PARAMETER;
	}

	strlcpy(newpath, path, sizeof newpath);

	r = malloc(strlen(newpath) + SLEN("file:///") + 1);
	if (r == NULL) {
		return NSERROR_NOMEM;
	}

	if ((colon = strchr(newpath, ':'))) {
		*colon = '/';
	}

	strcpy(r, "file:///");
	strcat(r, newpath);

	ret = nsurl_create(r, url_out);
	free(r);

	return ret;
}

/**
 * True if s begins with a known URI scheme and ':' (case-insensitive).
 * Used so http(s)/ftp/… are never treated as Amiga volume names.
 */
static int ami_string_has_uri_scheme(const char *s)
{
	static const char *const schemes[] = {
		"https", "http", "file", "ftp", "ftps",
		"about", "mailto", "data", "javascript", "resource",
		"gemini", "gopher", "news", "nntp", "irc", "ircs",
		"magnet", "blob", "ws", "wss", "view-source",
		NULL
	};
	const char *colon;
	size_t schemelen;
	int i;
	size_t j;
	const char *name;

	if(s == NULL || *s == '\0')
		return 0;

	colon = strchr(s, ':');
	if(colon == NULL || colon == s)
		return 0;

	schemelen = (size_t)(colon - s);
	for(i = 0; schemes[i] != NULL; i++) {
		name = schemes[i];
		if(strlen(name) != schemelen)
			continue;
		for(j = 0; j < schemelen; j++) {
			char a = s[j];
			char b = name[j];
			if(a >= 'A' && a <= 'Z')
				a = (char)(a - 'A' + 'a');
			if(a != b)
				break;
		}
		if(j == schemelen)
			return 1;
	}
	return 0;
}

/**
 * True if s has scheme:// (RFC-style hierarchical URI), case on scheme OK.
 */
static int ami_string_has_authority_uri(const char *s)
{
	const char *p;
	const char *slash;

	if(s == NULL)
		return 0;
	slash = strstr(s, "://");
	if(slash == NULL || slash == s)
		return 0;
	/* Scheme chars before :// */
	for(p = s; p < slash; p++) {
		char c = *p;
		if((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z'))
			continue;
		if(p != s && ((c >= '0' && c <= '9') || c == '+' ||
			      c == '-' || c == '.'))
			continue;
		return 0;
	}
	return 1;
}

/**
 * Turn a Shell / ARexx / OpenURL string into an nsurl.
 *
 * URI schemes are always tried as URIs first — never Lock()/Vol:path rewrite
 * a web URL (that became file:///https/... or a fake "https" volume).
 * Amiga Vol:path (PROGDIR:…, Work:Docs/…) only when it is not a known URI.
 */
nserror ami_string_to_nsurl(const char *s, struct nsurl **url_out)
{
	BPTR lock;
	nserror error;

	if(s == NULL || url_out == NULL)
		return NSERROR_BAD_PARAMETER;

	*url_out = NULL;

	/* http(s)://…, file://…, or known scheme:… — never as Amiga path */
	if(ami_string_has_authority_uri(s) || ami_string_has_uri_scheme(s)) {
		return nsurl_create(s, url_out);
	}

	/* Amiga path: Lock if it exists, else try path→file URL, else nsurl */
	lock = Lock((STRPTR)s, ACCESS_READ);
	if(lock != 0) {
		UnLock(lock);
		return netsurf_path_to_nsurl(s, url_out);
	}

	if(strchr(s, ':') != NULL) {
		error = netsurf_path_to_nsurl(s, url_out);
		if(error == NSERROR_OK)
			return error;
	}

	error = nsurl_create(s, url_out);
	if(error != NSERROR_OK)
		error = netsurf_path_to_nsurl(s, url_out);
	return error;
}

/**
 * returns a string with escape chars translated
 * and string converted to local charset
 * (based on remove_underscores from utils.c)
 */

char *translate_escape_chars(const char *s)
{
	size_t i, ii, len;
	char *ret;
	char *outs;
	len = strlen(s);
	ret = malloc(len + 1);
	if (ret == NULL)
		return NULL;
	for (i = 0, ii = 0; i < len; i++) {
		if (s[i] != '\\') {
			ret[ii++] = s[i];
		}
		else if (s[i+1] == 'n') {
			ret[ii++] = '\n';
			i++;
		}
	}
	ret[ii] = '\0';

	outs = ami_utf8_easy(ret);
	free(ret);
	return outs;
}

/**
 * Generate a posix path from one or more component elemnts.
 *
 * If a string is allocated it must be freed by the caller.
 *
 * @param[in,out] str pointer to string pointer if this is NULL enough
 *                    storage will be allocated for the complete path.
 * @param[in,out] size The size of the space available if \a str not
 *                     NULL on input and if not NULL set to the total
 *                     output length on output.
 * @param[in] nelm The number of elements.
 * @param[in] ap The elements of the path as string pointers.
 * @return NSERROR_OK and the complete path is written to str
 *         or error code on faliure.
 */
static nserror amiga_vmkpath(char **str, size_t *size, size_t nelm, va_list ap)
{
	const char *elm[16];
	size_t elm_len[16];
	size_t elm_idx;
	char *fname;
	size_t fname_len = 0;

	/* check the parameters are all sensible */
	if ((nelm == 0) || (nelm > 16)) {
		return NSERROR_BAD_PARAMETER;
	}
	if ((*str != NULL) && (size == NULL)) {
		/* if the caller is providing the buffer they must say
		 * how much space is available.
		 */
		return NSERROR_BAD_PARAMETER;
	}

	/* calculate how much storage we need for the complete path
	 * with all the elements.
	 */
	for (elm_idx = 0; elm_idx < nelm; elm_idx++) {
		elm[elm_idx] = va_arg(ap, const char *);
		/* check the argument is not NULL */
		if (elm[elm_idx] == NULL) {
			return NSERROR_BAD_PARAMETER;
		}
		elm_len[elm_idx] = strlen(elm[elm_idx]);
		fname_len += elm_len[elm_idx];
	}
	fname_len += nelm; /* allow for separators and terminator */

	/* ensure there is enough space */
	fname = *str;
	if (fname != NULL) {
		if (fname_len > *size) {
			return NSERROR_NOSPACE;
		}
	} else {
		fname = malloc(fname_len);
		if (fname == NULL) {
			return NSERROR_NOMEM;
		}
	}

	/* copy the first element complete */
	memmove(fname, elm[0], elm_len[0]);
	fname[elm_len[0]] = 0;

	/* add the remaining elements */
	for (elm_idx = 1; elm_idx < nelm; elm_idx++) {
		if (!AddPart(fname, elm[elm_idx], fname_len)) {
			break;
		}
	}

	*str = fname;
	if (size != NULL) {
		*size = fname_len;
	}

	return NSERROR_OK;
}

/**
 * Get the basename of a file using posix path handling.
 *
 * This gets the last element of a path and returns it.
 *
 * @param[in] path The path to extract the name from.
 * @param[in,out] str Pointer to string pointer if this is NULL enough
 *                    storage will be allocated for the path element.
 * @param[in,out] size The size of the space available if \a
 *                     str not NULL on input and set to the total
 *                     output length on output.
 * @return NSERROR_OK and the complete path is written to str
 *         or error code on faliure.
 */
static nserror amiga_basename(const char *path, char **str, size_t *size)
{
	const char *leafname;
	char *fname;

	if (path == NULL) {
		return NSERROR_BAD_PARAMETER;
	}

	leafname = FilePart(path);
	if (leafname == NULL) {
		return NSERROR_BAD_PARAMETER;
	}

	fname = strdup(leafname);
	if (fname == NULL) {
		return NSERROR_NOMEM;
	}

	*str = fname;
	if (size != NULL) {
		*size = strlen(fname);
	}
	return NSERROR_OK;
}

/**
 * Ensure that all directory elements needed to store a filename exist.
 *
 * @param fname The filename to ensure the path to exists.
 * @return NSERROR_OK on success or error code on failure.
 */
static nserror amiga_mkdir_all(const char *fname)
{
	char *dname;
	char *sep;
	struct stat sb;

	dname = strdup(fname);

	sep = strrchr(dname, '/');
	if (sep == NULL) {
		/* no directory separator path is just filename so its ok */
		free(dname);
		return NSERROR_OK;
	}

	*sep = 0; /* null terminate directory path */

	if (stat(dname, &sb) == 0) {
		free(dname);
		if (S_ISDIR(sb.st_mode)) {
			/* path to file exists and is a directory */
			return NSERROR_OK;
		}
		return NSERROR_NOT_DIRECTORY;
	}
	*sep = '/'; /* restore separator */

	sep = dname;
	while (*sep == '/') {
		sep++;
	}
	while ((sep = strchr(sep, '/')) != NULL) {
		*sep = 0;
		if (stat(dname, &sb) != 0) {
			if (nsmkdir(dname, S_IRWXU) != 0) {
				/* could not create path element */
				free(dname);
				return NSERROR_NOT_FOUND;
			}
		} else {
			if (! S_ISDIR(sb.st_mode)) {
				/* path element not a directory */
				free(dname);
				return NSERROR_NOT_DIRECTORY;
			}
		}
		*sep = '/'; /* restore separator */
		/* skip directory separators */
		while (*sep == '/') {
			sep++;
		}
	}

	free(dname);
	return NSERROR_OK;
}

/* amiga file handling operations */
static struct gui_file_table file_table = {
	.mkpath = amiga_vmkpath,
	.basename = amiga_basename,
	.nsurl_to_path = amiga_nsurl_to_path,
	.path_to_nsurl = amiga_path_to_nsurl,
	.mkdir_all = amiga_mkdir_all,
};

struct gui_file_table *amiga_file_table = &file_table;
