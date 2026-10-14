#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
check_mui_tree.py -- den MUI-Objektbaum in v4_gui.c pruefen, BEVOR er auf die
V4 kommt.

WOZU
----
Ein fehlerhaft verschachtelter MUI-Objektbaum laesst MUI **beim Aufbau**
abstuerzen -- auf dem Geraet ist dann nur noch "es kam kein Fenster" zu sehen.
Der C-Uebersetzer kann diese Klasse von Fehlern NICHT finden:

  * `Child`/`End` sind Makros (`#define Child MUIA_Group_Child`,
    `#define End TAG_DONE)`). Ein fehlendes `End` ergibt eine
    Klammerbalance, die der Uebersetzer sehr wohl meldet -- ein **falsch
    gesetztes** `End` dagegen schliesst eine Gruppe vorzeitig, ein Objekt
    landet an der falschen Stelle im Baum, und alles uebersetzt sauber.
  * Ein Tag ohne Wert ist ebenfalls nicht zu sehen: `MUIA_X, MUIA_Y, 1` ist
    fuer den Uebersetzer die regulaere Tag-Liste `{X, Y, 1, TAG_DONE}`. Fuer
    MUI ist sie es nicht: `Y` steht dort, wo ein Wert erwartet wird, und die
    Liste laeuft aus dem Tritt.
  * Ein Tag, das zu einer ANDEREN Klasse gehoert (z.B. `MUIA_Cycle_Entries`
    an einem `TextObject`), uebersetzt einwandfrei und laesst MUI beim Aufbau
    sterben.

DER WEG
-------
Ein MUI-Objektbaum ist eine Liste von Ausdruecken, die auf der obersten
Klammerungsebene durch Kommas getrennt sind:

    s_app = ApplicationObject,
        MUIA_Application_Title, (IPTR)"...",
        MUIA_Application_Window, s_win = WindowObject,
            MUIA_Window_Title, "...",
            WindowContents, VGroup,
                Child, s_link = TextObject,
                    MUIA_Text_Contents, (IPTR)"...",
                End,
            End,
        End,
    End;

Deshalb wird genau so zerlegt: an Kommas der Tiefe 0. Jedes Element ist dann
eindeutig eines von

    Objekt-Anfang   (ApplicationObject, WindowObject, TextObject, VGroup,
                     HGroup, CycleObject, ListviewObject, ListObject,
                     MUI_MakeObject)
    Kind-Marke      (Child)
    Fensterinhalt   (WindowContents)
    Ende            (End)
    Tag             (MUIA_* / MUIM_* / MUIV_*)
    Wert            (alles andere)

und daraus wird der Baum nachgebaut und geprueft:

  1. Klammerbalance: jedes Objekt wird von genau einem `End` geschlossen, die
     Verschachtelungstiefe endet bei 0.
  2. Jedes `Child` ist von genau EINEM Objekt gefolgt.
  3. Jedes Tag hat genau EINEN Wert (kein Tag in Wertposition).
  4. Jedes Tag gehoert zur Klasse seines Objekts (Klassenlisten unten).
  5. Jede Gruppe hat mindestens ein Kind.
  6. `WindowContents` kommt genau einmal vor und traegt ein Objekt.
  7. Die Wurzel ist genau eine Anwendung.
  8. `MUIA_Cycle_Entries` bekommt ein NULL-abgeschlossenes Array von
     Zeichenketten (`STRPTR *`), keine mit `|` getrennte Zeichenkette. Ein
     Zeiger auf eine Zeichenkette liest MUI als Zeigerarray: der Cycle-Knopf
     bleibt leer und wird ueber die ganze Breite gezogen.

Aufruf:  python3 tools/check_mui_tree.py [v4_gui.c]
Exit 0 = sauber, Exit 1 = Befund (mit Datei und Zeile), Exit 2 = Datei fehlt.
"""

import os
import re
import sys
import tempfile

# --------------------------------------------------------------------- #
# Klassen und ihre erlaubten Attribute.                                  #
#                                                                        #
# Die "gemeinsamen" Attribute gehoeren in MUI zu MUI_Area und sind damit  #
# an jedem Objekt erlaubt. Alles andere ist klassenspezifisch.            #
# --------------------------------------------------------------------- #

GEMEINSAM = {
    "MUIA_Weight", "MUIA_HorizWeight", "MUIA_VertWeight", "MUIA_FixWidth",
    "MUIA_FixHeight", "MUIA_MinWidth", "MUIA_MinHeight", "MUIA_MaxWidth",
    "MUIA_MaxHeight", "MUIA_Font", "MUIA_Background", "MUIA_Frame",
    "MUIA_FrameTitle", "MUIA_FramePhantomHoriz", "MUIA_InnerLeft",
    "MUIA_InnerRight", "MUIA_InnerTop", "MUIA_InnerBottom", "MUIA_Disabled",
    "MUIA_Hidden", "MUIA_ShowMe", "MUIA_HelpNode", "MUIA_ContextMenu",
    "MUIA_UserData", "MUIA_ShortHelp", "MUIA_ControlChar", "MUIA_InputMode",
    "MUIA_Selected", "MUIA_Image", "MUIA_Image_Spec", "MUIA_FillArea",
    "MUIA_Draggable", "MUIA_Dropable",
}

KLASSEN = {
    "MUI_MakeObject": set(),
    "ApplicationObject": {
        "MUIA_Application_Title", "MUIA_Application_Version",
        "MUIA_Application_Copyright", "MUIA_Application_Author",
        "MUIA_Application_Description", "MUIA_Application_Base",
        "MUIA_Application_Window", "MUIA_Application_SingleTask",
        "MUIA_Application_HelpGroup", "MUIA_Application_Iconified",
        "MUIA_Application_NewInputProc", "MUIA_Application_Menustrip",
        "MUIA_Application_UsedClasses", "MUIA_Application_DropObject",
        "MUIA_Application_Commands", "MUIA_Application_DiskObject",
        "MUIA_Application_Active", "MUIA_Application_RexxString",
        "MUIA_Application_RexxMsg", "MUIA_Application_OpenConfigWindow",
        "MUIA_Application_TitleBar", "MUIA_Application_Sleep",
    },
    "WindowObject": {
        "MUIA_Window_Title", "MUIA_Window_ID", "MUIA_Window_Width",
        "MUIA_Window_Height", "MUIA_Window_SizeGadget", "MUIA_Window_SizeRight",
        "MUIA_Window_SizeBottom", "MUIA_Window_SizeLeft", "MUIA_Window_SizeTop",
        "MUIA_Window_RootObject", "MUIA_Window_Open", "MUIA_Window_Screen",
        "MUIA_Window_ScreenTitle", "MUIA_Window_PublicScreen",
        "MUIA_Window_CloseRequest", "MUIA_Window_LeftEdge",
        "MUIA_Window_TopEdge", "MUIA_Window_NoMenus", "MUIA_Window_Menustrip",
        "MUIA_Window_Sleep", "MUIA_Window_IsSubWindow", "MUIA_Window_RefWindow",
        "MUIA_Window_MouseObject", "MUIA_Window_InputEvent",
        "MUIA_Window_UseBottomBorderScroller",
        "MUIA_Window_UseLeftBorderScroller",
        "MUIA_Window_UseRightBorderScroller", "MUIA_Window_Opacity",
        "MUIA_Window_Window", "MUIA_Window_ActiveObject",
        "MUIA_Window_DefaultObject", "MUIA_Window_AppWindow",
        "MUIA_Window_AltWidth", "MUIA_Window_AltHeight",
        "MUIA_Window_AltLeftEdge", "MUIA_Window_AltTopEdge",
        "MUIA_Window_FancyDrawing", "MUIA_Window_NeedsMouseObject",
        "MUIA_Window_Menu", "MUIA_Window_MenuAction",
    },
    "TextObject": {
        "MUIA_Text_Contents", "MUIA_Text_PreParse", "MUIA_Text_SetMin",
        "MUIA_Text_SetMax", "MUIA_Text_Shorten", "MUIA_Text_TextualIdentifier",
        "MUIA_Text_ControlChar", "MUIA_Text_HiChar", "MUIA_Text_VerticalSpace",
    },
    "GroupObject": {
        "MUIA_Group_Child", "MUIA_Group_Horiz", "MUIA_Group_Columns",
        "MUIA_Group_Rows", "MUIA_Group_SameHeight", "MUIA_Group_SameWidth",
        "MUIA_Group_Spacing", "MUIA_Group_HorizSpacing",
        "MUIA_Group_VertSpacing", "MUIA_Group_Homogeneous",
        "MUIA_Group_PageMode", "MUIA_Group_ActivePage", "MUIA_Group_Children",
        "MUIA_Group_VirtualSize", "MUIA_Group_VirtualWidth",
        "MUIA_Group_VirtualHeight", "MUIA_Group_SameSize",
        "MUIA_Group_LayoutHook", "MUIA_Group_Forward", "MUIA_Group_Regions",
    },
    "CycleObject": {
        "MUIA_Cycle_Entries", "MUIA_Cycle_Active", "MUIA_Cycle_Chain",
    },
    "ListviewObject": {
        "MUIA_Listview_List", "MUIA_Listview_Input",
        "MUIA_Listview_DoubleClick", "MUIA_Listview_SelectChange",
        "MUIA_Listview_MinVisible", "MUIA_Listview_MultiSelect",
        "MUIA_Listview_ScrollerPos", "MUIA_Listview_DefClickColumn",
        "MUIA_Listview_RenderTitles",
    },
    "ListObject": {
        "MUIA_List_SourceArray", "MUIA_List_Quiet", "MUIA_List_Active",
        "MUIA_List_Format", "MUIA_List_Title", "MUIA_List_DisplayHook",
        "MUIA_List_ConstructHook", "MUIA_List_DestructHook",
        "MUIA_List_CompareHook", "MUIA_List_InsertPosition",
        "MUIA_List_AutoVisible", "MUIA_List_MinLineHeight",
        "MUIA_List_MultiTestHook", "MUIA_List_AdjustHeight",
        "MUIA_List_AdjustWidth", "MUIA_List_Visible",
    },
    # Scrollgroup ist eine Group-Unterklasse: sie traegt dieselben
    # Group-Tags (Kinder!) und dazu ihre Scrollgroup-Attribute. Der Inhalt
    # MUSS ein Virtgroup-Objekt sein (MUI_Scrollgroup.doc).
    "ScrollgroupObject": {
        "MUIA_Scrollgroup_Contents", "MUIA_Scrollgroup_AutoBars",
        "MUIA_Scrollgroup_FreeHoriz", "MUIA_Scrollgroup_FreeVert",
        "MUIA_Scrollgroup_UseWinBorder", "MUIA_Scrollgroup_VertBar",
        "MUIA_Scrollgroup_HorizBar", "MUIA_Scrollgroup_NoVertBar",
        "MUIA_Scrollgroup_NoHorizBar",
        # Group-Tags, weil Scrollgroup von GroupObject abstammt
        "MUIA_Group_Child", "MUIA_Group_Horiz", "MUIA_Group_Columns",
        "MUIA_Group_Rows", "MUIA_Group_SameHeight", "MUIA_Group_SameWidth",
        "MUIA_Group_Spacing", "MUIA_Group_HorizSpacing",
        "MUIA_Group_VertSpacing", "MUIA_Group_Homogeneous",
        "MUIA_Group_PageMode", "MUIA_Group_ActivePage", "MUIA_Group_Children",
        "MUIA_Group_LayoutHook", "MUIA_Group_Forward", "MUIA_Group_Regions",
    },
    # Virtgroup ist ebenfalls eine Group-Unterklasse.
    "VirtgroupObject": {
        "MUIA_Virtgroup_Width", "MUIA_Virtgroup_Height",
        "MUIA_Virtgroup_Left", "MUIA_Virtgroup_Top",
        "MUIA_Virtgroup_Input", "MUIA_Virtgroup_TryFit",
        "MUIA_Group_Child", "MUIA_Group_Horiz", "MUIA_Group_Columns",
        "MUIA_Group_Rows", "MUIA_Group_SameHeight", "MUIA_Group_SameWidth",
        "MUIA_Group_Spacing", "MUIA_Group_HorizSpacing",
        "MUIA_Group_VertSpacing", "MUIA_Group_Homogeneous",
        "MUIA_Group_PageMode", "MUIA_Group_ActivePage", "MUIA_Group_Children",
        "MUIA_Group_LayoutHook", "MUIA_Group_Forward", "MUIA_Group_Regions",
    },
}

OBJEKTE = ("ApplicationObject", "WindowObject", "TextObject", "GroupObject",
           "VGroup", "HGroup", "CycleObject", "ListviewObject", "ListObject",
           "ScrollgroupObject", "VirtgroupObject")

# VGroup/HGroup sind GroupObject mit festem MUIA_Group_Horiz.
ALIAS = {"VGroup": "GroupObject", "HGroup": "GroupObject"}

GRUPPEN = {"GroupObject", "ScrollgroupObject", "VirtgroupObject"}

TAG_RE = re.compile(r"^(MUIA_|MUIM_|MUIV_)")
OBJ_RE = re.compile(r"\b(%s)\b" % "|".join(OBJEKTE))
# MUI_MakeObject(...) baut ebenfalls ein Objekt, nur ohne Objektmakro und ohne
# 'End' -- es ist ein Blatt.
MAKE_RE = re.compile(r"\bMUI_MakeObject\s*\(")

WURZEL_START = "s_app = ApplicationObject"

# Nach entstringen() steht fuer JEDE Zeichenkette nur noch 'S'. Damit laesst
# sich ein Wert, der eine Zeichenkette ist, eindeutig erkennen.
IPTR_RE = re.compile(r"^\(\s*IPTR\s*\)")


def ist_zeichenkette(text):
    """1, wenn der Wert (nur) aus Zeichenketten besteht, also 'S' ist."""
    t = IPTR_RE.sub("", text.strip()).strip()
    return t != "" and set(t.split()) == {"S"}


class Befund(Exception):
    pass


def entkommentieren(text):
    """Kommentare entfernen, Zeilennummern aber erhalten."""
    text = re.sub(r"/\*.*?\*/",
                  lambda m: "\n" * m.group(0).count("\n"), text, flags=re.S)
    return re.sub(r"//[^\n]*", "", text)


def entstringen(text, start):
    """Zeichenketten durch einen Platzhalter ersetzen (Kommas darin stoeren).

    Gibt (text, offset_karte) zurueck: offset_karte[i] ist die Position in der
    ORIGINALQUELLE fuer die Position i im neuen Text. Fuer die Zeilennummern
    genuegt es, die Zeilenzahl zu erhalten -- der Platzhalter 'S' hat keine
    Zeilenumbrueche, also stimmen die Zeilen genau dann, wenn wir die
    Umbrueche in den Zeichenketten vorher zaehlen. Zeichenketten im Aufbau
    enthalten keine Umbrueche, deshalb genuegt der Positionsversatz nicht --
    wir rechnen die Zeilen deshalb direkt aus der ORIGINALQUELLE.
    """
    return re.sub(r'"(?:[^"\\]|\\.)*"', "S", text)


def finde_rumpf(src):
    """Den Rumpf von gui_build() herausschneiden (ohne die Klammern)."""
    m = re.search(r"static\s+int\s+gui_build\s*\(\s*void\s*\)\s*\{", src)
    if not m:
        raise Befund("gui_build() nicht gefunden -- Aufbau nicht pruefbar")
    i = m.end()
    tiefe = 1
    start = i
    while i < len(src) and tiefe > 0:
        if src[i] == "{":
            tiefe += 1
        elif src[i] == "}":
            tiefe -= 1
        i += 1
    return src[start:i - 1]


def zerlege(elemente_text):
    """An Kommas der Tiefe 0 zerlegen. Elemente sind (text, startzeile)."""
    teile = []
    tiefe = 0
    aktuell = []
    zeile = 1
    startzeile = 1
    for ch in elemente_text:
        if ch == "\n":
            zeile += 1
        if ch in "([{":
            tiefe += 1
        elif ch in ")]}":
            tiefe -= 1
        if ch == "," and tiefe == 0:
            teile.append(("".join(aktuell), startzeile))
            aktuell = []
            startzeile = zeile
            continue
        if not aktuell and ch in " \t\n":
            startzeile = zeile
            continue
        aktuell.append(ch)
    teile.append(("".join(aktuell), startzeile))
    return [(t.strip(), z) for t, z in teile if t.strip()]


def pruefe(pfad, still=False):
    with open(pfad, "r", encoding="utf-8", errors="replace") as fh:
        roh = fh.read()

    # Zeilennummern: auf den Aufbau umrechnen.
    basis = roh.count("\n", 0, roh.index(WURZEL_START)) + 1
    # Grosszuegig: der Bereich ab der Wurzel bis zum Ende von gui_build().
    rumpf_roh = finde_rumpf(roh)
    ab = rumpf_roh.index(WURZEL_START)
    bereich_roh = rumpf_roh[ab:]
    bereich = entstringen(entkommentieren(bereich_roh), 0)

    elemente = zerlege(bereich)
    befund = []
    stapel = []
    wurzeln = []
    windowcontents = 0
    erwarte_kind = None      # Zeile des offenen "Child"
    erwarte_wert = None      # (tag, zeile)

    def melde(zeile, text):
        befund.append("Zeile %d: %s" % (zeile, text))

    for text, z in elemente:
        text = text.strip()
        # Das schliessende 'End;' des Baums traegt den Rest des Funktions-
        # rumpfes hinter sich (dort gibt es keine Kommas der Tiefe 0 mehr).
        if text.startswith("End"):
            text = "End"
        if not text:
            continue
        if text == "Child":
            if erwarte_kind is not None:
                melde(z, "zwei 'Child' hintereinander (Zeile %d wartet noch "
                         "auf sein Objekt)" % erwarte_kind)
            erwarte_kind = z
            continue

        if text == "WindowContents":
            windowcontents += 1
            if erwarte_kind is not None:
                melde(z, "'Child' (Zeile %d) wird von WindowContents "
                         "gefolgt" % erwarte_kind)
            if erwarte_wert is not None:
                melde(z, "Tag %s (Zeile %d) hat keinen Wert bekommen"
                     % (erwarte_wert[0], erwarte_wert[1]))
                erwarte_wert = None
            erwarte_wert = ("WindowContents", z)
            continue

        if text == "End":
            if erwarte_kind is not None:
                melde(z, "'End' statt eines Objekts fuer das 'Child' aus "
                         "Zeile %d" % erwarte_kind)
                erwarte_kind = None
            if erwarte_wert is not None:
                melde(z, "Tag %s (Zeile %d) hat keinen Wert bekommen"
                     % (erwarte_wert[0], erwarte_wert[1]))
                erwarte_wert = None
            if not stapel:
                melde(z, "ueberzaehliges 'End' -- der Baum ist hier schon zu")
                continue
            e = stapel.pop()
            if e["klasse"] in GRUPPEN and not e["kinder"]:
                melde(e["zeile"], "%s hat kein einziges Kind" % e["macro"])
            if stapel:
                stapel[-1]["kinder"].append(e)
            else:
                wurzeln.append(e)
            continue

        m = OBJ_RE.search(text)
        ist_make = MAKE_RE.search(text) is not None
        if (m is not None or ist_make) and not TAG_RE.match(text):
            macro = m.group(1) if m is not None else "MUI_MakeObject"
            name = ALIAS.get(macro, macro)
            if ist_make:
                name = "MUI_MakeObject"
            if ist_make:
                # Blatt: verbraucht kein 'End'.
                if erwarte_kind is not None:
                    erwarte_kind = None
                    if stapel:
                        stapel[-1]["kinder"].append(
                            {"klasse": "MUI_MakeObject",
                             "macro": "MUI_MakeObject", "zeile": z,
                             "kinder": [], "tags": []})
                elif erwarte_wert is not None:
                    erwarte_wert = None
                else:
                    melde(z, "MUI_MakeObject steht ohne 'Child' und ohne Tag "
                             "im Baum")
                continue
            neu = {"klasse": name, "macro": macro, "zeile": z,
                   "kinder": [], "tags": []}
            if erwarte_kind is not None:
                erwarte_kind = None
                stapel.append(neu)
            elif erwarte_wert is not None:
                # Objekt als Wert eines Tags (z.B. MUIA_Listview_List)
                erwarte_wert = None
                stapel.append(neu)
            elif not stapel:
                # Die Wurzel (s_app = ApplicationObject) braucht kein 'Child'.
                if macro != "ApplicationObject":
                    melde(z, "die Wurzel ist %s, erwartet ApplicationObject"
                         % macro)
                stapel.append(neu)
            else:
                melde(z, "%s steht ohne 'Child' und ohne Tag im Baum" % macro)
                stapel.append(neu)
            continue

        if TAG_RE.match(text):
            if erwarte_kind is not None:
                melde(z, "'Child' (Zeile %d) wird von Tag %s gefolgt"
                     % (erwarte_kind, text))
                erwarte_kind = None
            if erwarte_wert is not None:
                melde(z, "Tag %s (Zeile %d) hat keinen Wert bekommen -- "
                         "stattdessen folgt Tag %s"
                     % (erwarte_wert[0], erwarte_wert[1], text))
            if not stapel:
                melde(z, "Tag %s steht ausserhalb jedes Objekts" % text)
                erwarte_wert = (text, z)
                continue
            e = stapel[-1]
            erlaubt = KLASSEN[e["klasse"]] | GEMEINSAM
            if text.startswith("MUIA_") and text not in erlaubt:
                melde(z, "Tag %s gehoert nicht zu %s (Objekt in Zeile %d)"
                     % (text, e["macro"], e["zeile"]))
            e["tags"].append((text, z))
            erwarte_wert = (text, z)
            continue

        # Ein Wert.
        if erwarte_kind is not None:
            melde(z, "'Child' (Zeile %d) wird von einem Wert gefolgt: %s"
                 % (erwarte_kind, text[:40]))
            erwarte_kind = None
        if erwarte_wert is not None:
            # MUIA_Cycle_Entries ist ein STRPTR * -- ein NULL-abgeschlossenes
            # ARRAY von Zeichenketten, KEINE mit '|' getrennte Zeichenkette.
            # Beleg: libraries/mui.h ("V4 is. STRPTR *") und MUI_Cycle.doc
            # ("You must supply a pointer to a string array ... terminated
            # with a NULL"). Eine Zeichenkette hier liest MUI als Zeigerarray:
            # der Cycle-Knopf bleibt leer und wird (weil MUI seine Breite aus
            # den Eintraegen rechnet) viel zu gross. Genau dieser Fehler war
            # am 13.10.2026 in v4_gui.c.
            if (erwarte_wert[0] == "MUIA_Cycle_Entries"
                    and ist_zeichenkette(text)):
                melde(z, "MUIA_Cycle_Entries bekommt eine Zeichenkette "
                         "(\"a|b|c\") -- MUI erwartet ein NULL-abgeschlossenes "
                         "Array von Zeichenketten (STRPTR *, siehe "
                         "libraries/mui.h und MUI_Cycle.doc)")
            erwarte_wert = None

    if erwarte_kind is not None:
        melde(erwarte_kind, "'Child' ohne Objekt am Ende des Aufbaus")
    if erwarte_wert is not None:
        melde(erwarte_wert[1], "Tag %s hat am Ende keinen Wert bekommen"
              % erwarte_wert[0])
    for e in stapel:
        melde(e["zeile"], "%s wird nicht geschlossen ('End' fehlt)" % e["macro"])
    if windowcontents != 1:
        melde(basis, "WindowContents kommt %d mal vor (erwartet: genau einmal)"
              % windowcontents)
    if len(wurzeln) != 1 or wurzeln[0]["klasse"] != "ApplicationObject":
        melde(basis, "die Wurzel ist keine einzelne Anwendung (%d "
                     "Wurzelknoten)" % len(wurzeln))

    if befund:
        print("MUI-Baum: BEFUND in %s" % pfad)
        for b in befund:
            print("  " + b)
        return 1

    w = wurzeln[0]
    fenster = next((k for k in w["kinder"]
                    if k["klasse"] == "WindowObject"), None)
    inhalt = None
    if fenster is not None:
        inhalt = next((k for k in fenster["kinder"]
                       if k["klasse"] in GRUPPEN), None)
    if still:
        return 0
    print("MUI-Baum: sauber (%s)" % pfad)
    if inhalt is not None:
        print("  Fensterinhalt: %s mit %d Kindern"
              % (inhalt["macro"], len(inhalt["kinder"])))
        for k in inhalt["kinder"]:
            print("    - %s (%d eigene Kinder)"
                  % (k["macro"], len(k["kinder"])))
    print("  Klammerbalance 0, WindowContents genau einmal, jedes Child mit "
          "genau einem Objekt, jedes Tag mit genau einem Wert, alle Tags aus "
          "der Klasse des Objekts.")
    return 0


# --------------------------------------------------------------------- #
# Selbsttest: ein Pruefer, der nie etwas findet, ist wertlos.            #
# --------------------------------------------------------------------- #

# Geprueft wird gegen einen KLEINEN, kuenstlichen Baum. Damit haengt der
# Selbsttest nicht an der Formatierung von v4_gui.c und kann jeden der Fehler
# einzeln erzeugen, die er finden soll.
BAUM_GUT = '''static const char *const cyc_entries[] = { "aus", "Joystick", "Maus", NULL };

static int gui_build(void)
{
    Object *btn;

    s_app = ApplicationObject,
        MUIA_Application_Title, (IPTR)"Titel",
        MUIA_Application_Base,  (IPTR)"V4JOY",

        MUIA_Application_Window, s_win = WindowObject,
            MUIA_Window_Title, "Fenster",
            MUIA_Window_Width, 640,
            MUIA_Window_Height, 400,
            MUIA_Window_SizeGadget, TRUE,
            WindowContents, VGroup,
                Child, s_text = TextObject,
                    MUIA_Text_Contents, (IPTR)"erste Zeile",
                    MUIA_Text_PreParse, (IPTR)"\\33l",
                End,
                Child, HGroup,
                    Child, btn = MUI_MakeObject(MUIO_Button, (IPTR)"Knopf", NULL),
                    Child, s_cycle = CycleObject,
                        MUIA_Cycle_Entries, (IPTR)cyc_entries,
                        MUIA_Weight, 0,
                    End,
                End,
                Child, ListviewObject,
                    MUIA_Listview_List, s_list = ListObject,
                    End,
                End,
            End,
        End,
    End;

    return 0;
}
'''


# Derselbe Baum mit einer SEITENGRUPPE (MUIA_Group_PageMode) -- genau die
# Konstruktion, mit der v4_gui.c den gewachsenen Inhalt (fw_ver 4) auf 640x400
# unterbringt: Anzeige und Konfiguration liegen auf zwei Seiten, und die
# Mindestgroesse der Gruppe ist das Maximum ueber die Seiten (MUI_Group.doc),
# das Fenster wird also nicht groesser als die groesste Seite.
BAUM_GUT_SEITE = BAUM_GUT.replace(
    """                Child, ListviewObject,
                    MUIA_Listview_List, s_list = ListObject,
                    End,
                End,""",
    """                Child, VGroup,
                    MUIA_Group_PageMode, TRUE,
                    MUIA_Group_ActivePage, 0,
                    Child, VGroup,
                        Child, TextObject,
                            MUIA_Text_Contents, (IPTR)"Seite 1",
                        End,
                    End,
                    Child, VGroup,
                        Child, TextObject,
                            MUIA_Text_Contents, (IPTR)"Seite 2",
                        End,
                    End,
                End,
                Child, ListviewObject,
                    MUIA_Listview_List, s_list = ListObject,
                    End,
                End,""", 1)

assert BAUM_GUT_SEITE != BAUM_GUT, \
    "Selbsttestanker fuer die Seitengruppe passt nicht mehr"

_SELFTEST_N = [0]


def _selftest_ein(name, quelle, erwarte_sauber):
    """Eine (absichtlich kaputte) Quelle pruefen.

    Der Pfad ist ABSICHTLICH fest und nicht zufaellig: die Ausgabe von
    `make verify` soll bei jedem Lauf Zeichen fuer Zeichen dieselbe sein.
    """
    import shutil

    _SELFTEST_N[0] += 1
    tmp = os.path.join(tempfile.gettempdir(),
                       "v4mui-selftest-%02d" % _SELFTEST_N[0])
    shutil.rmtree(tmp, ignore_errors=True)
    os.makedirs(tmp)
    ziel = os.path.join(tmp, "v4_gui.c")
    try:
        with open(ziel, "w", encoding="utf-8") as fh:
            fh.write(quelle)
        ergebnis = pruefe(ziel, still=True)
    finally:
        shutil.rmtree(tmp, ignore_errors=True)
    ok = (ergebnis == 0) if erwarte_sauber else (ergebnis != 0)
    print("  %s %s" % ("ok     :" if ok else "FEHLER :", name))
    return ok


def selftest():
    print("Selbsttest des MUI-Baumpruefers (kuenstlicher Baum):")
    gut = _selftest_ein("unveraenderter Baum bleibt sauber", BAUM_GUT, True)
    gut = _selftest_ein("Seitengruppe (MUIA_Group_PageMode) bleibt sauber",
                        BAUM_GUT_SEITE, True) and gut

    faelle = [
        ("fehlendes End am TextObject",
         BAUM_GUT.replace("""                    MUIA_Text_PreParse, (IPTR)"\\33l",
                End,
                Child, HGroup,""",
                          """                    MUIA_Text_PreParse, (IPTR)"\\33l",
                Child, HGroup,""", 1)),
        ("ueberzaehliges End (VGroup zweimal geschlossen)",
         BAUM_GUT.replace("""            End,
        End,
    End;""",
                          """            End,
            End,
        End,
    End;""", 1)),
        ("Tag ohne Wert (MUIA_Text_Contents ohne Inhalt)",
         BAUM_GUT.replace('MUIA_Text_Contents, (IPTR)"erste Zeile",',
                          "MUIA_Text_Contents, MUIA_Text_PreParse,", 1)),
        ("Tag einer fremden Klasse am TextObject",
         BAUM_GUT.replace('MUIA_Text_PreParse, (IPTR)"\\33l",',
                          'MUIA_Cycle_Entries, (IPTR)"x",', 1)),
        ("Child ohne Objekt",
         BAUM_GUT.replace("                Child, s_text = TextObject,",
                          "                Child,\n"
                          "                Child, s_text = TextObject,", 1)),
        ("Gruppe ohne Kinder",
         BAUM_GUT.replace("""                Child, HGroup,
                    Child, btn = MUI_MakeObject(MUIO_Button, (IPTR)"Knopf", NULL),
                    Child, s_cycle = CycleObject,
                        MUIA_Cycle_Entries, (IPTR)cyc_entries,
                        MUIA_Weight, 0,
                    End,
                End,
""",
                          """                Child, HGroup,
                End,
""", 1)),
        # Die Fehlerklasse, die am 13.10.2026 auf der V4 zu sehen war: der
        # Cycle-Knopf blieb leer und wurde ueber die ganze Breite gezogen,
        # weil MUIA_Cycle_Entries eine Zeichenkette statt eines Arrays bekam.
        ("MUIA_Cycle_Entries als Zeichenkette statt NULL-Array",
         BAUM_GUT.replace("MUIA_Cycle_Entries, (IPTR)cyc_entries,",
                          'MUIA_Cycle_Entries, (IPTR)"Aus|Joystick|Maus",', 1)),
        ("WindowContents fehlt",
         BAUM_GUT.replace("WindowContents, VGroup,",
                          "MUIA_Window_RootObject, VGroup,", 1)),
        # Die Seitengruppe (fw_ver 4): MUIA_Group_PageMode gehoert an eine
        # GRUPPE. An einem TextObject ist es ein Tag der falschen Klasse und
        # laesst MUI beim Aufbau sterben -- genau die Fehlerklasse, die der
        # Pruefer finden soll.
        ("MUIA_Group_PageMode am TextObject",
         BAUM_GUT_SEITE.replace('MUIA_Text_Contents, (IPTR)"Seite 1",',
                                "MUIA_Group_PageMode, TRUE,", 1)),
    ]
    for name, quelle in faelle:
        if quelle in (BAUM_GUT, BAUM_GUT_SEITE):
            print("  FEHLER : Selbsttestfall '%s' hat nichts veraendert" % name)
            gut = False
            continue
        gut = _selftest_ein(name, quelle, False) and gut
    print("Selbsttest: %s" % ("OK" if gut else "FEHLGESCHLAGEN"))
    return 0 if gut else 1


def main(argv):
    if len(argv) > 1 and argv[1] == "--selftest":
        return selftest()
    pfad = argv[1] if len(argv) > 1 else os.path.join(
        os.path.dirname(os.path.abspath(__file__)), "..", "v4_gui.c")
    pfad = os.path.normpath(pfad)
    if not os.path.isfile(pfad):
        print("FEHLER: %s nicht gefunden" % pfad)
        return 2
    try:
        return pruefe(pfad)
    except Befund as b:
        print("MUI-Baum: BEFUND -- %s" % b)
        return 1


if __name__ == "__main__":
    sys.exit(main(sys.argv))
