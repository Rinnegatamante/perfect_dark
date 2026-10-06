#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <PR/ultratypes.h>
#include <PR/gbi.h>
#include "platform.h"
#include "config.h"
#include "system.h"
#include "video.h"

#include "../fast3d/gfx_api.h"
#include "../fast3d/gfx_sdl.h"
#include "../fast3d/gfx_opengl.h"
#ifdef __vita__
#include "../fast3d/gfx_vita_backend.h"
#endif

#ifdef PLATFORM_NSWITCH
#define DEFAULT_VID_WIDTH 1280
#define DEFAULT_VID_HEIGHT 720
#define DEFAULT_VID_FULLSCREEN true
#define DEFAULT_VID_FULLSCREEN_EXCLUSIVE true
#else
#define DEFAULT_VID_WIDTH 640
#define DEFAULT_VID_HEIGHT 480
#define DEFAULT_VID_FULLSCREEN false
#define DEFAULT_VID_FULLSCREEN_EXCLUSIVE false
#endif

#ifdef __vita__
#include <vitasdk.h>
//#define STATIC_FRAMESKIP
//#define AUTO_FRAMESKIP
#endif

#define AUTO_FRAMESKIP_TARGET (16667)

static struct GfxWindowManagerAPI *wmAPI;
static struct GfxRenderingAPI *renderingAPI;

static bool initDone = false;

static s32 vidWidth = DEFAULT_VID_WIDTH;
static s32 vidHeight = DEFAULT_VID_HEIGHT;
static s32 vidFramebuffers = true;
static s32 vidFullscreen = DEFAULT_VID_FULLSCREEN;
static s32 vidFullscreenExclusive = DEFAULT_VID_FULLSCREEN_EXCLUSIVE;
static s32 vidMaximize = false;
static s32 vidCenter = false;
static s32 vidAllowHiDpi = false;
static s32 vidVsync = 1;
static s32 vidMSAA = 1;
static s32 vidFramerateLimit = 0;

static s32 vidDisplayFPS = 0;
static f32 vidDisplayFPSInterval = 1.f;
static f32 vidAvgFPS = 0;

static s32 vidNumModes = 1;
static displaymode vidModeDefault;
static displaymode *vidModes = &vidModeDefault;

static s32 texFilter = FILTER_LINEAR;
static s32 texFilter2D = true;
static s32 texDetail = false;
static s32 texMipmapFilter = MIPMAP_LINEAR;
static u32 texAnisotropicFilter = 4;

static u32 dlcount = 0;
static u32 frames = 0;
static f64 startTime, endTime;
static f64 accumDelta = 0.0;
static f64 fpsTime = 0.0;
static s32 fpsNumFrames = 0;

#ifdef __vita__
static struct GfxVitaFrameState vitaFrameState = {
	.native_viewport = { 0, 0, 320, 220 },
	.native_aspect = 320.f / 220.f,
	.window_x = 0,
	.window_y = 0,
};

static void videoBackendInit(void *arg)
{
	gfx_init((const struct GfxInitSettings *)arg);
}

static void videoBackendDestroy(void *arg)
{
	(void)arg;
	gfx_destroy();
}

struct VideoBackendIntArg {
	s32 value;
	s32 result;
};

static void videoBackendSetVsync(void *arg)
{
	struct VideoBackendIntArg *data = arg;
	data->result = wmAPI->set_swap_interval(data->value);
}

static void videoBackendSetFramerateLimit(void *arg)
{
	struct VideoBackendIntArg *data = arg;
	wmAPI->set_target_fps(data->value);
}

static void videoBackendSetTextureFilter(void *arg)
{
	gfx_set_texture_filter((enum FilteringMode)((struct VideoBackendIntArg *)arg)->value);
}

static void videoBackendSetMipmapFilter(void *arg)
{
	gfx_set_mipmap_filter((enum MipmapFilteringMode)((struct VideoBackendIntArg *)arg)->value);
}

static void videoBackendSetAnisotropy(void *arg)
{
	renderingAPI->set_anisotropy_level(((struct VideoBackendIntArg *)arg)->value);
}

static void videoBackendSetDetailTextures(void *arg)
{
	gfx_detail_textures_enabled = ((struct VideoBackendIntArg *)arg)->value;
}

static void videoBackendSetMSAA(void *arg)
{
	gfx_msaa_level = ((struct VideoBackendIntArg *)arg)->value;
}

struct VideoBackendFramebufferArg {
	s32 target;
	u32 width;
	u32 height;
	s32 upscale;
	s32 autoresize;
	s32 src;
	s32 left;
	s32 top;
	s32 result;
};

static void videoBackendCreateFramebuffer(void *arg)
{
	struct VideoBackendFramebufferArg *data = arg;
	data->result = gfx_create_framebuffer(data->width, data->height, data->upscale, data->autoresize);
}

static void videoBackendSetFramebuffer(void *arg)
{
	gfx_set_framebuffer(((struct VideoBackendFramebufferArg *)arg)->target, 1.f);
}

static void videoBackendResetFramebuffer(void *arg)
{
	(void)arg;
	gfx_reset_framebuffer();
}

static void videoBackendResizeFramebuffer(void *arg)
{
	struct VideoBackendFramebufferArg *data = arg;
	gfx_resize_framebuffer(data->target, data->width, data->height, data->upscale, data->autoresize);
}

static void videoBackendCopyFramebuffer(void *arg)
{
	struct VideoBackendFramebufferArg *data = arg;
	gfx_copy_framebuffer(data->target, data->src, data->left, data->top, false);
}

static void videoBackendResetTextureCache(void *arg)
{
	(void)arg;
	gfx_texture_cache_clear();
}

struct VideoBackendTextureRangeArg {
	const u8 *start;
	const u8 *end;
};

static void videoBackendFreeCachedTexture(void *arg)
{
	gfx_texture_cache_delete(((struct VideoBackendTextureRangeArg *)arg)->start);
}

static void videoBackendFreeCachedTextures(void *arg)
{
	struct VideoBackendTextureRangeArg *data = arg;
	gfx_texture_cache_delete_range(data->start, data->end);
}
#endif

#ifndef __vita__
static s32 videoInitDisplayModes(void);
#endif
void optionsMenuInit();

s32 videoInit(void)
{
	wmAPI = &gfx_sdl;
	renderingAPI = &gfx_opengl_api;

	gfx_current_native_viewport.width = 320;
	gfx_current_native_viewport.height = 220;
	gfx_current_native_aspect = 320.f / 220.f;
	gfx_framebuffers_enabled = (bool)vidFramebuffers;
	gfx_detail_textures_enabled = (bool)texDetail;
	gfx_msaa_level = vidMSAA;

	struct GfxInitSettings set = {
		.wapi = wmAPI,
		.rapi = renderingAPI,
		.window_settings = {
			.title = "Perfect Dark",
			.width = vidWidth,
			.height = vidHeight,
			.x = 100,
			.y = 100,
			.fullscreen = vidFullscreen,
			.fullscreen_is_exclusive = vidFullscreenExclusive,
			.maximized = vidMaximize,
			.centered = vidCenter,
			.allow_hidpi = vidAllowHiDpi
		}
	};

#ifdef __vita__
	if (!gfx_vita_backend_start()) {
		return -1;
	}
	gfx_vita_backend_run_sync(videoBackendInit, &set);
#else
	gfx_init(&set);
#endif

#ifdef __vita__
	vidModeDefault.width = 960;
	vidModeDefault.height = 544;
	vidModes = &vidModeDefault;
	vidNumModes = 1;
#else
	videoInitDisplayModes();
#endif
	videoSetVsync(vidVsync);
	videoSetFramerateLimit(vidFramerateLimit);

#ifdef __vita__
	struct VideoBackendIntArg filter = { texFilter, 0 };
	struct VideoBackendIntArg mipmap = { texMipmapFilter, 0 };
	gfx_vita_backend_run_sync(videoBackendSetTextureFilter, &filter);
	gfx_vita_backend_run_sync(videoBackendSetMipmapFilter, &mipmap);
	videoSetAnisotropicFilter(texAnisotropicFilter);
#else
	gfx_set_texture_filter((enum FilteringMode)texFilter);
	gfx_set_mipmap_filter((enum MipmapFilteringMode)texMipmapFilter);
	videoSetAnisotropicFilter(texAnisotropicFilter);
#endif
	optionsMenuInit();

	initDone = true;
	return 0;
}

void videoStartFrame(void)
{
	if (initDone) {
		startTime = wmAPI->get_time();
#ifndef __vita__
		gfx_start_frame();
#endif
	}

#ifndef __vita__
	// Synchronize with their backend counterparts.
	vidFullscreen = videoGetFullscreen();
	vidMaximize = videoGetMaximizeWindow();
#endif
}

void videoSubmitCommands(Gfx *cmds)
{
#ifdef STATIC_FRAMESKIP
	static int skip_frame = 0;
#elif defined(AUTO_FRAMESKIP)
	static uint32_t last_frame_tick = 0;
	static uint32_t cur_delta = 0;
	static uint32_t expected_delta = 0;
#endif
	if (initDone) {
#ifdef STATIC_FRAMESKIP
		if (!skip_frame)
			gfx_run(cmds);
		skip_frame = !skip_frame;
#elif defined(AUTO_FRAMESKIP)
		uint32_t cur_frame_tick = sceKernelGetProcessTimeLow();
		uint32_t frame_delta = last_frame_tick ? (cur_frame_tick - last_frame_tick) : AUTO_FRAMESKIP_TARGET;
		expected_delta += AUTO_FRAMESKIP_TARGET;
		cur_delta += frame_delta;
		if (cur_delta <= expected_delta) {
			gfx_run(cmds);
		}
		last_frame_tick = cur_frame_tick;
#else
#ifdef __vita__
		gfx_vita_backend_submit_frame(cmds, &vitaFrameState);
#else
		gfx_run(cmds);
#endif
#endif
		++dlcount;
	}
}

void videoEndFrame(void)
{
	if (!initDone) {
		return;
	}

#ifndef __vita__
	gfx_end_frame();
#endif

	++frames;
	++fpsNumFrames;

	const f64 flipTime = wmAPI->get_time();
	accumDelta += flipTime - endTime;
	endTime = flipTime;

	if (endTime >= fpsTime) {
		char tmp[128];
		vidAvgFPS = fpsNumFrames ? ((f64)fpsNumFrames / accumDelta) : 0.f;
		fpsNumFrames = 0;
		accumDelta = 0.0;
		fpsTime = endTime + vidDisplayFPSInterval;
	}
}



f32 videoGetAverageFPS(void)
{
	return vidAvgFPS;
}

void videoClearScreen(void)
{
	videoStartFrame();
	// TODO: clear
	videoEndFrame();
}

void *videoGetWindowHandle(void)
{
	if (initDone) {
		return wmAPI->get_window_handle();
	}
	return NULL;
}

void videoUpdateNativeResolution(s32 w, s32 h)
{
#ifdef __vita__
	vitaFrameState.native_viewport.width = w;
	vitaFrameState.native_viewport.height = h;
	vitaFrameState.native_aspect = (float)w / (float)h;
#else
	gfx_current_native_viewport.width = w;
	gfx_current_native_viewport.height = h;
	gfx_current_native_aspect = (float)w / (float)h;
#endif
}

s32 videoGetNativeWidth(void)
{
#ifdef __vita__
	return vitaFrameState.native_viewport.width;
#else
	return gfx_current_native_viewport.width;
#endif
}

s32 videoGetNativeHeight(void)
{
#ifdef __vita__
	return vitaFrameState.native_viewport.height;
#else
	return gfx_current_native_viewport.height;
#endif
}

s32 videoGetWidth(void)
{
#ifdef __vita__
	return 960;
#else
	return gfx_current_dimensions.width;
#endif
}

s32 videoGetHeight(void)
{
#ifdef __vita__
	return 544;
#else
	return gfx_current_dimensions.height;
#endif
}

s32 videoGetFullscreen(void)
{
	vidFullscreen = wmAPI->get_fullscreen_state();
	return vidFullscreen;
}

s32 videoGetFullscreenMode(void)
{
	vidFullscreenExclusive = wmAPI->get_fullscreen_flag_mode();
	return vidFullscreenExclusive;
}

s32 videoGetMaximizeWindow(void)
{
	vidMaximize = wmAPI->get_maximized_state();
	return vidMaximize;
}

s32 videoGetCenterWindow(void)
{
	return vidCenter;
}

f32 videoGetAspect(void)
{
#ifdef __vita__
	return 960.f / 544.f;
#else
	return gfx_current_dimensions.aspect_ratio;
#endif
}

s32 videoGetDisplayModeIndex(void)
{
#ifdef __vita__
	return 0;
#else
	for (s32 i = 1; i < vidNumModes; ++i) {
		if (vidModes[i].width == gfx_current_dimensions.width &&
		    vidModes[i].height == gfx_current_dimensions.height) {
			return i;
		}
	}
	// Current dimensions don't match any known mode, so return index 0, "Custom".
	return 0;
#endif
}

s32 videoGetMSAA(void)
{
#ifdef __vita__
	return vidMSAA;
#else
	vidMSAA = (s32)gfx_msaa_level;
	return vidMSAA;
#endif
}

s32 videoGetVsync(void)
{
#ifdef __vita__
	return vidVsync;
#else
	vidVsync = wmAPI->get_swap_interval();
	return vidVsync;
#endif
}

s32 videoGetFramerateLimit(void)
{
#ifdef __vita__
	return vidFramerateLimit;
#else
	vidFramerateLimit = wmAPI->get_target_fps();
	return vidFramerateLimit;
#endif
}

s32 videoGetDisplayFPS(void)
{
	return vidDisplayFPS;
}

#ifndef __vita__
static s32 videoInitDisplayModes(void)
{
	if (!wmAPI->get_current_display_mode(&vidModeDefault.width, &vidModeDefault.height)) {
		vidModeDefault.width = 640;
		vidModeDefault.height = 480;
		return false;
	}

	const s32 numBaseModes = wmAPI->get_num_display_modes();
	if (!numBaseModes) {
		return false;
	}

	const s32 numCustomModes = 1;
	displaymode *modeList = sysMemZeroAlloc((numBaseModes + numCustomModes) * sizeof(displaymode));
	if (!modeList) {
		return false;
	}

	modeList[0].width = 0;
	modeList[0].height = 0;

	s32 numModes = 1;
	s32 w = -1, h = w, neww = w, newh = w;

	// SDL modes are guaranteed to be sorted high to low
	for (s32 i = 0; i < numBaseModes; ++i) {
		wmAPI->get_display_mode(i, &neww, &newh);

		if (neww != w || newh != h) {
			w = neww;
			h = newh;
			modeList[numModes].width = w;
			modeList[numModes].height = h;
			++numModes;
		}
	}

	modeList = sysMemRealloc(modeList, numModes * sizeof(displaymode));
	if (!modeList) {
		return false;
	}

	vidModes = modeList;
	vidNumModes = numModes;

	return true;
}
#endif

s32 videoGetDisplayMode(displaymode *out, const s32 index)
{
	if (index >= 0 && index < vidNumModes) {
		*out = vidModes[index];
		return true;
	}
	return false;
}

s32 videoGetNumDisplayModes(void)
{
	return vidNumModes;
}

void videoSetDisplayMode(const s32 index)
{
	const displaymode dm = vidModes[index];

	if (index == 0) {
		// "Custom" video mode.
		return;
	}

	vidWidth = dm.width;
	vidHeight = dm.height;

	s32 posX = 100;
	s32 posY = 100;
	if (vidCenter) {
		wmAPI->get_centered_positions(vidWidth, vidHeight, &posX, &posY);
	}

	if (vidFullscreen) {
		wmAPI->set_closest_resolution(vidWidth, vidHeight, vidCenter);
	} else {
		if (vidMaximize) {
			videoSetMaximizeWindow(false);
		} else {
			wmAPI->set_dimensions(vidWidth, vidHeight, posX, posY);
		}
	}
}

s32 videoGetTextureFilter2D(void)
{
	return texFilter2D;
}

u32 videoGetTextureFilter(void)
{
	return texFilter;
}

u32 videoGetAnisotropicFilter()
{
	return texAnisotropicFilter;
}

u32 videoGetMaxAnisotropyLevel()
{
#ifdef __vita__
	return 1;
#else
	return renderingAPI->get_max_anisotropy_level();
#endif
}

s32 videoGetDetailTextures(void)
{
	return texDetail;
}

void videoSetWindowOffset(s32 x, s32 y)
{
#ifdef __vita__
	vitaFrameState.window_x = x;
	vitaFrameState.window_y = y;
#else
	gfx_current_game_window_viewport.x = x;
	gfx_current_game_window_viewport.y = y;
#endif
}

void videoSetFullscreen(s32 fs)
{
	if (fs != vidFullscreen) {
		vidFullscreen = !!fs;
		wmAPI->set_closest_resolution(vidWidth, vidHeight, vidCenter);
		wmAPI->set_fullscreen(vidFullscreen);
		if (!vidFullscreen && vidMaximize) {
			wmAPI->set_maximize(false);
			wmAPI->set_maximize(true);
		}
	}
}

void videoSetFullscreenMode(s32 mode)
{
	vidFullscreenExclusive = mode;
	wmAPI->set_fullscreen_flag(mode);
	if (vidFullscreen) {
		wmAPI->set_fullscreen(false);
		wmAPI->set_fullscreen(true);
	}
}

void videoSetMaximizeWindow(s32 fs)
{
	if (fs != vidMaximize) {
		vidMaximize = !!fs;
		wmAPI->set_maximize(vidMaximize);
		if (vidCenter && !vidMaximize) {
			s32 posX = 0;
			s32 posY = 0;
			wmAPI->get_centered_positions(vidWidth, vidHeight, &posX, &posY);
			wmAPI->set_dimensions(vidWidth, vidHeight, posX, posY);
		}
	}
}

void videoSetCenterWindow(s32 center)
{
	vidCenter = center;
	if (vidCenter && !vidMaximize) {
		s32 posX = 0;
		s32 posY = 0;
		wmAPI->get_centered_positions(vidWidth, vidHeight, &posX, &posY);
		wmAPI->set_dimensions(vidWidth, vidHeight, posX, posY);
	}
}

void videoSetTextureFilter(u32 filter)
{
	if (filter > FILTER_THREE_POINT) filter = FILTER_THREE_POINT;
	if (texFilter == filter) return;
	texFilter = filter;
#ifdef __vita__
	struct VideoBackendIntArg arg = { filter, 0 };
	gfx_vita_backend_run_sync(videoBackendSetTextureFilter, &arg);
#else
	gfx_set_texture_filter((enum FilteringMode)filter);
#endif
}

void videoSetTextureFilter2D(s32 filter)
{
	texFilter2D = !!filter;
}

void videoSetAnisotropicFilter(u32 level)
{
#ifdef __vita__
	texAnisotropicFilter = 1;
	struct VideoBackendIntArg arg = { 1, 0 };
	gfx_vita_backend_run_sync(videoBackendSetAnisotropy, &arg);
#else
	texAnisotropicFilter = level;
	renderingAPI->set_anisotropy_level(level);
#endif
}

void videoSetDetailTextures(s32 detail)
{
	texDetail = !!detail;
#ifdef __vita__
	struct VideoBackendIntArg arg = { texDetail, 0 };
	gfx_vita_backend_run_sync(videoBackendSetDetailTextures, &arg);
#else
	gfx_detail_textures_enabled = (bool)texDetail;
#endif
}

s32 videoCreateFramebuffer(u32 w, u32 h, s32 upscale, s32 autoresize)
{
#ifdef __vita__
	struct VideoBackendFramebufferArg arg = {
		.width = w,
		.height = h,
		.upscale = upscale,
		.autoresize = autoresize,
	};
	gfx_vita_backend_run_sync(videoBackendCreateFramebuffer, &arg);
	return arg.result;
#else
	return gfx_create_framebuffer(w, h, upscale, autoresize);
#endif
}

void videoSetMSAA(const s32 msaa)
{
	vidMSAA = msaa;
#ifdef __vita__
	struct VideoBackendIntArg arg = { vidMSAA, 0 };
	gfx_vita_backend_run_sync(videoBackendSetMSAA, &arg);
#else
	gfx_msaa_level = (u32)vidMSAA;
#endif
}

void videoSetVsync(const s32 vsync)
{
#ifdef __vita__
	struct VideoBackendIntArg arg = { vsync, 0 };
	gfx_vita_backend_run_sync(videoBackendSetVsync, &arg);
	vidVsync = arg.result ? vsync : 0;
#else
	vidVsync = wmAPI->set_swap_interval(vsync) ? vsync : 0;
#endif

	if (vidVsync == 0 && vidFramerateLimit == 0) {
		// cap FPS if there's no vsync to prevent the game from exploding
		videoSetFramerateLimit(VIDEO_MAX_FPS);
	}
}

void videoSetFramerateLimit(const s32 limit)
{
	vidFramerateLimit = (vidVsync == 0 && limit == 0) ? VIDEO_MAX_FPS : limit;
#ifdef __vita__
	struct VideoBackendIntArg arg = { vidFramerateLimit, 0 };
	gfx_vita_backend_run_sync(videoBackendSetFramerateLimit, &arg);
#else
	wmAPI->set_target_fps(vidFramerateLimit);
#endif
}

void videoSetDisplayFPS(const s32 displayfps)
{
	vidDisplayFPS = displayfps;
}

void videoSetFramebuffer(s32 target)
{
#ifdef __vita__
	struct VideoBackendFramebufferArg arg = { .target = target };
	gfx_vita_backend_run_sync(videoBackendSetFramebuffer, &arg);
#else
	return gfx_set_framebuffer(target, 1.f);
#endif
}

void videoResetFramebuffer(void)
{
#ifdef __vita__
	gfx_vita_backend_run_sync(videoBackendResetFramebuffer, NULL);
#else
	return gfx_reset_framebuffer();
#endif
}

s32 videoFramebuffersSupported(void)
{
	return gfx_framebuffers_enabled;
}

void videoResizeFramebuffer(s32 target, u32 w, u32 h, s32 upscale, s32 autoresize)
{
#ifdef __vita__
	struct VideoBackendFramebufferArg arg = {
		.target = target,
		.width = w,
		.height = h,
		.upscale = upscale,
		.autoresize = autoresize,
	};
	gfx_vita_backend_run_sync(videoBackendResizeFramebuffer, &arg);
#else
	gfx_resize_framebuffer(target, w, h, upscale, autoresize);
#endif
}

void videoCopyFramebuffer(s32 dst, s32 src, s32 left, s32 top)
{
	// assume immediate copies always read the front buffer
#ifdef __vita__
	struct VideoBackendFramebufferArg arg = {
		.target = dst,
		.src = src,
		.left = left,
		.top = top,
	};
	gfx_vita_backend_run_sync(videoBackendCopyFramebuffer, &arg);
#else
	gfx_copy_framebuffer(dst, src, left, top, false);
#endif
}

void videoResetTextureCache(void)
{
#ifdef __vita__
	gfx_vita_backend_run_sync(videoBackendResetTextureCache, NULL);
#else
	gfx_texture_cache_clear();
#endif
}

void videoFreeCachedTexture(const void *texptr)
{
#ifdef __vita__
	struct VideoBackendTextureRangeArg arg = { texptr, NULL };
	gfx_vita_backend_run_sync(videoBackendFreeCachedTexture, &arg);
#else
	gfx_texture_cache_delete(texptr);
#endif
}

void videoFreeCachedTextures(const void *start, const void *end)
{
#ifdef __vita__
	struct VideoBackendTextureRangeArg arg = { start, end };
	gfx_vita_backend_run_sync(videoBackendFreeCachedTextures, &arg);
#else
	gfx_texture_cache_delete_range(start, end);
#endif
}

void videoShutdown(void)
{
#ifdef __vita__
	gfx_vita_backend_run_sync(videoBackendDestroy, NULL);
	gfx_vita_backend_shutdown();
#endif
	free(vidModes);
}

PD_CONSTRUCTOR static void videoConfigInit(void)
{
	configRegisterInt("Video.DefaultFullscreen", &vidFullscreen, 0, 1);
	configRegisterInt("Video.DefaultMaximize", &vidMaximize, 0, 1);
	configRegisterInt("Video.DefaultWidth", &vidWidth, 0, 32767);
	configRegisterInt("Video.DefaultHeight", &vidHeight, 0, 32767);
	configRegisterInt("Video.ExclusiveFullscreen", &vidFullscreenExclusive, 0, 1);
	configRegisterInt("Video.CenterWindow", &vidCenter, 0, 1);
	configRegisterInt("Video.AllowHiDpi", &vidAllowHiDpi, 0, 1);
	configRegisterInt("Video.VSync", &vidVsync, -1, 10);
	configRegisterInt("Video.FramebufferEffects", &vidFramebuffers, 0, 1);
	configRegisterInt("Video.FramerateLimit", &vidFramerateLimit, 0, VIDEO_MAX_FPS);
	configRegisterInt("Video.DisplayFPS", &vidDisplayFPS, 0, 1);
	configRegisterFloat("Video.DisplayFPSInterval", &vidDisplayFPSInterval, 0.01f, 32.f);
	configRegisterInt("Video.MSAA", &vidMSAA, 1, 16);
	configRegisterInt("Video.TextureFilter", &texFilter, 0, 2);
	configRegisterInt("Video.TextureFilter2D", &texFilter2D, 0, 1);
	configRegisterInt("Video.DetailTextures", &texDetail, 0, 1);
	configRegisterInt("Video.MipmapFilter", &texMipmapFilter, 0, 2);
	configRegisterInt("Video.AnisotropicFilter", &texAnisotropicFilter, 0, 16);
}
