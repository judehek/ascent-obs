// src/types/enums.rs
use serde::{Serialize, Deserialize};
use serde_repr::{Deserialize_repr, Serialize_repr};

#[derive(Serialize_repr, Deserialize_repr, Debug, Clone, Copy, PartialEq, Eq)]
#[repr(i32)] // Or u8, u32, etc. - Choose the integer type expected by the C++ side. i32 is a common choice.
pub enum RecorderType {
    // Using integer representation as defined in C++
    Video = 1,
    Replay = 2,
    Streaming = 3,
}

impl Default for RecorderType {
    fn default() -> Self {
        RecorderType::Video
    }
}

// OBS Flip Type (Based on common usage, might need adjustment if specific values differ)
// Defined here for use in GameSourceSettings, though not directly in protocol.h
#[derive(Serialize_repr, Deserialize_repr, Debug, Clone, Copy, PartialEq, Eq)]
#[repr(u8)] // Example: Assuming u8 is appropriate here
pub enum ObsFlipType {
    None = 0,
    FlipHorizontal = 1,
    FlipVertical = 2,
}

// Add other relevant enums here if discovered later