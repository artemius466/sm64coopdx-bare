#include <PR/ultratypes.h>

#include "engine/math_util.h"
#include "game/memory.h"
#include "game/segment2.h"
#include "game/segment7.h"
#include "intro_geo.h"
#include "sm64.h"
#include "textures.h"
#include "types.h"
#include "buffers/framebuffers.h"
#include "game/game_init.h"
#include "audio/external.h"
#include "prevent_bss_reordering.h"

#include "gfx_dimensions.h"
#include "game/rendering_graph_node.h"
#include "pc/utils/misc.h"

// frame counts for the zoom in, hold, and zoom out of title model
#define INTRO_STEPS_ZOOM_IN 20
#define INTRO_STEPS_HOLD_1 75
#define INTRO_STEPS_ZOOM_OUT 91

// background types
#define INTRO_BACKGROUND_SUPER_MARIO 0
#define INTRO_BACKGROUND_GAME_OVER 1

struct GraphNodeMore {
    /*0x00*/ struct GraphNode node;
    /*0x14*/ void *todo;
    /*0x18*/ u32 unk18;
};

// intro geo bss
static s32 sGameOverFrameCounter;
static s32 sGameOverTableIndex;
static s16 sIntroFrameCounter;
static s32 sTmCopyrightAlpha;

static Gfx *sIntroScalePos;
static Vec3f sIntroScale;
static Vec3f sIntroScalePrev;

bool gSkipInterpolationTitleScreen = false;

void patch_title_screen_before(void) {
    sIntroScalePos = NULL;
}

void patch_title_screen_interpolated(f32 delta) {
    if (sIntroScalePos != NULL) {
        Mtx *scaleMat = alloc_display_list(sizeof(*scaleMat));
        if (scaleMat == NULL) { return; }
        Vec3f scaleInterp;
        delta_interpolate_vec3f(scaleInterp, sIntroScalePrev, sIntroScale, delta);
        guScale(scaleMat, scaleInterp[0], scaleInterp[1], scaleInterp[2]);
        gSPMatrix(sIntroScalePos, scaleMat, G_MTX_MODELVIEW | G_MTX_MUL | G_MTX_PUSH);
    }
}

/**
 * Geo callback to render the "Super Mario 64" logo on the title screen
 */
Gfx *geo_intro_super_mario_64_logo(s32 state, struct GraphNode *node, UNUSED void *context) {
    struct GraphNode *graphNode = node;
    Gfx *dl = NULL;
    Gfx *dlIter = NULL;
    Mtx *scaleMat;
    f32 *scaleTable1 = segmented_to_virtual(intro_seg7_table_0700C790);
    f32 *scaleTable2 = segmented_to_virtual(intro_seg7_table_0700C880);
    f32 scaleX;
    f32 scaleY;
    f32 scaleZ;

    if (state != 1) {
        sIntroFrameCounter = 0;
    } else if (state == 1) {
        graphNode->flags = (graphNode->flags & 0xFF) | (LAYER_OPAQUE << 8);
        scaleMat = alloc_display_list(sizeof(*scaleMat));
        dl = alloc_display_list(4 * sizeof(*dl));
        if (scaleMat == NULL || dl == NULL) { return NULL; }
        dlIter = dl;

        // determine scale based on the frame counter
        if (sIntroFrameCounter >= 0 && sIntroFrameCounter < INTRO_STEPS_ZOOM_IN) {
            // zooming in
            scaleX = scaleTable1[sIntroFrameCounter * 3];
            scaleY = scaleTable1[sIntroFrameCounter * 3 + 1];
            scaleZ = scaleTable1[sIntroFrameCounter * 3 + 2];
        } else if (sIntroFrameCounter >= INTRO_STEPS_ZOOM_IN && sIntroFrameCounter < INTRO_STEPS_HOLD_1) {
            // holding
            scaleX = 1.0f;
            scaleY = 1.0f;
            scaleZ = 1.0f;
        } else if (sIntroFrameCounter >= INTRO_STEPS_HOLD_1 && sIntroFrameCounter < INTRO_STEPS_ZOOM_OUT) {
            // zooming out
            scaleX = scaleTable2[(sIntroFrameCounter - INTRO_STEPS_HOLD_1) * 3];
            scaleY = scaleTable2[(sIntroFrameCounter - INTRO_STEPS_HOLD_1) * 3 + 1];
            scaleZ = scaleTable2[(sIntroFrameCounter - INTRO_STEPS_HOLD_1) * 3 + 2];
        } else {
            // disappeared
            scaleX = 0.0f;
            scaleY = 0.0f;
            scaleZ = 0.0f;
        }
        guScale(scaleMat, scaleX, scaleY, scaleZ);

        gSPMatrix(dlIter++, scaleMat, G_MTX_MODELVIEW | G_MTX_MUL | G_MTX_PUSH);
        gSPDisplayList(dlIter++, &intro_seg7_dl_0700B3A0);  // draw model
        gSPPopMatrix(dlIter++, G_MTX_MODELVIEW);
        gSPEndDisplayList(dlIter);

        sIntroFrameCounter++;
    }
    return dl;
}

/**
 * Geo callback to render TM and Copyright on the title screen
 */
Gfx *geo_intro_tm_copyright(s32 state, struct GraphNode *node, UNUSED void *context) {
    struct GraphNode *graphNode = node;
    Gfx *dl = NULL;
    Gfx *dlIter = NULL;

    if (state != 1) {  // reset
        sTmCopyrightAlpha = 0;
    } else if (state == 1) {  // draw
        dl = alloc_display_list(5 * sizeof(*dl));
        if (dl == NULL) { return NULL; }
        dlIter = dl;
        gSPDisplayList(dlIter++, dl_proj_mtx_fullscreen);
        gDPSetEnvColor(dlIter++, 255, 255, 255, sTmCopyrightAlpha);
        switch (sTmCopyrightAlpha) {
            case 255: // opaque
                graphNode->flags = (graphNode->flags & 0xFF) | (LAYER_OPAQUE << 8);
                gDPSetRenderMode(dlIter++, G_RM_AA_OPA_SURF, G_RM_AA_OPA_SURF2);
                break;
            default: // blend
                graphNode->flags = (graphNode->flags & 0xFF) | (LAYER_TRANSPARENT << 8);
                gDPSetRenderMode(dlIter++, G_RM_AA_XLU_SURF, G_RM_AA_XLU_SURF2);
                break;
        }
        gSPDisplayList(dlIter++, &intro_seg7_dl_0700C6A0);  // draw model
        gSPEndDisplayList(dlIter);

        // Once the "Super Mario 64" logo has just about zoomed fully, fade in the "TM" and copyright text
        if (sIntroFrameCounter >= 19) {
            sTmCopyrightAlpha += 26;
            if (sTmCopyrightAlpha > 255) {
                sTmCopyrightAlpha = 255;
            }
        }
    }
    return dl;
}

/**
 * Generates a display list for a single background tile
 *
 * @param index            which tile to render (value from 0 to 11)
 * @param backgroundTable  array describing which image to use for each tile (0 denotes a "Super Mario 64" image, and 1 denotes a "Game Over" image)
 */
Gfx *intro_backdrop_set_image(f32 x, f32 y, s8 index) {
    // intro screen background display lists for each of four 80x20 textures
    static const Gfx *introBackgroundDlRows[] = { title_screen_bg_dl_0A000130, title_screen_bg_dl_0A000148,
                                                  title_screen_bg_dl_0A000160, title_screen_bg_dl_0A000178 };

    // table that points to either the "Super Mario 64" or "Game Over" tables
    static const u8 *const *textureTables[] = { mario_title_texture_table, game_over_texture_table };

    s32 i;
    Mtx *mtx = alloc_display_list(sizeof(*mtx));
    Gfx *displayList = alloc_display_list(36 * sizeof(*displayList));
    if (mtx == NULL || displayList == NULL) { return NULL; }
    Gfx *displayListIter = displayList;
    if (index < 0 || index > 1) { index = 0; }
    const u8 *const *vIntroBgTable = segmented_to_virtual(textureTables[index]);
    guTranslate(mtx, x, y, 0.0f);
    gSPMatrix(displayListIter++, mtx, G_MTX_MODELVIEW | G_MTX_LOAD | G_MTX_PUSH);
    gSPDisplayList(displayListIter++, &title_screen_bg_dl_0A000118);
    for (i = 0; i < 4; ++i) {
        gDPLoadTextureBlock(displayListIter++, vIntroBgTable[i], G_IM_FMT_RGBA, G_IM_SIZ_16b, 80, 20, 0,
                            G_TX_CLAMP, G_TX_CLAMP, 7, 6, G_TX_NOLOD, G_TX_NOLOD)
        gSPDisplayList(displayListIter++, introBackgroundDlRows[i]);
    }
    gSPPopMatrix(displayListIter++, G_MTX_MODELVIEW);
    gSPEndDisplayList(displayListIter);
    return displayList;
}

/**
 * Geo callback to render the intro background tiles
 */
Gfx *geo_intro_regular_backdrop(s32 state, struct GraphNode *node, UNUSED void *context) {
    struct GraphNodeMore *graphNode;
    Gfx *displayList;
    Gfx *displayListIter;
    s32 w, x, y;

    // Get how many horizontal tiles to draw (limit width to prevent segfault)
    w = ((SCREEN_HEIGHT * GFX_DIMENSIONS_ASPECT_RATIO) + 79) / 80;
    if (w > 50)
        w = 50;

    graphNode = (struct GraphNodeMore *) node;
    displayList = NULL;
    displayListIter = NULL;
    if (state == 1) {
        displayList = alloc_display_list((4 + w * 3) * sizeof(*displayList));
        if (displayList == NULL) { return NULL; }
        displayListIter = displayList;
        graphNode->node.flags = (graphNode->node.flags & 0xFF) | (LAYER_OPAQUE << 8);
        gSPDisplayList(displayListIter++, &dl_proj_mtx_fullscreen);
        gSPDisplayList(displayListIter++, &title_screen_bg_dl_0A000100);
        for (y = 0; y < 3; ++y) {
            for (x = 0; x < w; ++x) {
                gSPDisplayList(displayListIter++, intro_backdrop_set_image(80.0f * x + SCREEN_WIDTH / 2 - w * 40.0f, 160.0f - 80.0f * y, INTRO_BACKGROUND_SUPER_MARIO));
            }
        }
        gSPDisplayList(displayListIter++, &title_screen_bg_dl_0A000190);
        gSPEndDisplayList(displayListIter);
    }
    return displayList;
}

/**
 * Geo callback to render the Game Over background tiles
 */
Gfx *geo_intro_gameover_backdrop(s32 state, struct GraphNode *node, UNUSED void *context) {
    struct GraphNodeMore *graphNode;
    Gfx *displayList;
    Gfx *displayListIter;
    s32 w, x, y;
    s8 index;

    // Get how many horizontal tiles to draw (limit width to prevent segfault)
    w = ((SCREEN_HEIGHT * GFX_DIMENSIONS_ASPECT_RATIO) + 79) / 80;
    if (w > 50)
        w = 50;

    graphNode = (struct GraphNodeMore *) node;
    displayList = NULL;
    displayListIter = NULL;
    if (state != 1) {
        sGameOverFrameCounter = 0;
        sGameOverTableIndex = -1;
    } else {
        displayList = alloc_display_list((4 + w * 3) * sizeof(*displayList));
        if (displayList == NULL) { return NULL; }
        displayListIter = displayList;

        // Wait 180 frames, then increment flipped tile index
        if (sGameOverTableIndex == -1) {
            if (sGameOverFrameCounter == 180) {
                sGameOverTableIndex++;
                sGameOverFrameCounter = 0;
            } else {
                sGameOverFrameCounter++;
            }
        }
        else
        {
            if ((++sGameOverFrameCounter & 1) == 0) {
                if (sGameOverTableIndex < 0x7FFFFFFF) {
                    sGameOverTableIndex++;
                }
            }
        }

        graphNode->node.flags = (graphNode->node.flags & 0xFF) | (LAYER_OPAQUE << 8);
        gSPDisplayList(displayListIter++, &dl_proj_mtx_fullscreen);
        gSPDisplayList(displayListIter++, &title_screen_bg_dl_0A000100);
        for (y = 0; y < 3; ++y) {
            for (x = 0; x < w; ++x) {
                // Get if this tile should be a "Game Over" or "Super Mario 64" tile based off of the current flip index
                index = INTRO_BACKGROUND_GAME_OVER;
                switch (y)
                {
                    case 2:
                        if (x > w - (sGameOverTableIndex - w)) {
                            index = INTRO_BACKGROUND_SUPER_MARIO;
                        }
                        break;
                    case 1:
                        if ((x == w - 1 && sGameOverTableIndex > w) || (x < (sGameOverTableIndex - w * 2 - 1))) {
                            index = INTRO_BACKGROUND_SUPER_MARIO;
                        }
                        break;
                    case 0:
                        if (x < sGameOverTableIndex) {
                            index = INTRO_BACKGROUND_SUPER_MARIO;
                        }
                        break;
                }

                gSPDisplayList(displayListIter++, intro_backdrop_set_image(80.0f * x + SCREEN_WIDTH / 2 - w * 40.0f, 160.0f - 80.0f * y, index));
            }
        }
        gSPDisplayList(displayListIter++, &title_screen_bg_dl_0A000190);
        gSPEndDisplayList(displayListIter);
    }
    return displayList;
}
