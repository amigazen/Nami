/*
 * Copyright 2026 amigazen project
 *
 * Synthwave KITT / Cylon eye busy bar.
 * Always visible: activity advances the eye; idle fades LEDs to black.
 */

#ifndef TSUNAMI_MUI_THROBBER_H
#define TSUNAMI_MUI_THROBBER_H

#include "mui/os3support.h"
#include <libraries/mui.h>

BOOL tsunami_throbber_class_init(void);
void tsunami_throbber_class_fini(void);

Object *tsunami_throbber_new(void);

/**
 * One animation frame.
 * activity == 0: phosphor decay only (fade to black).
 * activity  > 0: light/advance the eye (higher = more steps this frame).
 * Returns TRUE while any LED still glows or the eye is moving.
 */
BOOL tsunami_throbber_tick(Object *obj, int activity);

#endif
