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
#include <string.h>

#include <windows.h>

#include "ascent-input-overlay-shmem.h"

// Public-domain TrueType rasterizer. We include the implementation here so
// the plugin doesn't need a separate .c file for it.
#define STB_TRUETYPE_IMPLEMENTATION
#define STBTT_STATIC
#include "stb_truetype.h"

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
//
// Look: no surrounding panel — only the keys and mouse silhouette occupy
// pixels. Each key/button has its own opaque-ish fill so it reads against
// any game background; outlines are soft white; pressed state fills purple.
#define COLOR_KEY_FILL     0x801A1A20u  // ~50% dark — semi-transparent at rest
#define COLOR_KEY_OUTLINE  0xC0F0F0F0u  // soft white outline
#define COLOR_KEY_ON_FILL  0xFFF050C0u  // Ascent purple, fully opaque on press
#define COLOR_LABEL        0xF0F0F0F0u  // labels, just shy of full white
#define COLOR_MOUSE_FILL   0x701A1A20u  // mouse body slightly more see-through
#define COLOR_MOUSE_OUTLINE 0xC0F0F0F0u
#define COLOR_BUTTON_FILL  0x802A2A30u  // distinct from body so buttons read
#define COLOR_BUTTON_ON_FILL 0xFFF050C0u

// Corner radii and stroke widths (native pixel space).
#define KEY_RADIUS         3.0f
#define MOUSE_BODY_RADIUS 22.0f
#define BUTTON_RADIUS      6.0f
#define SIDE_BUTTON_RADIUS 2.0f
#define PANEL_RADIUS      10.0f
#define OUTLINE_THICKNESS  1.2f

struct ascent_input_overlay {
	obs_source_t *src;
	HANDLE        mapping;
	const ascent_input_overlay_state_t *state;

	uint32_t width;
	uint32_t height;
	uint32_t min_visible_ms;
};

// ===== Effect (loaded once at module load) =====

static gs_effect_t *g_effect           = NULL;
static gs_eparam_t *g_param_color      = NULL;
static gs_eparam_t *g_param_size       = NULL;
static gs_eparam_t *g_param_radius     = NULL;
static gs_eparam_t *g_param_outline    = NULL;

// Glyph atlas for anti-aliased label text. Rasterized at module load from
// a system TTF (Segoe UI by default) and uploaded as a single-channel R8
// texture sampled by the glyph effect with a tint color.
// 512x512 = 256KB, enough room for the full ASCII printable range at 32px
// with 2x oversampling. (256x256 ran out of room and silently lost glyphs.)
#define ATLAS_W           512
#define ATLAS_H           512
#define ATLAS_FONT_PX     32.0f       // glyph rasterization size
#define ATLAS_FIRST_CHAR  32          // ' '
#define ATLAS_NUM_CHARS   96          // through '~'

static gs_effect_t  *g_glyph_effect       = NULL;
static gs_eparam_t  *g_glyph_param_image  = NULL;
static gs_eparam_t  *g_glyph_param_color  = NULL;
static gs_eparam_t  *g_glyph_param_src    = NULL;
static gs_texture_t *g_atlas_tex          = NULL;
static stbtt_packedchar g_packed[ATLAS_NUM_CHARS];
static float        g_font_ascent_px      = 0.0f;
static float        g_font_descent_px     = 0.0f;

// ===== Layout tables =====

struct key_rect {
	uint8_t     key_index; // matches state.rs KeyIndex
	float       x, y, w, h;
	const char *label; // single-char or short string; NULL = no label
};

// Half keyboard layout — Esc + numbers, then four letter rows ending at the
// modifier/space row. Indices match the KeyIndex enum in
// apps/desktop/src-tauri/src/input_overlay/state.rs.
static const struct key_rect KEY_LAYOUT[] = {
	// Row 1: Esc + 1..6
	{  0,   6.0f,   8.0f, 18.0f, 18.0f, NULL }, // Escape (icon-shaped key)
	{  1,  26.0f,   8.0f, 18.0f, 18.0f, "1" },
	{  2,  46.0f,   8.0f, 18.0f, 18.0f, "2" },
	{  3,  66.0f,   8.0f, 18.0f, 18.0f, "3" },
	{  4,  86.0f,   8.0f, 18.0f, 18.0f, "4" },
	{  5, 106.0f,   8.0f, 18.0f, 18.0f, "5" },
	{  6, 126.0f,   8.0f, 18.0f, 18.0f, "6" },
	// Row 2: Tab + Q W E R T
	{ 14,   6.0f,  28.0f, 28.0f, 18.0f, NULL }, // Tab
	{ 15,  36.0f,  28.0f, 18.0f, 18.0f, "Q" },
	{ 16,  56.0f,  28.0f, 18.0f, 18.0f, "W" },
	{ 17,  76.0f,  28.0f, 18.0f, 18.0f, "E" },
	{ 18,  96.0f,  28.0f, 18.0f, 18.0f, "R" },
	{ 19, 116.0f,  28.0f, 18.0f, 18.0f, "T" },
	// Row 3: Caps + A S D F G
	{ 28,   6.0f,  48.0f, 32.0f, 18.0f, NULL }, // CapsLock
	{ 29,  40.0f,  48.0f, 18.0f, 18.0f, "A" },
	{ 30,  60.0f,  48.0f, 18.0f, 18.0f, "S" },
	{ 31,  80.0f,  48.0f, 18.0f, 18.0f, "D" },
	{ 32, 100.0f,  48.0f, 18.0f, 18.0f, "F" },
	{ 33, 120.0f,  48.0f, 18.0f, 18.0f, "G" },
	// Row 4: Shift + Z X C V
	{ 41,   6.0f,  68.0f, 40.0f, 18.0f, NULL }, // ShiftLeft
	{ 42,  48.0f,  68.0f, 18.0f, 18.0f, "Z" },
	{ 43,  68.0f,  68.0f, 18.0f, 18.0f, "X" },
	{ 44,  88.0f,  68.0f, 18.0f, 18.0f, "C" },
	{ 45, 108.0f,  68.0f, 18.0f, 18.0f, "V" },
	// Row 5: Ctrl + Alt + Space
	{ 53,   6.0f,  88.0f, 24.0f, 18.0f, NULL }, // ControlLeft
	{ 55,  32.0f,  88.0f, 24.0f, 18.0f, NULL }, // Alt
	{ 56,  58.0f,  88.0f, 76.0f, 18.0f, NULL }, // Space
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

// Draw a rounded rectangle using the SDF effect. `outline_thickness` 0 means
// filled; >0 draws only a stroke band of that width along the edge.
static void draw_shape(uint32_t rgba, float x, float y, float w, float h,
                       float radius, float outline_thickness)
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
	gs_effect_set_float(g_param_outline, outline_thickness);

	while (gs_effect_loop(g_effect, "Draw")) {
		gs_draw_sprite(0, 0, (uint32_t)w, (uint32_t)h);
	}

	gs_matrix_pop();
}

// Convenience: filled rounded rect.
static void draw_rounded(uint32_t rgba, float x, float y, float w, float h,
                         float radius)
{
	draw_shape(rgba, x, y, w, h, radius, 0.0f);
}

// Convenience: outlined rounded rect (no fill).
static void draw_outlined(uint32_t rgba, float x, float y, float w, float h,
                          float radius, float thickness)
{
	draw_shape(rgba, x, y, w, h, radius, thickness);
}

// Draw a single glyph from the atlas at (x, y) (top-left of the glyph's
// bounding box, NOT the baseline). `scale` multiplies the glyph's atlas-
// rasterized pixel size; 1.0 means render at the same size as the atlas.
static void draw_glyph(uint32_t rgba, float x, float y,
                       const stbtt_packedchar *pc, float scale)
{
	if (!g_glyph_effect || !g_atlas_tex || !pc)
		return;

	const float w = (float)(pc->x1 - pc->x0) * scale;
	const float h = (float)(pc->y1 - pc->y0) * scale;
	if (w <= 0.0f || h <= 0.0f)
		return;

	struct vec4 c;
	vec4_from_rgba(&c, rgba);

	struct vec4 src;
	src.x = (float)pc->x0 / (float)ATLAS_W;
	src.y = (float)pc->y0 / (float)ATLAS_H;
	src.z = (float)(pc->x1 - pc->x0) / (float)ATLAS_W;
	src.w = (float)(pc->y1 - pc->y0) / (float)ATLAS_H;

	gs_matrix_push();
	gs_matrix_translate3f(x, y, 0.0f);

	gs_effect_set_texture(g_glyph_param_image, g_atlas_tex);
	gs_effect_set_vec4(g_glyph_param_color, &c);
	gs_effect_set_vec4(g_glyph_param_src, &src);

	while (gs_effect_loop(g_glyph_effect, "Draw")) {
		gs_draw_sprite(0, 0, (uint32_t)w, (uint32_t)h);
	}

	gs_matrix_pop();
}

// Draw a horizontally + vertically centered text label at (cx, cy). `scale`
// is the multiplier from atlas pixels to layout units. Centering uses the
// actual painted glyph bounding boxes in the label (rather than typographic
// ascent/descent) so labels sit visually on the key center regardless of
// which characters they contain.
static void draw_label(uint32_t rgba, float cx, float cy, float scale,
                       const char *text)
{
	if (!text || !g_atlas_tex)
		return;

	// Measure visible extent in both axes by sweeping painted bboxes.
	float min_x = 0.0f, max_x = 0.0f;
	float min_y = 0.0f, max_y = 0.0f;
	float pen   = 0.0f;
	bool  first = true;
	for (const char *p = text; *p; p++) {
		unsigned int ch = (unsigned char)*p;
		if (ch < ATLAS_FIRST_CHAR || ch >= ATLAS_FIRST_CHAR + ATLAS_NUM_CHARS)
			continue;
		const stbtt_packedchar *pc = &g_packed[ch - ATLAS_FIRST_CHAR];
		const float left   = pen + pc->xoff * scale;
		const float right  = left + (float)(pc->x1 - pc->x0) * scale;
		const float top    = pc->yoff * scale;          // relative to baseline
		const float bottom = top + (float)(pc->y1 - pc->y0) * scale;
		if (first) {
			min_x = left;  max_x = right;
			min_y = top;   max_y = bottom;
			first = false;
		} else {
			if (left   < min_x) min_x = left;
			if (right  > max_x) max_x = right;
			if (top    < min_y) min_y = top;
			if (bottom > max_y) max_y = bottom;
		}
		pen += pc->xadvance * scale;
	}
	if (first) // empty string after filtering
		return;

	// Place baseline so the visual bbox is centered on (cx, cy).
	const float visible_w = max_x - min_x;
	const float baseline  = cy - (min_y + max_y) * 0.5f;
	float pen_x           = cx - (visible_w * 0.5f) - min_x;

	for (const char *p = text; *p; p++) {
		unsigned int ch = (unsigned char)*p;
		if (ch < ATLAS_FIRST_CHAR || ch >= ATLAS_FIRST_CHAR + ATLAS_NUM_CHARS)
			continue;
		const stbtt_packedchar *pc = &g_packed[ch - ATLAS_FIRST_CHAR];
		const float gx = pen_x + pc->xoff * scale;
		const float gy = baseline + pc->yoff * scale;
		draw_glyph(rgba, gx, gy, pc, scale);
		pen_x += pc->xadvance * scale;
	}
}

// Read a system TTF and rasterize a packed glyph atlas. Returns true on
// success; false leaves the plugin running without label rendering.
static bool load_font_atlas(void)
{
	// Try a couple of common Windows system fonts in priority order.
	static const wchar_t *FONT_PATHS[] = {
		L"C:\\Windows\\Fonts\\segoeuib.ttf", // Segoe UI Bold (better at small sizes)
		L"C:\\Windows\\Fonts\\segoeui.ttf",
		L"C:\\Windows\\Fonts\\arialbd.ttf",
		L"C:\\Windows\\Fonts\\arial.ttf",
	};

	uint8_t *ttf_data = NULL;
	DWORD    ttf_size = 0;
	for (size_t i = 0; i < sizeof(FONT_PATHS) / sizeof(FONT_PATHS[0]); i++) {
		HANDLE hf = CreateFileW(FONT_PATHS[i], GENERIC_READ, FILE_SHARE_READ,
		                        NULL, OPEN_EXISTING, 0, NULL);
		if (hf == INVALID_HANDLE_VALUE)
			continue;
		const DWORD size = GetFileSize(hf, NULL);
		if (size == 0 || size == INVALID_FILE_SIZE) {
			CloseHandle(hf);
			continue;
		}
		ttf_data = bmalloc(size);
		DWORD bytes_read = 0;
		const BOOL ok = ReadFile(hf, ttf_data, size, &bytes_read, NULL);
		CloseHandle(hf);
		if (ok && bytes_read == size) {
			ttf_size = size;
			break;
		}
		bfree(ttf_data);
		ttf_data = NULL;
	}

	if (!ttf_data) {
		blog(LOG_WARNING,
		     "[ascent-input-overlay] no system font found — labels disabled");
		return false;
	}

	uint8_t *atlas = bzalloc(ATLAS_W * ATLAS_H);
	stbtt_pack_context pack;
	if (!stbtt_PackBegin(&pack, atlas, ATLAS_W, ATLAS_H, 0, 1, NULL)) {
		bfree(atlas);
		bfree(ttf_data);
		blog(LOG_WARNING, "[ascent-input-overlay] stbtt_PackBegin failed");
		return false;
	}
	stbtt_PackSetOversampling(&pack, 2, 2); // softer AA

	const int packed_ok = stbtt_PackFontRange(&pack, ttf_data, 0, ATLAS_FONT_PX,
	                                          ATLAS_FIRST_CHAR, ATLAS_NUM_CHARS,
	                                          g_packed);
	stbtt_PackEnd(&pack);

	if (!packed_ok) {
		bfree(atlas);
		bfree(ttf_data);
		blog(LOG_WARNING,
		     "[ascent-input-overlay] failed to pack font into atlas");
		return false;
	}

	// Capture vertical metrics for vertical centering in draw_label.
	stbtt_fontinfo info;
	if (stbtt_InitFont(&info, ttf_data, 0)) {
		int ascent_unit, descent_unit, line_gap_unit;
		stbtt_GetFontVMetrics(&info, &ascent_unit, &descent_unit, &line_gap_unit);
		const float scale = stbtt_ScaleForPixelHeight(&info, ATLAS_FONT_PX);
		g_font_ascent_px  = (float)ascent_unit * scale;
		g_font_descent_px = (float)descent_unit * scale;
	}

	obs_enter_graphics();
	const uint8_t *atlas_data[1] = { atlas };
	g_atlas_tex = gs_texture_create(ATLAS_W, ATLAS_H, GS_R8, 1, atlas_data, 0);
	obs_leave_graphics();

	bfree(atlas);
	bfree(ttf_data);

	if (!g_atlas_tex) {
		blog(LOG_WARNING,
		     "[ascent-input-overlay] failed to upload glyph atlas to GPU");
		return false;
	}

	// Diagnostic: confirm a known glyph (uppercase 'W') has non-empty
	// metrics, so we can tell whether packing actually produced bitmaps.
	const stbtt_packedchar *w = &g_packed['W' - ATLAS_FIRST_CHAR];
	blog(LOG_INFO,
	     "[ascent-input-overlay] glyph atlas loaded "
	     "(W rect=[%d,%d..%d,%d] xadv=%.1f ascent=%.1f descent=%.1f)",
	     w->x0, w->y0, w->x1, w->y1, w->xadvance,
	     g_font_ascent_px, g_font_descent_px);
	return true;
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

	// No panel background — only the keys and mouse silhouette draw, so
	// the game shows through everywhere else. Each key carries its own
	// opaque fill so labels stay readable against any background.

	// Mouse body — fill then outline.
	draw_rounded(COLOR_MOUSE_FILL, MOUSE_BODY_X, MOUSE_BODY_Y,
	             MOUSE_BODY_W, MOUSE_BODY_H, MOUSE_BODY_RADIUS);
	draw_outlined(COLOR_MOUSE_OUTLINE, MOUSE_BODY_X, MOUSE_BODY_Y,
	              MOUSE_BODY_W, MOUSE_BODY_H, MOUSE_BODY_RADIUS,
	              OUTLINE_THICKNESS);

	// Keys: fill (off / on) → outline → label.
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

		draw_rounded(on ? COLOR_KEY_ON_FILL : COLOR_KEY_FILL,
		             k->x, k->y, k->w, k->h, KEY_RADIUS);
		draw_outlined(COLOR_KEY_OUTLINE, k->x, k->y, k->w, k->h,
		              KEY_RADIUS, OUTLINE_THICKNESS);

		// Anti-aliased label centered on the key. Atlas glyphs are 32px
		// tall; scale 0.2 yields ~6-layout-unit caps on 18-unit keys
		// (~33% of key height) — clean and uncrowded.
		if (k->label) {
			draw_label(COLOR_LABEL, k->x + k->w * 0.5f,
			           k->y + k->h * 0.5f, 0.2f, k->label);
		}
	}

	// Mouse buttons: fill + outline.
	for (size_t i = 0; i < MOUSE_LAYOUT_COUNT; i++) {
		const struct mouse_rect *m = &MOUSE_LAYOUT[i];
		const uint8_t  bit  = m->bit_index;
		const uint32_t mask = 1u << bit;

		const bool pressed_now = (s->mouse_buttons & mask) != 0;
		const uint64_t tick = bit < ASCENT_MOUSE_TICK_SLOTS
		                              ? s->mouse_last_press_tick[bit]
		                              : 0;
		const bool on = is_visible(pressed_now, tick, now, window);

		draw_rounded(on ? COLOR_BUTTON_ON_FILL : COLOR_BUTTON_FILL,
		             m->x, m->y, m->w, m->h, m->radius);
		draw_outlined(COLOR_MOUSE_OUTLINE, m->x, m->y, m->w, m->h,
		              m->radius, OUTLINE_THICKNESS);
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

static gs_effect_t *load_effect_file(const char *name)
{
	char *path = obs_module_file(name);
	if (!path) {
		blog(LOG_ERROR,
		     "[ascent-input-overlay] effect '%s' not found in module data",
		     name);
		return NULL;
	}
	obs_enter_graphics();
	gs_effect_t *e = gs_effect_create_from_file(path, NULL);
	obs_leave_graphics();
	bfree(path);
	if (!e) {
		blog(LOG_ERROR,
		     "[ascent-input-overlay] failed to compile effect '%s'", name);
	}
	return e;
}

bool obs_module_load(void)
{
	g_effect = load_effect_file("ascent-input-overlay.effect");
	if (!g_effect)
		return false;

	g_param_color   = gs_effect_get_param_by_name(g_effect, "color");
	g_param_size    = gs_effect_get_param_by_name(g_effect, "size");
	g_param_radius  = gs_effect_get_param_by_name(g_effect, "radius");
	g_param_outline = gs_effect_get_param_by_name(g_effect, "outline_thickness");

	g_glyph_effect = load_effect_file("ascent-input-overlay-glyph.effect");
	if (g_glyph_effect) {
		g_glyph_param_image = gs_effect_get_param_by_name(g_glyph_effect, "image");
		g_glyph_param_color = gs_effect_get_param_by_name(g_glyph_effect, "color");
		g_glyph_param_src   = gs_effect_get_param_by_name(g_glyph_effect, "src_rect");

		// Best-effort: if atlas loading fails, labels are skipped but
		// keys still render fine.
		(void)load_font_atlas();
	}

	obs_register_source(&ascent_input_overlay_info);
	blog(LOG_INFO, "[ascent-input-overlay] module loaded");
	return true;
}

void obs_module_unload(void)
{
	obs_enter_graphics();
	if (g_atlas_tex) {
		gs_texture_destroy(g_atlas_tex);
		g_atlas_tex = NULL;
	}
	if (g_glyph_effect) {
		gs_effect_destroy(g_glyph_effect);
		g_glyph_effect = NULL;
	}
	if (g_effect) {
		gs_effect_destroy(g_effect);
		g_effect = NULL;
	}
	obs_leave_graphics();
	blog(LOG_INFO, "[ascent-input-overlay] module unloaded");
}
