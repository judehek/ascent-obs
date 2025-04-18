// src/lib.rs
pub mod types;
pub mod errors;
pub mod communication;
pub mod recorder; // Add this line

// Re-export main client, recorder, error type, and event receiver
pub use communication::ObsClient; // Keep ObsClient public if direct access is desired
pub use recorder::{Recorder, EventReceiver}; // Export Recorder and the type alias
pub use errors::ObsError;