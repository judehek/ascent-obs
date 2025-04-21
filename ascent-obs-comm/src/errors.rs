use thiserror::Error;

#[derive(Error, Debug)]
pub enum ObsError {
    #[error("Invalid path provided")]
    InvalidPath,

    #[error("Failed to start ascent-obs process: {0}")]
    ProcessStart(#[source] std::io::Error),

    #[error("Pipe communication error: {0}")]
    PipeError(String), // Include more context

    #[error("Serialization error: {0}")]
    Serialization(#[from] serde_json::Error),

    #[error("Deserialization error: {0}")]
    Deserialization(String), // Include context/line

    #[error("Failed to send command to writer task: {0}")]
    CommandSend(String),

    #[error("Received invalid event data: {0}")]
    InvalidEventData(String),

    #[error("OwObs process exited unexpectedly with status: {0:?}")]
    ProcessExited(Option<std::process::ExitStatus>),

    #[error("Invalid configuration provided: {0}")] // Added for Builder
    Configuration(String),

    #[error("Client is not running or already shut down")]
    NotRunning,

    #[error("IO error: {0}")]
    Io(#[from] std::io::Error),
    
    #[error("A recording session is already active")]
    AlreadyRecording,

    #[error("No recording session is currently active")]
    NotRecording,

    #[error("Critical error, this should never happen: {0}")]
    ShouldNotHappen(String),
}