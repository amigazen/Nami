/*
 * Copyright 2026 amigazen project
 *
 * Custom MUI Area: KITT / Cylon eye — discrete LEDs, per-cell phosphor
 * decay, synthwave palette that cycles as the eye sweeps.
 *
 * Always visible. activity>0 drives the eye; activity==0 only decays so
 * the strip fades to black instead of being hidden.
 */

#include "mui/os3support.h"

#include <proto/exec.h>
#include <proto/graphics.h>
#include <proto/intuition.h>
#include <proto/utility.h>
#include <proto/muimaster.h>
#include <libraries/mui.h>

#include <stdio.h>
#include <string.h>

#include "mui/throbber.h"

#define THROB_LEDS	16
#define THROB_HUES	6
#define THROB_LEVELS	5
#define THROB_HUE_HOLD	10

static const int throb_level_pct[THROB_LEVELS] = { 100, 72, 45, 24, 10 };

static const UBYTE throb_hue_rgb[THROB_HUES][3] = {
	{ 255, 110, 200 },
	{ 255, 70, 180 },
	{ 200, 120, 255 },
	{ 150, 90, 255 },
	{ 80, 230, 230 },
	{ 70, 170, 255 }
};

struct ThrobData {
	UBYTE bright[THROB_LEDS];
	int pos;
	int dir;
	int hue;
	int hue_hold;
	LONG pen[THROB_HUES][THROB_LEVELS];
	LONG pen_off;
	LONG pen_bg;
	int pens_ok;
};

static struct MUI_CustomClass *TsunamiThrobberClass;

static ULONG rgb8_to_penspec_chan(UBYTE v)
{
	ULONG x;

	x = (ULONG)v;
	return (x << 24) | (x << 16) | (x << 8) | x;
}

static void make_penspec(struct MUI_PenSpec *spec,
			 UBYTE r, UBYTE g, UBYTE b, int pct)
{
	ULONG rr;
	ULONG gg;
	ULONG bb;

	rr = ((ULONG)r * (ULONG)pct) / 100UL;
	gg = ((ULONG)g * (ULONG)pct) / 100UL;
	bb = ((ULONG)b * (ULONG)pct) / 100UL;
	sprintf(spec->buf, "r%08lx,%08lx,%08lx",
		(unsigned long)rgb8_to_penspec_chan((UBYTE)rr),
		(unsigned long)rgb8_to_penspec_chan((UBYTE)gg),
		(unsigned long)rgb8_to_penspec_chan((UBYTE)bb));
}

static void throb_release_pens(Object *obj, struct ThrobData *data)
{
	int h;
	int lv;

	if (data->pens_ok == 0) {
		return;
	}
	for (h = 0; h < THROB_HUES; h++) {
		for (lv = 0; lv < THROB_LEVELS; lv++) {
			MUI_ReleasePen(muiRenderInfo(obj), data->pen[h][lv]);
		}
	}
	MUI_ReleasePen(muiRenderInfo(obj), data->pen_off);
	MUI_ReleasePen(muiRenderInfo(obj), data->pen_bg);
	data->pens_ok = 0;
}

static void throb_obtain_pens(Object *obj, struct ThrobData *data)
{
	struct MUI_PenSpec spec;
	int h;
	int lv;

	for (h = 0; h < THROB_HUES; h++) {
		for (lv = 0; lv < THROB_LEVELS; lv++) {
			make_penspec(&spec,
				     throb_hue_rgb[h][0],
				     throb_hue_rgb[h][1],
				     throb_hue_rgb[h][2],
				     throb_level_pct[lv]);
			data->pen[h][lv] = MUI_ObtainPen(muiRenderInfo(obj),
							 &spec, 0);
		}
	}

	make_penspec(&spec, 24, 12, 36, 100);
	data->pen_off = MUI_ObtainPen(muiRenderInfo(obj), &spec, 0);
	make_penspec(&spec, 12, 6, 20, 100);
	data->pen_bg = MUI_ObtainPen(muiRenderInfo(obj), &spec, 0);
	data->pens_ok = 1;
}

static int bright_to_level(UBYTE b)
{
	if (b >= 220) {
		return 0;
	}
	if (b >= 150) {
		return 1;
	}
	if (b >= 90) {
		return 2;
	}
	if (b >= 40) {
		return 3;
	}
	if (b >= 12) {
		return 4;
	}
	return -1;
}

static void decay_leds(struct ThrobData *data)
{
	int i;
	int dec;
	int b;

	for (i = 0; i < THROB_LEDS; i++) {
		b = (int)data->bright[i];
		if (b <= 0) {
			continue;
		}
		dec = (b >> 3) + (b >> 5) + 5;
		if (dec > b) {
			data->bright[i] = 0;
		} else {
			data->bright[i] = (UBYTE)(b - dec);
		}
	}
}

static int any_led_lit(struct ThrobData *data)
{
	int i;

	for (i = 0; i < THROB_LEDS; i++) {
		if (data->bright[i] != 0) {
			return 1;
		}
	}
	return 0;
}

static void advance_eye(struct ThrobData *data)
{
	data->bright[data->pos] = 255;

	data->pos += data->dir;
	if (data->pos >= THROB_LEDS - 1) {
		data->pos = THROB_LEDS - 1;
		data->dir = -1;
	} else if (data->pos <= 0) {
		data->pos = 0;
		data->dir = 1;
	}

	data->hue_hold--;
	if (data->hue_hold <= 0) {
		data->hue_hold = THROB_HUE_HOLD;
		data->hue++;
		if (data->hue >= THROB_HUES) {
			data->hue = 0;
		}
	}
}

static ULONG mNew(struct IClass *cl, Object *obj, struct opSet *msg)
{
	struct ThrobData *data;
	int i;

	obj = (Object *)DoSuperMethodA(cl, obj, (Msg)msg);
	if (obj == NULL) {
		return 0;
	}

	data = INST_DATA(cl, obj);
	for (i = 0; i < THROB_LEDS; i++) {
		data->bright[i] = 0;
	}
	data->pos = 0;
	data->dir = 1;
	data->hue = 0;
	data->hue_hold = THROB_HUE_HOLD;
	data->pens_ok = 0;
	return (ULONG)obj;
}

static ULONG mAskMinMax(struct IClass *cl, Object *obj,
			struct MUIP_AskMinMax *msg)
{
	DoSuperMethodA(cl, obj, (Msg)msg);
	msg->MinMaxInfo->MinWidth += 120;
	msg->MinMaxInfo->DefWidth += 120;
	msg->MinMaxInfo->MaxWidth = MUI_MAXMAX;
	msg->MinMaxInfo->MinHeight += 12;
	msg->MinMaxInfo->DefHeight += 12;
	msg->MinMaxInfo->MaxHeight += 16;
	return 0;
}

static ULONG mSetup(struct IClass *cl, Object *obj, Msg msg)
{
	struct ThrobData *data;

	data = INST_DATA(cl, obj);
	if (!DoSuperMethodA(cl, obj, msg)) {
		return FALSE;
	}
	throb_obtain_pens(obj, data);
	return TRUE;
}

static ULONG mCleanup(struct IClass *cl, Object *obj, Msg msg)
{
	struct ThrobData *data;

	data = INST_DATA(cl, obj);
	throb_release_pens(obj, data);
	return DoSuperMethodA(cl, obj, msg);
}

static ULONG mDraw(struct IClass *cl, Object *obj, struct MUIP_Draw *msg)
{
	struct ThrobData *data;
	struct RastPort *rp;
	LONG left;
	LONG top;
	LONG width;
	LONG height;
	LONG gap;
	LONG led_w;
	LONG x0;
	LONG x1;
	LONG i;
	int lv;
	int hue;

	data = INST_DATA(cl, obj);
	DoSuperMethodA(cl, obj, (Msg)msg);

	if (!(msg->flags & MADF_DRAWOBJECT)) {
		return 0;
	}
	if (data->pens_ok == 0) {
		return 0;
	}

	rp = _rp(obj);
	left = _mleft(obj);
	top = _mtop(obj);
	width = _mwidth(obj);
	height = _mheight(obj);
	if (width < THROB_LEDS * 2 || height < 4) {
		return 0;
	}

	SetAPen(rp, MUIPEN(data->pen_bg));
	RectFill(rp, left, top, left + width - 1, top + height - 1);

	gap = 2;
	led_w = (width - gap * (THROB_LEDS - 1)) / THROB_LEDS;
	if (led_w < 2) {
		led_w = 2;
		gap = 1;
	}

	hue = data->hue;
	if (hue < 0 || hue >= THROB_HUES) {
		hue = 0;
	}

	for (i = 0; i < THROB_LEDS; i++) {
		x0 = left + i * (led_w + gap);
		x1 = x0 + led_w - 1;
		if (x1 >= left + width) {
			x1 = left + width - 1;
		}

		lv = bright_to_level(data->bright[i]);
		if (lv < 0) {
			SetAPen(rp, MUIPEN(data->pen_off));
		} else {
			SetAPen(rp, MUIPEN(data->pen[hue][lv]));
		}
		RectFill(rp, x0, top + 1, x1, top + height - 2);
	}

	return 0;
}

BOOL tsunami_throbber_tick(Object *obj, int activity)
{
	struct ThrobData *data;
	int steps;
	int i;

	if (obj == NULL || TsunamiThrobberClass == NULL) {
		return FALSE;
	}
	data = INST_DATA(TsunamiThrobberClass->mcc_Class, obj);

	decay_leds(data);

	if (activity > 0) {
		steps = 1;
		if (activity >= 6) {
			steps = 2;
		}
		if (activity >= 10) {
			steps = 3;
		}
		for (i = 0; i < steps; i++) {
			advance_eye(data);
		}
	}

	MUI_Redraw(obj, MADF_DRAWOBJECT);
	return any_led_lit(data) || (activity > 0);
}

static ULONG ASM SAVEDS ThrobDispatcher(REG(a0, struct IClass *cl),
					REG(a2, Object *obj),
					REG(a1, Msg msg))
{
	switch (msg->MethodID) {
	case OM_NEW:
		return mNew(cl, obj, (struct opSet *)msg);
	case MUIM_AskMinMax:
		return mAskMinMax(cl, obj, (struct MUIP_AskMinMax *)msg);
	case MUIM_Setup:
		return mSetup(cl, obj, msg);
	case MUIM_Cleanup:
		return mCleanup(cl, obj, msg);
	case MUIM_Draw:
		return mDraw(cl, obj, (struct MUIP_Draw *)msg);
	}
	return DoSuperMethodA(cl, obj, msg);
}

BOOL tsunami_throbber_class_init(void)
{
	TsunamiThrobberClass = MUI_CreateCustomClass(NULL, MUIC_Area, NULL,
						     sizeof(struct ThrobData),
						     (APTR)ThrobDispatcher);
	return TsunamiThrobberClass != NULL;
}

void tsunami_throbber_class_fini(void)
{
	if (TsunamiThrobberClass != NULL) {
		MUI_DeleteCustomClass(TsunamiThrobberClass);
		TsunamiThrobberClass = NULL;
	}
}

Object *tsunami_throbber_new(void)
{
	if (TsunamiThrobberClass == NULL ||
	    TsunamiThrobberClass->mcc_Class == NULL) {
		return NULL;
	}
	return NewObject(TsunamiThrobberClass->mcc_Class, NULL,
			 GaugeFrame,
			 MUIA_FixWidth, 120,
			 MUIA_Weight, 0,
			 MUIA_FillArea, FALSE,
			 MUIA_ShortHelp, "Activity",
			 TAG_DONE);
}
