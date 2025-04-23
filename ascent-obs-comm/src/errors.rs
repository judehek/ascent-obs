// src/errors.rs
use thiserror::Error;

// Add #[derive(Clone)] here
#[derive(Error, Debug, Clone)] // <--- Add Clone
pub enum ObsError {
    #[error("Invalid path provided")]
    InvalidPath,

    // Change std::io::Error to String
    #[error("Failed to start ascent-obs process: {0}")]
    ProcessStart(String), // <--- Changed from std::io::Error

    #[error("Pipe communication error: {0}")]
    PipeError(String),

    // serde_json::Error is not Clone, so use String representation
    #[error("Serialization error: {0}")]
    Serialization(String), // <--- Changed from serde_json::Error

    #[error("Deserialization error: {0}")]
    Deserialization(String),

    #[error("Failed to send command to writer task: {0}")]
    CommandSend(String),

    #[error("Received invalid event data: {0}")]
    InvalidEventData(String),

    // ExitStatus doesn't implement Clone, store Option<i32> (exit code)
    #[error("OwObs process exited unexpectedly with status code: {0:?}")]
    ProcessExited(Option<i32>), // <--- Changed from Option<std::process::ExitStatus>

    #[error("Invalid configuration provided: {0}")]
    Configuration(String),

    #[error("Client is not running or already shut down")]
    NotRunning,

    // Change std::io::Error to String
    #[error("IO error: {0}")]
    Io(String), // <--- Changed from std::io::Error

    #[error("A recording session is already active")]
    AlreadyRecording,

    #[error("No recording session is currently active")]
    NotRecording,

    #[error("Critical error, this should never happen: {0}")]
    ShouldNotHappen(String),

    #[error("Internal error occured: {0}")]
    InternalError(String),

    #[error("Timeout error: {0}")]
    Timeout(String),

    #[error("Event manager error: {0:?}")]
    EventManagerError(String),
}

// Add From implementations manually where #[from] was removed or changed

impl From<serde_json::Error> for ObsError {
    fn from(err: serde_json::Error) -> Self {
        ObsError::Serialization(err.to_string())
    }
}

// Keep original From<std::io::Error> for convenience where needed,
// but convert it to the String variant immediately.
// Note: You might need to decide which variant (Io or ProcessStart)
// is the most appropriate default for a generic io::Error.
impl From<std::io::Error> for ObsError {
    fn from(err: std::io::Error) -> Self {
        ObsError::Io(err.to_string()) // Or PipeError, or InternalError? Choose one.
    }
}

// Add From for the send error if needed
impl<T> From<std::sync::mpsc::SendError<T>> for ObsError {
     fn from(err: std::sync::mpsc::SendError<T>) -> Self {
         ObsError::InternalError(format!("Channel send error: {}", err))
     }
}

// Add From for TrySendError if needed (might occur with sync_channel)
impl<T> From<std::sync::mpsc::TrySendError<T>> for ObsError {
     fn from(err: std::sync::mpsc::TrySendError<T>) -> Self {
         ObsError::InternalError(format!("Channel try_send error: {}", err))
     }
}