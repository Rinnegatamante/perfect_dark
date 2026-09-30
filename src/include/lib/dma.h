#ifndef _IN_LIB_DMA_H
#define _IN_LIB_DMA_H
#include <ultra64.h>
#include "data.h"
#include "types.h"

void dmaInit(void);
void dmaStart(void *memaddr, romptr_t romaddr, u32 len, bool priority);
void dmaCheckPiracy(void *memaddr, u32 len);
void dmaWait(void);

#ifdef __vita__
#include <vitasdk.h>
#define dmaExec(memaddr, romaddr, len) \
	sceClibMemcpy((memaddr), (const void *)(uintptr_t)(romaddr), (len))
#define dmaExecHighPriority(memaddr, romaddr, len) \
	sceClibMemcpy((memaddr), (const void *)(uintptr_t)(romaddr), (len))
#else
void dmaExec(void *memaddr, romptr_t romaddr, u32 len);
void dmaExecHighPriority(void *memaddr, romptr_t romaddr, u32 len);
#endif
void *dmaExecWithAutoAlign(void *memaddr, romptr_t romaddr, u32 len);

#endif
