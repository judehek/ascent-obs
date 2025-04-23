// src/lib.rs
pub mod types;
pub mod errors;
pub mod communication;
mod recorder;
mod config;
mod event_handler;

// Re-export main client, recorder, error type, and event receiver
pub use communication::ObsClient; // Keep ObsClient public if direct access is desired
pub use recorder::Recorder; // Export Recorder and the type alias
pub use config::RecordingConfig;
pub use errors::ObsError;
pub use recorder::query_machine_info;