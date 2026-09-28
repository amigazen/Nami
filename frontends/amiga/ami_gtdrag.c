/*
 * Optional gtdrag.library glue for Nami.
 * Soft-fails when the library is not installed.
 *
 * ReAction window.class turns gadget presses into WMHI_* and does not feed
 * raw IDCMP_GADGETDOWN into GTD_FilterIMsg the way GadTools apps do.  Tab
 * buttons live in a layout.gadget sidebar, so we re-sync GA_Left/Top from
 * layout before hit-testing and synthesise GADGETDOWN / MOUSEMOVE.  We never
 * synthesise SELECTUP — CreateDragObj's FakeInputEvent must clear fakemsg via
 * FilterIMsg, and only the real LMB release may FreeDragObj / MakeDropMessage.
 */

#include "amiga/os3support.h"

#include <string.h>

#include <proto/exec.h>
#include <proto/intuition.h>
#include <devices/input.h>
#include <intuition/gadgetclass.h>
#include <libraries/gadtools.h>
#include <libraries/gtdrag.h>
#include <proto/gtdrag.h>

/* PeekQualifier lives in input.device; open a throwaway IORequest for the base. */
struct Device *InputBase = NULL;
#include <proto/input.h>

#include "utils/log.h"

#include "amiga/ami_gtdrag.h"
#include "amiga/gui.h"

/* Proto inlines need this symbol. */
struct Library *GTDragBase = NULL;

static bool ami_gtdrag_app_ok = false;

/* Armed tab press awaiting move/up (window-relative start). */
static struct Window *ami_gtdrag_arm_win = NULL;
static struct Gadget *ami_gtdrag_arm_gad = NULL;
static WORD ami_gtdrag_arm_x;
static WORD ami_gtdrag_arm_y;
static bool ami_gtdrag_moved = false;
/*
 * After a real drag ends, refuse re-arm until LMB is released
 * (CreateDragObj FakeInputEvent can leave Qualifier sticky).
 */
static bool ami_gtdrag_wait_lmb_up = false;
/* OBJECTDROP may be queued at FakeInputEvent UnlockLayers — discard it. */
static bool ami_gtdrag_drop_queued = false;
/* Swallow the FakeInputEvent SELECTUP twin so it does not finish early. */
static bool ami_gtdrag_skip_next_poll = false;
/* Tab gui_window being dragged (set at arm; used on real LMB release). */
static struct gui_window *ami_gtdrag_src_gw = NULL;

static struct MsgPort *ami_gtd_iport = NULL;
static struct IOStdReq *ami_gtd_ireq = NULL;

static void ami_gtdrag_free_drop_imsg(struct IntuiMessage *imsg);

static void ami_gtdrag_clear_arm(void)
{
	ami_gtdrag_arm_win = NULL;
	ami_gtdrag_arm_gad = NULL;
	ami_gtdrag_moved = false;
}

void ami_gtdrag_set_drag_source(struct gui_window *gw)
{
	ami_gtdrag_src_gw = gw;
}

struct gui_window *ami_gtdrag_get_drag_source(void)
{
	return ami_gtdrag_src_gw;
}

void ami_gtdrag_clear_drag_source(void)
{
	ami_gtdrag_src_gw = NULL;
}

static void ami_gtdrag_close_input(void)
{
	if(ami_gtd_ireq != NULL) {
		if(InputBase != NULL) {
			CloseDevice((struct IORequest *)ami_gtd_ireq);
			InputBase = NULL;
		}
		DeleteExtIO((struct IORequest *)ami_gtd_ireq);
		ami_gtd_ireq = NULL;
	}
	if(ami_gtd_iport != NULL) {
		DeletePort(ami_gtd_iport);
		ami_gtd_iport = NULL;
	}
}

static bool ami_gtdrag_open_input(void)
{
	if(InputBase != NULL)
		return true;

	ami_gtd_iport = CreatePort(NULL, 0);
	if(ami_gtd_iport == NULL)
		return false;
	ami_gtd_ireq = (struct IOStdReq *)CreateExtIO(ami_gtd_iport,
			sizeof(struct IOStdReq));
	if(ami_gtd_ireq == NULL) {
		ami_gtdrag_close_input();
		return false;
	}
	if(OpenDevice("input.device", 0, (struct IORequest *)ami_gtd_ireq, 0) != 0) {
		ami_gtdrag_close_input();
		return false;
	}
	InputBase = ami_gtd_ireq->io_Device;
	return true;
}

void ami_gtdrag_init(void)
{
	LONG rc;

	ami_gtdrag_app_ok = false;
	ami_gtdrag_clear_arm();
	ami_gtdrag_wait_lmb_up = false;
	ami_gtdrag_drop_queued = false;
	ami_gtdrag_skip_next_poll = false;
	ami_gtdrag_src_gw = NULL;

	GTDragBase = OpenLibrary("gtdrag.library", 3);
	if(GTDragBase == NULL) {
		NSLOG(netsurf, INFO,
				"gtdrag.library v3 not available (tab DnD disabled)");
		return;
	}

	NSLOG(netsurf, INFO, "Opened gtdrag.library v%d.%d",
			GTDragBase->lib_Version, GTDragBase->lib_Revision);

	rc = GTD_AddApp("Nami",
			GTDA_NewStyle, TRUE,
			TAG_DONE);
	if(rc == 0) {
		NSLOG(netsurf, WARNING, "GTD_AddApp failed — tab DnD disabled");
		CloseLibrary(GTDragBase);
		GTDragBase = NULL;
		return;
	}

	(void)ami_gtdrag_open_input();

	ami_gtdrag_app_ok = true;
	NSLOG(netsurf, INFO, "gtdrag: Nami app registered");
}

void ami_gtdrag_fini(void)
{
	if(GTDragBase == NULL) {
		ami_gtdrag_close_input();
		return;
	}

	ami_gtdrag_clear_arm();

	if(ami_gtdrag_app_ok) {
		GTD_RemoveApp();
		ami_gtdrag_app_ok = false;
	}

	CloseLibrary(GTDragBase);
	GTDragBase = NULL;
	ami_gtdrag_close_input();
}

bool ami_gtdrag_available(void)
{
	return (ami_gtdrag_app_ok && GTDragBase != NULL) ? true : false;
}

ULONG ami_gtdrag_idcmp_bits(void)
{
	if(!ami_gtdrag_available())
		return 0UL;
	return (ULONG)(IDCMP_OBJECTDROP | IDCMP_GADGETDOWN |
			IDCMP_MOUSEMOVE | IDCMP_MOUSEBUTTONS |
			IDCMP_INTUITICKS);
}

bool ami_gtdrag_lmb_physically_down(void)
{
	if(InputBase == NULL && !ami_gtdrag_open_input())
		return false;
	return ((PeekQualifier() & IEQUALIFIER_LEFTBUTTON) != 0) ? true : false;
}

void ami_gtdrag_sync_gadget_box(struct Gadget *gad)
{
	ULONG left, top, width, height;

	if(gad == NULL)
		return;
	if(GetAttr(GA_Left, (Object *)gad, &left) == 0)
		left = gad->LeftEdge;
	if(GetAttr(GA_Top, (Object *)gad, &top) == 0)
		top = gad->TopEdge;
	if(GetAttr(GA_Width, (Object *)gad, &width) == 0)
		width = gad->Width;
	if(GetAttr(GA_Height, (Object *)gad, &height) == 0)
		height = gad->Height;
	gad->LeftEdge = (WORD)left;
	gad->TopEdge = (WORD)top;
	gad->Width = (WORD)width;
	gad->Height = (WORD)height;
}

void ami_gtdrag_filter_imsg(struct IntuiMessage *imsg)
{
	if(!ami_gtdrag_available() || imsg == NULL)
		return;
	(void)GTD_FilterIMsg(imsg);
}

static void ami_gtdrag_feed(struct Window *win, ULONG class, UWORD code,
		struct Gadget *gad)
{
	struct IntuiMessage fake;

	if(!ami_gtdrag_available() || win == NULL)
		return;

	memset(&fake, 0, sizeof(fake));
	fake.Class = class;
	fake.Code = code;
	fake.IAddress = gad;
	fake.IDCMPWindow = win;
	fake.MouseX = win->MouseX;
	fake.MouseY = win->MouseY;
	if(class == IDCMP_GADGETDOWN || class == IDCMP_MOUSEMOVE)
		fake.Qualifier = IEQUALIFIER_LEFTBUTTON;

	(void)GTD_FilterIMsg(&fake);
}

void ami_gtdrag_arm(struct Gadget *gad, struct Window *win)
{
	WORD mx, my;

	if(!ami_gtdrag_available() || gad == NULL || win == NULL)
		return;

	/*
	 * Caller must apply window-absolute gadget boxes before arming.
	 * Do not mutate LeftEdge here — that permanently corrupts layout
	 * coords (save/restore then stamps a ghost favicon beside the sidebar).
	 */
	mx = win->MouseX;
	my = win->MouseY;

	ami_gtdrag_feed(win, IDCMP_GADGETDOWN, 0, gad);

	ami_gtdrag_arm_win = win;
	ami_gtdrag_arm_gad = gad;
	ami_gtdrag_arm_x = mx;
	ami_gtdrag_arm_y = my;
	ami_gtdrag_moved = false;
	ami_gtdrag_wait_lmb_up = false;

	NSLOG(netsurf, INFO, "gtdrag: armed gadget id=%u at %d,%d box=%d,%d %dx%d",
			(unsigned)gad->GadgetID,
			(int)mx, (int)my,
			(int)gad->LeftEdge, (int)gad->TopEdge,
			(int)gad->Width, (int)gad->Height);
}

void ami_gtdrag_mouse_move(struct Window *win)
{
	WORD dx, dy;

	if(!ami_gtdrag_available() || win == NULL)
		return;
	if(ami_gtdrag_arm_win != win)
		return;

	dx = (WORD)(win->MouseX - ami_gtdrag_arm_x);
	dy = (WORD)(win->MouseY - ami_gtdrag_arm_y);
	if(dx < 0)
		dx = (WORD)(-dx);
	if(dy < 0)
		dy = (WORD)(-dy);
	if(dx > 3 || dy > 3)
		ami_gtdrag_moved = true;

	ami_gtdrag_feed(win, IDCMP_MOUSEMOVE, 0, NULL);
}

bool ami_gtdrag_select_up(struct Window *win)
{
	bool was_drag;
	struct Window *arm_win;

	if(!ami_gtdrag_available())
		return false;

	/*
	 * Unlock on the armed window even if SELECTUP/ticks arrived on another
	 * (cross-window drop).  CreateDragObj LockLayers freezes the UI until
	 * FreeDragObj — refusing unlock because win != arm_win hangs everything.
	 */
	arm_win = ami_gtdrag_arm_win;
	if(arm_win == NULL)
		return false;
	if(win == NULL)
		win = arm_win;
	else if(win != arm_win)
		win = arm_win;

	/*
	 * Must feed SELECTUP into FilterIMsg so FreeDragObj runs UnlockLayers.
	 * CreateDragObj holds LockLayers until then — skipping this freezes the
	 * UI.  FakeInputEvent only clears fakemsg; it does not FreeDragObj.
	 *
	 * MakeDropMessage may fire here (early, at ghost-create time).  Caller
	 * must defer poll_drops until the physical LMB release so hit-tests use
	 * the real drop position.
	 */
	was_drag = ami_gtdrag_moved;
	ami_gtdrag_feed(win, IDCMP_MOUSEBUTTONS, SELECTUP, NULL);

	ami_gtdrag_wait_lmb_up = true;
	if(was_drag) {
		ami_gtdrag_drop_queued = true;
		ami_gtdrag_skip_next_poll = true;
	} else {
		ami_gtdrag_src_gw = NULL;
	}
	ami_gtdrag_clear_arm();

	if(was_drag)
		NSLOG(netsurf, INFO, "gtdrag: select-up after drag (UnlockLayers)");
	return was_drag;
}

struct Window *ami_gtdrag_arm_window(void)
{
	return ami_gtdrag_arm_win;
}

bool ami_gtdrag_drop_waiting(void)
{
	return ami_gtdrag_drop_queued;
}

bool ami_gtdrag_should_poll_drop(ULONG qualifier)
{
	/*
	 * CreateDragObj FakeInputEvent clears IEQUALIFIER_LEFTBUTTON on the
	 * IntuiMessage while the hardware button is still down.  Finish only
	 * when PeekQualifier says LMB is up (and after swallowing the Fake twin).
	 */
	(void)qualifier;
	if(!ami_gtdrag_drop_queued)
		return false;
	if(ami_gtdrag_skip_next_poll) {
		ami_gtdrag_skip_next_poll = false;
		return false;
	}
	if(ami_gtdrag_lmb_physically_down())
		return false;
	return true;
}

void ami_gtdrag_drop_polled(void)
{
	ami_gtdrag_drop_queued = false;
	ami_gtdrag_skip_next_poll = false;
}

/*
 * CreateDragObj FakeInputEvent makes us UnlockLayers via a synthesised
 * SELECTUP, which also MakeDropMessages at the wrong mouse position.
 * Drain those stale OBJECTDROPs from every Nami window without acting.
 */
void ami_gtdrag_discard_drops(void)
{
	struct MinList *wlist;
	struct nsObject *node;
	struct nsObject *nnode;
	struct gui_window_2 *gwin;
	struct Window *win;
	struct MsgPort *port;
	struct IntuiMessage *imsg;
	struct Message *msg;
	struct Message *succ;

	if(!ami_gtdrag_available())
		return;
	wlist = ami_gui_get_window_list();
	if(wlist == NULL || IsMinListEmpty(wlist))
		return;

	node = (struct nsObject *)GetHead((struct List *)wlist);
	while(node != NULL) {
		nnode = (struct nsObject *)GetSucc((struct Node *)node);
		if(node->Type == AMINS_WINDOW) {
			gwin = node->objstruct;
			win = ami_gui2_get_window(gwin);
			if(win != NULL && win->UserPort != NULL) {
				port = win->UserPort;
				Forbid();
				msg = (struct Message *)port->mp_MsgList.lh_Head;
				while(msg != NULL && msg->mn_Node.ln_Succ != NULL) {
					succ = (struct Message *)msg->mn_Node.ln_Succ;
					imsg = (struct IntuiMessage *)msg;
					if(imsg->IDCMPWindow == win &&
					   imsg->Class == IDCMP_OBJECTDROP) {
						Remove((struct Node *)msg);
						Permit();
						NSLOG(netsurf, INFO,
							"gtdrag: discard stale OBJECTDROP");
						ami_gtdrag_free_drop_imsg(imsg);
						Forbid();
						msg = (struct Message *)port->mp_MsgList.lh_Head;
						continue;
					}
					msg = succ;
				}
				Permit();
			}
		}
		node = nnode;
	}
}

bool ami_gtdrag_armed(void)
{
	return (ami_gtdrag_arm_win != NULL) ? true : false;
}

bool ami_gtdrag_can_arm(ULONG qualifier)
{
	/*
	 * After UnlockLayers, FakeInputEvent leaves message Qualifier without
	 * LMB while the user may still be holding.  Block re-arm until the
	 * hardware button is actually up.
	 */
	(void)qualifier;
	if(!ami_gtdrag_wait_lmb_up)
		return true;
	if(!ami_gtdrag_lmb_physically_down()) {
		ami_gtdrag_wait_lmb_up = false;
		return true;
	}
	return false;
}

void ami_gtdrag_note_lmb_up(void)
{
	ami_gtdrag_wait_lmb_up = false;
}

/*
 * OBJECTDROP is AllocMem'd by gtdrag and PutMsg'd to our UserPort with
 * ReplyPort = gtdrag's dmport.  We Remove() it before RA_HandleInput sees
 * it, so we own it — free DropMessage + ExtIntuiMessage directly.
 *
 * Do NOT call GTD_ReplyIMsg here: FilterIMsg on normal IDCMP traffic can
 * leave da_GTMsg set, and ReplyIMsg then runs GT_PostFilterIMsg on the
 * drop message (crash).  ReplyMsg to dmport is also wrong after Remove().
 */
static void ami_gtdrag_free_drop_imsg(struct IntuiMessage *imsg)
{
	struct DropMessage *dm;
	ULONG len;

	if(imsg == NULL)
		return;

	dm = (struct DropMessage *)imsg->IAddress;
	if(dm != NULL)
		FreeMem(dm, sizeof(struct DropMessage));

	len = imsg->ExecMessage.mn_Length;
	if(len < sizeof(struct IntuiMessage))
		len = sizeof(struct ExtIntuiMessage);
	FreeMem(imsg, len);
}

void ami_gtdrag_poll_drops(struct gui_window_2 *gwin)
{
	struct Window *win;
	struct MsgPort *port;
	struct IntuiMessage *imsg;
	struct Message *msg;
	struct Message *succ;
	struct DropMessage *dm;

	if(!ami_gtdrag_available() || gwin == NULL)
		return;
	win = ami_gui2_get_window(gwin);
	if(win == NULL || win->UserPort == NULL)
		return;
	port = win->UserPort;

	Forbid();
	msg = (struct Message *)port->mp_MsgList.lh_Head;
	while(msg != NULL && msg->mn_Node.ln_Succ != NULL) {
		succ = (struct Message *)msg->mn_Node.ln_Succ;
		imsg = (struct IntuiMessage *)msg;
		if(imsg->IDCMPWindow == win &&
		   imsg->Class == IDCMP_OBJECTDROP) {
			Remove((struct Node *)msg);
			Permit();

			dm = (struct DropMessage *)imsg->IAddress;
			NSLOG(netsurf, INFO, "gtdrag: OBJECTDROP target=%p src=%p",
					(void *)(dm != NULL ? dm->dm_Target : NULL),
					(void *)(dm != NULL ? dm->dm_Gadget : NULL));
			if(dm != NULL)
				ami_gui_nami_gtdrag_drop(gwin, dm);

			ami_gtdrag_free_drop_imsg(imsg);

			Forbid();
			msg = (struct Message *)port->mp_MsgList.lh_Head;
			continue;
		}
		msg = succ;
	}
	Permit();
}

void ami_gtdrag_add_window(struct Window *win)
{
	if(!ami_gtdrag_available() || win == NULL)
		return;
	GTD_AddWindow(win, TAG_DONE);
}

void ami_gtdrag_rem_window(struct Window *win)
{
	if(!ami_gtdrag_available() || win == NULL)
		return;
	if(ami_gtdrag_arm_win == win)
		ami_gtdrag_clear_arm();
	GTD_RemoveWindow(win);
}

void ami_gtdrag_add_tab_button(struct Gadget *gad, struct Window *win,
		struct ImageNode *inode, ami_gtdrag_object_func_t objfunc)
{
	struct Hook *hook;

	if(!ami_gtdrag_available() || gad == NULL || win == NULL)
		return;

	hook = GTD_GetHook(GTDH_IMAGE);

	GTD_AddGadget(BUTTON_KIND, gad, win,
			GTDA_Object, inode,
			GTDA_RenderHook, hook,
			GTDA_Width, (ULONG)(gad->Width > 0 ? gad->Width : 96),
			GTDA_Height, (ULONG)(gad->Height > 0 ? gad->Height : 22),
			GTDA_Type, ODT_IMAGENODE,
			GTDA_InternalType, AMI_GTD_TAB,
			/* AcceptTypes toggled for filled tabs / empty favicon wells. */
			GTDA_AcceptTypes, 0UL,
			GTDA_Same, TRUE,
			GTDA_ObjectFunc, objfunc,
			TAG_DONE);
}

void ami_gtdrag_add_hot_button(struct Gadget *gad, struct Window *win)
{
	if(!ami_gtdrag_available() || gad == NULL || win == NULL)
		return;

	GTD_AddGadget(BUTTON_KIND, gad, win,
			GTDA_NoDrag, TRUE,
			/* Empty favicon wells enable AcceptTypes later. */
			GTDA_AcceptTypes, 0UL,
			TAG_DONE);
}

void ami_gtdrag_set_accept(struct Gadget *gad, ULONG accept_types)
{
	if(!ami_gtdrag_available() || gad == NULL)
		return;
	GTD_SetAttrs(gad,
			GTDA_AcceptTypes, accept_types,
			TAG_DONE);
}

void ami_gtdrag_set_tab_object(struct Gadget *gad, struct ImageNode *inode,
		UWORD width, UWORD height)
{
	struct Hook *hook;

	if(!ami_gtdrag_available() || gad == NULL)
		return;

	hook = GTD_GetHook(GTDH_IMAGE);
	GTD_SetAttrs(gad,
			GTDA_Object, inode,
			GTDA_RenderHook, hook,
			GTDA_Width, (ULONG)width,
			GTDA_Height, (ULONG)height,
			GTDA_Type, ODT_IMAGENODE,
			GTDA_InternalType, AMI_GTD_TAB,
			TAG_DONE);
}
