/*
 * Copyright 2026 amigazen project
 *
 * This file is part of NetSurf / Tsunami (MUI frontend).
 */

#ifndef MUI_PLOTTERS_H
#define MUI_PLOTTERS_H

#include "netsurf/plotters.h"

struct RastPort;
struct ColorMap;

/**
 * Set the RastPort / origin used by subsequent plotter calls.
 * Called from the browser Area MUIM_Draw before browser_window_redraw.
 */
void tsunami_plot_set_target(struct RastPort *rp, int origin_x, int origin_y);

/**
 * ColorMap for ObtainBestPen during the current redraw.
 */
void tsunami_plot_set_colormap(struct ColorMap *cm);

extern const struct plotter_table *tsunami_plotters;

#endif /* MUI_PLOTTERS_H */
