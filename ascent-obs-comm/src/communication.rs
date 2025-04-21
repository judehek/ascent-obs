use crate::types::{CommandRequest, SimpleCommandRequest, EventNotification}; // Assuming EventNotification is in types
use crate::errors::ObsError;
use log::{debug, error, info, trace, warn};
use serde::{Deserialize, Serialize};
use serde_json::Deserializer;
use std::path::Path;
use std::process::Stdio;
use tokio::io::{AsyncBufReadExt, AsyncReadExt, AsyncWriteExt, BufReader};
use tokio::process::{Child, ChildStdin, ChildStdout, Command as TokioCommand};
use tokio::sync::{mpsc, oneshot};
use tokio::task::JoinHandle;

/// Handles communication with a running ascent-obs.exe process.
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
    /// Starts the ascent-obs.exe process and establishes communication channels.
    pub async fn start(
        ascent_obs_path: impl AsRef<Path>,
        channel_id: Option<&str>,
        buffer_size: usize,
    ) -> Result<(Self, mpsc::Receiver<Result<EventNotification, ObsError>>), ObsError>
    {
        let ascent_obs_path_str = ascent_obs_path
            .as_ref()
            .to_str()
            .ok_or(ObsError::InvalidPath)?;

        info!("Starting ascent-obs process: {}", ascent_obs_path_str);

        let mut command = TokioCommand::new(ascent_obs_path_str);
        command
            .kill_on_drop(true)
            .stdin(Stdio::piped())
            .stdout(Stdio::piped())
            .stderr(Stdio::piped());

        if let Some(id) = channel_id {
             warn!("Channel ID provided ('{}'), but Rust client primarily uses stdio. Ensure ascent-obs handles this.", id);
             command.arg("--channel").arg(id);
        } else {
            info!("Using stdio communication mode.");
        }

        let mut child = command.spawn().map_err(ObsError::ProcessStart)?;
        info!("ascent-obs process started (PID: {:?})", child.id());

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
        debug!("Preparing to send command: {:?}", command);
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

    /// Shuts down the ascent-obs process and associated communication tasks gracefully.
    pub async fn shutdown(mut self) -> Result<(), ObsError> {
        info!("Shutting down ObsClient (PID: {:?})", self.child.id());

        drop(self._shutdown_signal_sender);
        drop(self.command_sender);

        match self.writer_handle.await {
            Ok(Ok(())) => info!("Writer task finished gracefully."),
            Ok(Err(e)) => error!("Writer task finished with error: {}", e),
            Err(e) => error!("Failed to join writer task: {}", e),
        }

        info!("Attempting to terminate ascent-obs process...");
        if let Err(e) = self.child.kill().await {
             error!("Failed to kill ascent-obs process: {}. It might have already exited.", e);
        } else {
             info!("Kill signal sent to ascent-obs process.");
        }

        match self.child.wait().await {
            Ok(status) => info!("ascent-obs process exited with status: {}", status),
            Err(e) => error!("Error waiting for ascent-obs process exit: {}", e),
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

/// Task to read bytes from stdout, manage a buffer, parse JSON stream using iterator, and send events.
fn spawn_reader_task(
    stdout: ChildStdout,
    event_sender: mpsc::Sender<Result<EventNotification, ObsError>>,
) -> JoinHandle<()> {
    tokio::spawn(async move {
        let mut reader = BufReader::new(stdout);
        let mut read_buf = vec![0u8; 4096]; // Buffer for reading directly from stdout
        let mut process_buf = Vec::with_capacity(8192); // Buffer to accumulate data for parsing

        loop {
            // Read data from stdout into read_buf
            match reader.read(&mut read_buf).await {
                Ok(0) => {
                    info!("Reader task reached EOF on stdout. Exiting.");
                    if !process_buf.is_empty() {
                        warn!(
                            "Reader task exited with unprocessed data in buffer ({} bytes): '{}'",
                            process_buf.len(),
                            String::from_utf8_lossy(&process_buf).chars().take(100).collect::<String>() // Log start of remaining data
                        );
                        // Consider one last parse attempt on process_buf here if necessary
                    }
                    break; // End of stream
                }
                Ok(n) => {
                    // Append newly read data to the processing buffer
                    process_buf.extend_from_slice(&read_buf[..n]);
                }
                Err(e) => {
                    error!("Reader task failed to read from stdout: {}", e);
                    let err_res = Err(ObsError::PipeError(format!("Read error: {}", e)));
                    let _ = event_sender.send(err_res).await; // Ignore send error if stopping
                    break; // Exit on IO read error
                }
            }

            // --- Process the accumulated buffer ---
            let mut bytes_consumed_in_buffer = 0;

            // Create a Deserializer for the current processing buffer
            // Loop as long as we can successfully deserialize items or until an error occurs
            loop {
                 // Work on a slice starting after the already consumed bytes
                let current_slice = &process_buf[bytes_consumed_in_buffer..];
                if current_slice.is_empty() {
                    break; // Nothing more to process in the buffer for now
                }

                // Create the Deserializer from the slice
                let de = Deserializer::from_slice(current_slice);
                // Get the streaming iterator
                let mut iter = de.into_iter::<EventNotification>();

                match iter.next() {
                    Some(Ok(event)) => {
                        // Successfully parsed an event
                        debug!("Parsed event: {:?}", event);
                        // Update total consumed bytes based on the iterator's offset AFTER success
                        bytes_consumed_in_buffer += iter.byte_offset();

                        // Send the event
                        if event_sender.send(Ok(event)).await.is_err() {
                            warn!("Reader task failed to send event: receiver dropped.");
                            return; // Exit task if receiver is gone
                        }
                        // Continue inner loop to try and parse the *next* object from the remaining buffer slice
                    }
                    Some(Err(e)) => {
                        // Deserialization failed for an item in the stream
                        let error_offset = iter.byte_offset(); // Offset *within the current_slice* where error occurred

                        if e.is_eof() {
                            // EOF error means the slice ended mid-object. This is expected.
                            // We need more data. Break the inner loop.
                            // Don't increment bytes_consumed_in_buffer for EOF.
                            debug!("Parser needs more data (EOF encountered).");
                            break; // Break inner loop to read more data
                        } else {
                            // Syntax or Data error. Log it.
                            error!(
                                "JSON parse error: {} (at offset {} within current parse attempt, absolute {}) Context: '{}'",
                                e,
                                error_offset,
                                bytes_consumed_in_buffer + error_offset, // Approximate absolute offset
                                String::from_utf8_lossy(current_slice).chars().take(80).collect::<String>()
                            );

                            // Send the error notification
                            let err_res = Err(ObsError::Deserialization(format!("JSON Parse Error: {}", e)));
                            if event_sender.send(err_res).await.is_err() {
                                warn!("Reader task failed to send parsing error: receiver dropped.");
                                return; // Exit task
                            }

                            // Consume the buffer *up to the error* to avoid retrying the bad data.
                            // This might discard subsequent valid objects if the error was recoverable,
                            // but it's safer than getting stuck in a loop on bad data.
                            bytes_consumed_in_buffer += error_offset;

                            // Break the inner loop; we'll drain the consumed part and then read more data.
                            break;
                        }
                    }
                    None => {
                        // The iterator finished *cleanly* for the current slice (no more items found, no EOF error)
                        // This usually means the buffer ended exactly after a complete object or contained only whitespace.
                        // Consume the entire slice we tried to process.
                        bytes_consumed_in_buffer = process_buf.len();
                        break; // Break inner loop, nothing more to parse in this buffer load
                    }
                }
            } // End inner loop (processing current buffer content)

            // After trying to process the buffer, drain the consumed part
            if bytes_consumed_in_buffer > 0 {
                debug!("Draining {} bytes from process buffer", bytes_consumed_in_buffer);
                process_buf.drain(..bytes_consumed_in_buffer);
            }
            // Reset for next pass (though drain does this implicitly)
            // bytes_consumed_in_buffer = 0;

        } // End outer loop (reading from stdout)
        info!("Reader task finished.");
    })
}


fn spawn_stderr_task(stderr: tokio::process::ChildStderr) -> JoinHandle<()> {
   // Stderr task remains the same
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
                        warn!("[ascent-obs stderr] {}", trimmed_line);
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