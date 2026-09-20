/*
 * Copyright 2026 AmigaZen / Nami contributors
 *
 * After a JS textContent write, push the new string into the box tree
 * and ask the HTML content to reflow and redraw. NetSurf's Duktape path
 * mutates libdom only; without this hook the page never changes on screen.
 */

#include <stddef.h>
#include <string.h>

#include <dom/dom.h>

#include "utils/log.h"
#include "utils/talloc.h"
#include "content/content_protected.h"
#include "html/private.h"
#include "html/box.h"
#include "html/box_construct.h"

#include "dom_sync.h"

static struct box *qjs_find_text_box(struct box *box)
{
	struct box *child;
	struct box *found;

	if (box == NULL) {
		return NULL;
	}

	if (box->type == BOX_TEXT && box->text != NULL) {
		return box;
	}

	for (child = box->children; child != NULL; child = child->next) {
		found = qjs_find_text_box(child);
		if (found != NULL) {
			return found;
		}
	}

	return NULL;
}

void qjs_dom_sync_text(struct html_content *html, struct dom_node *node)
{
	struct box *root;
	struct box *text_box;
	dom_exception exc;
	dom_string *content;
	char *copy;
	size_t len;
	struct content *c;

	if (html == NULL || node == NULL) {
		return;
	}

	if (html->base.status != CONTENT_STATUS_READY &&
			html->base.status != CONTENT_STATUS_DONE) {
		return;
	}

	root = box_for_node(node);
	if (root == NULL) {
		NSLOG(netsurf, DEBUG, "qjs_dom_sync: no box for node %p",
				(void *)node);
		return;
	}

	text_box = qjs_find_text_box(root);
	if (text_box == NULL) {
		NSLOG(netsurf, DEBUG, "qjs_dom_sync: no BOX_TEXT under node");
		return;
	}

	exc = dom_node_get_text_content(node, &content);
	if (exc != DOM_NO_ERR || content == NULL) {
		return;
	}

	len = dom_string_length(content);
	if (html->bctx == NULL) {
		dom_string_unref(content);
		return;
	}

	copy = talloc_strdup(html->bctx, dom_string_data(content));
	dom_string_unref(content);
	if (copy == NULL) {
		return;
	}

	/* Previous box->text was allocated from html->bctx. */
	if (text_box->text != NULL) {
		talloc_free(text_box->text);
	}
	text_box->text = copy;
	text_box->length = len;

	c = &html->base;
	NSLOG(netsurf, INFO, "qjs_dom_sync: text updated len=%lu, reformat",
			(unsigned long)len);
	content__reformat(c, false, c->available_width, c->available_height);
	html__redraw_a_box(html, text_box);
}
