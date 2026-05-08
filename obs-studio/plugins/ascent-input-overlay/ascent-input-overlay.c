// Ascent input-overlay OBS source.
//
// Reads a shared-memory snapshot of the user's keyboard/mouse state written
// by the desktop app (apps/desktop/src-tauri/src/input_overlay/) and renders
// a half-keyboard + mouse visualizer that's composited into the recording.
// Invisible to the player because compositing happens inside OBS, not on
// their monitor.

#include <obs-module.h>
#include <util/platform.h>
#include <graphics/matrix4.h>

#include <stdbool.h>
#include <stdint.h>

#include <windows.h>

#include "ascent-input-overlay-shmem.h"

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("ascent-input-overlay", "en-US")

#define DEFAULT_WIDTH          260u
#define DEFAULT_HEIGHT         120u
#define DEFAULT_MIN_VISIBLE_MS 50u

// Native layout dimensions. All key/mouse positions below are in this
// coordinate space; the renderer scales to the configured source size.
#define LAYOUT_WIDTH  260.0f
#define LAYOUT_HEIGHT 120.0f

// Color palette. Format is 0xAABBGGRR — when packed little-endian the bytes
// land as R, G, B, A which is what `vec4_from_rgba` expects.
#define COLOR_PANEL_BG     0xE01E1E22u  // dark panel, slightly translucent
#define COLOR_KEY_OFF      0xFF2E2E32u  // dim key, distinct from panel
#define COLOR_KEY_ON       0xFFF050C0u  // Ascent purple (R=C0,G=50,B=F0)
#define COLOR_MOUSE_BODY   0xFF26262Au  // mouse silhouette (slightly darker)
#define COLOR_BUTTON_OFF   0xFF3A3A40u  // distinct from body so buttons read
#define COLOR_BUTTON_ON    0xFFF050C0u

// Corner radii (native pixel space).
#define KEY_RADIUS         3.0f
#define MOUSE_BODY_RADIUS 22.0f
#define BUTTON_RADIUS      6.0f
#define SIDE_BUTTON_RADIUS 2.0f
#define PANEL_RADIUS       8.0f

struct ascent_input_overlay {
	obs_source_t *src;
	HANDLE        mapping;
	const ascent_input_overlay_state_t *state;

	uint32_t width;
	uint32_t height;
	uint32_t min_visible_ms;
};

// ===== Effect (loaded once at module load) =====

static gs_effect_t *g_effect       = NULL;
static gs_eparam_t *g_param_color  = NULL;
static gs_eparam_t *g_param_size   = NULL;
static gs_eparam_t *g_param_radius = NULL;

// ===== Layout tables =====

struct key_rect {
	uint8_t key_index; // matches state.rs KeyIndex
	float   x, y, w, h;
};

// Half keyboard layout — Esc + numbers, then four letter rows ending at the
// modifier/space row. Indices match the KeyIndex enum in
// apps/desktop/src-tauri/src/input_overlay/state.rs.
static const struct key_rect KEY_LAYOUT[] = {
	// Row 1: Esc + 1..6
	{  0,   6.0f,   8.0f, 18.0f, 18.0f }, // Escape
	{  1,  26.0f,   8.0f, 18.0f, 18.0f }, // Num1
	{  2,  46.0f,   8.0f, 18.0f, 18.0f }, // Num2
	{  3,  66.0f,   8.0f, 18.0f, 18.0f }, // Num3
	{  4,  86.0f,   8.0f, 18.0f, 18.0f }, // Num4
	{  5, 106.0f,   8.0f, 18.0f, 18.0f }, // Num5
	{  6, 126.0f,   8.0f, 18.0f, 18.0f }, // Num6
	// Row 2: Tab + Q W E R T
	{ 14,   6.0f,  28.0f, 28.0f, 18.0f }, // Tab
	{ 15,  36.0f,  28.0f, 18.0f, 18.0f }, // Q
	{ 16,  56.0f,  28.0f, 18.0f, 18.0f }, // W
	{ 17,  76.0f,  28.0f, 18.0f, 18.0f }, // E
	{ 18,  96.0f,  28.0f, 18.0f, 18.0f }, // R
	{ 19, 116.0f,  28.0f, 18.0f, 18.0f }, // T
	// Row 3: Caps + A S D F G
	{ 28,   6.0f,  48.0f, 32.0f, 18.0f }, // CapsLock
	{ 29,  40.0f,  48.0f, 18.0f, 18.0f }, // A
	{ 30,  60.0f,  48.0f, 18.0f, 18.0f }, // S
	{ 31,  80.0f,  48.0f, 18.0f, 18.0f }, // D
	{ 32, 100.0f,  48.0f, 18.0f, 18.0f }, // F
	{ 33, 120.0f,  48.0f, 18.0f, 18.0f }, // G
	// Row 4: Shift + Z X C V
	{ 41,   6.0f,  68.0f, 40.0f, 18.0f }, // ShiftLeft
	{ 42,  48.0f,  68.0f, 18.0f, 18.0f }, // Z
	{ 43,  68.0f,  68.0f, 18.0f, 18.0f }, // X
	{ 44,  88.0f,  68.0f, 18.0f, 18.0f }, // C
	{ 45, 108.0f,  68.0f, 18.0f, 18.0f }, // V
	// Row 5: Ctrl + Alt + Space
	{ 53,   6.0f,  88.0f, 24.0f, 18.0f }, // ControlLeft
	{ 55,  32.0f,  88.0f, 24.0f, 18.0f }, // Alt
	{ 56,  58.0f,  88.0f, 76.0f, 18.0f }, // Space
};

#define KEY_LAYOUT_COUNT (sizeof(KEY_LAYOUT) / sizeof(KEY_LAYOUT[0]))

struct mouse_rect {
	uint8_t bit_index; // bit position into mouse_buttons
	float   x, y, w, h;
	float   radius;
};

// Mouse cluster: rounded body with three top buttons (LMB / scroll / RMB)
// and two thin side buttons. Body drawn first, buttons composited on top.
// Positioned right after the keyboard with a small gap.
#define MOUSE_BODY_X 156.0f
#define MOUSE_BODY_Y  10.0f
#define MOUSE_BODY_W  92.0f
#define MOUSE_BODY_H 100.0f

static const struct mouse_rect MOUSE_LAYOUT[] = {
	// Top buttons. Inner corners (where LMB meets scroll, etc.) get
	// rounded too — visually fine because the body shows through behind.
	{ 0, 161.0f,  16.0f, 38.0f, 40.0f, BUTTON_RADIUS }, // LMB
	{ 2, 201.0f,  18.0f, 12.0f, 36.0f, BUTTON_RADIUS }, // MMB / scroll wheel
	{ 1, 215.0f,  16.0f, 28.0f, 40.0f, BUTTON_RADIUS }, // RMB
	// Side buttons (left edge of mouse body).
	{ 3, 156.0f,  46.0f,  4.0f, 11.0f, SIDE_BUTTON_RADIUS }, // Back
	{ 4, 156.0f,  60.0f,  4.0f, 11.0f, SIDE_BUTTON_RADIUS }, // Forward
};

#define MOUSE_LAYOUT_COUNT (sizeof(MOUSE_LAYOUT) / sizeof(MOUSE_LAYOUT[0]))

// ===== Source lifecycle =====

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

// ===== Drawing =====

// Draw a rounded rectangle using the SDF effect. Anti-aliased edges,
// alpha-blended over whatever's underneath.
static void draw_rounded(uint32_t rgba, float x, float y, float w, float h,
                         float radius)
{
	if (!g_effect)
		return;

	struct vec4 c;
	vec4_from_rgba(&c, rgba);

	struct vec2 sz;
	sz.x = w;
	sz.y = h;

	gs_matrix_push();
	gs_matrix_translate3f(x, y, 0.0f);

	gs_effect_set_vec4(g_param_color, &c);
	gs_effect_set_vec2(g_param_size, &sz);
	gs_effect_set_float(g_param_radius, radius);

	while (gs_effect_loop(g_effect, "Draw")) {
		gs_draw_sprite(0, 0, (uint32_t)w, (uint32_t)h);
	}

	gs_matrix_pop();
}

// Visibility rule: pressed right now, OR pressed within the last
// `min_visible_ms` so sub-frame taps don't disappear.
static bool is_visible(bool pressed_now, uint64_t last_press_tick,
                       uint64_t now, uint32_t min_visible_ms)
{
	if (pressed_now)
		return true;
	if (last_press_tick == 0)
		return false;
	return (now - last_press_tick) < (uint64_t)min_visible_ms;
}

static void ascent_input_overlay_render(void *data, gs_effect_t *effect)
{
	UNUSED_PARAMETER(effect);

	struct ascent_input_overlay *ctx = data;

	if (!ctx->state) {
		if (!open_shmem(ctx)) {
			draw_rounded(0xC0333338u, 0.0f, 0.0f,
			             (float)ctx->width, (float)ctx->height,
			             8.0f);
			return;
		}
	}

	const ascent_input_overlay_state_t *s = ctx->state;
	const uint64_t now = GetTickCount64();
	const uint32_t window = ctx->min_visible_ms;

	// Scale the native layout to the configured source size.
	gs_matrix_push();
	gs_matrix_scale3f((float)ctx->width / LAYOUT_WIDTH,
	                  (float)ctx->height / LAYOUT_HEIGHT, 1.0f);

	// No panel background — surrounding area stays transparent so the
	// game shows through. Keys and mouse silhouette are drawn opaque
	// so they remain readable against any game background.

	// Mouse body (drawn first so buttons sit on top).
	draw_rounded(COLOR_MOUSE_BODY, MOUSE_BODY_X, MOUSE_BODY_Y, MOUSE_BODY_W,
	             MOUSE_BODY_H, MOUSE_BODY_RADIUS);

	// Keys.
	for (size_t i = 0; i < KEY_LAYOUT_COUNT; i++) {
		const struct key_rect *k = &KEY_LAYOUT[i];
		const uint8_t  idx   = k->key_index;
		const size_t   chunk = idx / 64;
		const uint64_t bit   = 1ULL << (idx % 64);

		const bool pressed_now =
			chunk < 4 && (s->keys[chunk] & bit) != 0;
		const uint64_t tick = idx < ASCENT_KEY_TICK_SLOTS
		                              ? s->key_last_press_tick[idx]
		                              : 0;

		const bool on = is_visible(pressed_now, tick, now, window);
		draw_rounded(on ? COLOR_KEY_ON : COLOR_KEY_OFF, k->x, k->y,
		             k->w, k->h, KEY_RADIUS);
	}

	// Mouse buttons.
	for (size_t i = 0; i < MOUSE_LAYOUT_COUNT; i++) {
		const struct mouse_rect *m = &MOUSE_LAYOUT[i];
		const uint8_t  bit  = m->bit_index;
		const uint32_t mask = 1u << bit;

		const bool pressed_now = (s->mouse_buttons & mask) != 0;
		const uint64_t tick = bit < ASCENT_MOUSE_TICK_SLOTS
		                              ? s->mouse_last_press_tick[bit]
		                              : 0;

		const bool on = is_visible(pressed_now, tick, now, window);
		draw_rounded(on ? COLOR_BUTTON_ON : COLOR_BUTTON_OFF, m->x,
		             m->y, m->w, m->h, m->radius);
	}

	gs_matrix_pop();
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
	char *path = obs_module_file("ascent-input-overlay.effect");
	if (!path) {
		blog(LOG_ERROR,
		     "[ascent-input-overlay] effect file not found in module data");
		return false;
	}

	obs_enter_graphics();
	g_effect = gs_effect_create_from_file(path, NULL);
	obs_leave_graphics();
	bfree(path);

	if (!g_effect) {
		blog(LOG_ERROR,
		     "[ascent-input-overlay] failed to compile effect");
		return false;
	}

	g_param_color  = gs_effect_get_param_by_name(g_effect, "color");
	g_param_size   = gs_effect_get_param_by_name(g_effect, "size");
	g_param_radius = gs_effect_get_param_by_name(g_effect, "radius");

	obs_register_source(&ascent_input_overlay_info);
	blog(LOG_INFO, "[ascent-input-overlay] module loaded");
	return true;
}

void obs_module_unload(void)
{
	if (g_effect) {
		obs_enter_graphics();
		gs_effect_destroy(g_effect);
		obs_leave_graphics();
		g_effect = NULL;
	}
	blog(LOG_INFO, "[ascent-input-overlay] module unloaded");
}
