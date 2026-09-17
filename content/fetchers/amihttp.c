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
 * run HttpTransactionPerform (sync) on NetSurf-owned persistent worker tasks
 * with a large stack, then poll for completion via a
 * shared Exec notify signal.
 *
 * Keep-alive is enabled per worker HttpSession.  Workers stay alive across
 * fetches and each owns its own session/pool partition (HTSA_TASK_SERIAL =
 * that task) so AmiHTTP never hands a bsdsocket handle to a different Exec
 * task — sharing one pool across workers corrupted AmiTCP on exit.
 * Bodies are fully drained on the worker before Dispose so the library can
 * return the connection to that worker's pool.
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
#include <proto/intuition.h>
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

/** Stack for NetSurf perform worker (TLS needs more than amihttp's 8K). */
#define AMIHTTP_WORKER_STACK 49152

/** Cap workers — keep stack budget modest on classic Amiga RAM. */
#define AMIHTTP_MAX_WORKERS 8

/**
 * Seconds a fetch may sit in AH_WAIT before we poke CTRL_C.
 * Leave headroom under FAIL so AmiHTTP can unwind after the signal.
 */
#define AMIHTTP_WATCHDOG_SECS 20

/** Hard-fail after this many seconds in AH_WAIT (must be > WATCHDOG, ≤30). */
#define AMIHTTP_WATCHDOG_FAIL_SECS 30
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
	int worker_idx;
	ULONG wait_started_sec;
	bool watchdog_signalled;
	/** Job pointer so a wedged fetch can detach without Free-while-Perform. */
	volatile struct amihttp_worker_job *job;
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

/**
 * Job handed to a persistent worker.  Allocated on main, freed by the worker
 * after Perform + body drain (or immediately if the worker is quitting).
 */
struct amihttp_worker_job {
	struct amihttp_fetch_info *f;
	struct HttpTransaction *txn;
	struct Task *notify_task;
	BYTE notify_sig;
};

/**
 * Long-lived Perform task with its own HttpSession.
 *
 * Keep-alive sockets are task-affine (bsdsocket handle).  Sharing one session
 * pool across workers let task B reuse task A's socket — concurrent I/O on the
 * same SocketBase corrupts AmiTCP (corrupt memory list on exit).  Each worker
 * therefore owns its session + pool partition (HTSA_TASK_SERIAL = this task).
 */
struct amihttp_worker {
	struct Task *task;
	struct HttpSession *session;
	lwc_string *last_host;
	BYTE wake_sigbit;
	volatile bool ready;
	volatile bool busy;
	volatile bool quit;
	volatile struct amihttp_worker_job *pending;
};

struct Library *HttpBase = NULL;

static int amihttp_fetchers_registered = 0;
static BYTE amihttp_notify_sig = -1;
static struct amihttp_fetch_info *amihttp_ring = NULL;
static char amihttp_proxy_buf[256];
static char amihttp_proxy_auth_buf[256];
static struct amihttp_worker amihttp_workers[AMIHTTP_MAX_WORKERS];
static int amihttp_worker_count = 0;

/**
 * Wall-clock seconds for the fetch watchdog (intuition CurrentTime).
 */
static ULONG
amihttp_now_sec(void)
{
	ULONG secs;
	ULONG micros;

	secs = 0;
	micros = 0;
	CurrentTime(&secs, &micros);
	return secs;
}

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
 * Fatal fetch failure for the watchdog — must be FETCH_ERROR.
 * FETCH_TIMEDOUT makes llcache retry, so HTML stays blocked on stylesheets.
 */
static void
amihttp_send_fatal_error(struct amihttp_fetch_info *f, const char *why)
{
	fetch_msg msg;

	NSLOG(netsurf, WARNING, "amihttp fatal: %s for %s",
	      why != NULL ? why : "failed",
	      f->url != NULL ? nsurl_access(f->url) : "(null)");

	msg.type = FETCH_ERROR;
	msg.data.error = why != NULL ? why : "HTTP transfer failed";
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
			/* Give up: detach so we do not FreeMem under Perform. */
			if (spins >= 500) {
				Forbid();
				if (f->job != NULL) {
					((struct amihttp_worker_job *)f->job)->f =
						NULL;
					f->job = NULL;
				}
				/* Worker will DisposeHttpTransaction. */
				f->txn = NULL;
				f->perform_done = true;
				Permit();
				NSLOG(netsurf, WARNING,
				      "amihttp_stop_txn: detached wedged fetch %s",
				      f->url != NULL ? nsurl_access(f->url)
						     : "(null)");
				break;
			}
		}
	}

	if (f->txn != NULL) {
		DisposeHttpTransaction(f->txn);
		f->txn = NULL;
	}
	f->worker_task = NULL;
	f->phase = AH_DONE;
}

/**
 * Append ReadBody chunks into f->body_data.  Runs on the worker task only.
 *
 * Must run to EOF on success so AmiHTTP's ht_http_body_finish can release the
 * socket into the keep-alive pool.  DisposeHttpTransaction with ht_Conn still
 * set always closes (keepalive=FALSE).
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
 * Persistent worker: Perform + ReadBody, then wait for the next job.
 *
 * Own HttpSession + TaskSerial == this task so keep-alive sockets stay on
 * this SocketBase.  On quit, flush this task's pool before RemTask.
 */
static void
amihttp_perform_worker(void)
{
	struct amihttp_worker *w;
	struct amihttp_worker_job *job;
	struct amihttp_fetch_info *f;
	struct Task *self;
	BYTE wake_sig;
	LONG rv;
	ULONG wakeset;

	self = FindTask(NULL);
	w = (struct amihttp_worker *)self->tc_UserData;
	if (w == NULL) {
		RemTask(NULL);
		return;
	}

	wake_sig = AllocSignal(-1L);
	if (wake_sig == -1) {
		w->task = NULL;
		w->ready = true;
		RemTask(NULL);
		return;
	}

	/*
	 * Partition the keep-alive pool by this Exec task so HTBT_POOL_FLUSH
	 * with ti_Data==0 (on quit) closes only our sockets, on this task.
	 */
	if (w->session != NULL) {
		SetHttpSessionAttrs(
			w->session,
			HTSA_KEEPALIVE, (ULONG)TRUE,
			HTSA_TASK_SERIAL, (ULONG)self,
			TAG_DONE);
	}

	w->wake_sigbit = wake_sig;
	w->task = self;
	w->ready = true;
	wakeset = (1UL << wake_sig) | SIGBREAKF_CTRL_C;

	for (;;) {
		Wait(wakeset);

		if (w->quit) {
			break;
		}

		job = (struct amihttp_worker_job *)w->pending;
		if (job == NULL) {
			/* Spurious CTRL_C while idle — ignore. */
			continue;
		}
		w->pending = NULL;

		f = job->f;
		rv = HttpTransactionPerform(job->txn);
		/*
		 * f may have been detached by the main-task watchdog while we
		 * were inside Perform — do not touch fetch_info in that case.
		 */
		f = job->f;
		if (f != NULL) {
			f->perform_rv = rv;
			/*
			 * Perform returns TRUE (non-zero) on success.  Drain
			 * the entity so keep-alive can pool the socket.
			 */
			if (rv != 0) {
				amihttp_worker_drain_body(f, job->txn);
			} else if (HttpBase != NULL) {
				/*
				 * Failed Perform often leaves a dead socket in
				 * this worker's pool (watchdog CTRL_C, SSL
				 * death).  Next navigate then fails instantly
				 * with 8702 keepalive reuse — flush here.
				 */
				HttpBaseTags(HTBT_POOL_FLUSH, (ULONG)FALSE,
					     TAG_DONE);
			}
			f->perform_done = true;
			f->job = NULL;
		} else if (job->txn != NULL) {
			/* Orphaned (watchdog detach): drain if Perform
			 * succeeded, always dispose, flush poisoned pool.
			 */
			if (rv != 0) {
				uint8_t sink[AMIHTTP_BODY_CHUNK];
				LONG n;

				for (;;) {
					n = HttpTransactionReadBody(job->txn,
							(APTR)sink,
							(ULONG)sizeof(sink));
					if (n <= 0) {
						break;
					}
				}
			}
			DisposeHttpTransaction(job->txn);
			job->txn = NULL;
			if (HttpBase != NULL) {
				HttpBaseTags(HTBT_POOL_FLUSH, (ULONG)FALSE,
					     TAG_DONE);
			}
		}

		if (job->notify_task != NULL && job->notify_sig != -1) {
			Signal(job->notify_task, 1UL << job->notify_sig);
		}

		FreeMem(job, sizeof(*job));
		w->busy = false;
	}

	job = (struct amihttp_worker_job *)w->pending;
	if (job != NULL) {
		w->pending = NULL;
		if (job->f != NULL) {
			job->f->perform_done = true;
			job->f->perform_rv = 0;
		}
		FreeMem(job, sizeof(*job));
	}
	w->busy = false;

	/*
	 * Close idle keep-alive sockets on THIS task before RemTask.  Main
	 * must never HTBT_POOL_FLUSH(TRUE) — CloseSocket/CloseLibrary of
	 * another task's bsdsocket corrupts AmiTCP.
	 */
	if (HttpBase != NULL) {
		HttpBaseTags(HTBT_POOL_FLUSH, (ULONG)FALSE, TAG_DONE);
		HttpBaseTags(HTBT_TASK_SSL_RELEASE, (ULONG)TRUE, TAG_DONE);
		HttpBaseTags(HTBT_TASK_SOCKET_RELEASE, (ULONG)TRUE, TAG_DONE);
	}

	if (w->session != NULL) {
		DisposeHttpSession(w->session);
		w->session = NULL;
	}

	w->task = NULL;
	FreeSignal(wake_sig);
	RemTask(NULL);
}

/**
 * Create one persistent worker and wait until it is ready to accept jobs.
 */
static bool
amihttp_spawn_worker(struct amihttp_worker *w)
{
	struct Process *proc;
	ULONG spins;

	w->task = NULL;
	w->wake_sigbit = -1;
	w->ready = false;
	w->busy = false;
	w->quit = false;
	w->pending = NULL;
	w->last_host = NULL;

	w->session = NewHttpSession();
	if (w->session == NULL) {
		return false;
	}

	SetHttpSessionAttrs(
		w->session,
		HTSA_FOLLOW_REDIRECTS, (ULONG)FALSE,
		HTSA_ACCEPT_ENCODING, (ULONG)"gzip",
		HTSA_KEEPALIVE, (ULONG)TRUE,
		HTSA_CONNECT_TIMEOUT, (ULONG)nsoption_uint(curl_fetch_timeout),
		HTSA_READ_TIMEOUT, (ULONG)nsoption_uint(curl_fetch_timeout),
		HTSA_MAX_CONNECTIONS,
			(ULONG)(nsoption_int(max_fetchers_per_host) +
				nsoption_int(max_cached_fetch_handles)),
		TAG_DONE);

	Forbid();
	proc = CreateNewProcTags(
		NP_Entry, (ULONG)amihttp_perform_worker,
		NP_StackSize, (ULONG)AMIHTTP_WORKER_STACK,
		NP_Name, (ULONG)"ns-amihttp",
		TAG_END);
	if (proc == NULL) {
		Permit();
		DisposeHttpSession(w->session);
		w->session = NULL;
		return false;
	}
	((struct Task *)proc)->tc_UserData = (APTR)w;
	Permit();

	spins = 0;
	while (w->ready == false && spins < 500) {
		Delay(1);
		spins++;
	}

	if (w->ready == false || w->task == NULL || w->wake_sigbit == -1) {
		NSLOG(netsurf, ERROR, "amihttp worker spawn timed out");
		w->quit = true;
		DisposeHttpSession(w->session);
		w->session = NULL;
		return false;
	}

	return true;
}

/**
 * Start persistent workers used for keep-alive connection reuse.
 */
static bool
amihttp_workers_start(int want)
{
	int i;

	if (want < 1) {
		want = 1;
	}
	if (want > AMIHTTP_MAX_WORKERS) {
		want = AMIHTTP_MAX_WORKERS;
	}

	amihttp_worker_count = 0;
	for (i = 0; i < want; i++) {
		if (amihttp_spawn_worker(&amihttp_workers[i]) == false) {
			break;
		}
		amihttp_worker_count++;
	}

	if (amihttp_worker_count < 1) {
		NSLOG(netsurf, ERROR, "amihttp: no workers started");
		return false;
	}

	NSLOG(netsurf, INFO, "amihttp: %d persistent workers (keepalive)",
	      amihttp_worker_count);
	return true;
}

/**
 * Signal workers to exit and wait for RemTask (each flushes its own pool).
 */
static void
amihttp_workers_stop(void)
{
	int i;
	ULONG spins;
	struct amihttp_worker *w;
	bool any;

	for (i = 0; i < amihttp_worker_count; i++) {
		w = &amihttp_workers[i];
		w->quit = true;
		if (w->task != NULL && w->wake_sigbit != -1) {
			Signal(w->task, 1UL << w->wake_sigbit);
		}
	}

	spins = 0;
	for (;;) {
		any = false;
		for (i = 0; i < amihttp_worker_count; i++) {
			if (amihttp_workers[i].task != NULL) {
				any = true;
				break;
			}
		}
		if (any == false) {
			break;
		}
		Delay(1);
		spins++;
		if ((spins % 500) == 0) {
			NSLOG(netsurf, WARNING,
			      "amihttp_workers_stop: waiting (%lu ticks)",
			      (unsigned long)spins);
		}
	}

	for (i = 0; i < amihttp_worker_count; i++) {
		w = &amihttp_workers[i];
		if (w->last_host != NULL) {
			lwc_string_unref(w->last_host);
			w->last_host = NULL;
		}
		/* Session should already be disposed on the worker task. */
		if (w->session != NULL) {
			DisposeHttpSession(w->session);
			w->session = NULL;
		}
	}

	amihttp_worker_count = 0;
}

/**
 * Claim an idle worker, preferring one that last fetched the same host
 * so keep-alive sockets are more likely to be reused.
 */
static struct amihttp_worker *
amihttp_claim_worker(lwc_string *host)
{
	struct amihttp_worker *w;
	struct amihttp_worker *fallback;
	int i;
	bool match;

	w = NULL;
	fallback = NULL;

	Forbid();
	for (i = 0; i < amihttp_worker_count; i++) {
		if (amihttp_workers[i].task == NULL ||
		    amihttp_workers[i].busy ||
		    amihttp_workers[i].quit ||
		    amihttp_workers[i].session == NULL) {
			continue;
		}
		match = false;
		if (host != NULL && amihttp_workers[i].last_host != NULL) {
			if (amihttp_workers[i].last_host == host) {
				match = true;
			}
		}
		if (match) {
			amihttp_workers[i].busy = true;
			w = &amihttp_workers[i];
			break;
		}
		if (fallback == NULL) {
			fallback = &amihttp_workers[i];
		}
	}
	if (w == NULL && fallback != NULL) {
		fallback->busy = true;
		w = fallback;
	}
	Permit();

	return w;
}

/**
 * Queue Perform+drain on an already-claimed worker.
 */
static bool
amihttp_queue_worker(struct amihttp_worker *w, struct amihttp_fetch_info *f)
{
	struct amihttp_worker_job *job;

	job = AllocMem(sizeof(*job), MEMF_PUBLIC | MEMF_CLEAR);
	if (job == NULL) {
		w->busy = false;
		return false;
	}

	job->f = f;
	job->txn = f->txn;
	job->notify_task = FindTask(NULL);
	job->notify_sig = amihttp_notify_sig;
	f->perform_done = false;
	f->perform_rv = 0;
	f->worker_task = w->task;
	f->worker_idx = (int)(w - amihttp_workers);
	f->wait_started_sec = amihttp_now_sec();
	f->watchdog_signalled = false;
	f->job = job;

	if (f->host != NULL) {
		if (w->last_host != NULL) {
			lwc_string_unref(w->last_host);
		}
		w->last_host = lwc_string_ref(f->host);
	}

	/*
	 * Assign pending under Forbid so the worker cannot observe a half
	 * update if it is already runnable; then wake it.
	 */
	Forbid();
	w->pending = job;
	Permit();
	Signal(w->task, 1UL << w->wake_sigbit);
	return true;
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

	amihttp_workers_stop();

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

	/* SVG may be rendered by svg.datatype when installed. */

	f = calloc(1, sizeof(*f));
	if (f == NULL) {
		return NULL;
	}

	f->fetch_handle = parent_fetch;
	f->only_2xx = only_2xx;
	f->phase = AH_DONE;
	f->worker_idx = -1;
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
 * Does not touch HTSA_TASK_SERIAL (owned by the worker task).
 */
static void
amihttp_configure_session(struct HttpSession *session,
			  struct amihttp_fetch_info *f)
{
	const char *auth;
	ULONG ssl_verify;
	int proxy_port;

	if (session == NULL) {
		return;
	}

	SetHttpSessionAttrs(
		session,
		HTSA_USERAGENT, (ULONG)user_agent_string(),
		HTSA_FOLLOW_REDIRECTS, (ULONG)FALSE,
		HTSA_ACCEPT_ENCODING, (ULONG)"gzip",
		HTSA_KEEPALIVE, (ULONG)TRUE,
		HTSA_CONNECT_TIMEOUT,
			(ULONG)nsoption_uint(curl_fetch_timeout),
		HTSA_READ_TIMEOUT,
			(ULONG)nsoption_uint(curl_fetch_timeout),
		TAG_DONE);

	if (nsoption_charp(ca_bundle) != NULL &&
	    nsoption_charp(ca_bundle)[0] != '\0') {
		SetHttpSessionAttrs(
			session,
			HTSA_CA_BUNDLE_PATH, (ULONG)nsoption_charp(ca_bundle),
			TAG_DONE);
	}

	if (urldb_get_cert_permissions(f->url)) {
		ssl_verify = HTSSL_VERIFY_NONE;
	} else {
		ssl_verify = HTSSL_VERIFY_PEER;
	}
	SetHttpSessionAttrs(
		session,
		HTSA_SSL_VERIFY, ssl_verify,
		TAG_DONE);

	auth = urldb_get_auth_details(f->url, NULL);
	if (auth != NULL) {
		SetHttpSessionAttrs(
			session,
			HTSA_CREDENTIALS, (ULONG)auth,
			TAG_DONE);
	} else {
		SetHttpSessionAttrs(
			session,
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
			session,
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
				session,
				HTSA_PROXY_AUTH, (ULONG)amihttp_proxy_auth_buf,
				TAG_DONE);
		}
	} else {
		SetHttpSessionAttrs(
			session,
			HTSA_PROXY, (ULONG)NULL,
			HTSA_PROXY_AUTH, (ULONG)NULL,
			TAG_DONE);
	}
}

static bool
fetch_amihttp_start(void *handle)
{
	struct amihttp_fetch_info *f = handle;
	struct amihttp_worker *w;
	struct HttpTransaction *txn;
	const char *method;
	char lang[96];
	char charset[96];
	int i;
	LONG ok;

	if (amihttp_notify_sig == -1 || amihttp_worker_count < 1) {
		return false;
	}

	w = amihttp_claim_worker(f->host);
	if (w == NULL) {
		NSLOG(netsurf, WARNING, "amihttp: no idle worker for %s",
		      nsurl_access(f->url));
		return false;
	}

	amihttp_configure_session(w->session, f);

	txn = NewHttpTransaction(w->session);
	if (txn == NULL) {
		w->busy = false;
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
		w->busy = false;
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
			w->busy = false;
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
	if (amihttp_queue_worker(w, f) == false) {
		NSLOG(netsurf, INFO, "amihttp worker queue failed for %s",
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
	struct HttpTiming timing;
	ULONG connect_ms;

	if (f->abort) {
		NSLOG(netsurf, INFO, "amihttp abort %s",
		      f->url != NULL ? nsurl_access(f->url) : "(null)");
		amihttp_finish(f, false);
		return false;
	}

	if (f->phase == AH_WAIT) {
		if (f->perform_done == false) {
			ULONG now;
			ULONG elapsed;
			struct amihttp_worker *w;

			now = amihttp_now_sec();
			elapsed = (now >= f->wait_started_sec)
				? (now - f->wait_started_sec) : 0;

			/*
			 * Stuck fetch: HTML conversion waits forever while
			 * stylesheet fetches sit in AH_WAIT (amigans xoops.css
			 * / style.css never completed in ns.log).  Re-signal
			 * wake if the job never left pending; CTRL_C if AmiHTTP
			 * I/O is wedged past the watchdog.
			 */
			if (f->worker_idx >= 0 &&
			    f->worker_idx < amihttp_worker_count) {
				w = &amihttp_workers[f->worker_idx];
				if (w->task != NULL &&
				    w->pending != NULL &&
				    w->wake_sigbit != -1) {
					Signal(w->task,
					       1UL << w->wake_sigbit);
				}
			}

			if (elapsed >= AMIHTTP_WATCHDOG_SECS &&
			    f->watchdog_signalled == false) {
				f->watchdog_signalled = true;
				NSLOG(netsurf, WARNING,
				      "amihttp watchdog %lus: poking CTRL_C for %s",
				      (unsigned long)elapsed,
				      f->url != NULL ? nsurl_access(f->url)
						     : "(null)");
				if (f->worker_task != NULL) {
					Signal(f->worker_task,
					       SIGBREAKF_CTRL_C);
				}
			}

			/*
			 * Still wedged: fail with FETCH_ERROR (not TIMEDOUT) so
			 * llcache does not retry and HTML can leave stylesheet
			 * wait. Wikipedia's combined load.php CSS hung ~81s.
			 */
			if (elapsed >= AMIHTTP_WATCHDOG_FAIL_SECS) {
				NSLOG(netsurf, WARNING,
				      "amihttp watchdog %lus: failing wedged %s",
				      (unsigned long)elapsed,
				      f->url != NULL ? nsurl_access(f->url)
						     : "(null)");
				amihttp_send_fatal_error(f,
						"Read timed out (watchdog)");
				amihttp_finish(f, false);
				return false;
			}

			return true;
		}

		err = HttpTransactionGetLastError(f->txn);
		connect_ms = 0;
		if (HttpTransactionGetTiming(f->txn, &timing)) {
			connect_ms = timing.ht_ConnectMs;
		}
		NSLOG(netsurf, INFO,
		      "amihttp complete wait %s rv=%ld err=%ld "
		      "status=%ld connect_ms=%lu%s (%s)",
		      nsurl_access(f->url),
		      (long)f->perform_rv,
		      (long)err,
		      (long)HttpTransactionGetStatusCode(f->txn),
		      (unsigned long)connect_ms,
		      connect_ms == 0 ? " [keepalive reuse]" : "",
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

	/* Base tags: CA bundle, timeouts, peer verify, pool. */
	if (nsoption_charp(ca_bundle) != NULL &&
	    nsoption_charp(ca_bundle)[0] != '\0') {
		HttpBaseTags(
			HTBT_DEFAULT_TIMEOUT, (ULONG)30,
			HTBT_SSL_VERIFY, (ULONG)HTSSL_VERIFY_PEER,
			HTBT_CA_BUNDLE_PATH, (ULONG)nsoption_charp(ca_bundle),
			HTBT_BREAKMASK, (ULONG)SIGBREAKF_CTRL_C,
			HTBT_MAX_IDLE_CONNECTIONS,
				(ULONG)(nsoption_int(max_cached_fetch_handles) +
					nsoption_int(max_fetchers_per_host)),
			HTBT_IDLE_TIMEOUT, (ULONG)30,
			TAG_DONE);
		NSLOG(netsurf, INFO, "amihttp CA bundle %s",
		      nsoption_charp(ca_bundle));
	} else {
		HttpBaseTags(
			HTBT_DEFAULT_TIMEOUT, (ULONG)30,
			HTBT_SSL_VERIFY, (ULONG)HTSSL_VERIFY_PEER,
			HTBT_BREAKMASK, (ULONG)SIGBREAKF_CTRL_C,
			HTBT_MAX_IDLE_CONNECTIONS,
				(ULONG)(nsoption_int(max_cached_fetch_handles) +
					nsoption_int(max_fetchers_per_host)),
			HTBT_IDLE_TIMEOUT, (ULONG)30,
			TAG_DONE);
	}

	if (amihttp_workers_start(nsoption_int(max_fetchers)) == false) {
		FreeSignal(amihttp_notify_sig);
		amihttp_notify_sig = -1;
		CloseLibrary(HttpBase);
		HttpBase = NULL;
		return NSERROR_INIT_FAILED;
	}

	scheme = lwc_string_ref(corestring_lwc_http);
	ret = fetcher_add(scheme, &fetcher_ops);
	if (ret != NSERROR_OK) {
		amihttp_workers_stop();
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

	NSLOG(netsurf, INFO, "amihttp fetcher registered (keepalive on)");
	return NSERROR_OK;
}

BYTE
fetch_amihttp_signal(void)
{
	return amihttp_notify_sig;
}

#endif /* WITH_AMIHTTP */
