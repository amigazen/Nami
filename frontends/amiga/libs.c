/*
 * Copyright 2014-2020 Chris Young <chris@unsatisfactorysoftware.co.uk>
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

#include "amiga/libs.h"
#include "amiga/misc.h"
#include "utils/utils.h"
#include "utils/log.h"

#include <proto/dos.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/utility.h>

#include <graphics/gfxbase.h> /* Needed for v54 version check */

#ifndef __amigaos4__
/* OS3 needs these for the XXXX_GetClass() functions */
#include <proto/arexx.h>
#include <proto/bevel.h>
#include <proto/bitmap.h>
#include <proto/button.h>
#include <proto/chooser.h>
#include <proto/checkbox.h>
#include <proto/clicktab.h>
#include <proto/fuelgauge.h>
#include <proto/getfile.h>
#include <proto/getfont.h>
#include <proto/getscreenmode.h>
#include <proto/integer.h>
#include <proto/label.h>
#include <proto/layout.h>
#include <proto/listbrowser.h>
#include <proto/radiobutton.h>
#include <proto/requester.h>
#include <proto/scroller.h>
#include <proto/space.h>
#include <proto/speedbar.h>
#include <proto/string.h>
#include <proto/texteditor.h>
#include <proto/virtual.h>
#include <proto/window.h>
/* listview.gadget has inline GetClass but no proto/ wrapper in NDK3.2 */
#include <intuition/classes.h>
#include <inline/listview_protos.h>
#endif

#ifdef __amigaos4__
#define AMINS_LIB_OPEN(LIB, LIBVER, PREFIX, INTERFACE, INTVER, FAIL)	\
	NSLOG(netsurf, INFO, "Opening %s v%d", LIB, LIBVER);		\
	if((PREFIX##Base = (struct PREFIX##Base *)OpenLibrary(LIB, LIBVER))) {	\
		NSLOG(netsurf, INFO, " -> opened v%d.%d", ((struct Library *)PREFIX##Base)->lib_Version, ((struct Library *)PREFIX##Base)->lib_Revision);	\
		I##PREFIX = (struct PREFIX##IFace *)GetInterface((struct Library *)PREFIX##Base, INTERFACE, INTVER, NULL);	\
		if(I##PREFIX == NULL) {	\
			NSLOG(netsurf, INFO, "Failed to get %s interface v%d of %s", INTERFACE, INTVER, LIB); \
			AMINS_LIB_CLOSE(PREFIX)	\
			if(FAIL == true) {	\
				STRPTR error = ASPrintf("Unable to open interface %s v%d\nof %s v%ld (fatal error - not an OS4 lib?)", INTERFACE, INTVER, LIB, LIBVER);	\
				ami_misc_fatal_error(error);	\
				FreeVec(error);	\
				return false;	\
			}	\
		}	\
	} else {	\
		NSLOG(netsurf, INFO, "Failed to open %s v%d", LIB, LIBVER); \
		if(FAIL == true) {	\
			STRPTR error = ASPrintf("Unable to open %s v%ld (fatal error)", LIB, LIBVER);	\
			ami_misc_fatal_error(error);	\
			FreeVec(error);	\
			return false;	\
		}	\
	}

#define AMINS_LIB_CLOSE(PREFIX)	\
	if(I##PREFIX) DropInterface((struct Interface *)I##PREFIX);	\
	if(PREFIX##Base) CloseLibrary((struct Library *)PREFIX##Base);	\
	I##PREFIX = NULL;	\
	PREFIX##Base = NULL;

#define AMINS_LIB_STRUCT(PREFIX)	\
	struct PREFIX##Base *PREFIX##Base = NULL;	\
	struct PREFIX##IFace *I##PREFIX = NULL;

#define AMINS_CLASS_OPEN(CLASS, CLASSVER, PREFIX, CLASSGET, NEEDINTERFACE)	\
	NSLOG(netsurf, INFO, "Opening %s v%d", CLASS, CLASSVER);		\
	if((PREFIX##Base = OpenClass(CLASS, CLASSVER, &PREFIX##Class))) {	\
		NSLOG(netsurf, INFO, " -> opened v%d.%d", ((struct Library *)PREFIX##Base)->lib_Version, ((struct Library *)PREFIX##Base)->lib_Revision);	\
		if(NEEDINTERFACE == true) {	\
			NSLOG(netsurf, INFO, "        + interface");	\
			I##PREFIX = (struct PREFIX##IFace *)GetInterface((struct Library *)PREFIX##Base, "main", 1, NULL);	\
			if(I##PREFIX == NULL) {	\
				NSLOG(netsurf, ERROR, "Failed to get main interface v1 of %s", CLASS); \
			}	\
		}	\
	}	\
	if(PREFIX##Class == NULL) {	\
		NSLOG(netsurf, INFO, "Failed to open %s v%d", CLASS, CLASSVER); \
		STRPTR error = ASPrintf("Unable to open %s v%d (fatal error)", CLASS, CLASSVER);	\
		ami_misc_fatal_error(error);	\
		FreeVec(error);	\
		return false;	\
	}

#define AMINS_CLASS_CLOSE(PREFIX)	\
	if(I##PREFIX) DropInterface((struct Interface *)I##PREFIX);	\
	if(PREFIX##Base) CloseClass(PREFIX##Base);	\
	I##PREFIX = NULL;	\
	PREFIX##Base = NULL;

/* Optional class: no interface, non-fatal if missing (prefs/chrome). */
#define AMINS_CLASS_OPEN_OPT(CLASS, CLASSVER, PREFIX, CLASSGET)	\
	NSLOG(netsurf, INFO, "Opening %s v%d (optional)", CLASS, CLASSVER);		\
	if((PREFIX##Base = OpenClass(CLASS, CLASSVER, &PREFIX##Class))) {	\
		NSLOG(netsurf, INFO, " -> opened v%d.%d", ((struct Library *)PREFIX##Base)->lib_Version, ((struct Library *)PREFIX##Base)->lib_Revision);	\
	} else {	\
		NSLOG(netsurf, INFO, "Failed to open %s v%d (optional)", CLASS, CLASSVER); \
	}

#define AMINS_CLASS_CLOSE_OPT(PREFIX)	\
	if(PREFIX##Base) CloseClass(PREFIX##Base);	\
	PREFIX##Base = NULL;	\
	PREFIX##Class = NULL;

#define AMINS_CLASS_STRUCT(PREFIX)	\
	struct ClassLibrary *PREFIX##Base = NULL;	\
	struct PREFIX##IFace *I##PREFIX = NULL;	\
	Class *PREFIX##Class = NULL;

#define AMINS_CLASS_STRUCT_OPT(PREFIX)	\
	struct ClassLibrary *PREFIX##Base = NULL;	\
	Class *PREFIX##Class = NULL;

#else
#define AMINS_LIB_OPEN(LIB, LIBVER, PREFIX, INTERFACE, INTVER, FAIL)	\
	NSLOG(netsurf, INFO, "Opening %s v%d", LIB, LIBVER);		\
	if((PREFIX##Base = (struct PREFIX##Base *)OpenLibrary(LIB, LIBVER))) {	\
		NSLOG(netsurf, INFO, " -> opened v%d.%d", ((struct Library *)PREFIX##Base)->lib_Version, ((struct Library *)PREFIX##Base)->lib_Revision);	\
	} else {	\
		NSLOG(netsurf, INFO, "Failed to open %s v%d", LIB, LIBVER); \
		if(FAIL == true) {	\
			STRPTR error = ASPrintf("Unable to open %s v%d (fatal error)", LIB, LIBVER);	\
			ami_misc_fatal_error(error);	\
			FreeVec(error);	\
			return false;	\
		}	\
	}

#define AMINS_LIB_CLOSE(PREFIX)	\
	if(PREFIX##Base) CloseLibrary((struct Library *)PREFIX##Base);	\
	PREFIX##Base = NULL;

#define AMINS_LIB_STRUCT(PREFIX)	\
	struct PREFIX##Base *PREFIX##Base = NULL;

#define AMINS_CLASS_OPEN(CLASS, CLASSVER, PREFIX, CLASSGET, NEEDINTERFACE)	\
	NSLOG(netsurf, INFO, "Opening %s v%d", CLASS, CLASSVER);	\
	if((PREFIX##Base = OpenLibrary(CLASS, CLASSVER))) {	\
		NSLOG(netsurf, INFO, " -> opened v%d.%d", ((struct Library *)PREFIX##Base)->lib_Version, ((struct Library *)PREFIX##Base)->lib_Revision);	\
		PREFIX##Class = CLASSGET##_GetClass();	\
	}	\
	if(PREFIX##Class == NULL) {	\
		NSLOG(netsurf, INFO, "Failed to open %s v%d", CLASS, CLASSVER); \
		STRPTR error = ASPrintf("Unable to open %s v%d (fatal error)", CLASS, CLASSVER);	\
		ami_misc_fatal_error(error);	\
		FreeVec(error);	\
		return false;	\
	}

#define AMINS_CLASS_CLOSE(PREFIX)	\
	if(PREFIX##Base) CloseLibrary(PREFIX##Base);	\
	PREFIX##Base = NULL;

#define AMINS_CLASS_OPEN_OPT(CLASS, CLASSVER, PREFIX, CLASSGET)	\
	NSLOG(netsurf, INFO, "Opening %s v%d (optional)", CLASS, CLASSVER);	\
	if((PREFIX##Base = OpenLibrary(CLASS, CLASSVER))) {	\
		NSLOG(netsurf, INFO, " -> opened v%d.%d", ((struct Library *)PREFIX##Base)->lib_Version, ((struct Library *)PREFIX##Base)->lib_Revision);	\
		PREFIX##Class = CLASSGET##_GetClass();	\
	}	\
	if(PREFIX##Class == NULL) {	\
		NSLOG(netsurf, INFO, "Failed to open %s v%d (optional)", CLASS, CLASSVER); \
		if(PREFIX##Base) { CloseLibrary(PREFIX##Base); PREFIX##Base = NULL; }	\
	}

#define AMINS_CLASS_CLOSE_OPT(PREFIX)	\
	if(PREFIX##Base) CloseLibrary(PREFIX##Base);	\
	PREFIX##Base = NULL;	\
	PREFIX##Class = NULL;

#define AMINS_CLASS_STRUCT(PREFIX)	\
	struct Library *PREFIX##Base = NULL;	\
	Class *PREFIX##Class = NULL;

#define AMINS_CLASS_STRUCT_OPT(PREFIX)	\
	struct Library *PREFIX##Base = NULL;	\
	Class *PREFIX##Class = NULL;

#endif

#define GraphicsBase GfxBase /* graphics.library is a bit weird */

#ifdef __amigaos4__
AMINS_LIB_STRUCT(Application);
#else
	AMINS_LIB_STRUCT(Utility);
#endif
AMINS_LIB_STRUCT(Asl);
AMINS_LIB_STRUCT(DataTypes);
AMINS_LIB_STRUCT(Diskfont);
AMINS_LIB_STRUCT(Graphics);
AMINS_LIB_STRUCT(GadTools);
AMINS_LIB_STRUCT(Icon);
AMINS_LIB_STRUCT(IFFParse);
AMINS_LIB_STRUCT(Intuition);
AMINS_LIB_STRUCT(Keymap);
AMINS_LIB_STRUCT(Layers);
AMINS_LIB_STRUCT(Locale);
AMINS_LIB_STRUCT(Workbench);

AMINS_LIB_STRUCT(Codesets);
AMINS_LIB_STRUCT(GuiGFX);
/* proto/cybergraphics.h and proto/ttengine.h expect Library * bases */
struct Library *CyberGfxBase = NULL;
struct Library *TTEngineBase = NULL;
/* proto/bullet.h — direct GlyphEngine SetInfo/ObtainInfo (BulletExamples) */
struct Library *BulletBase = NULL;

AMINS_CLASS_STRUCT(ARexx);
AMINS_CLASS_STRUCT(Bevel);
AMINS_CLASS_STRUCT(BitMap);
AMINS_CLASS_STRUCT(Button);
AMINS_CLASS_STRUCT(Chooser);
AMINS_CLASS_STRUCT(CheckBox);
AMINS_CLASS_STRUCT(ClickTab);
AMINS_CLASS_STRUCT(FuelGauge);
AMINS_CLASS_STRUCT(GetFile);
AMINS_CLASS_STRUCT(GetFont);
AMINS_CLASS_STRUCT(GetScreenMode);
AMINS_CLASS_STRUCT(Integer);
AMINS_CLASS_STRUCT(Label);
AMINS_CLASS_STRUCT(Layout);
AMINS_CLASS_STRUCT(ListBrowser);
AMINS_CLASS_STRUCT_OPT(ListView);
AMINS_CLASS_STRUCT(RadioButton);
AMINS_CLASS_STRUCT_OPT(Requester);
#ifndef __amigaos4__
AMINS_CLASS_STRUCT(Page);
#endif
AMINS_CLASS_STRUCT(Scroller);
AMINS_CLASS_STRUCT(Space);
AMINS_CLASS_STRUCT(SpeedBar);
AMINS_CLASS_STRUCT(String);
AMINS_CLASS_STRUCT_OPT(Tabs);
AMINS_CLASS_STRUCT_OPT(Virtual);
AMINS_CLASS_STRUCT(Window);

/* texteditor.gadget: NDK base name is TextFieldBase, not TextEditorBase */
struct Library *TextFieldBase = NULL;
Class *TextEditorClass = NULL;

/* Optional LED / BoingBall (public class names; no GetClass required) */
struct Library *PenMapBase = NULL;
struct Library *LedBase = NULL;
struct Library *BoingBallBase = NULL;


bool ami_libs_open(void)
{
#ifdef __amigaos4__
	/* Libraries only needed on OS4 */
	AMINS_LIB_OPEN("application.library",  53, Application, "application", 2, false)
	AMINS_LIB_OPEN("dos.library",          37, DOS,         "main",        1, true)
#else
	/* Libraries we get automatically on OS4 but not OS3 */
	AMINS_LIB_OPEN("utility.library",      37, Utility,     "main",        1, true)
#endif
	/* Standard libraries for both versions */
	AMINS_LIB_OPEN("asl.library",          37, Asl,         "main",        1, true)
	AMINS_LIB_OPEN("datatypes.library",    39, DataTypes,   "main",        1, true)
	AMINS_LIB_OPEN("diskfont.library",     40, Diskfont,    "main",        1, true)
	AMINS_LIB_OPEN("gadtools.library",     37, GadTools,    "main",        1, true)
	AMINS_LIB_OPEN("graphics.library",     40, Graphics,    "main",        1, true)
	AMINS_LIB_OPEN("icon.library",         44, Icon,        "main",        1, true)
	AMINS_LIB_OPEN("iffparse.library",     37, IFFParse,    "main",        1, true)
	AMINS_LIB_OPEN("intuition.library",    40, Intuition,   "main",        1, true)
	AMINS_LIB_OPEN("keymap.library",       37, Keymap,      "main",        1, true)
	AMINS_LIB_OPEN("layers.library",       37, Layers,      "main",        1, true)
	AMINS_LIB_OPEN("locale.library",       38, Locale,      "main",        1, true)
	AMINS_LIB_OPEN("workbench.library",    37, Workbench,   "main",        1, true)

	/* cybergraphics.library for chunky Write/ReadPixelArray on OS3 RTG */
	NSLOG(netsurf, INFO, "Opening cybergraphics.library v41");
	CyberGfxBase = OpenLibrary("cybergraphics.library", 41);
	if (CyberGfxBase != NULL) {
		NSLOG(netsurf, INFO, " -> opened v%d.%d",
		      CyberGfxBase->lib_Version, CyberGfxBase->lib_Revision);
	} else {
		NSLOG(netsurf, INFO, "Failed to open cybergraphics.library v41 (optional)");
	}

	/* bullet.library for direct GlyphEngine calls (see BulletExamples) */
	NSLOG(netsurf, INFO, "Opening bullet.library");
	BulletBase = OpenLibrary("bullet.library", 0);
	if (BulletBase != NULL) {
		NSLOG(netsurf, INFO, " -> opened v%d.%d",
		      BulletBase->lib_Version, BulletBase->lib_Revision);
	} else {
		NSLOG(netsurf, INFO, "Failed to open bullet.library (optional; OutlineFont may still open it)");
	}

	/* Non-OS provided libraries */
	AMINS_LIB_OPEN("codesets.library",    6, Codesets,   "main",        1, false)
	AMINS_LIB_OPEN("guigfx.library",      9, GuiGFX,     "main",        1, false)
	/* POSIX regex is statically linked (frontends/amiga/regex/) — no regex.library */
	NSLOG(netsurf, INFO, "Opening ttengine.library v6");
	TTEngineBase = OpenLibrary("ttengine.library", 6);
	if (TTEngineBase != NULL) {
		NSLOG(netsurf, INFO, " -> opened v%d.%d",
		      TTEngineBase->lib_Version, TTEngineBase->lib_Revision);
	} else {
		NSLOG(netsurf, INFO, "Failed to open ttengine.library v6 (optional)");
	}

	/* NB: timer.device is opened in schedule.c (ultimately by the scheduler process).
	 * The library base and interface are obtained there, rather than here, due to
	 * the additional complexities of opening devices, which aren't important here
	 * (as we only need the library interface), but are important for the scheduler
	 * (as it also uses the device interface).  We trust that the scheduler has
	 * initialised before any other code requires the timer's library interface,
	 * to avoid opening it twice.
	 */

	/* BOOPSI classes.
	 * Opened using class functions rather than the old-fashioned method.
	 * We get the class pointer once and used our stored copy.
	 * On OS4 these must be opened *after* intuition.library.
	 * NB: the last argument should be "true" only if the class also has
	 * library functions we use.
	 */
	AMINS_CLASS_OPEN("arexx.class",                  41, ARexx,         AREXX,         false)
	AMINS_CLASS_OPEN("images/bevel.image",           41, Bevel,         BEVEL,         false)
	AMINS_CLASS_OPEN("images/bitmap.image",          41, BitMap,        BITMAP,        false)
	AMINS_CLASS_OPEN("gadgets/button.gadget",        42, Button,        BUTTON,        false)
	AMINS_CLASS_OPEN("gadgets/checkbox.gadget",      41, CheckBox,      CHECKBOX,      false)
	AMINS_CLASS_OPEN("gadgets/chooser.gadget",       41, Chooser,       CHOOSER,       true)
	AMINS_CLASS_OPEN("gadgets/clicktab.gadget",      42, ClickTab,      CLICKTAB,      true)
	AMINS_CLASS_OPEN("gadgets/fuelgauge.gadget",     41, FuelGauge,     FUELGAUGE,     false)
	AMINS_CLASS_OPEN("gadgets/getfile.gadget",       41, GetFile,       GETFILE,       false)
	AMINS_CLASS_OPEN("gadgets/getfont.gadget",       41, GetFont,       GETFONT,       false)
	AMINS_CLASS_OPEN("gadgets/getscreenmode.gadget", 41, GetScreenMode, GETSCREENMODE, false)
	AMINS_CLASS_OPEN("gadgets/integer.gadget",       41, Integer,       INTEGER,       false)
	AMINS_CLASS_OPEN("images/label.image",           41, Label,         LABEL,         false)
	AMINS_CLASS_OPEN("gadgets/layout.gadget",        43, Layout,        LAYOUT,        true)
	AMINS_CLASS_OPEN("gadgets/listbrowser.gadget",   41, ListBrowser,   LISTBROWSER,   true)
	AMINS_CLASS_OPEN("gadgets/radiobutton.gadget",   41, RadioButton,   RADIOBUTTON,   false)
	AMINS_CLASS_OPEN("gadgets/scroller.gadget",      42, Scroller,      SCROLLER,      false)
	AMINS_CLASS_OPEN("gadgets/space.gadget",         41, Space,         SPACE,         false)
	AMINS_CLASS_OPEN("gadgets/speedbar.gadget",      41, SpeedBar,      SPEEDBAR,      true)
	AMINS_CLASS_OPEN("gadgets/string.gadget",        41, String,        STRING,        false)
	AMINS_CLASS_OPEN("window.class",                 42, Window,        WINDOW,        false)

	/* Optional ReAction classes for prefs/chrome UI (not HTML form controls). */
	AMINS_CLASS_OPEN_OPT("requester.class",            47, Requester,     REQUESTER)
	AMINS_CLASS_OPEN_OPT("gadgets/listview.gadget",    40, ListView,      LISTVIEW)
	AMINS_CLASS_OPEN_OPT("gadgets/virtual.gadget",     41, Virtual,       VIRTUAL)
#ifdef __amigaos4__
	AMINS_CLASS_OPEN_OPT("gadgets/tabs.gadget",        47, Tabs,          TABS)
#else
	/* tabs.gadget has no GetClass() in NDK3.2 — open the library so the
	 * public class name works with NewObject(NULL, "gadgets/tabs.gadget").
	 */
	NSLOG(netsurf, INFO, "Opening gadgets/tabs.gadget v47 (optional)");
	TabsBase = OpenLibrary("gadgets/tabs.gadget", 47);
	if (TabsBase != NULL) {
		NSLOG(netsurf, INFO, " -> opened v%d.%d",
		      TabsBase->lib_Version, TabsBase->lib_Revision);
	} else {
		NSLOG(netsurf, INFO, "Failed to open gadgets/tabs.gadget v47 (optional)");
	}
#endif

	NSLOG(netsurf, INFO, "Opening gadgets/texteditor.gadget v41 (optional)");
#ifdef __amigaos4__
	TextFieldBase = (struct Library *)OpenClass("gadgets/texteditor.gadget", 41,
						    &TextEditorClass);
	if (TextFieldBase != NULL) {
		NSLOG(netsurf, INFO, " -> opened v%d.%d",
		      TextFieldBase->lib_Version, TextFieldBase->lib_Revision);
	} else {
		NSLOG(netsurf, INFO, "Failed to open gadgets/texteditor.gadget v41 (optional)");
	}
#else
	TextFieldBase = OpenLibrary("gadgets/texteditor.gadget", 41);
	if (TextFieldBase != NULL) {
		NSLOG(netsurf, INFO, " -> opened v%d.%d",
		      TextFieldBase->lib_Version, TextFieldBase->lib_Revision);
		TextEditorClass = TEXTEDITOR_GetClass();
		if (TextEditorClass == NULL) {
			CloseLibrary(TextFieldBase);
			TextFieldBase = NULL;
			NSLOG(netsurf, INFO,
			      "Failed to get texteditor class (optional)");
		}
	} else {
		NSLOG(netsurf, INFO, "Failed to open gadgets/texteditor.gadget v41 (optional)");
	}
#endif

	/* LED blink + BoingBall spinner (optional; theme filmstrip is fallback) */
	NSLOG(netsurf, INFO, "Opening images/penmap.image v47 (optional)");
	PenMapBase = OpenLibrary("images/penmap.image", 47);
	if(PenMapBase != NULL) {
		NSLOG(netsurf, INFO, " -> opened v%d.%d",
		      PenMapBase->lib_Version, PenMapBase->lib_Revision);
	} else {
		NSLOG(netsurf, INFO, "Failed to open images/penmap.image v47 (optional)");
	}
	NSLOG(netsurf, INFO, "Opening images/led.image v47 (optional)");
	LedBase = OpenLibrary("images/led.image", 47);
	if(LedBase != NULL) {
		NSLOG(netsurf, INFO, " -> opened v%d.%d",
		      LedBase->lib_Version, LedBase->lib_Revision);
	} else {
		NSLOG(netsurf, INFO, "Failed to open images/led.image v47 (optional)");
	}
	NSLOG(netsurf, INFO, "Opening images/boingball.image v47 (optional)");
	BoingBallBase = OpenLibrary("images/boingball.image", 47);
	if(BoingBallBase != NULL) {
		NSLOG(netsurf, INFO, " -> opened v%d.%d",
		      BoingBallBase->lib_Version, BoingBallBase->lib_Revision);
	} else {
		NSLOG(netsurf, INFO, "Failed to open images/boingball.image v47 (optional)");
	}

#ifndef __amigaos4__
	/* BOOPSI classes only required prior to OS4 */
	PageClass = PAGE_GetClass();
#endif

	return true;
}

void ami_libs_close(void)
{
	/* BOOPSI Classes.
	 * On OS4 these must be closed *before* intuition.library
	 */
	AMINS_CLASS_CLOSE(ARexx)
	AMINS_CLASS_CLOSE(Bevel)
	AMINS_CLASS_CLOSE(BitMap)
	AMINS_CLASS_CLOSE(Button)
	AMINS_CLASS_CLOSE(CheckBox)
	AMINS_CLASS_CLOSE(Chooser)
	AMINS_CLASS_CLOSE(ClickTab)
	AMINS_CLASS_CLOSE(FuelGauge)
	AMINS_CLASS_CLOSE(GetFile)
	AMINS_CLASS_CLOSE(GetFont)
	AMINS_CLASS_CLOSE(GetScreenMode)
	AMINS_CLASS_CLOSE(Integer)
	AMINS_CLASS_CLOSE(Label)
	AMINS_CLASS_CLOSE(Layout)
	AMINS_CLASS_CLOSE(ListBrowser)
	AMINS_CLASS_CLOSE_OPT(ListView)
	AMINS_CLASS_CLOSE(RadioButton)
	AMINS_CLASS_CLOSE_OPT(Requester)
	AMINS_CLASS_CLOSE(Scroller)
	AMINS_CLASS_CLOSE(Space)
	AMINS_CLASS_CLOSE(SpeedBar)
	AMINS_CLASS_CLOSE(String)
	AMINS_CLASS_CLOSE_OPT(Tabs)
	AMINS_CLASS_CLOSE_OPT(Virtual)
	AMINS_CLASS_CLOSE(Window)

#ifdef __amigaos4__
	if (TextFieldBase != NULL) {
		CloseClass((struct ClassLibrary *)TextFieldBase);
	}
#else
	if (TextFieldBase != NULL) {
		CloseLibrary(TextFieldBase);
	}
#endif
	TextFieldBase = NULL;
	TextEditorClass = NULL;

	if(BoingBallBase != NULL) {
		CloseLibrary(BoingBallBase);
		BoingBallBase = NULL;
	}
	if(LedBase != NULL) {
		CloseLibrary(LedBase);
		LedBase = NULL;
	}
	if(PenMapBase != NULL) {
		CloseLibrary(PenMapBase);
		PenMapBase = NULL;
	}

	/* Libraries */
	AMINS_LIB_CLOSE(Codesets)
	AMINS_LIB_CLOSE(GuiGFX)
	if (TTEngineBase != NULL) {
		CloseLibrary(TTEngineBase);
		TTEngineBase = NULL;
	}
	if (CyberGfxBase != NULL) {
		CloseLibrary(CyberGfxBase);
		CyberGfxBase = NULL;
	}
	if (BulletBase != NULL) {
		CloseLibrary(BulletBase);
		BulletBase = NULL;
	}

	AMINS_LIB_CLOSE(Asl)
	AMINS_LIB_CLOSE(DataTypes)
	AMINS_LIB_CLOSE(Diskfont)
	AMINS_LIB_CLOSE(GadTools)
	AMINS_LIB_CLOSE(Graphics)
	AMINS_LIB_CLOSE(Icon)
	AMINS_LIB_CLOSE(IFFParse)
	AMINS_LIB_CLOSE(Intuition)
	AMINS_LIB_CLOSE(Keymap)
	AMINS_LIB_CLOSE(Layers)
	AMINS_LIB_CLOSE(Locale)
	AMINS_LIB_CLOSE(Workbench)
#ifdef __amigaos4__
	AMINS_LIB_CLOSE(Application)
	AMINS_LIB_CLOSE(DOS)
#else
	AMINS_LIB_CLOSE(Utility)
#endif
}

