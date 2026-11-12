#include <ultra64.h>

#include "buffers.h"
#include "audio/data.h"

ALIGNED16 u8 gAudioHeap[DOUBLE_SIZE_ON_64_BIT(AUDIO_HEAP_SIZE)] = { 0 };

// 0x200 bytes
ALIGNED8 struct SaveBuffer gSaveBuffer = { 0 };
// 0x190a0 bytes
struct GfxPool gGfxPools[GFX_NUM_POOLS] = { 0 };
