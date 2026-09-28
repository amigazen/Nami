/*
 * Copyright 2026 amigazen project
 *
 * This file is part of NetSurf / Tsunami (MUI frontend).
 *
 * Callback scheduler using gettimeofday (newlib) — same shape as monkey,
 * so the MUI NewInput loop can poll due work between Wait() calls.
 */

#include <stdlib.h>
#include <sys/time.h>

#include "utils/sys_time.h"
#include "utils/log.h"
#include "mui/schedule.h"
#include "mui/gui.h"

struct nscallback {
	struct nscallback *next;
	struct timeval tv;
	void (*callback)(void *p);
	void *p;
};

static struct nscallback *schedule_list = NULL;

/**
 * Remove all scheduled entries matching callback and p.
 */
static nserror schedule_remove(void (*callback)(void *p), void *p)
{
	struct nscallback *cur_nscb;
	struct nscallback *prev_nscb;
	struct nscallback *unlnk_nscb;
	int removed;

	removed = 0;

	if (schedule_list == NULL) {
		return NSERROR_NOT_FOUND;
	}

	cur_nscb = schedule_list;
	prev_nscb = NULL;

	while (cur_nscb != NULL) {
		if ((cur_nscb->callback == callback) && (cur_nscb->p == p)) {
			unlnk_nscb = cur_nscb;
			cur_nscb = unlnk_nscb->next;

			if (prev_nscb == NULL) {
				schedule_list = cur_nscb;
			} else {
				prev_nscb->next = cur_nscb;
			}
			free(unlnk_nscb);
			removed = 1;
		} else {
			prev_nscb = cur_nscb;
			cur_nscb = prev_nscb->next;
		}
	}

	if (removed == 0) {
		return NSERROR_NOT_FOUND;
	}
	return NSERROR_OK;
}

/**
 * Schedule a callback after tival milliseconds.
 */
nserror tsunami_schedule(int tival, void (*callback)(void *p), void *p)
{
	struct nscallback *nscb;
	struct timeval tv;
	nserror ret;

	NSLOG(netsurf, INFO, "TABTRACE: schedule tival=%d cb=%p p=%p",
	      tival, (void *)callback, p);

	ret = schedule_remove(callback, p);
	if (tival < 0) {
		NSLOG(netsurf, INFO, "TABTRACE: schedule remove-only ret=%d",
		      (int)ret);
		return ret;
	}

	nscb = calloc(1, sizeof(struct nscallback));
	if (nscb == NULL) {
		return NSERROR_NOMEM;
	}

	/*
	 * tival==0 must be immediately due. Storing gettimeofday()+0 failed on
	 * OS3 when the next schedule_run's clock read was still "before" due,
	 * so the callback never fired and the UI looked frozen after New tab.
	 */
	if (tival == 0) {
		nscb->tv.tv_sec = 0;
		nscb->tv.tv_usec = 0;
	} else {
		tv.tv_sec = tival / 1000;
		tv.tv_usec = (tival % 1000) * 1000;
		gettimeofday(&nscb->tv, NULL);
		timeradd(&nscb->tv, &tv, &nscb->tv);
	}

	nscb->callback = callback;
	nscb->p = p;
	nscb->next = schedule_list;
	schedule_list = nscb;

	NSLOG(netsurf, INFO, "TABTRACE: schedule queued due sec=%ld usec=%ld",
	      (long)nscb->tv.tv_sec, (long)nscb->tv.tv_usec);
	return NSERROR_OK;
}

/**
 * Run due callbacks; return ms until next event, or -1 if none.
 */
int tsunami_schedule_run(void)
{
	struct timeval tv;
	struct timeval nexttime;
	struct timeval rettime;
	struct nscallback *cur_nscb;
	struct nscallback *prev_nscb;
	struct nscallback *unlnk_nscb;

	if (schedule_list == NULL) {
		return -1;
	}

	cur_nscb = schedule_list;
	prev_nscb = NULL;
	nexttime = cur_nscb->tv;

	gettimeofday(&tv, NULL);

	/* Only log when head is due — polling spam hid the real hang point. */
	if (timercmp(&tv, &cur_nscb->tv, >) ||
	    timercmp(&tv, &cur_nscb->tv, ==)) {
		NSLOG(netsurf, INFO,
		      "TABTRACE: schedule_run due now=%ld.%06ld head=%ld.%06ld",
		      (long)tv.tv_sec, (long)tv.tv_usec,
		      (long)cur_nscb->tv.tv_sec, (long)cur_nscb->tv.tv_usec);
	}

	while (cur_nscb != NULL) {
		if (timercmp(&tv, &cur_nscb->tv, >) ||
		    timercmp(&tv, &cur_nscb->tv, ==)) {
			unlnk_nscb = cur_nscb;

			if (prev_nscb == NULL) {
				schedule_list = unlnk_nscb->next;
			} else {
				prev_nscb->next = unlnk_nscb->next;
			}

			NSLOG(netsurf, INFO,
			      "TABTRACE: schedule_run FIRE cb=%p p=%p",
			      (void *)unlnk_nscb->callback, unlnk_nscb->p);
			unlnk_nscb->callback(unlnk_nscb->p);
			NSLOG(netsurf, INFO,
			      "TABTRACE: schedule_run FIRE returned");
			/* Keep KITT moving between long jobs on the event loop. */
			tsunami_gui_throbber_pulse();
			free(unlnk_nscb);

			if (schedule_list == NULL) {
				return -1;
			}

			cur_nscb = schedule_list;
			prev_nscb = NULL;
			nexttime = cur_nscb->tv;
		} else {
			if (timercmp(&nexttime, &cur_nscb->tv, >)) {
				nexttime = cur_nscb->tv;
			}
			prev_nscb = cur_nscb;
			cur_nscb = prev_nscb->next;
		}
	}

	timersub(&nexttime, &tv, &rettime);
	return (int)((rettime.tv_sec * 1000) + (rettime.tv_usec / 1000));
}
