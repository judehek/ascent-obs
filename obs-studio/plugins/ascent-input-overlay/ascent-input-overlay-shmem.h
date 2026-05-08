// Reader-side definition of the Ascent input-overlay shared-memory snapshot.
//
// Must stay byte-for-byte identical to:
//   ascent-app/apps/desktop/src-tauri/src/input_overlay/state.rs
//
// Both sides assume x86-64, so naturally aligned 32/64-bit loads/stores are
// atomic at the hardware level. The reader does plain reads; the writer uses
// Rust atomics with Release ordering. Since the reader runs in a different
// process, no further synchronization is required: we only ever observe a
// consistent value per field, and torn states across fields self-correct
// within a frame.

#pragma once

#include <stdint.h>

#define ASCENT_INPUT_OVERLAY_MAGIC      0xA5CE0001u
#define ASCENT_INPUT_OVERLAY_VERSION    2u
#define ASCENT_INPUT_OVERLAY_SHMEM_NAME L"Local\\AscentInputOverlay"

#define ASCENT_KEY_TICK_SLOTS   96
#define ASCENT_MOUSE_TICK_SLOTS 5

typedef struct ascent_input_overlay_state {
	// offset   0
	uint32_t magic;
	uint32_t version;
	// offset   8
	uint64_t keys[4];
	// offset  40
	uint32_t mouse_buttons;
	uint32_t scroll_seq;
	int32_t  scroll_delta_y;
	int32_t  mouse_x;
	int32_t  mouse_y;
	uint32_t _reserved;
	// offset  64
	uint64_t key_last_press_tick[ASCENT_KEY_TICK_SLOTS];
	// offset 832
	uint64_t mouse_last_press_tick[ASCENT_MOUSE_TICK_SLOTS];
	// offset 872
	uint8_t  _reserved2[24];
} ascent_input_overlay_state_t;

_Static_assert(sizeof(ascent_input_overlay_state_t) == 896,
	"input overlay state layout mismatch with Rust producer");

// Convenience: bit positions within `mouse_buttons`.
#define ASCENT_MOUSE_BIT_LEFT    (1u << 0)
#define ASCENT_MOUSE_BIT_RIGHT   (1u << 1)
#define ASCENT_MOUSE_BIT_MIDDLE  (1u << 2)
#define ASCENT_MOUSE_BIT_BACK    (1u << 3)
#define ASCENT_MOUSE_BIT_FORWARD (1u << 4)

// KeyIndex values mirrored from state.rs. Only the ones the scaffold uses
// for visual tests are defined; the rest can be added as needed.
#define ASCENT_KEY_W 16
