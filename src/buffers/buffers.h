#ifndef BUFFERS_H
#define BUFFERS_H

#include <PR/ultratypes.h>

#include "audio/data.h"
#include "game/save_file.h"
#include "game/game_init.h"

extern u8 gAudioHeap[];

extern struct SaveBuffer gSaveBuffer;

#define GFX_NUM_POOLS 1

extern struct GfxPool gGfxPools[GFX_NUM_POOLS];

#endif // BUFFERS_H
