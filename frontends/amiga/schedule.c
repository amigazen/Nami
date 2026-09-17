/*
 * Copyright 2008 - 2016 Chris Young <chris@unsatisfactorysoftware.co.uk>
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

#include <proto/dos.h>
#include <proto/exec.h>
#include <proto/timer.h>
#include <proto/utility.h> /* For Amiga2Date */

#include <stdio.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#include <exec/lists.h>

#include "utils/errors.h"
#include "utils/log.h"

#include "amiga/memory.h"
#include "amiga/schedule.h"

/* TimeRequest must be first — used as IORequest for timer.device.
 * schedule_node is a separate list node (cannot reuse mn_Node). */
struct nscallback
{
	struct TimeRequest timereq;
	struct MinNode schedule_node;
	struct TimeVal tv;
	void *restrict callback;
	void *restrict p;
};

static struct nscallback *tioreq;
struct Device *TimerBase;
#ifdef __amigaos4__
struct TimerIFace *ITimer;
#else
static struct MsgPort *schedule_msgport = NULL;
#endif

static struct MinList schedule_list;
static bool schedule_list_valid = false;

#define NSCB_FROM_NODE(n) \
	((struct nscallback *)(((char *)(n)) - \
		offsetof(struct nscallback, schedule_node)))

static void ami_schedule_remove_timer_event(struct nscallback *nscb)
{
	if (nscb == NULL) {
		return;
	}

	if (CheckIO((struct IORequest *)nscb) == NULL) {
		AbortIO((struct IORequest *)nscb);
	}

	WaitIO((struct IORequest *)nscb);
}

static nserror ami_schedule_add_timer_event(struct nscallback *nscb, int t)
{
	struct TimeVal tv;
	ULONG time_us;

	time_us = (ULONG)t * 1000UL;

#ifdef __amigaos4__
	tv.Seconds = time_us / 1000000UL;
	tv.Microseconds = time_us % 1000000UL;
#else
	tv.tv_secs = time_us / 1000000UL;
	tv.tv_micro = time_us % 1000000UL;
#endif

	GetSysTime(&nscb->tv);
	AddTime(&nscb->tv, &tv);

	nscb->timereq.tr_node.io_Command = TR_ADDREQUEST;
#ifdef __amigaos4__
	nscb->timereq.Time.Seconds = tv.Seconds;
	nscb->timereq.Time.Microseconds = tv.Microseconds;
#else
	nscb->timereq.tr_time.tv_secs = tv.tv_secs;
	nscb->timereq.tr_time.tv_micro = tv.tv_micro;
#endif
	SendIO((struct IORequest *)nscb);

	return NSERROR_OK;
}

static struct nscallback *
ami_schedule_locate(void (*callback)(void *p), void *p, bool remove)
{
	struct MinNode *node;
	struct nscallback *nscb;

	if (schedule_list_valid == false) {
		return NULL;
	}
	if (IsMinListEmpty(&schedule_list)) {
		return NULL;
	}

	for (node = schedule_list.mlh_Head;
	     node->mln_Succ != NULL;
	     node = node->mln_Succ) {
		nscb = NSCB_FROM_NODE(node);
		if ((nscb->callback == callback) && (nscb->p == p)) {
			if (remove == true) {
				Remove((struct Node *)node);
			}
			return nscb;
		}
	}

	return NULL;
}

static nserror ami_schedule_reschedule(struct nscallback *nscb, int t)
{
	ami_schedule_remove_timer_event(nscb);
	if (ami_schedule_add_timer_event(nscb, t) != NSERROR_OK) {
		return NSERROR_NOMEM;
	}
	return NSERROR_OK;
}

static nserror schedule_remove(void (*callback)(void *p), void *p, bool abort)
{
	struct nscallback *nscb;

	nscb = ami_schedule_locate(callback, p, true);

	if (nscb != NULL) {
		if (abort == true) {
			ami_schedule_remove_timer_event(nscb);
		}
#ifdef __amigaos4__
		FreeSysObject(ASOT_IOREQUEST, nscb);
#else
		FreeVec(nscb);
#endif
	}

	return NSERROR_OK;
}

static void schedule_remove_all(void)
{
	struct MinNode *node;
	struct MinNode *next;
	struct nscallback *nscb;

	if (schedule_list_valid == false) {
		return;
	}

	for (node = schedule_list.mlh_Head;
	     (next = node->mln_Succ) != NULL;
	     node = next) {
		nscb = NSCB_FROM_NODE(node);
		Remove((struct Node *)node);
		ami_schedule_remove_timer_event(nscb);
#ifdef __amigaos4__
		FreeSysObject(ASOT_IOREQUEST, nscb);
#else
		FreeVec(nscb);
#endif
	}
}

static void ami_schedule_dump(void)
{
	struct MinNode *node;
	struct nscallback *nscb;
	struct ClockData clockdata;
	struct TimeVal tv;
	ULONG secs;
	ULONG micro;

	if (schedule_list_valid == false || IsMinListEmpty(&schedule_list)) {
		return;
	}

	GetSysTime(&tv);
#ifdef __amigaos4__
	secs = tv.Seconds;
	micro = tv.Microseconds;
#else
	secs = tv.tv_secs;
	micro = tv.tv_micro;
#endif
	Amiga2Date(secs, &clockdata);

	NSLOG(netsurf, INFO, "Current time = %d-%d-%d %d:%d:%d.%lu",
	      clockdata.mday, clockdata.month, clockdata.year,
	      clockdata.hour, clockdata.min, clockdata.sec, micro);
	NSLOG(netsurf, INFO, "Events remaining in queue:");

	for (node = schedule_list.mlh_Head;
	     node->mln_Succ != NULL;
	     node = node->mln_Succ) {
		nscb = NSCB_FROM_NODE(node);
#ifdef __amigaos4__
		secs = nscb->tv.Seconds;
		micro = nscb->tv.Microseconds;
#else
		secs = nscb->tv.tv_secs;
		micro = nscb->tv.tv_micro;
#endif
		Amiga2Date(secs, &clockdata);
		NSLOG(netsurf, INFO,
		      "nscb: %p, at %d-%d-%d %d:%d:%d.%lu, callback: %p, %p",
		      nscb, clockdata.mday, clockdata.month, clockdata.year,
		      clockdata.hour, clockdata.min, clockdata.sec,
		      micro, nscb->callback, nscb->p);
		if (CheckIO((struct IORequest *)nscb) == NULL) {
			NSLOG(netsurf, INFO, "-> ACTIVE");
		} else {
			NSLOG(netsurf, INFO, "-> COMPLETE");
		}
	}
}

static bool ami_scheduler_run(struct nscallback *nscb)
{
	void (*callback)(void *p);
	void *p;

	callback = nscb->callback;
	p = nscb->p;

	schedule_remove(callback, p, false);
	callback(p);
	return true;
}

static void ami_schedule_open_timer(struct MsgPort *msgport)
{
#ifdef __amigaos4__
	tioreq = (struct nscallback *)AllocSysObjectTags(ASOT_IOREQUEST,
				ASOIOR_Size, sizeof(struct nscallback),
				ASOIOR_ReplyPort, msgport,
				ASO_NoTrack, FALSE,
				TAG_DONE);
#else
	tioreq = (struct nscallback *)CreateIORequest(msgport,
						     sizeof(struct nscallback));
#endif

	OpenDevice("timer.device", UNIT_VBLANK, (struct IORequest *)tioreq, 0);

	TimerBase = (struct Device *)tioreq->timereq.tr_node.io_Device;
#ifdef __amigaos4__
	ITimer = (struct TimerIFace *)GetInterface((struct Library *)TimerBase,
						   "main", 1, NULL);
#endif
}

static void ami_schedule_close_timer(void)
{
#ifdef __amigaos4__
	if (ITimer) {
		DropInterface((struct Interface *)ITimer);
	}
#endif
	CloseDevice((struct IORequest *)tioreq);
#ifdef __amigaos4__
	FreeSysObject(ASOT_IOREQUEST, tioreq);
#else
	DeleteIORequest((struct IORequest *)tioreq);
#endif
	tioreq = NULL;
}

nserror ami_schedule_create(struct MsgPort *msgport)
{
	ami_schedule_open_timer(msgport);
#ifndef __amigaos4__
	schedule_msgport = msgport;
#endif
	NewList((struct List *)&schedule_list);
	schedule_list_valid = true;

	return NSERROR_OK;
}

void ami_schedule_free(void)
{
	ami_schedule_dump();
	schedule_remove_all();
	schedule_list_valid = false;

	ami_schedule_close_timer();
}

nserror ami_schedule(int t, void (*callback)(void *p), void *p)
{
	struct nscallback *nscb;

	if (t == 0) {
		t = 1;
	}

	if (schedule_list_valid == false) {
		return NSERROR_INIT_FAILED;
	}
	if (t < 0) {
		return schedule_remove(callback, p, true);
	}

	if ((nscb = ami_schedule_locate(callback, p, false))) {
		return ami_schedule_reschedule(nscb, t);
	}

#ifdef __amigaos4__
	nscb = AllocSysObjectTags(ASOT_IOREQUEST,
				  ASOIOR_Duplicate, tioreq,
				  TAG_DONE);
	if (nscb == NULL) {
		return NSERROR_NOMEM;
	}
#else
	if (schedule_msgport == NULL) {
		return NSERROR_NOMEM;
	}
	nscb = ami_memory_allocvec(sizeof(struct nscallback), MEMF_CLEAR);
	if (nscb == NULL) {
		return NSERROR_NOMEM;
	}
	*nscb = *tioreq;
#endif

	if (ami_schedule_add_timer_event(nscb, t) != NSERROR_OK) {
		return NSERROR_NOMEM;
	}

	nscb->callback = callback;
	nscb->p = p;

	AddTail((struct List *)&schedule_list,
		(struct Node *)&nscb->schedule_node);

	return NSERROR_OK;
}

void ami_schedule_handle(struct MsgPort *nsmsgport)
{
	struct nscallback *timermsg;

	while ((timermsg = (struct nscallback *)GetMsg(nsmsgport))) {
		ami_scheduler_run(timermsg);
	}
}
