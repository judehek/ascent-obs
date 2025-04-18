use thiserror::Error;

#[derive(Error, Debug)]
pub enum ObsError {
    #[error("Invalid path provided")]
    InvalidPath,

    #[error("Failed to start ow-obs process: {0}")]
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

    #[error("Client is not running or already shut down")]
    NotRunning,

    #[error("IO error: {0}")]
    Io(#[from] std::io::Error),
    // Add other specific error types as needed
}