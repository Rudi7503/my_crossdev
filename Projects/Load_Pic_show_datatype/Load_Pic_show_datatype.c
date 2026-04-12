
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
#include <exec/memory.h>
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

struct TileMeta
{
    ULONG tm_Offset;
    ULONG tm_Size;
    ULONG tm_Width;
    ULONG tm_Height;
    ULONG tm_Pitch;
    ULONG tm_Bpp;
};

static struct TileMeta *gTileMeta = NULL;
static int gTileMetaCount = 0;
static APTR gTileBlob = NULL;
static ULONG gTileBlobSize = 0;

static APTR AllocMemAligned32(ULONG size)
{
    ULONG allocSize;
    UBYTE *raw;
    UBYTE *aligned;

    if (size == 0)
    {
        return NULL;
    }

    allocSize = size + 31 + sizeof(APTR);
    raw = (UBYTE *)AllocMem(allocSize, MEMF_ANY);
    if (!raw)
    {
        return NULL;
    }

    aligned = (UBYTE *)((((ULONG)(raw + sizeof(APTR))) + 31UL) & ~31UL);
    *((APTR *)(aligned - sizeof(APTR))) = (APTR)raw;
    return (APTR)aligned;
}

static void FreeMemAligned32(APTR aligned, ULONG size)
{
    APTR raw;

    if (!aligned || size == 0)
    {
        return;
    }

    raw = *((APTR *)((UBYTE *)aligned - sizeof(APTR)));
    if (raw)
    {
        FreeMem(raw, size + 31 + sizeof(APTR));
    }
}

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

static void FreeTileStorageBlob(void)
{
    if (gTileBlob)
    {
        FreeMemAligned32(gTileBlob, gTileBlobSize);
        gTileBlob = NULL;
        gTileBlobSize = 0;
    }

    free(gTileMeta);
    gTileMeta = NULL;
    gTileMetaCount = 0;
}

static int LoadAllTilesToOwnMemory(struct Library *base, ULONG targetBpp)
{
    int i;
    struct vup_Image **loaded = NULL;
    ULONG totalSize = 0;
    ULONG runningOffset = 0;

    FreeTileStorageBlob();

    if (!base || gTileCount <= 0)
    {
        return 0;
    }

    gTileMeta = (struct TileMeta *)calloc((size_t)gTileCount, sizeof(struct TileMeta));
    if (!gTileMeta)
    {
        printf("[ERROR] Kein Speicher fuer Tile-Metadaten\n");
        return 0;
    }

    loaded = (struct vup_Image **)calloc((size_t)gTileCount, sizeof(struct vup_Image *));
    if (!loaded)
    {
        printf("[ERROR] Kein Speicher fuer temporaere Tile-Images\n");
        FreeTileStorageBlob();
        return 0;
    }

    for (i = 0; i < gTileCount; i++)
    {
        struct vup_Image *img = VUP_LoadImage(base, (STRPTR)gTiles[i], targetBpp);
        ULONG copySize;

        if (!img)
        {
            printf("[ERROR] Tile %d konnte nicht geladen werden: %s\n", i, gTiles[i]);
            goto fail;
        }

        copySize = img->vi_Pitch * img->vi_Height;
        if (copySize == 0)
        {
            printf("[ERROR] Tile %d hat ungueltige Groesse\n", i);
            VUP_FreeImage(base, img);
            goto fail;
        }

        loaded[i] = img;
        gTileMeta[i].tm_Offset = totalSize;
        gTileMeta[i].tm_Size = copySize;
        gTileMeta[i].tm_Width = img->vi_Width;
        gTileMeta[i].tm_Height = img->vi_Height;
        gTileMeta[i].tm_Pitch = img->vi_Pitch;
        gTileMeta[i].tm_Bpp = img->vi_BPP;

        totalSize += copySize;
    }

    gTileBlob = AllocMemAligned32(totalSize);
    if (!gTileBlob)
    {
        printf("[ERROR] Kein 32-byte align Speicher fuer Tile-Blob (%lu bytes)\n", totalSize);
        goto fail;
    }

    gTileBlobSize = totalSize;

    for (i = 0; i < gTileCount; i++)
    {
        UBYTE *dst = (UBYTE *)gTileBlob + runningOffset;
        ULONG copySize = gTileMeta[i].tm_Size;

        CopyMem(loaded[i]->vi_PixelData, dst, copySize);
        runningOffset += copySize;

        VUP_FreeImage(base, loaded[i]);
        loaded[i] = NULL;
    }

    free(loaded);
    gTileMetaCount = gTileCount;
    printf("[OK]   Alle %d Tiles hintereinander in einen 32-byte align Speicher kopiert (%lu bytes)\n",
           gTileMetaCount,
           gTileBlobSize);
    return gTileMetaCount;

fail:
    if (loaded)
    {
        for (i = 0; i < gTileCount; i++)
        {
            if (loaded[i])
            {
                VUP_FreeImage(base, loaded[i]);
            }
        }
        free(loaded);
    }

    FreeTileStorageBlob();
    return 0;
}

static APTR GetTilePixels(int index)
{
    if (!gTileBlob || !gTileMeta || index < 0 || index >= gTileMetaCount)
    {
        return NULL;
    }

    return (APTR)((UBYTE *)gTileBlob + gTileMeta[index].tm_Offset);
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
    struct vup_BOB          **bobs    = NULL;
    struct Window             *win    = NULL;
    ULONG activeWidth = 0;
    ULONG activeHeight = 0;
    ULONG activeBpp = 0;
    ULONG frameCount = 0;
    ULONG maxTileW = 0;
    ULONG maxTileH = 0;
    ULONG cellW = 0;
    ULONG cellH = 0;
    ULONG cols = 0;
    int bobCount = 0;
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

    printf("[STEP] 3/7: Alle Tiles laden + in 32-byte align Speicher kopieren\n");

    if (!LoadAllTilesToOwnMemory(VampupBase, activeBpp))
    {
        printf("[FEHLER] Konnte Tiles nicht in eigenen Speicher kopieren\n");
        VUP_CloseDisplay(VampupBase, ctx);
        CloseLibrary(VampupBase);
        FreeMemoryArray();
        return 1;
    }
    printf("[OK]   3/7: Tile-Speicher bereit (count=%d)\n", gTileMetaCount);

    printf("[STEP] 4/7: VUP_CreateBOB() fuer alle Tiles\n");

    bobs = (struct vup_BOB **)calloc((size_t)gTileMetaCount, sizeof(struct vup_BOB *));
    if (!bobs)
    {
        printf("[FEHLER] Kein Speicher fuer BOB-Liste\n");
        FreeTileStorageBlob();
        VUP_CloseDisplay(VampupBase, ctx);
        CloseLibrary(VampupBase);
        FreeMemoryArray();
        return 1;
    }

    for (i = 0; i < gTileMetaCount; i++)
    {
        APTR tilePixels = GetTilePixels(i);
        if (!tilePixels)
        {
            printf("[FEHLER] Tile-Pixel fehlen fuer Index %d\n", i);
            break;
        }

        bobs[i] = VUP_CreateBOB(VampupBase,
                                tilePixels,
                                gTileMeta[i].tm_Width,
                                gTileMeta[i].tm_Height,
                                1,
                                gTileMeta[i].tm_Bpp);
        if (!bobs[i])
        {
            printf("[FEHLER] Kann BOB %d nicht erstellen!\n", i);
            break;
        }

        if (gTileMeta[i].tm_Width > maxTileW)
            maxTileW = gTileMeta[i].tm_Width;
        if (gTileMeta[i].tm_Height > maxTileH)
            maxTileH = gTileMeta[i].tm_Height;

        bobCount++;
    }

    if (bobCount <= 0)
    {
        printf("[FEHLER] Keine BOBs erstellt\n");
        free(bobs);
        FreeTileStorageBlob();
        VUP_CloseDisplay(VampupBase, ctx);
        CloseLibrary(VampupBase);
        FreeMemoryArray();
        return 1;
    }

    if (bobCount < gTileMetaCount)
    {
        printf("[WARN]  Nur %d/%d BOBs erstellt\n", bobCount, gTileMetaCount);
    }

    cellW = maxTileW + 4;
    cellH = maxTileH + 4;
    if (cellW == 0)
        cellW = 1;
    cols = activeWidth / cellW;
    if (cols == 0)
        cols = 1;

    printf("[OK]   4/7: %d BOBs erstellt, Grid: cols=%lu cell=%lux%lu\n",
           bobCount, cols, cellW, cellH);

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

        /* Alle BOBs in Backbuffer zeichnen */
        for (i = 0; i < bobCount; i++)
        {
            ULONG col = (ULONG)i % cols;
            ULONG row = (ULONG)i / cols;
            LONG x = (LONG)(col * cellW);
            LONG y = (LONG)(row * cellH);

            bobs[i]->vb_WorldX = x;
            bobs[i]->vb_WorldY = y;
            VUP_DrawBOB(VampupBase, bobs[i], ctx);
        }

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
    if (bobs)
    {
        for (i = 0; i < bobCount; i++)
        {
            if (bobs[i])
            {
                VUP_FreeBOB(VampupBase, bobs[i]);
            }
        }
        free(bobs);
        bobs = NULL;
    }
    FreeTileStorageBlob();
    VUP_CloseDisplay(VampupBase, ctx);
    CloseLibrary(VampupBase);
    FreeMemoryArray();

    return 0;
}


