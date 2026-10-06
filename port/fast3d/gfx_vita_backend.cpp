#ifdef __vita__

#include <vitasdk.h>

#include "gfx_vita_backend.h"

namespace {

static SceUID backend_thread = -1;
static SceKernelLwMutexWork backend_mutex;
static SceKernelLwCondWork backend_cond;

static bool backend_running;
static bool backend_exit;

static Gfx *frame_commands;
static GfxVitaFrameState frame_state;
static bool frame_pending;
static bool frame_active;

static GfxVitaBackendSyncFn sync_fn;
static void *sync_arg;
static bool sync_pending;
static bool sync_done;

static void lock_backend() {
    sceKernelLockLwMutex(&backend_mutex, 1, nullptr);
}

static void unlock_backend() {
    sceKernelUnlockLwMutex(&backend_mutex, 1);
}

static bool backend_busy() {
    return frame_pending || frame_active || sync_pending;
}

static int backend_main(SceSize, void *) {
    for (;;) {
        Gfx *commands = nullptr;
        GfxVitaFrameState state = {};
        GfxVitaBackendSyncFn fn = nullptr;
        void *arg = nullptr;

        lock_backend();
        while (!backend_exit && !frame_pending && !sync_pending) {
            sceKernelWaitLwCond(&backend_cond, nullptr);
        }

        if (backend_exit && !frame_pending && !sync_pending) {
            unlock_backend();
            break;
        }

        if (sync_pending) {
            fn = sync_fn;
            arg = sync_arg;
            sync_pending = false;
            unlock_backend();

            fn(arg);

            lock_backend();
            sync_done = true;
            sceKernelSignalLwCondAll(&backend_cond);
            unlock_backend();
            continue;
        }

        commands = frame_commands;
        state = frame_state;
        frame_pending = false;
        frame_active = true;
        unlock_backend();

        gfx_current_native_viewport = state.native_viewport;
        gfx_current_native_aspect = state.native_aspect;
        gfx_current_game_window_viewport.x = state.window_x;
        gfx_current_game_window_viewport.y = state.window_y;

        gfx_start_frame();
        gfx_run(commands);
        gfx_end_frame();

        lock_backend();
        frame_active = false;
        frame_commands = nullptr;
        sceKernelSignalLwCondAll(&backend_cond);
        unlock_backend();
    }

    sceKernelExitThread(0);
    return 0;
}

}

bool gfx_vita_backend_start(void) {
    if (backend_running) {
        return true;
    }

    backend_exit = false;
    frame_commands = nullptr;
    frame_pending = false;
    frame_active = false;
    sync_fn = nullptr;
    sync_arg = nullptr;
    sync_pending = false;
    sync_done = false;

    if (sceKernelCreateLwMutex(&backend_mutex, "PD Render Mutex", 0, 0, nullptr) < 0) {
        return false;
    }

    if (sceKernelCreateLwCond(&backend_cond, "PD Render Cond", 0, &backend_mutex, nullptr) < 0) {
        sceKernelDeleteLwMutex(&backend_mutex);
        return false;
    }

    backend_thread = sceKernelCreateThread("PD Render Backend", backend_main, 0x10000100, 1024 * 1024, 0, 0, nullptr);

    if (backend_thread < 0 || sceKernelStartThread(backend_thread, 0, nullptr) < 0) {
        if (backend_thread >= 0) {
            sceKernelDeleteThread(backend_thread);
        }
        backend_thread = -1;
        sceKernelDeleteLwCond(&backend_cond);
        sceKernelDeleteLwMutex(&backend_mutex);
        return false;
    }

    backend_running = true;
    return true;
}

void gfx_vita_backend_run_sync(GfxVitaBackendSyncFn fn, void *arg) {
    if (!fn) {
        return;
    }

    if (!backend_running || sceKernelGetThreadId() == backend_thread) {
        fn(arg);
        return;
    }

    lock_backend();
    while (backend_busy()) {
        sceKernelWaitLwCond(&backend_cond, nullptr);
    }

    sync_fn = fn;
    sync_arg = arg;
    sync_done = false;
    sync_pending = true;
    sceKernelSignalLwCondAll(&backend_cond);

    while (!sync_done) {
        sceKernelWaitLwCond(&backend_cond, nullptr);
    }

    sync_fn = nullptr;
    sync_arg = nullptr;
    unlock_backend();
}

void gfx_vita_backend_submit_frame(Gfx *commands, const struct GfxVitaFrameState *state) {
    if (!backend_running || !commands || !state) {
        return;
    }

    lock_backend();

    while (frame_pending || frame_active || sync_pending) {
        sceKernelWaitLwCond(&backend_cond, nullptr);
    }

    frame_commands = commands;
    frame_state = *state;
    frame_pending = true;
    sceKernelSignalLwCondAll(&backend_cond);
    unlock_backend();
}

void gfx_vita_backend_wait_idle(void) {
    if (!backend_running) {
        return;
    }

    lock_backend();
    while (backend_busy()) {
        sceKernelWaitLwCond(&backend_cond, nullptr);
    }
    unlock_backend();
}

void gfx_vita_backend_shutdown(void) {
    if (!backend_running) {
        return;
    }

    gfx_vita_backend_wait_idle();

    lock_backend();
    backend_exit = true;
    sceKernelSignalLwCondAll(&backend_cond);
    unlock_backend();

    sceKernelWaitThreadEnd(backend_thread, nullptr, nullptr);
    sceKernelDeleteThread(backend_thread);
    backend_thread = -1;

    sceKernelDeleteLwCond(&backend_cond);
    sceKernelDeleteLwMutex(&backend_mutex);
    backend_running = false;
}

#endif
