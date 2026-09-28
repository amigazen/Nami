/*
 * Optional gtdrag.library glue for Nami.
 * Soft-fails when the library is not installed.
 */

#ifndef AMIGA_AMI_GTDRAG_H
#define AMIGA_AMI_GTDRAG_H

#include <stdbool.h>
#include <exec/types.h>

struct gui_window;
struct gui_window_2;
struct IntuiMessage;
struct Window;
struct Gadget;
struct ImageNode;
struct DropMessage;
struct ObjectDescription;

/* Internal type bit for tab drag payloads (GTDA_AcceptTypes). */
#define AMI_GTD_TAB		(1UL << 0)

void ami_gtdrag_init(void);
void ami_gtdrag_fini(void);
bool ami_gtdrag_available(void);
ULONG ami_gtdrag_idcmp_bits(void);

void ami_gtdrag_filter_imsg(struct IntuiMessage *imsg);
void ami_gtdrag_poll_drops(struct gui_window_2 *gwin);
void ami_gtdrag_discard_drops(void);

void ami_gtdrag_add_window(struct Window *win);
void ami_gtdrag_rem_window(struct Window *win);

typedef void (*ami_gtdrag_object_func_t)(struct Window *, struct Gadget *,
		struct ObjectDescription *, LONG);

void ami_gtdrag_add_tab_button(struct Gadget *gad, struct Window *win,
		struct ImageNode *inode, ami_gtdrag_object_func_t objfunc);
void ami_gtdrag_add_hot_button(struct Gadget *gad, struct Window *win);
void ami_gtdrag_set_tab_object(struct Gadget *gad, struct ImageNode *inode,
		UWORD width, UWORD height);
/* Drop highlight: non-zero AcceptTypes = destination; 0 = source-only.
 * Filled tabs accept for reorder; empty favicon wells accept for bookmark. */
void ami_gtdrag_set_accept(struct Gadget *gad, ULONG accept_types);

/* ReAction WMHI bridge — synthesise the IDCMP stream gtdrag expects. */
void ami_gtdrag_sync_gadget_box(struct Gadget *gad);
void ami_gtdrag_arm(struct Gadget *gad, struct Window *win);
void ami_gtdrag_mouse_move(struct Window *win);
bool ami_gtdrag_select_up(struct Window *win); /* TRUE if press became a drag */
bool ami_gtdrag_armed(void);
/* Window that currently owns the arm (for UnlockLayers from another win). */
struct Window *ami_gtdrag_arm_window(void);
/* After a drag ends, refuse re-arm until LMB is released (FakeInputEvent). */
bool ami_gtdrag_can_arm(ULONG qualifier);
void ami_gtdrag_note_lmb_up(void);
/* Hardware LMB still down?  Distinguishes CreateDragObj FakeInputEvent. */
bool ami_gtdrag_lmb_physically_down(void);
/* Stale OBJECTDROP queued at UnlockLayers — finish on real LMB release. */
bool ami_gtdrag_drop_waiting(void);
bool ami_gtdrag_should_poll_drop(ULONG qualifier);
void ami_gtdrag_drop_polled(void);
void ami_gtdrag_set_drag_source(struct gui_window *gw);
struct gui_window *ami_gtdrag_get_drag_source(void);
void ami_gtdrag_clear_drag_source(void);

/**
 * Called from ami_gtdrag_poll_drops with a DropMessage for this window.
 * Implemented in gui.c (has sidebar state).
 */
void ami_gui_nami_gtdrag_drop(struct gui_window_2 *gwin,
		struct DropMessage *dm);

/** Resolve drop under the mouse using the armed tab source (gui.c). */
void ami_gui_nami_gtdrag_finish_drop(void);

/** Sync all registered tab/hotlist gadget boxes (gui.c). */
void ami_gui_nami_gtdrag_sync_bounds(struct gui_window_2 *gwin);

#endif /* AMIGA_AMI_GTDRAG_H */
