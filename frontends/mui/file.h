/*
 * Copyright 2011 Chris Young <chris@unsatisfactorysoftware.co.uk>
 * Copyright 2026 amigazen project
 *
 * Forked from frontends/amiga/file.h for Tsunami.
 */

#ifndef MUI_FILE_H
#define MUI_FILE_H

struct hlcache_handle;
struct gui_window;
struct Window;
struct browser_window;

extern struct FileRequester *filereq;
extern struct FileRequester *savereq;

enum {
	AMINS_SAVE_SOURCE,
	AMINS_SAVE_TEXT,
	AMINS_SAVE_COMPLETE,
	AMINS_SAVE_PDF,
	AMINS_SAVE_IFF,
	AMINS_SAVE_SELECTION
};

void ami_file_req_init(void);
void ami_file_req_free(void);

void ami_file_open(struct gui_window *gw);
void ami_file_save_req(int type, struct gui_window *gw,
		       struct hlcache_handle *object);
void ami_file_save(int type, char *fname, struct Window *win,
		   struct hlcache_handle *object,
		   struct hlcache_handle *favicon,
		   struct browser_window *bw);

#endif /* MUI_FILE_H */
