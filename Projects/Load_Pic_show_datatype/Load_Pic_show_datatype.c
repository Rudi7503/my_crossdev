

/*
 * Load_Pic_show_datatype.c - Vereinfacht: Script → ASL → Screen
 * 
 * Schritte:
 * 1. Script einlesen und Pfade ausgeben
 * 2. ASL ScreenModeRequester öffnen
 * 3. Screen öffnen mit ASL-Auswahl
 * 4. Fertig
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <exec/types.h>
#include <exec/memory.h>
#include <dos/dos.h>
#include <intuition/intuition.h>
#include <intuition/screens.h>
#include <libraries/asl.h>

#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/asl.h>

struct IntuitionBase *IntuitionBase = NULL;
struct Library *AslBase = NULL;

#define MAX_TILES      10
#define MAX_PATH_LEN   256

typedef struct
{
    char path[MAX_PATH_LEN];
    char label[64];
} TileInfo;

static TileInfo gTiles[MAX_TILES];
static int gTileCount = 0;

/* ========================================================================== */
/* ReadScript: Load tile paths from PROGDIR:Data/Scripts/Tiles2load.txt      */
/* ========================================================================== */

static int ReadScript(void)
{
    FILE *f;
    char line[MAX_PATH_LEN];
    int count = 0;
    int len;
    
    printf("[ReadScript] Opening PROGDIR:Data/Scripts/Tiles2load.txt\n");
    
    f = fopen("PROGDIR:Data/Scripts/Tiles2load.txt", "r");
    if (!f)
    {
        printf("[ERROR] Cannot open script file\n");
        return 0;
    }
    
    while (count < MAX_TILES && fgets(line, sizeof(line), f))
    {
        len = strlen(line);
        while (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r' || 
                           line[len-1] == ' ' || line[len-1] == ','))
            line[--len] = '\0';
        
        if (len == 0 || line[0] == '#')
            continue;
        
        snprintf(gTiles[count].path, MAX_PATH_LEN, "PROGDIR:Data/Pics/%s", line);
        snprintf(gTiles[count].label, 64, "%s", line);
        
        printf("  [%d] %s\n", count, gTiles[count].label);
        count++;
    }
    
    fclose(f);
    gTileCount = count;
    printf("[ReadScript] Total: %d tiles\n\n", count);
    
    return count;
}

/* ========================================================================== */
/* main                                                                       */
/* ========================================================================== */

int main(void)
{
    struct Screen *screen = NULL;
    struct ScreenModeRequester *sm = NULL;
    ULONG modeid = INVALID_ID;
    ULONG width, height, depth;
    
    printf("=== Load_Pic_show_datatype START ===\n\n");
    
    /* Open intuition library */
    printf("[1] Opening intuition.library...\n");
    IntuitionBase = (struct IntuitionBase *)OpenLibrary("intuition.library", 39);
    if (!IntuitionBase)
    {
        printf("[ERROR] Cannot open intuition.library\n");
        return 1;
    }
    printf("[1] OK\n\n");
    
    /* Open ASL library */
    printf("[2] Opening asl.library...\n");
    AslBase = OpenLibrary("asl.library", 38);
    if (!AslBase)
    {
        printf("[ERROR] Cannot open asl.library\n");
        CloseLibrary((struct Library *)IntuitionBase);
        return 2;
    }
    printf("[2] OK\n\n");
    
    /* Set current directory to program dir */
    CurrentDir(GetProgramDir());
    
    /* Step 1: Read script */
    printf("=== STEP 1: Read Script ===\n");
    if (!ReadScript())
    {
        printf("[ERROR] No tiles loaded\n");
        goto cleanup;
    }
    
    /* Step 2: ASL ScreenModeRequester */
    printf("\n=== STEP 2: ASL ScreenMode Requester ===\n");
    
    sm = (struct ScreenModeRequester *)AllocAslRequest(ASL_ScreenModeRequest, NULL);
    if (!sm)
    {
        printf("[ERROR] Cannot allocate ScreenModeRequester\n");
        goto cleanup;
    }
    printf("[ASL] Requester allocated\n");
    
    if (!AslRequestTags(sm, TAG_DONE))
    {
        printf("[ASL] User cancelled\n");
        goto cleanup;
    }
    
    printf("[ASL] User selected:\n");
    printf("  Width:  %d\n", sm->sm_DisplayWidth);
    printf("  Height: %d\n", sm->sm_DisplayHeight);
    printf("  Depth:  %d\n", sm->sm_DisplayDepth);
    printf("  ModeID: 0x%lx\n", sm->sm_DisplayID);
    
    width  = sm->sm_DisplayWidth;
    height = sm->sm_DisplayHeight;
    depth  = sm->sm_DisplayDepth;
    modeid = sm->sm_DisplayID;
    
    /* Step 3: Open screen with ASL selection */
    printf("\n=== STEP 3: Open Screen ===\n");
    printf("[Screen] Opening screen with ModeID=0x%lx, %ldx%ldx%ld\n", 
           modeid, width, height, depth);
    
    screen = OpenScreenTags(NULL,
                            SA_DisplayID, modeid,
                            SA_Width, width,
                            SA_Height, height,
                            SA_Depth, depth,
                            SA_Title, (ULONG)"Load_Pic_show_datatype",
                            TAG_DONE);
    
    if (!screen)
    {
        printf("[ERROR] OpenScreenTags failed, IoErr=%ld\n", (long)IoErr());
        goto cleanup;
    }
    
    printf("[Screen] OK - Screen opened successfully\n");
    printf("[Screen] Waiting 10 seconds...\n");
    
    Delay(500);  /* ~10 seconds */
    
    printf("[Screen] Done\n");

cleanup:
    printf("\n=== CLEANUP ===\n");
    
    if (screen)
    {
        printf("[Cleanup] Closing screen...\n");
        CloseScreen(screen);
    }
    
    if (sm)
    {
        printf("[Cleanup] Freeing ASL requester...\n");
        FreeAslRequest(sm);
    }
    
    if (AslBase)
    {
        printf("[Cleanup] Closing asl.library...\n");
        CloseLibrary(AslBase);
    }
    
    if (IntuitionBase)
    {
        printf("[Cleanup] Closing intuition.library...\n");
        CloseLibrary((struct Library *)IntuitionBase);
    }
    
    printf("=== Load_Pic_show_datatype END ===\n\n");
    
    return 0;
}
