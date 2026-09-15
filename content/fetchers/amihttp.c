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
 * HTTP/HTTPS fetch via amihttp.library.
 *
 * NetSurf owns redirects and cookies (FOLLOW_REDIRECTS is false; Cookie and
 * Set-Cookie go through urldb).
 *
 * We do not use HttpTransactionPerformAsync: shipped amihttp workers can race
 * CreateNewProc vs tc_UserData and use a stack too small for TLS.  Instead we
 * run HttpTransactionPerform (sync) on a NetSurf-owned CreateNewProc with a
 * large stack (same pattern as AGet's default path), then poll
 * for completion via a shared Exec notify signal.
 */

#include "utils/config.h"

#ifdef WITH_AMIHTTP

#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include <exec/types.h>
#include <exec/lists.h>
#include <exec/memory.h>
#include <exec/tasks.h>
#include <dos/dostags.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/amihttp.h>
#include <libraries/amihttp.h>

#include <libwapcaplet/libwapcaplet.h>

#include "utils/corestrings.h"
#include "utils/log.h"
#include "utils/messages.h"
#include "utils/nsoption.h"
#include "utils/nsurl.h"
#include "utils/ring.h"
#include "utils/useragent.h"
#include "utils/utils.h"
#include "content/fetch.h"
#include "content/fetchers.h"
#include "content/fetchers/amihttp.h"
#include "content/urldb.h"
#include "netsurf/misc.h"
#include "desktop/gui_internal.h"

/** Bytes of body drained per poll to keep the UI responsive. */
#define AMIHTTP_POLL_BUDGET 65536

/** ReadBody chunk size. */
#define AMIHTTP_BODY_CHUNK 8192

/** Stack for NetSurf perform worker (TLS needs far more than amihttp's 8K). */
#define AMIHTTP_WORKER_STACK 65536

/** Per-fetch progress after sync Perform on the worker returns. */
enum amihttp_phase {
	AH_WAIT = 0,
	AH_BODY,
	AH_DONE
};

/** One active or queued http(s) fetch. */
struct amihttp_fetch_info {
	struct fetch *fetch_handle;
	struct HttpTransaction *txn;
	nsurl *url;
	lwc_string *host;
	bool abort;
	bool only_2xx;
	bool had_headers;
	bool started;
	/** Set by worker after HttpTransactionPerform returns. */
	volatile bool perform_done;
	LONG perform_rv;
	struct Task *worker_task;
	enum amihttp_phase phase;
	char *cookie_string;
	char *post_urlenc;
	struct fetch_multipart_data *post_multipart;
	struct List form_parts;
	bool form_parts_valid;
	char **req_headers;
	int req_header_count;
	long http_code;
	char *location;
	char *realm;
	/** Full entity body drained on the worker (main must not ReadBody). */
	uint8_t *body_data;
	size_t body_len;
	size_t body_sent;
	struct amihttp_fetch_info *r_prev;
	struct amihttp_fetch_info *r_next;
};

/** Passed to CreateNewProc; freed by the worker after Perform. */
struct amihttp_worker_job {
	struct amihttp_fetch_info *f;
	struct HttpTransaction *txn;
	struct Task *notify_task;
	BYTE notify_sig;
};

struct Library *HttpBase = NULL;

static struct HttpSession *amihttp_session = NULL;
static int amihttp_fetchers_registered = 0;
static BYTE amihttp_notify_sig = -1;
static struct amihttp_fetch_info *amihttp_ring = NULL;
static char amihttp_proxy_buf[256];
static char amihttp_proxy_auth_buf[256];

/**
 * Free HttpFormPart nodes and any file buffers we allocated.
 */
static void
amihttp_free_form_parts(struct amihttp_fetch_info *f)
{
	struct HttpFormPart *part;
	struct HttpFormPart *next;

	if (f->form_parts_valid == false) {
		return;
	}

	part = (struct HttpFormPart *)f->form_parts.lh_Head;
	while (part != NULL && part->hfp_Node.ln_Succ != NULL) {
		next = (struct HttpFormPart *)part->hfp_Node.ln_Succ;
		Remove((struct Node *)part);
		if (part->hfp_Data != NULL) {
			free(part->hfp_Data);
		}
		free(part);
		part = next;
	}
	f->form_parts_valid = false;
}

/**
 * Load a local file into memory for multipart upload.
 */
static bool
amihttp_load_file(const char *path, APTR *data_out, ULONG *len_out)
{
	FILE *fp;
	long sz;
	APTR buf;
	size_t nread;

	*data_out = NULL;
	*len_out = 0;

	fp = fopen(path, "rb");
	if (fp == NULL) {
		return false;
	}

	if (fseek(fp, 0, SEEK_END) != 0) {
		fclose(fp);
		return false;
	}
	sz = ftell(fp);
	if (sz < 0) {
		fclose(fp);
		return false;
	}
	if (fseek(fp, 0, SEEK_SET) != 0) {
		fclose(fp);
		return false;
	}

	buf = malloc((size_t)sz + 1);
	if (buf == NULL) {
		fclose(fp);
		return false;
	}

	nread = fread(buf, 1, (size_t)sz, fp);
	fclose(fp);
	if (nread != (size_t)sz) {
		free(buf);
		return false;
	}

	*data_out = buf;
	*len_out = (ULONG)sz;
	return true;
}

/**
 * Build amihttp multipart form list from NetSurf post controls.
 */
static bool
amihttp_build_multipart(struct amihttp_fetch_info *f)
{
	const struct fetch_multipart_data *md;
	struct HttpFormPart *part;
	APTR filedata;
	ULONG filelen;
	const char *filename;

	NewList(&f->form_parts);
	f->form_parts_valid = true;

	for (md = f->post_multipart; md != NULL; md = md->next) {
		part = calloc(1, sizeof(*part));
		if (part == NULL) {
			return false;
		}

		part->hfp_Name = (STRPTR)md->name;

		if (md->file) {
			filename = (md->rawfile != NULL) ? md->rawfile : md->value;
			part->hfp_Filename = (STRPTR)filename;
			filedata = NULL;
			filelen = 0;
			if (md->value != NULL &&
			    amihttp_load_file(md->value, &filedata, &filelen)) {
				part->hfp_Data = filedata;
				part->hfp_Length = filelen;
			} else {
				part->hfp_Value = (STRPTR)md->value;
			}
		} else {
			part->hfp_Value = (STRPTR)md->value;
		}

		AddTail(&f->form_parts, (struct Node *)part);
	}

	return true;
}

/**
 * Split a "Name: value" request header and add it to the transaction.
 */
static void
amihttp_add_request_header_line(struct HttpTransaction *txn, const char *line)
{
	char *copy;
	char *colon;
	char *value;

	if (line == NULL || line[0] == '\0') {
		return;
	}

	/* Curl clears some defaults with "Name:" — skip empty-value clears. */
	copy = strdup(line);
	if (copy == NULL) {
		return;
	}

	colon = strchr(copy, ':');
	if (colon == NULL) {
		free(copy);
		return;
	}

	*colon = '\0';
	value = colon + 1;
	while (*value == ' ' || *value == '\t') {
		value++;
	}

	if (value[0] != '\0') {
		HttpTransactionAddHeader(txn, (STRPTR)copy, (STRPTR)value);
	}

	free(copy);
}

/**
 * Emit one response header line in the format NetSurf's llcache expects.
 */
static void
amihttp_emit_header_line(struct amihttp_fetch_info *f,
			 const char *name, const char *value)
{
	char line[1024];
	fetch_msg msg;
	size_t len;
	int n;

	if (name == NULL) {
		return;
	}

	if (value == NULL) {
		value = "";
	}

	n = snprintf(line, sizeof(line), "%s: %s\r\n", name, value);
	if (n <= 0) {
		return;
	}
	len = (size_t)n;
	if (len >= sizeof(line)) {
		len = sizeof(line) - 1;
	}

	msg.type = FETCH_HEADER;
	msg.data.header_or_data.buf = (const uint8_t *)line;
	msg.data.header_or_data.len = len;
	fetch_send_callback(&msg, f->fetch_handle);

	if (strcasecmp(name, "Location") == 0) {
		free(f->location);
		f->location = strdup(value);
	} else if (strcasecmp(name, "WWW-Authenticate") == 0) {
		const char *p;
		const char *start;
		const char *end;

		p = value;
		while (*p != '\0' && strncasecmp(p, "realm", 5) != 0) {
			p++;
		}
		if (strncasecmp(p, "realm", 5) == 0) {
			p += 5;
			while (*p == ' ' || *p == '\t' || *p == '=') {
				p++;
			}
			if (*p == '"') {
				p++;
				start = p;
				end = start;
				while (*end != '\0' && *end != '"') {
					end++;
				}
				free(f->realm);
				f->realm = malloc((size_t)(end - start) + 1);
				if (f->realm != NULL) {
					memcpy(f->realm, start, (size_t)(end - start));
					f->realm[end - start] = '\0';
				}
			}
		}
	} else if (strcasecmp(name, "Set-Cookie") == 0) {
		fetch_set_cookie(f->fetch_handle, value);
	}
}

/**
 * After headers are available, decide redirect / auth / error / continue.
 *
 * \return true if the fetch should stop (callback already sent).
 */
static bool
amihttp_process_headers(struct amihttp_fetch_info *f)
{
	fetch_msg msg;
	STRPTR status_line;
	STRPTR name;
	STRPTR value;
	ULONG idx;
	long http_code;

	f->had_headers = true;

	http_code = (long)HttpTransactionGetStatusCode(f->txn);
	f->http_code = http_code;
	fetch_set_http_code(f->fetch_handle, (http_response_code)http_code);

	status_line = HttpTransactionGetStatusLine(f->txn);
	if (status_line != NULL) {
		char line[512];
		size_t len;
		int n;

		n = snprintf(line, sizeof(line), "%s\r\n", status_line);
		if (n > 0) {
			len = (size_t)n;
			if (len >= sizeof(line)) {
				len = sizeof(line) - 1;
			}
			msg.type = FETCH_HEADER;
			msg.data.header_or_data.buf = (const uint8_t *)line;
			msg.data.header_or_data.len = len;
			fetch_send_callback(&msg, f->fetch_handle);
		}
	}

	idx = 0;
	while (HttpTransactionRespHeaderByIndex(f->txn, idx, &name, &value)) {
		amihttp_emit_header_line(f, (const char *)name,
					 (const char *)value);
		idx++;
	}

	/* Fallback Location if header walk missed it. */
	if (f->location == NULL && HTTP_RESPONSE_IS_3XX(http_code)) {
		STRPTR loc;

		loc = HttpTransactionGetRedirectLocation(f->txn);
		if (loc != NULL) {
			f->location = strdup((const char *)loc);
		}
	}

	NSLOG(netsurf, INFO, "amihttp HTTP %ld for %s",
	      http_code, nsurl_access(f->url));

	if (http_code == HTTP_RESPONSE_NOT_MODIFIED &&
	    f->post_urlenc == NULL && f->post_multipart == NULL) {
		msg.type = FETCH_NOTMODIFIED;
		fetch_send_callback(&msg, f->fetch_handle);
		return true;
	}

	if (HTTP_RESPONSE_IS_3XX(http_code) && f->location != NULL) {
		msg.type = FETCH_REDIRECT;
		msg.data.redirect = f->location;
		fetch_send_callback(&msg, f->fetch_handle);
		return true;
	}

	if (http_code == HTTP_RESPONSE_UNAUTHORIZED) {
		msg.type = FETCH_AUTH;
		msg.data.auth.realm = f->realm;
		fetch_send_callback(&msg, f->fetch_handle);
		return true;
	}

	if (f->only_2xx &&
	    strncmp(nsurl_access(f->url), "http", 4) == 0 &&
	    HTTP_RESPONSE_IS_2XX(http_code) == false) {
		msg.type = FETCH_ERROR;
		msg.data.error = messages_get("Not2xx");
		fetch_send_callback(&msg, f->fetch_handle);
		return true;
	}

	if (f->abort) {
		return true;
	}

	return false;
}

/**
 * Map amihttp transport errors onto fetch_msg types.
 */
static void
amihttp_send_error(struct amihttp_fetch_info *f, LONG code)
{
	fetch_msg msg;
	STRPTR estr;

	estr = HttpGetErrorString(code);
	if (estr == NULL) {
		estr = (STRPTR)"HTTP transfer failed";
	}

	NSLOG(netsurf, INFO, "amihttp error %ld (%s) for %s",
	      (long)code, (const char *)estr,
	      f->url != NULL ? nsurl_access(f->url) : "(null)");

	switch (code) {
	case ERROR_HTTP_SSL_VERIFY:
		msg.type = FETCH_CERT_ERR;
		break;
	case ERROR_HTTP_SSL_HANDSHAKE:
		msg.type = FETCH_SSL_ERR;
		break;
	case ERROR_HTTP_CONNECT_TIMEOUT:
	case ERROR_HTTP_READ_TIMEOUT:
		msg.type = FETCH_TIMEDOUT;
		msg.data.error = (const char *)estr;
		break;
	default:
		msg.type = FETCH_ERROR;
		msg.data.error = (const char *)estr;
		break;
	}

	fetch_send_callback(&msg, f->fetch_handle);
}

/**
 * Tear down transaction resources but leave the fetch_info for free().
 */
static void
amihttp_stop_txn(struct amihttp_fetch_info *f)
{
	ULONG spins;

	if (f->txn == NULL) {
		f->worker_task = NULL;
		f->phase = AH_DONE;
		return;
	}

	/*
	 * Do NOT call AbortHttpTransaction here: the stock library frees
	 * ht_Conn under the worker (corrupt memory list). Signal only;
	 * never Dispose until perform_done.
	 */
	if (f->perform_done == false) {
		if (f->worker_task != NULL) {
			Signal(f->worker_task, SIGBREAKF_CTRL_C);
		}
		spins = 0;
		while (f->perform_done == false) {
			Delay(1);
			spins++;
			if ((spins % 500) == 0) {
				NSLOG(netsurf, WARNING,
				      "amihttp_stop_txn: waiting for worker (%lu ticks)",
				      (unsigned long)spins);
			}
		}
	}

	DisposeHttpTransaction(f->txn);
	f->txn = NULL;
	f->worker_task = NULL;
	f->phase = AH_DONE;
}

/**
 * Append ReadBody chunks into f->body_data.  Runs on the worker task only.
 */
static void
amihttp_worker_drain_body(struct amihttp_fetch_info *f,
			  struct HttpTransaction *txn)
{
	uint8_t chunk[AMIHTTP_BODY_CHUNK];
	uint8_t *nbuf;
	LONG n;
	size_t cap;

	f->body_data = NULL;
	f->body_len = 0;
	f->body_sent = 0;
	cap = 0;

	for (;;) {
		n = HttpTransactionReadBody(txn, (APTR)chunk,
					    (ULONG)sizeof(chunk));
		if (n <= 0) {
			break;
		}
		if (f->body_len + (size_t)n > cap) {
			cap = (cap == 0) ? 65536 : (cap * 2);
			while (cap < f->body_len + (size_t)n) {
				cap *= 2;
			}
			nbuf = realloc(f->body_data, cap);
			if (nbuf == NULL) {
				free(f->body_data);
				f->body_data = NULL;
				f->body_len = 0;
				return;
			}
			f->body_data = nbuf;
		}
		memcpy(f->body_data + f->body_len, chunk, (size_t)n);
		f->body_len += (size_t)n;
	}
}

/**
 * Finish a fetch: stop txn, free parent fetch.
 * Caller must already have removed f from amihttp_ring (or f was never inserted).
 */
static void
amihttp_finish(struct amihttp_fetch_info *f, bool send_finished)
{
	fetch_msg msg;

	f->phase = AH_DONE;
	amihttp_stop_txn(f);

	if (f->abort == false && send_finished) {
		msg.type = FETCH_FINISHED;
		fetch_send_callback(&msg, f->fetch_handle);
	}

	fetch_remove_from_queues(f->fetch_handle);
	fetch_free(f->fetch_handle);
}

/**
 * Worker entry: sync Perform + ReadBody on this task, then notify main.
 *
 * bsdsocket handles are task-affine; draining the body here avoids hanging
 * the UI when main would otherwise ReadBody on the wrong SocketBase.
 */
static void
amihttp_perform_worker(void)
{
	struct amihttp_worker_job *job;
	struct amihttp_fetch_info *f;
	struct Task *self;
	LONG rv;

	self = FindTask(NULL);
	job = (struct amihttp_worker_job *)self->tc_UserData;
	if (job == NULL) {
		RemTask(NULL);
		return;
	}

	f = job->f;
	rv = HttpTransactionPerform(job->txn);
	if (f != NULL) {
		f->perform_rv = rv;
		if (rv != 0) {
			amihttp_worker_drain_body(f, job->txn);
		}
		f->perform_done = true;
	}

	if (job->notify_task != NULL && job->notify_sig != -1) {
		Signal(job->notify_task, 1UL << job->notify_sig);
	}

	FreeMem(job, sizeof(*job));
	RemTask(NULL);
}

/**
 * Start sync Perform on a NetSurf-owned process (avoids library PerformAsync).
 */
static bool
amihttp_start_worker(struct amihttp_fetch_info *f)
{
	struct amihttp_worker_job *job;
	struct Process *proc;

	job = AllocMem(sizeof(*job), MEMF_PUBLIC | MEMF_CLEAR);
	if (job == NULL) {
		return false;
	}

	job->f = f;
	job->txn = f->txn;
	job->notify_task = FindTask(NULL);
	job->notify_sig = amihttp_notify_sig;
	f->perform_done = false;
	f->perform_rv = 0;

	/*
	 * Forbid so tc_UserData is set before the worker can run — same
	 * CreateNewProc race the library async path is vulnerable to.
	 */
	Forbid();
	proc = CreateNewProcTags(
		NP_Entry, (ULONG)amihttp_perform_worker,
		NP_StackSize, (ULONG)AMIHTTP_WORKER_STACK,
		NP_Name, (ULONG)"ns-amihttp",
		TAG_END);
	if (proc == NULL) {
		Permit();
		FreeMem(job, sizeof(*job));
		return false;
	}
	((struct Task *)proc)->tc_UserData = (APTR)job;
	f->worker_task = (struct Task *)proc;
	Permit();
	return true;
}

static bool
fetch_amihttp_initialise(lwc_string *scheme)
{
	(void)scheme;
	amihttp_fetchers_registered++;
	return true;
}

static void
fetch_amihttp_finalise(lwc_string *scheme)
{
	(void)scheme;

	amihttp_fetchers_registered--;
	if (amihttp_fetchers_registered > 0) {
		return;
	}

	while (amihttp_ring != NULL) {
		struct amihttp_fetch_info *f;

		f = amihttp_ring;
		RING_REMOVE(amihttp_ring, f);
		f->abort = true;
		amihttp_finish(f, false);
	}

	if (amihttp_session != NULL) {
		DisposeHttpSession(amihttp_session);
		amihttp_session = NULL;
	}

	if (amihttp_notify_sig != -1) {
		FreeSignal(amihttp_notify_sig);
		amihttp_notify_sig = -1;
	}

	if (HttpBase != NULL) {
		CloseLibrary(HttpBase);
		HttpBase = NULL;
	}
}

static bool
fetch_amihttp_can_fetch(const nsurl *url)
{
	return nsurl_has_component(url, NSURL_HOST);
}

static void *
fetch_amihttp_setup(struct fetch *parent_fetch,
		    nsurl *url,
		    bool only_2xx,
		    bool downgrade_tls,
		    const char *post_urlenc,
		    const struct fetch_multipart_data *post_multipart,
		    const char **headers)
{
	struct amihttp_fetch_info *f;
	int i;
	int count;

	(void)downgrade_tls;

	f = calloc(1, sizeof(*f));
	if (f == NULL) {
		return NULL;
	}

	f->fetch_handle = parent_fetch;
	f->only_2xx = only_2xx;
	f->phase = AH_DONE;
	f->url = nsurl_ref(url);
	f->host = nsurl_get_component(url, NSURL_HOST);
	if (f->host == NULL) {
		goto failed;
	}

	if (post_urlenc != NULL) {
		f->post_urlenc = strdup(post_urlenc);
		if (f->post_urlenc == NULL) {
			goto failed;
		}
	}

	if (post_multipart != NULL) {
		f->post_multipart = fetch_multipart_data_clone(post_multipart);
		if (f->post_multipart == NULL) {
			goto failed;
		}
	}

	count = 0;
	if (headers != NULL) {
		for (i = 0; headers[i] != NULL; i++) {
			count++;
		}
	}
	f->req_header_count = count;
	if (count > 0) {
		f->req_headers = calloc((size_t)count, sizeof(char *));
		if (f->req_headers == NULL) {
			goto failed;
		}
		for (i = 0; i < count; i++) {
			f->req_headers[i] = strdup(headers[i]);
			if (f->req_headers[i] == NULL) {
				goto failed;
			}
		}
	}

	return f;

failed:
	if (f->req_headers != NULL) {
		for (i = 0; i < f->req_header_count; i++) {
			free(f->req_headers[i]);
		}
		free(f->req_headers);
	}
	fetch_multipart_data_destroy(f->post_multipart);
	free(f->post_urlenc);
	if (f->host != NULL) {
		lwc_string_unref(f->host);
	}
	if (f->url != NULL) {
		nsurl_unref(f->url);
	}
	free(f);
	return NULL;
}

/**
 * Apply session-level options that may change between fetches.
 */
static void
amihttp_configure_session(struct amihttp_fetch_info *f)
{
	const char *auth;
	ULONG ssl_verify;
	int proxy_port;

	SetHttpSessionAttrs(
		amihttp_session,
		HTSA_USERAGENT, (ULONG)user_agent_string(),
		HTSA_FOLLOW_REDIRECTS, (ULONG)FALSE,
		HTSA_ACCEPT_ENCODING, (ULONG)"gzip",
		HTSA_CONNECT_TIMEOUT,
			(ULONG)nsoption_uint(curl_fetch_timeout),
		TAG_DONE);

	if (nsoption_charp(ca_bundle) != NULL &&
	    nsoption_charp(ca_bundle)[0] != '\0') {
		SetHttpSessionAttrs(
			amihttp_session,
			HTSA_CA_BUNDLE_PATH, (ULONG)nsoption_charp(ca_bundle),
			TAG_DONE);
	}

	if (urldb_get_cert_permissions(f->url)) {
		ssl_verify = HTSSL_VERIFY_NONE;
	} else {
		ssl_verify = HTSSL_VERIFY_PEER;
	}
	SetHttpSessionAttrs(
		amihttp_session,
		HTSA_SSL_VERIFY, ssl_verify,
		TAG_DONE);

	auth = urldb_get_auth_details(f->url, NULL);
	if (auth != NULL) {
		SetHttpSessionAttrs(
			amihttp_session,
			HTSA_CREDENTIALS, (ULONG)auth,
			TAG_DONE);
	} else {
		SetHttpSessionAttrs(
			amihttp_session,
			HTSA_CREDENTIALS, (ULONG)NULL,
			TAG_DONE);
	}

	if (nsoption_bool(http_proxy) &&
	    nsoption_charp(http_proxy_host) != NULL &&
	    nsoption_charp(http_proxy_host)[0] != '\0') {
		proxy_port = nsoption_int(http_proxy_port);
		snprintf(amihttp_proxy_buf, sizeof(amihttp_proxy_buf),
			 "%s:%d",
			 nsoption_charp(http_proxy_host),
			 proxy_port);
		SetHttpSessionAttrs(
			amihttp_session,
			HTSA_PROXY, (ULONG)amihttp_proxy_buf,
			TAG_DONE);

		if (nsoption_int(http_proxy_auth) !=
		    OPTION_HTTP_PROXY_AUTH_NONE) {
			snprintf(amihttp_proxy_auth_buf,
				 sizeof(amihttp_proxy_auth_buf),
				 "%s:%s",
				 nsoption_charp(http_proxy_auth_user) ?
				 nsoption_charp(http_proxy_auth_user) : "",
				 nsoption_charp(http_proxy_auth_pass) ?
				 nsoption_charp(http_proxy_auth_pass) : "");
			SetHttpSessionAttrs(
				amihttp_session,
				HTSA_PROXY_AUTH, (ULONG)amihttp_proxy_auth_buf,
				TAG_DONE);
		}
	} else {
		SetHttpSessionAttrs(
			amihttp_session,
			HTSA_PROXY, (ULONG)NULL,
			HTSA_PROXY_AUTH, (ULONG)NULL,
			TAG_DONE);
	}
}

static bool
fetch_amihttp_start(void *handle)
{
	struct amihttp_fetch_info *f = handle;
	struct HttpTransaction *txn;
	const char *method;
	char lang[96];
	char charset[96];
	int i;
	LONG ok;

	if (amihttp_session == NULL || amihttp_notify_sig == -1) {
		return false;
	}

	amihttp_configure_session(f);

	txn = NewHttpTransaction(amihttp_session);
	if (txn == NULL) {
		return false;
	}

	method = "GET";
	if (f->post_urlenc != NULL || f->post_multipart != NULL) {
		method = "POST";
	}

	ok = SetHttpTransactionAttrs(
		txn,
		HTTA_URL, (ULONG)nsurl_access(f->url),
		HTTA_METHOD, (ULONG)method,
		TAG_DONE);
	if (!ok) {
		DisposeHttpTransaction(txn);
		return false;
	}

	if (f->post_urlenc != NULL) {
		SetHttpTransactionAttrs(
			txn,
			HTTA_POST_BODY, (ULONG)f->post_urlenc,
			HTTA_POST_LENGTH, (ULONG)strlen(f->post_urlenc),
			HTTA_CONTENT_TYPE,
				(ULONG)"application/x-www-form-urlencoded",
			TAG_DONE);
	} else if (f->post_multipart != NULL) {
		if (amihttp_build_multipart(f) == false) {
			DisposeHttpTransaction(txn);
			return false;
		}
		SetHttpTransactionAttrs(
			txn,
			HTTA_FORM_MULTIPART, (ULONG)&f->form_parts,
			TAG_DONE);
	}

	/* Default Accept-Language / Charset / DNT like the curl fetcher. */
	if (nsoption_charp(accept_language) != NULL &&
	    nsoption_charp(accept_language)[0] != '\0') {
		snprintf(lang, sizeof(lang), "%s, *;q=0.1",
			 nsoption_charp(accept_language));
		HttpTransactionAddHeader(txn, (STRPTR)"Accept-Language",
					 (STRPTR)lang);
	}
	if (nsoption_charp(accept_charset) != NULL &&
	    nsoption_charp(accept_charset)[0] != '\0') {
		snprintf(charset, sizeof(charset), "%s, *;q=0.1",
			 nsoption_charp(accept_charset));
		HttpTransactionAddHeader(txn, (STRPTR)"Accept-Charset",
					 (STRPTR)charset);
	}
	if (nsoption_bool(do_not_track)) {
		HttpTransactionAddHeader(txn, (STRPTR)"DNT", (STRPTR)"1");
	}

	for (i = 0; i < f->req_header_count; i++) {
		amihttp_add_request_header_line(txn, f->req_headers[i]);
	}

	f->cookie_string = urldb_get_cookie(f->url, true);
	if (f->cookie_string != NULL) {
		HttpTransactionAddHeader(txn, (STRPTR)"Cookie",
					 (STRPTR)f->cookie_string);
	}

	f->txn = txn;
	if (amihttp_start_worker(f) == false) {
		NSLOG(netsurf, INFO, "amihttp worker start failed for %s",
		      nsurl_access(f->url));
		DisposeHttpTransaction(txn);
		f->txn = NULL;
		amihttp_free_form_parts(f);
		return false;
	}

	f->started = true;
	f->phase = AH_WAIT;
	RING_INSERT(amihttp_ring, f);

	NSLOG(netsurf, INFO, "amihttp start %s (worker, notify_sig=%d)",
	      nsurl_access(f->url), (int)amihttp_notify_sig);
	return true;
}

static void
fetch_amihttp_abort(void *handle)
{
	struct amihttp_fetch_info *f = handle;

	f->abort = true;
	/* Never AbortHttpTransaction — library frees Conn under the worker. */
	if (f->txn != NULL && f->phase == AH_WAIT &&
	    f->perform_done == false && f->worker_task != NULL) {
		Signal(f->worker_task, SIGBREAKF_CTRL_C);
	}
}

static void
fetch_amihttp_free(void *handle)
{
	struct amihttp_fetch_info *f = handle;
	int i;

	amihttp_stop_txn(f);
	amihttp_free_form_parts(f);

	free(f->body_data);
	f->body_data = NULL;

	free(f->cookie_string);
	free(f->post_urlenc);
	fetch_multipart_data_destroy(f->post_multipart);
	free(f->location);
	free(f->realm);

	if (f->req_headers != NULL) {
		for (i = 0; i < f->req_header_count; i++) {
			free(f->req_headers[i]);
		}
		free(f->req_headers);
	}

	if (f->host != NULL) {
		lwc_string_unref(f->host);
	}
	if (f->url != NULL) {
		nsurl_unref(f->url);
	}

	free(f);
}

/**
 * Progress one fetch that is waiting or draining the body.
 *
 * \return true if the fetch is still active (caller must keep it in the ring).
 */
static bool
amihttp_poll_one(struct amihttp_fetch_info *f)
{
	LONG err;
	ULONG budget;
	fetch_msg msg;
	size_t n;

	if (f->abort) {
		NSLOG(netsurf, INFO, "amihttp abort %s",
		      f->url != NULL ? nsurl_access(f->url) : "(null)");
		amihttp_finish(f, false);
		return false;
	}

	if (f->phase == AH_WAIT) {
		if (f->perform_done == false) {
			return true;
		}

		err = HttpTransactionGetLastError(f->txn);
		NSLOG(netsurf, INFO,
		      "amihttp complete wait %s rv=%ld err=%ld status=%ld (%s)",
		      nsurl_access(f->url),
		      (long)f->perform_rv,
		      (long)err,
		      (long)HttpTransactionGetStatusCode(f->txn),
		      HttpGetErrorString(err) != NULL
			      ? (char *)HttpGetErrorString(err) : "");

		if ((f->perform_rv == 0 || err != 0) &&
		    HttpTransactionGetStatusCode(f->txn) == 0) {
			amihttp_send_error(f, err != 0 ? err : ERROR_HTTP_PROTOCOL);
			amihttp_finish(f, false);
			return false;
		}

		if (amihttp_process_headers(f)) {
			/* Redirect/auth/error — FETCH_* already sent; llcache
			 * may have started a replacement fetch and aborted us.
			 */
			amihttp_finish(f, false);
			return false;
		}

		f->phase = AH_BODY;
	}

	if (f->phase == AH_BODY) {
		budget = 0;
		while (budget < AMIHTTP_POLL_BUDGET &&
		       f->body_sent < f->body_len) {
			n = f->body_len - f->body_sent;
			if (n > AMIHTTP_BODY_CHUNK) {
				n = AMIHTTP_BODY_CHUNK;
			}

			if (f->abort) {
				amihttp_finish(f, false);
				return false;
			}

			msg.type = FETCH_DATA;
			msg.data.header_or_data.buf = f->body_data + f->body_sent;
			msg.data.header_or_data.len = n;
			fetch_send_callback(&msg, f->fetch_handle);
			f->body_sent += n;
			budget += n;
		}

		if (f->body_sent >= f->body_len) {
			NSLOG(netsurf, INFO,
			      "amihttp finished body %s bytes=%lu",
			      nsurl_access(f->url),
			      (unsigned long)f->body_len);
			amihttp_finish(f, true);
			return false;
		}
	}

	return true;
}

static void
fetch_amihttp_poll(lwc_string *scheme)
{
	struct amihttp_fetch_info *f;
	struct amihttp_fetch_info *save_ring = NULL;
	ULONG got;

	(void)scheme;

	/*
	 * Worker Signals us when Perform returns.  Clear the bit if set;
	 * ami_get_msg already Wait()s on this signal so the UI stays awake.
	 */
	if (amihttp_notify_sig != -1) {
		got = SetSignal(0, 1UL << amihttp_notify_sig);
		if (got & (1UL << amihttp_notify_sig)) {
			NSLOG(netsurf, INFO, "amihttp notify signal received");
		}
	}

	/*
	 * Detach each entry before polling so FETCH_REDIRECT (which starts a
	 * new fetch and aborts this one) cannot leave us iterating a freed
	 * ring node.  New starts land on amihttp_ring and are processed in
	 * this same loop; survivors go on save_ring.
	 */
	while (amihttp_ring != NULL) {
		f = amihttp_ring;
		RING_REMOVE(amihttp_ring, f);

		if (amihttp_poll_one(f)) {
			RING_INSERT(save_ring, f);
		}
	}

	amihttp_ring = save_ring;
}

/* exported interface documented in content/fetchers/amihttp.h */
nserror
fetch_amihttp_register(void)
{
	lwc_string *scheme;
	nserror ret;
	const struct fetcher_operation_table fetcher_ops = {
		.initialise = fetch_amihttp_initialise,
		.acceptable = fetch_amihttp_can_fetch,
		.setup = fetch_amihttp_setup,
		.start = fetch_amihttp_start,
		.abort = fetch_amihttp_abort,
		.free = fetch_amihttp_free,
		.poll = fetch_amihttp_poll,
		.fdset = NULL,
		.finalise = fetch_amihttp_finalise
	};

	HttpBase = OpenLibrary(AMIHTTPNAME, AMIHTTPVERSION);
	if (HttpBase == NULL) {
		NSLOG(netsurf, ERROR, "amihttp.library open failed");
		return NSERROR_INIT_FAILED;
	}

	amihttp_notify_sig = AllocSignal(-1L);
	if (amihttp_notify_sig == -1) {
		NSLOG(netsurf, ERROR, "amihttp AllocSignal failed");
		CloseLibrary(HttpBase);
		HttpBase = NULL;
		return NSERROR_INIT_FAILED;
	}

	amihttp_session = NewHttpSession();
	if (amihttp_session == NULL) {
		NSLOG(netsurf, ERROR, "NewHttpSession failed");
		FreeSignal(amihttp_notify_sig);
		amihttp_notify_sig = -1;
		CloseLibrary(HttpBase);
		HttpBase = NULL;
		return NSERROR_INIT_FAILED;
	}

	/* Base tags: CA bundle, timeouts, peer verify. */
	if (nsoption_charp(ca_bundle) != NULL &&
	    nsoption_charp(ca_bundle)[0] != '\0') {
		HttpBaseTags(
			HTBT_DEFAULT_TIMEOUT, (ULONG)15,
			HTBT_SSL_VERIFY, (ULONG)HTSSL_VERIFY_PEER,
			HTBT_CA_BUNDLE_PATH, (ULONG)nsoption_charp(ca_bundle),
			HTBT_BREAKMASK, (ULONG)SIGBREAKF_CTRL_C,
			TAG_DONE);
		NSLOG(netsurf, INFO, "amihttp CA bundle %s",
		      nsoption_charp(ca_bundle));
	} else {
		HttpBaseTags(
			HTBT_DEFAULT_TIMEOUT, (ULONG)15,
			HTBT_SSL_VERIFY, (ULONG)HTSSL_VERIFY_PEER,
			HTBT_BREAKMASK, (ULONG)SIGBREAKF_CTRL_C,
			TAG_DONE);
	}

	SetHttpSessionAttrs(
		amihttp_session,
		HTSA_FOLLOW_REDIRECTS, (ULONG)FALSE,
		HTSA_ACCEPT_ENCODING, (ULONG)"gzip",
		HTSA_KEEPALIVE, (ULONG)FALSE,
		HTSA_CONNECT_TIMEOUT, (ULONG)15,
		HTSA_READ_TIMEOUT, (ULONG)30,
		HTSA_MAX_CONNECTIONS,
			(ULONG)(nsoption_int(max_fetchers) +
				nsoption_int(max_cached_fetch_handles)),
		TAG_DONE);

	scheme = lwc_string_ref(corestring_lwc_http);
	ret = fetcher_add(scheme, &fetcher_ops);
	if (ret != NSERROR_OK) {
		DisposeHttpSession(amihttp_session);
		amihttp_session = NULL;
		FreeSignal(amihttp_notify_sig);
		amihttp_notify_sig = -1;
		CloseLibrary(HttpBase);
		HttpBase = NULL;
		return ret;
	}

	scheme = lwc_string_ref(corestring_lwc_https);
	ret = fetcher_add(scheme, &fetcher_ops);
	if (ret != NSERROR_OK) {
		return ret;
	}

	NSLOG(netsurf, INFO, "amihttp fetcher registered");
	return NSERROR_OK;
}

BYTE
fetch_amihttp_signal(void)
{
	return amihttp_notify_sig;
}

#endif /* WITH_AMIHTTP */
