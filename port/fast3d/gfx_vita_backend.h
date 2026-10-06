#ifndef GFX_VITA_BACKEND_H
#define GFX_VITA_BACKEND_H

#ifdef __vita__

#ifndef _LANGUAGE_C
#define _LANGUAGE_C
#endif
#include <PR/gbi.h>

#ifdef __cplusplus
extern "C" {
#endif

#include "gfx_api.h"

typedef void (*GfxVitaBackendSyncFn)(void *arg);

struct GfxVitaFrameState {
    struct XYWidthHeight native_viewport;
    float native_aspect;
    int16_t window_x;
    int16_t window_y;
};

bool gfx_vita_backend_start(void);
void gfx_vita_backend_run_sync(GfxVitaBackendSyncFn fn, void *arg);
void gfx_vita_backend_submit_frame(Gfx *commands, const struct GfxVitaFrameState *state);
void gfx_vita_backend_wait_idle(void);
void gfx_vita_backend_shutdown(void);

#ifdef __cplusplus
}
#endif

#endif

#endif
