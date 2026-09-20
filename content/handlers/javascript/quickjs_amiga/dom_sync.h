/*
 * Copyright 2026 AmigaZen / Nami contributors
 *
 * Sync libdom text mutations into the HTML box tree and request reflow.
 */

#ifndef NETSURF_JS_QUICKJS_AMIGA_DOM_SYNC_H_
#define NETSURF_JS_QUICKJS_AMIGA_DOM_SYNC_H_

struct html_content;
struct dom_node;

/**
 * After textContent (or similar) mutates a node, update the matching
 * BOX_TEXT (if any), reformat the document, and redraw the affected box.
 */
void qjs_dom_sync_text(struct html_content *html, struct dom_node *node);

#endif /* NETSURF_JS_QUICKJS_AMIGA_DOM_SYNC_H_ */
