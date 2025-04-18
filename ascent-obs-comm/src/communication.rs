use crate::types::{CommandRequest, SimpleCommandRequest, EventNotification}; // Assuming EventNotification is in types
use crate::errors::ObsError; // Assuming ObsError is in errors
use log::{debug, error, info, warn};
use serde::Serialize;
use std::path::Path;
use std::process::Stdio;
use tokio::io::{AsyncBufReadExt, AsyncWriteExt, BufReader};
use tokio::process::{Child, ChildStdin, ChildStdout, Command as TokioCommand};
use tokio::sync::{mpsc, oneshot};
use tokio::task::JoinHandle;

/// Handles communication with a running ow-obs.exe process.
#[derive(Debug)]
pub struct ObsClient {
    child: Child,
    writer_handle: JoinHandle<Result<(), ObsError>>,
    reader_handle: JoinHandle<()>,
    stderr_handle: JoinHandle<()>,
    command_sender: mpsc::Sender<String>,
    _shutdown_signal_sender: oneshot::Sender<()>,
}

// Public interface
impl ObsClient {
    /// Starts the ow-obs.exe process and establishes communication channels.
    pub async fn start(
        ow_obs_path: impl AsRef<Path>,
        channel_id: Option<&str>,
        buffer_size: usize,
    ) -> Result<(Self, mpsc::Receiver<Result<EventNotification, ObsError>>), ObsError>
    {
        let ow_obs_path_str = ow_obs_path
            .as_ref()
            .to_str()
            .ok_or(ObsError::InvalidPath)?;

        info!("Starting ow-obs process: {}", ow_obs_path_str);

        let mut command = TokioCommand::new(ow_obs_path_str);
        command
            .kill_on_drop(true)
            .stdin(Stdio::piped())
            .stdout(Stdio::piped())
            .stderr(Stdio::piped());

        if let Some(id) = channel_id {
             warn!("Channel ID provided ('{}'), but Rust client primarily uses stdio. Ensure ow-obs handles this.", id);
             command.arg("--channel").arg(id);
        } else {
            info!("Using stdio communication mode.");
        }

        let mut child = command.spawn().map_err(ObsError::ProcessStart)?;
        info!("ow-obs process started (PID: {:?})", child.id());

        let stdin = child.stdin.take().ok_or(ObsError::PipeError(
            "Failed to take stdin".to_string(),
        ))?;
        let stdout = child.stdout.take().ok_or(ObsError::PipeError(
            "Failed to take stdout".to_string(),
        ))?;
        let stderr = child.stderr.take().ok_or(ObsError::PipeError(
            "Failed to take stderr".to_string(),
        ))?;

        let (command_sender, command_receiver) = mpsc::channel::<String>(buffer_size);
        // CORRECTED TYPE for the channel
        let (event_sender, event_receiver) =
            mpsc::channel::<Result<EventNotification, ObsError>>(buffer_size);
        let (shutdown_sender, shutdown_receiver) = oneshot::channel::<()>();

        let writer_handle = spawn_writer_task(stdin, command_receiver, shutdown_receiver);
        // Pass the corrected sender type
        let reader_handle = spawn_reader_task(stdout, event_sender.clone());
        let stderr_handle = spawn_stderr_task(stderr);

        Ok((
            Self {
                child,
                writer_handle,
                reader_handle,
                stderr_handle,
                command_sender,
                _shutdown_signal_sender: shutdown_sender,
            },
            event_receiver,
        ))
    }

    /// Sends a command without a specific payload.
    pub async fn send_simple_command(
        &self,
        cmd_code: i32,
        identifier: Option<i32>,
    ) -> Result<(), ObsError> {
        let command = SimpleCommandRequest {
            cmd: cmd_code,
            identifier,
        };
        let json_string = serde_json::to_string(&command).map_err(ObsError::Serialization)?;
        self.send_json_string(json_string).await
    }

    /// Sends a command with a specific payload structure.
    pub async fn send_command<T: Serialize + std::fmt::Debug>( // Added Debug constraint for logging
        &self,
        cmd_code: i32,
        identifier: Option<i32>,
        payload: T,
    ) -> Result<(), ObsError> {
        let command = CommandRequest {
            cmd: cmd_code,
            identifier,
            payload,
        };
        // Log the command before serialization if needed (requires Debug on T)
        // debug!("Preparing to send command: {:?}", command);
        let json_string = serde_json::to_string(&command).map_err(ObsError::Serialization)?;
        self.send_json_string(json_string).await
    }

    /// Sends a pre-serialized JSON string command.
    async fn send_json_string(&self, json_string: String) -> Result<(), ObsError> {
        debug!("Sending command JSON: {}", json_string);
        self.command_sender
            .send(json_string)
            .await
            .map_err(|e| ObsError::CommandSend(format!("Command channel closed: {}", e)))?; // Include error detail
        Ok(())
    }

    /// Shuts down the ow-obs process and associated communication tasks gracefully.
    pub async fn shutdown(mut self) -> Result<(), ObsError> {
        info!("Shutting down ObsClient (PID: {:?})", self.child.id());

        drop(self._shutdown_signal_sender);
        drop(self.command_sender);

        match self.writer_handle.await {
            Ok(Ok(())) => info!("Writer task finished gracefully."),
            Ok(Err(e)) => error!("Writer task finished with error: {}", e),
            Err(e) => error!("Failed to join writer task: {}", e),
        }

        info!("Attempting to terminate ow-obs process...");
        if let Err(e) = self.child.kill().await {
             error!("Failed to kill ow-obs process: {}. It might have already exited.", e);
        } else {
             info!("Kill signal sent to ow-obs process.");
        }

        match self.child.wait().await {
            Ok(status) => info!("ow-obs process exited with status: {}", status),
            Err(e) => error!("Error waiting for ow-obs process exit: {}", e),
        }

        if let Err(e) = self.reader_handle.await {
            error!("Failed to join reader task: {}", e);
        } else {
             info!("Reader task finished.");
        }
        if let Err(e) = self.stderr_handle.await {
            error!("Failed to join stderr task: {}", e);
        } else {
             info!("Stderr task finished.");
        }

        info!("ObsClient shutdown complete.");
        Ok(())
    }
}

// --- Background Task Functions ---

fn spawn_writer_task(
    mut stdin: ChildStdin,
    mut command_receiver: mpsc::Receiver<String>,
    mut shutdown_receiver: oneshot::Receiver<()>,
) -> JoinHandle<Result<(), ObsError>> {
    tokio::spawn(async move {
        loop {
            tokio::select! {
                biased; // Prioritize shutdown check

                _ = &mut shutdown_receiver => {
                    info!("Writer task received shutdown signal.");
                    drop(stdin); // Close stdin gracefully
                    return Ok(());
                }
                command_opt = command_receiver.recv() => {
                    match command_opt {
                        Some(mut json_string) => {
                            if !json_string.ends_with('\n') {
                                json_string.push('\n');
                            }
                            debug!("Writer task sending: {}", json_string.trim_end());
                            if let Err(e) = stdin.write_all(json_string.as_bytes()).await {
                                error!("Writer task failed to write to stdin: {}", e);
                                drop(stdin);
                                return Err(ObsError::PipeError(format!("Write error: {}", e)));
                            }
                            if let Err(e) = stdin.flush().await {
                                 error!("Writer task failed to flush stdin: {}", e);
                                 drop(stdin);
                                 return Err(ObsError::PipeError(format!("Flush error: {}", e)));
                            }
                        }
                        None => {
                            info!("Writer task command channel closed. Exiting.");
                            drop(stdin);
                            return Ok(());
                        }
                    }
                }
            }
        }
    })
}

/// Task to read lines from process stdout, parse JSON events, and send them via mpsc channel.
fn spawn_reader_task(
    stdout: ChildStdout,
    event_sender: mpsc::Sender<Result<EventNotification, ObsError>>,
) -> JoinHandle<()> {
    tokio::spawn(async move {
        let mut reader = BufReader::new(stdout);
        let mut buffer = Vec::new(); // Use a byte buffer for reading

        loop {
            buffer.clear();
            // Read *some* data, not necessarily a full line
            match reader.read_until(b'}', &mut buffer).await {
                // Attempt to read until a '}' which likely terminates a JSON object.
                // This isn't foolproof if '}' appears inside strings, but better than read_line.
                // A more robust solution might involve a custom framing protocol or
                // expecting exactly one JSON per line from ow-obs.
                Ok(0) => {
                    info!("Reader task reached EOF on stdout. Exiting.");
                    break; // End of stream
                }
                Ok(bytes_read) => {
                    if bytes_read == 0 { // Should be caught by Ok(0) but belt-and-suspenders
                        continue;
                    }
                    // Convert buffer to string slice (handle potential UTF-8 errors)
                    match std::str::from_utf8(&buffer) {
                        Ok(data_str) => {
                            debug!("Reader task received data chunk: {}", data_str.trim());

                            // Use a streaming deserializer to handle multiple JSON objects
                            let stream =
                                serde_json::Deserializer::from_str(data_str).into_iter::<EventNotification>();

                            for result in stream {
                                match result {
                                    Ok(event) => {
                                        debug!("Parsed event: {:?}", event);
                                        // Send the successfully parsed event
                                        if event_sender.send(Ok(event)).await.is_err() {
                                            warn!("Reader task failed to send event: receiver dropped.");
                                            // Break outer loop if receiver is gone
                                            return; // Exit task immediately
                                        }
                                    }
                                    Err(e) => {
                                        // Check if the error is because of trailing data (indicating more objects)
                                        // or a genuine syntax error within an object.
                                        if e.is_syntax() || e.is_eof() { // is_eof might mean incomplete object read
                                             // Often indicates incomplete object in the buffer or actual syntax error.
                                             // If it's consistently `trailing characters`, it means the previous
                                             // object parsed okay, and we just haven't read the *next* full object yet.
                                             // We might need more sophisticated buffering/parsing if ow-obs
                                             // doesn't guarantee newline delimiters.
                                             // For now, log and try to continue with the next read.
                                            if !e.is_eof() { // Don't log partial reads as errors yet
                                                error!(
                                                    "Reader task JSON stream parsing error: {} (Data chunk: '{}')",
                                                    e, data_str.trim()
                                                );
                                                 // Send the error if it's not just EOF/incomplete
                                                 let err_res = Err(ObsError::Deserialization(format!(
                                                     "Streaming JSON parse error: {} on data: {}", e, data_str.trim()
                                                 )));
                                                if event_sender.send(err_res).await.is_err() {
                                                     warn!("Reader task failed to send parsing error: receiver dropped.");
                                                     return; // Exit task immediately
                                                 }
                                            } else {
                                                debug!("Reader task encountered EOF during stream parse, likely incomplete object in buffer.");
                                            }


                                        } else {
                                             // Other errors (IO, data format issues within a valid JSON structure)
                                            error!("Reader task JSON data error: {} (Data chunk: '{}')", e, data_str.trim());
                                            let err_res = Err(ObsError::Deserialization(format!(
                                                "JSON data error: {} on data: {}", e, data_str.trim()
                                            )));
                                            if event_sender.send(err_res).await.is_err() {
                                                warn!("Reader task failed to send parsing error: receiver dropped.");
                                                return; // Exit task immediately
                                            }
                                        }
                                        // Don't break the outer loop on *all* parse errors,
                                        // just log and try the next read, unless it's an IO error below.
                                    }
                                }
                            } // end for loop over stream results
                        }
                        Err(e) => {
                             error!("Reader task received invalid UTF-8 data: {}", e);
                             let err_res = Err(ObsError::PipeError(format!("Invalid UTF-8 data: {}", e)));
                            if event_sender.send(err_res).await.is_err() {
                                warn!("Reader task failed to send UTF-8 error: receiver dropped.");
                            }
                             // Decide whether to break or continue after UTF-8 error
                             // Breaking might be safer if data corruption is suspected.
                             break;
                        }
                    }
                }
                Err(e) => {
                    error!("Reader task failed to read from stdout: {}", e);
                    let err_res = Err(ObsError::PipeError(format!("Read error: {}", e)));
                    if event_sender.send(err_res).await.is_err() {
                        warn!("Reader task failed to send read error: receiver dropped.");
                    }
                    break; // Definitively exit on IO read error
                }
            }
        }
        info!("Reader task finished.");
    })
}

/// Task to read and log lines from process stderr.
fn spawn_stderr_task(stderr: tokio::process::ChildStderr) -> JoinHandle<()> {
    tokio::spawn(async move {
        let mut reader = BufReader::new(stderr);
        let mut line_buf = String::new();
        info!("Stderr logging task started.");
        loop {
            line_buf.clear();
            match reader.read_line(&mut line_buf).await {
                Ok(0) => {
                    info!("Stderr task reached EOF. Exiting.");
                    break;
                }
                Ok(_) => {
                    let trimmed_line = line_buf.trim();
                    if !trimmed_line.is_empty() {
                        warn!("[ow-obs stderr] {}", trimmed_line);
                    }
                }
                Err(e) => {
                    error!("Stderr task failed to read: {}", e);
                    break;
                }
            }
        }
        info!("Stderr task finished.");
    })
}