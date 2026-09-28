/*
 * Copyright 2026 amigazen project
 *
 * This file is part of NetSurf / Tsunami (MUI frontend).
 */

#ifndef MUI_SCHEDULE_H
#define MUI_SCHEDULE_H

#include "utils/errors.h"

/**
 * Schedule a callback after tival milliseconds.
 * Pass a negative tival to remove a matching callback.
 */
nserror tsunami_schedule(int tival, void (*callback)(void *p), void *p);

/**
 * Run due callbacks; return milliseconds until next event, or -1 if none.
 */
int tsunami_schedule_run(void);

#endif /* MUI_SCHEDULE_H */
