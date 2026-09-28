/*
 * Copyright 2026 Nami / NetSurf contributors
 *
 * This file is part of NetSurf.
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
 * Built-in about:home — search box, recent history, hotlist links.
 */

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "utils/errors.h"
#include "utils/nsurl.h"
#include "netsurf/url_db.h"
#include "desktop/hotlist.h"

#include "private.h"
#include "home.h"

#define HOME_HISTORY_MAX 30
#define HOME_HOTLIST_MAX 40

struct home_hist_entry {
	nsurl *url;
	const char *title;
	time_t last_visit;
	unsigned int visits;
};

struct home_hist_ctx {
	struct home_hist_entry entries[HOME_HISTORY_MAX];
	int count;
};

struct home_hot_ctx {
	struct fetch_about_context *fetch;
	int count;
	nserror res;
};

/* urldb_iterate_entries has no userdata — hold ctx for the walk. */
static struct home_hist_ctx *home_hist_walk_ctx = NULL;

/**
 * Escape text for HTML body / attribute context into dest.
 * Returns bytes written excluding NUL, or -1 if dest too small.
 */
static int
home_html_escape(char *dest, size_t destsz, const char *src)
{
	size_t o;
	const unsigned char *p;
	const char *rep;
	size_t rlen;

	if(dest == NULL || destsz == 0)
		return -1;
	o = 0;
	if(src == NULL)
		src = "";
	p = (const unsigned char *)src;
	while(*p != '\0') {
		rep = NULL;
		rlen = 0;
		if(*p == '&') {
			rep = "&amp;";
			rlen = 5;
		} else if(*p == '<') {
			rep = "&lt;";
			rlen = 4;
		} else if(*p == '>') {
			rep = "&gt;";
			rlen = 4;
		} else if(*p == '"') {
			rep = "&quot;";
			rlen = 6;
		} else if(*p < 0x20) {
			p++;
			continue;
		} else {
			if(o + 1 >= destsz)
				return -1;
			dest[o++] = (char)*p;
			p++;
			continue;
		}
		if(o + rlen >= destsz)
			return -1;
		memcpy(dest + o, rep, rlen);
		o += rlen;
		p++;
	}
	dest[o] = '\0';
	return (int)o;
}

static nserror
home_emit_link(struct fetch_about_context *ctx, struct nsurl *url,
		const char *title)
{
	char esc_title[256];
	char esc_url[1024];
	const char *href;
	const char *label;

	if(url == NULL)
		return NSERROR_OK;

	href = nsurl_access(url);
	if(href == NULL)
		return NSERROR_OK;
	label = title;
	if(label == NULL || label[0] == '\0')
		label = href;

	if(home_html_escape(esc_url, sizeof esc_url, href) < 0)
		return NSERROR_OK;
	if(home_html_escape(esc_title, sizeof esc_title, label) < 0)
		return NSERROR_OK;

	return fetch_about_ssenddataf(ctx,
			"<li><a href=\"%s\">%s</a></li>\n",
			esc_url, esc_title);
}

static bool
home_hist_collect(struct nsurl *url, const struct url_data *data)
{
	struct home_hist_ctx *ctx;
	int i;
	int worst;
	time_t lv;

	ctx = home_hist_walk_ctx;
	if(ctx == NULL || url == NULL || data == NULL)
		return false;
	if(data->visits == 0)
		return true;

	lv = data->last_visit;

	if(ctx->count < HOME_HISTORY_MAX) {
		ctx->entries[ctx->count].url = url;
		ctx->entries[ctx->count].title = data->title;
		ctx->entries[ctx->count].last_visit = lv;
		ctx->entries[ctx->count].visits = data->visits;
		ctx->count++;
		return true;
	}

	worst = 0;
	for(i = 1; i < ctx->count; i++) {
		if(ctx->entries[i].last_visit < ctx->entries[worst].last_visit)
			worst = i;
	}
	if(lv <= ctx->entries[worst].last_visit)
		return true;

	ctx->entries[worst].url = url;
	ctx->entries[worst].title = data->title;
	ctx->entries[worst].last_visit = lv;
	ctx->entries[worst].visits = data->visits;
	return true;
}

static int
home_hist_cmp(const void *a, const void *b)
{
	const struct home_hist_entry *ea;
	const struct home_hist_entry *eb;

	ea = (const struct home_hist_entry *)a;
	eb = (const struct home_hist_entry *)b;
	if(ea->last_visit > eb->last_visit)
		return -1;
	if(ea->last_visit < eb->last_visit)
		return 1;
	if(ea->visits > eb->visits)
		return -1;
	if(ea->visits < eb->visits)
		return 1;
	return 0;
}

static nserror
home_hot_address(void *ctx, struct nsurl *url, const char *title)
{
	struct home_hot_ctx *hctx;
	nserror res;

	hctx = (struct home_hot_ctx *)ctx;
	if(hctx == NULL || hctx->res != NSERROR_OK)
		return NSERROR_OK;
	if(hctx->count >= HOME_HOTLIST_MAX)
		return NSERROR_OK;

	res = home_emit_link(hctx->fetch, url, title);
	if(res != NSERROR_OK) {
		hctx->res = res;
		return res;
	}
	hctx->count++;
	return NSERROR_OK;
}

bool fetch_about_home_handler(struct fetch_about_context *ctx)
{
	nserror res;
	struct home_hist_ctx hist;
	struct home_hot_ctx hot;
	int i;

	fetch_about_set_http_code(ctx, 200);

	if(fetch_about_send_header(ctx,
			"Content-Type: text/html; charset=utf-8"))
		goto home_aborted;

	res = fetch_about_ssenddataf(ctx,
			"<html>\n<head>\n"
			"<title>Home</title>\n"
			"<link rel=\"stylesheet\" type=\"text/css\" "
			"href=\"resource:internal.css\">\n"
			"</head>\n"
			"<body id=\"home\" "
			"class=\"ns-even-bg ns-even-fg ns-border\">\n"
			"<h1 class=\"ns-border\">Home</h1>\n"
			"<form class=\"home-search\" action=\"about:websearch\" "
			"method=\"get\">\n"
			"<input type=\"text\" name=\"q\" size=\"40\" value=\"\">\n"
			"<input type=\"submit\" value=\"Search\">\n"
			"</form>\n");
	if(res != NSERROR_OK)
		goto home_aborted;

	memset(&hist, 0, sizeof hist);
	home_hist_walk_ctx = &hist;
	urldb_iterate_entries(home_hist_collect);
	home_hist_walk_ctx = NULL;

	if(hist.count > 1)
		qsort(hist.entries, (size_t)hist.count,
				sizeof(hist.entries[0]), home_hist_cmp);

	res = fetch_about_ssenddataf(ctx,
			"<section>\n"
			"<h2 class=\"ns-border\">Recent history</h2>\n"
			"<ul>\n");
	if(res != NSERROR_OK)
		goto home_aborted;

	if(hist.count == 0) {
		res = fetch_about_ssenddataf(ctx,
				"<li>No visited pages yet.</li>\n");
		if(res != NSERROR_OK)
			goto home_aborted;
	} else {
		for(i = 0; i < hist.count; i++) {
			res = home_emit_link(ctx, hist.entries[i].url,
					hist.entries[i].title);
			if(res != NSERROR_OK)
				goto home_aborted;
		}
	}

	res = fetch_about_ssenddataf(ctx, "</ul>\n</section>\n");
	if(res != NSERROR_OK)
		goto home_aborted;

	res = fetch_about_ssenddataf(ctx,
			"<section>\n"
			"<h2 class=\"ns-border\">Hotlist</h2>\n"
			"<ul>\n");
	if(res != NSERROR_OK)
		goto home_aborted;

	hot.fetch = ctx;
	hot.count = 0;
	hot.res = NSERROR_OK;
	hotlist_iterate(&hot, NULL, home_hot_address, NULL);
	if(hot.res != NSERROR_OK)
		goto home_aborted;

	if(hot.count == 0) {
		res = fetch_about_ssenddataf(ctx,
				"<li>Hotlist is empty.</li>\n");
		if(res != NSERROR_OK)
			goto home_aborted;
	}

	res = fetch_about_ssenddataf(ctx,
			"</ul>\n</section>\n"
			"</body>\n</html>\n");
	if(res != NSERROR_OK)
		goto home_aborted;

	fetch_about_send_finished(ctx);
	return true;

home_aborted:
	home_hist_walk_ctx = NULL;
	return false;
}
