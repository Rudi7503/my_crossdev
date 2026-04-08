
/*
 * Load_Pic_show_datatype.c
 * Laedt Tiles per vampup.library und zeigt das erste Tile in einem
 * 1280x720 16-Bit Double-Buffer Display an. ESC beendet das Programm.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <exec/types.h>
#include <exec/libraries.h>
#include <intuition/intuition.h>

#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>

#include "vampup.h"
#include "vampup_protos.h"

#define SCREEN_WIDTH    1280
#define SCREEN_HEIGHT   720
#define SCREEN_BPP      2       /* 16-bit, 2 bytes per pixel */

#define SCRIPT_FILE     "PROGDIR:Data/Scripts/Tiles2load.txt"
#define MAX_PATH_LEN    256

#define RAWKEY_ESC      0x45    /* ESC raw key code on Amiga */
#define AUTO_EXIT_FRAMES 600     /* ca. 10 Sekunden bei 60 Hz */

struct Library *VampupBase = NULL;

static char **gTiles        = NULL;
static char  *gTileStorage  = NULL;
static int    gTileCount    = 0;
static size_t gTileEntryLen = 0;

static void TrimLine(char *line)
{
    size_t len = strlen(line);

    while (len > 0 &&
           (line[len - 1] == '\n' || line[len - 1] == '\r' ||
            line[len - 1] == ' ' || line[len - 1] == '\t' ||
            line[len - 1] == ','))
    {
        line[--len] = '\0';
    }
}

static int AnalyzeScript(const char *scriptPath, int *outCount, size_t *outMaxLen)
{
    FILE *f;
    char line[MAX_PATH_LEN];
    int count = 0;
    size_t maxLen = 0;

    f = fopen(scriptPath, "r");
    if (!f)
    {
        printf("[ERROR] Kann Script nicht oeffnen: %s\n", scriptPath);
        return 0;
    }

    while (fgets(line, sizeof(line), f) != NULL)
    {
        TrimLine(line);
        if (line[0] == '\0' || line[0] == '#')
        {
            continue;
        }

        if (strlen(line) > maxLen)
        {
            maxLen = strlen(line);
        }

        count++;
    }

    fclose(f);
    *outCount = count;
    *outMaxLen = maxLen;
    return 1;
}

static int ReadScriptToMemoryArray(const char *scriptPath)
{
    FILE *f;
    char line[MAX_PATH_LEN];
    int expectedCount;
    size_t maxLen;

    gTileCount = 0;
    gTileEntryLen = 0;

    if (!AnalyzeScript(scriptPath, &expectedCount, &maxLen))
    {
        return 0;
    }

    if (expectedCount <= 0)
    {
        return 0;
    }

    gTileEntryLen = maxLen + 1;

    gTiles = (char **)malloc((size_t)expectedCount * sizeof(char *));
    if (!gTiles)
    {
        printf("[ERROR] Kein Speicher fuer Tile-Array\n");
        return 0;
    }

    gTileStorage = (char *)malloc((size_t)expectedCount * gTileEntryLen);
    if (!gTileStorage)
    {
        printf("[ERROR] Kein Speicher fuer Tile-Daten\n");
        free(gTiles);
        gTiles = NULL;
        gTileEntryLen = 0;
        return 0;
    }

    {
        int i;
        for (i = 0; i < expectedCount; i++)
        {
            gTiles[i] = gTileStorage + ((size_t)i * gTileEntryLen);
            gTiles[i][0] = '\0';
        }
    }

    f = fopen(scriptPath, "r");
    if (!f)
    {
        printf("[ERROR] Kann Script nicht oeffnen: %s\n", scriptPath);
        free(gTileStorage);
        gTileStorage = NULL;
        free(gTiles);
        gTiles = NULL;
        gTileEntryLen = 0;
        return 0;
    }

    while (fgets(line, sizeof(line), f) != NULL)
    {
        TrimLine(line);

        if (line[0] == '\0' || line[0] == '#')
        {
            continue;
        }

        if (gTileCount >= expectedCount)
        {
            break;
        }

        strncpy(gTiles[gTileCount], line, gTileEntryLen - 1);
        gTiles[gTileCount][gTileEntryLen - 1] = '\0';
        gTileCount++;
    }

    fclose(f);
    return gTileCount;
}

static void FreeMemoryArray(void)
{
    free(gTileStorage);
    gTileStorage = NULL;
    free(gTiles);
    gTiles = NULL;
    gTileCount = 0;
    gTileEntryLen = 0;
}

static struct vup_DisplayContext *TryOpenDisplay(struct Library *base,
                                                 ULONG *outWidth,
                                                 ULONG *outHeight,
                                                 ULONG *outBpp)
{
    static const struct
    {
        ULONG w;
        ULONG h;
        ULONG bpp;
    } modes[] = {
        {SCREEN_WIDTH, SCREEN_HEIGHT, SCREEN_BPP},
        {640,  480, 2},
        {800,  600, 2},
        {1024, 768, 2},
        {320,  240, 2},
        {SCREEN_WIDTH, SCREEN_HEIGHT, 4},
        {640,  480, 4},
        {320,  240, 4}
    };
    size_t i;

    for (i = 0; i < (sizeof(modes) / sizeof(modes[0])); i++)
    {
        struct vup_DisplayContext *ctx;

        printf("[TRY]  VUP_OpenDisplay(%lu,%lu,%lu)\n",
               modes[i].w, modes[i].h, modes[i].bpp);

        ctx = VUP_OpenDisplay(base, modes[i].w, modes[i].h, modes[i].bpp);
        if (ctx)
        {
            *outWidth = modes[i].w;
            *outHeight = modes[i].h;
            *outBpp = modes[i].bpp;
            return ctx;
        }
    }

    *outWidth = 0;
    *outHeight = 0;
    *outBpp = 0;
    return NULL;
}

/* ---------------------------------------------------------------------- */

int main(void)
{
    struct vup_DisplayContext *ctx    = NULL;
    struct vup_Image          *img    = NULL;
    struct vup_BOB            *bob    = NULL;
    struct Window             *win    = NULL;
    ULONG activeWidth = 0;
    ULONG activeHeight = 0;
    ULONG activeBpp = 0;
    ULONG frameCount = 0;
    int    running = 1;
    int    i;

    /* --- Tile-Liste aus Script laden ---------------------------------- */
    {
        int count = ReadScriptToMemoryArray(SCRIPT_FILE);
        printf("Tiles geladen: %d\n", count);
        for (i = 0; i < count; i++)
            printf("  [%d] %s\n", i, gTiles[i]);

        if (count == 0)
        {
            printf("[FEHLER] Keine Tiles im Script.\n");
            return 1;
        }
    }

        printf("[STEP] 1/7: OpenLibrary(vampup.library)\n");

        /* --- vampup.library oeffnen --------------------------------------- */
    VampupBase = OpenLibrary("vampup.library", 0);
    if (!VampupBase)
    {
        printf("[FEHLER] Kann vampup.library nicht oeffnen!\n");
        FreeMemoryArray();
        return 1;
    }
    printf("vampup.library Version: %lu.%lu\n",
           (ULONG)VampupBase->lib_Version,
           (ULONG)VampupBase->lib_Revision);
        printf("[OK]   1/7: Library offen @ %p\n", (void *)VampupBase);

            printf("[STEP] 2/7: VUP_OpenDisplay() mit Fallback-Modi\n");

            /* --- Display oeffnen: automatischer Fallback ueber mehrere Modi --- */
            ctx = TryOpenDisplay(VampupBase, &activeWidth, &activeHeight, &activeBpp);
    if (!ctx)
    {
             printf("[FEHLER] Kein unterstuetzter Display-Modus gefunden!\n");
        CloseLibrary(VampupBase);
        FreeMemoryArray();
        return 1;
    }
        printf("[OK]   2/7: DisplayContext @ %p\n", (void *)ctx);
            printf("Aktiver Modus: %lu x %lu @ %lu BPP\n",
                activeWidth, activeHeight, activeBpp);

            printf("[STEP] 3/7: VUP_LoadImage(%s, %lu)\n", gTiles[0], activeBpp);

    /* --- Erstes Tile als 16-Bit Bild laden --------------------------- */
            img = VUP_LoadImage(VampupBase, (STRPTR)gTiles[0], activeBpp);
    if (!img)
    {
        printf("[FEHLER] Kann Bild nicht laden: %s\n", gTiles[0]);
        VUP_CloseDisplay(VampupBase, ctx);
        CloseLibrary(VampupBase);
        FreeMemoryArray();
        return 1;
    }
    printf("[OK]   3/7: Image @ %p\n", (void *)img);
    printf("Bild geladen: %lux%lu @ %lu BPP\n",
           img->vi_Width, img->vi_Height, img->vi_BPP);

    printf("[STEP] 4/7: VUP_CreateBOB()\n");

    /* --- BOB aus Bild erstellen -------------------------------------- */
    bob = VUP_CreateBOB(VampupBase,
                        img->vi_PixelData,
                        img->vi_Width,
                        img->vi_Height,
                        1,            /* maxFrames */
                        img->vi_BPP); /* bytes per pixel */
    if (!bob)
    {
        printf("[FEHLER] Kann BOB nicht erstellen!\n");
        VUP_FreeImage(VampupBase, img);
        VUP_CloseDisplay(VampupBase, ctx);
        CloseLibrary(VampupBase);
        FreeMemoryArray();
        return 1;
    }
    printf("[OK]   4/7: BOB @ %p\n", (void *)bob);
    printf("BOB erstellt: %lux%lu @ %lu BPP\n",
            bob->vb_Width, bob->vb_Height, activeBpp);

    /* --- Tastatureingaben auf dem Display-Fenster aktivieren ---------- */
    win = (struct Window *)ctx->vdc_Window;
    if (win && win->UserPort)
    {
        ModifyIDCMP(win, IDCMP_RAWKEY);
        printf("[OK]   5/7: IDCMP_RAWKEY aktiviert\n");
    }
    else
    {
        printf("[WARN] 5/7: Kein gueltiges Window/UserPort im DisplayContext\n");
    }

    /* --- Render-Loop: ESC beendet das Programm ------------------------ */
    printf("[STEP] 6/7: Render-Loop startet (ESC oder Auto-Exit nach 10s)\n");
    while (running)
    {
        ULONG bufSize = (ULONG)(activeWidth * activeHeight * activeBpp);

        /* Backbuffer leeren */
        VUP_FastMemClear(VampupBase, ctx->vdc_BufferBackPtr, bufSize);

        /* BOB in Backbuffer zeichnen */
        VUP_DrawBOB(VampupBase, bob, ctx);

        /* Display flippen (wartet auf VSync) */
        VUP_FlipDisplay(VampupBase, ctx);

        /* Tastatureingaben pruefen */
        if (win && win->UserPort)
        {
            struct IntuiMessage *msg;
            while ((msg = (struct IntuiMessage *)GetMsg(win->UserPort)) != NULL)
            {
                ULONG class = msg->Class;
                UWORD code  = msg->Code;
                ReplyMsg((struct Message *)msg);
                if (class == IDCMP_RAWKEY && code == RAWKEY_ESC)
                    running = 0;
            }
        }

        frameCount++;
        if (frameCount >= AUTO_EXIT_FRAMES)
        {
            printf("[INFO] Auto-Exit nach ca. 10 Sekunden.\n");
            running = 0;
        }
    }

    /* --- Aufraeumen --------------------------------------------------- */
    printf("[STEP] 7/7: Aufraeumen\n");
    VUP_FreeBOB(VampupBase, bob);
    VUP_FreeImage(VampupBase, img);
    VUP_CloseDisplay(VampupBase, ctx);
    CloseLibrary(VampupBase);
    FreeMemoryArray();

    return 0;
}


