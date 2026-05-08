// Ascent input-overlay OBS source.
//
// Reads a shared-memory snapshot of the user's keyboard/mouse state written
// by the desktop app (apps/desktop/src-tauri/src/input_overlay/) and renders
// a visualizer that's composited into the recording. Invisible to the
// player because compositing happens inside the OBS render pipeline.
//
// This file is a scaffold: it opens the shmem, verifies the layout, and
// renders a single colored rectangle that switches between two colors based
// on whether the W key is currently pressed. End-to-end IPC validation only.
// The real visualizer (half-keyboard + mouse) is built on top of this.

#include <obs-module.h>
#include <util/platform.h>

#include <stdbool.h>

#include <windows.h>

#include "ascent-input-overlay-shmem.h"

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("ascent-input-overlay", "en-US")

#define DEFAULT_WIDTH          400u
#define DEFAULT_HEIGHT         120u
#define DEFAULT_MIN_VISIBLE_MS 50u

struct ascent_input_overlay {
	obs_source_t *src;
	HANDLE        mapping;
	const ascent_input_overlay_state_t *state; // mapped, read-only view

	uint32_t width;
	uint32_t height;
	/// Minimum time (ms) a key/button stays "visible" after press, even if
	/// it released between two render calls.
	uint32_t min_visible_ms;
};

static const char *ascent_input_overlay_get_name(void *unused)
{
	UNUSED_PARAMETER(unused);
	return "Ascent Input Overlay";
}

static void ascent_input_overlay_get_defaults(obs_data_t *settings)
{
	obs_data_set_default_int(settings, "width", DEFAULT_WIDTH);
	obs_data_set_default_int(settings, "height", DEFAULT_HEIGHT);
	obs_data_set_default_int(settings, "min_visible_ms", DEFAULT_MIN_VISIBLE_MS);
}

static void ascent_input_overlay_update(void *data, obs_data_t *settings)
{
	struct ascent_input_overlay *ctx = data;
	long long w = obs_data_get_int(settings, "width");
	long long h = obs_data_get_int(settings, "height");
	long long m = obs_data_get_int(settings, "min_visible_ms");

	ctx->width  = w > 0 ? (uint32_t)w : DEFAULT_WIDTH;
	ctx->height = h > 0 ? (uint32_t)h : DEFAULT_HEIGHT;
	ctx->min_visible_ms = m > 0 ? (uint32_t)m : DEFAULT_MIN_VISIBLE_MS;
}

static bool open_shmem(struct ascent_input_overlay *ctx)
{
	HANDLE m = OpenFileMappingW(FILE_MAP_READ, FALSE,
	                            ASCENT_INPUT_OVERLAY_SHMEM_NAME);
	if (!m) {
		blog(LOG_WARNING,
		     "[ascent-input-overlay] OpenFileMappingW failed (err=%lu); "
		     "is the desktop app running?",
		     GetLastError());
		return false;
	}

	void *view = MapViewOfFile(m, FILE_MAP_READ, 0, 0,
	                           sizeof(ascent_input_overlay_state_t));
	if (!view) {
		blog(LOG_WARNING,
		     "[ascent-input-overlay] MapViewOfFile failed (err=%lu)",
		     GetLastError());
		CloseHandle(m);
		return false;
	}

	const ascent_input_overlay_state_t *s = view;
	if (s->magic != ASCENT_INPUT_OVERLAY_MAGIC) {
		blog(LOG_WARNING,
		     "[ascent-input-overlay] magic mismatch: got 0x%08x, "
		     "expected 0x%08x",
		     s->magic, ASCENT_INPUT_OVERLAY_MAGIC);
		UnmapViewOfFile(view);
		CloseHandle(m);
		return false;
	}
	if (s->version != ASCENT_INPUT_OVERLAY_VERSION) {
		blog(LOG_WARNING,
		     "[ascent-input-overlay] version mismatch: got %u, "
		     "expected %u",
		     s->version, ASCENT_INPUT_OVERLAY_VERSION);
		UnmapViewOfFile(view);
		CloseHandle(m);
		return false;
	}

	ctx->mapping = m;
	ctx->state = s;
	blog(LOG_INFO, "[ascent-input-overlay] connected to shmem");
	return true;
}

static void close_shmem(struct ascent_input_overlay *ctx)
{
	if (ctx->state) {
		UnmapViewOfFile(ctx->state);
		ctx->state = NULL;
	}
	if (ctx->mapping) {
		CloseHandle(ctx->mapping);
		ctx->mapping = NULL;
	}
}

static void *ascent_input_overlay_create(obs_data_t *settings,
                                         obs_source_t *source)
{
	struct ascent_input_overlay *ctx =
		bzalloc(sizeof(struct ascent_input_overlay));
	ctx->src = source;

	ascent_input_overlay_update(ctx, settings);

	// Best-effort: if the desktop app isn't up yet, the source still
	// exists but renders a "disconnected" color until it can connect.
	open_shmem(ctx);

	return ctx;
}

static void ascent_input_overlay_destroy(void *data)
{
	struct ascent_input_overlay *ctx = data;
	close_shmem(ctx);
	bfree(ctx);
}

static uint32_t ascent_input_overlay_get_width(void *data)
{
	struct ascent_input_overlay *ctx = data;
	return ctx->width;
}

static uint32_t ascent_input_overlay_get_height(void *data)
{
	struct ascent_input_overlay *ctx = data;
	return ctx->height;
}

// Solid-color helper: draw a filled rect at the source's full dimensions.
static void draw_solid(uint32_t rgba, uint32_t width, uint32_t height)
{
	gs_effect_t   *solid = obs_get_base_effect(OBS_EFFECT_SOLID);
	gs_eparam_t   *color = gs_effect_get_param_by_name(solid, "color");
	gs_technique_t *tech = gs_effect_get_technique(solid, "Solid");

	struct vec4 c;
	vec4_from_rgba(&c, rgba);

	gs_effect_set_vec4(color, &c);

	gs_technique_begin(tech);
	gs_technique_begin_pass(tech, 0);
	gs_draw_sprite(0, 0, width, height);
	gs_technique_end_pass(tech);
	gs_technique_end(tech);
}

static void ascent_input_overlay_render(void *data, gs_effect_t *effect)
{
	UNUSED_PARAMETER(effect);

	struct ascent_input_overlay *ctx = data;

	// If the shmem isn't open, try once more — the desktop app may have
	// started after the source was created. If still no good, paint a
	// dim diagnostic color so the user can tell the source exists but
	// isn't receiving data.
	if (!ctx->state) {
		if (!open_shmem(ctx)) {
			draw_solid(0xFF333333, ctx->width, ctx->height);
			return;
		}
	}

	const ascent_input_overlay_state_t *s = ctx->state;
	const uint64_t now = GetTickCount64();

	// Visibility rule: pressed right now, OR pressed within the last
	// `min_visible_ms` so that taps shorter than a frame don't flash by
	// invisibly. KeyIndex values <64 live in `keys[0]`.
	const uint64_t w_bit = 1ULL << ASCENT_KEY_W;
	const bool w_pressed_now = (s->keys[0] & w_bit) != 0;
	const uint64_t w_last = s->key_last_press_tick[ASCENT_KEY_W];
	const bool w_recently = w_last != 0 && (now - w_last) < ctx->min_visible_ms;
	const bool w_visible = w_pressed_now || w_recently;

	// Magenta when W is registered, otherwise a darker grey background.
	const uint32_t color = w_visible ? 0xFFFF00FF : 0xFF555555;
	draw_solid(color, ctx->width, ctx->height);
}

struct obs_source_info ascent_input_overlay_info = {
	.id           = "ascent_input_overlay",
	.type         = OBS_SOURCE_TYPE_INPUT,
	.output_flags = OBS_SOURCE_VIDEO | OBS_SOURCE_CUSTOM_DRAW,
	.get_name     = ascent_input_overlay_get_name,
	.create       = ascent_input_overlay_create,
	.destroy      = ascent_input_overlay_destroy,
	.update       = ascent_input_overlay_update,
	.get_defaults = ascent_input_overlay_get_defaults,
	.get_width    = ascent_input_overlay_get_width,
	.get_height   = ascent_input_overlay_get_height,
	.video_render = ascent_input_overlay_render,
	.icon_type    = OBS_ICON_TYPE_CUSTOM,
};

bool obs_module_load(void)
{
	obs_register_source(&ascent_input_overlay_info);
	blog(LOG_INFO, "[ascent-input-overlay] module loaded");
	return true;
}

void obs_module_unload(void)
{
	blog(LOG_INFO, "[ascent-input-overlay] module unloaded");
}
