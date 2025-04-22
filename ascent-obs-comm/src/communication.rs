use crate::types::{CommandRequest, SimpleCommandRequest, EventNotification};
use crate::errors::ObsError;
use log::{debug, error, info, trace, warn};
use serde::{Deserialize, Serialize};
use serde_json::Deserializer;
use std::io::{BufRead, BufReader, Read, Write}; // Use std::io
use std::path::Path;
use std::process::{Child, ChildStdin, ChildStdout, ChildStderr, Command, Stdio}; // Use std::process
use std::sync::mpsc; // Use std::sync::mpsc
use std::thread::{self, JoinHandle}; // Use std::thread
use std::time::Duration;

/// Handles communication with a running ascent-obs.exe process (Synchronous Version).
///
/// WARNING: Methods on this client (start, send*, shutdown) are blocking.
/// Ensure this client is managed appropriately, potentially in its own thread,
/// to avoid blocking critical parts of your application (like a UI thread).
#[derive(Debug)]
pub struct ObsClient {
    child: Child,
    writer_handle: JoinHandle<Result<(), ObsError>>,
    reader_handle: JoinHandle<()>,
    stderr_handle: JoinHandle<()>,
    command_sender: mpsc::SyncSender<String>, // Use std mpsc Sender
    // No explicit shutdown signal needed; dropping command_sender signals the writer.
}

// Public interface
impl ObsClient {
    /// Starts the ascent-obs.exe process and establishes communication channels (Synchronous).
    ///
    /// This function blocks until the process is started and threads are spawned.
    pub fn start(
        ascent_obs_path: impl AsRef<Path>,
        channel_id: Option<&str>,
        buffer_size: usize, // Buffer size for the command channel
    ) -> Result<(Self, mpsc::Receiver<Result<EventNotification, ObsError>>), ObsError> // Use std mpsc Receiver
    {
        let ascent_obs_path_str = ascent_obs_path
            .as_ref()
            .to_str()
            .ok_or(ObsError::InvalidPath)?;

        info!("(Sync) Starting ascent-obs process: {}", ascent_obs_path_str);

        let mut command = Command::new(ascent_obs_path_str); // Use std::process::Command
        command
            // kill_on_drop is implicit for std::process::Child
            .stdin(Stdio::piped())
            .stdout(Stdio::piped())
            .stderr(Stdio::piped());

        if let Some(id) = channel_id {
             warn!("(Sync) Channel ID provided ('{}'), but Rust client primarily uses stdio. Ensure ascent-obs handles this.", id);
             command.arg("--channel").arg(id);
        } else {
            info!("(Sync) Using stdio communication mode.");
        }

        let mut child = command.spawn().map_err(ObsError::ProcessStart)?;
        info!("(Sync) ascent-obs process started (PID: {:?})", child.id());

        // take stdin/out/err (synchronous)
        let stdin = child.stdin.take().ok_or(ObsError::PipeError(
            "Failed to take stdin".to_string(),
        ))?;
        let stdout = child.stdout.take().ok_or(ObsError::PipeError(
            "Failed to take stdout".to_string(),
        ))?;
        let stderr = child.stderr.take().ok_or(ObsError::PipeError(
            "Failed to take stderr".to_string(),
        ))?;

        // Create standard mpsc channels
        let (command_sender, command_receiver) = mpsc::sync_channel::<String>(buffer_size); // Use sync_channel for bounded buffer
        let (event_sender, event_receiver) =
            mpsc::sync_channel::<Result<EventNotification, ObsError>>(buffer_size); // Use sync_channel

        // Spawn standard OS threads
        let writer_handle = spawn_writer_thread(stdin, command_receiver);
        let reader_handle = spawn_reader_thread(stdout, event_sender.clone());
        let stderr_handle = spawn_stderr_thread(stderr);

        Ok((
            Self {
                child,
                writer_handle,
                reader_handle,
                stderr_handle,
                command_sender,
                // No shutdown sender field needed
            },
            event_receiver, // Return the standard mpsc receiver
        ))
    }

    /// Sends a command without a specific payload (Synchronous).
    /// Blocks if the command channel buffer is full.
    pub fn send_simple_command(
        &self,
        cmd_code: i32,
        identifier: Option<i32>,
    ) -> Result<(), ObsError> {
        let command = SimpleCommandRequest {
            cmd: cmd_code,
            identifier,
        };
        let json_string = serde_json::to_string(&command).map_err(ObsError::Serialization)?;
        self.send_json_string(json_string)
    }

    /// Sends a command with a specific payload structure (Synchronous).
    /// Blocks if the command channel buffer is full.
    pub fn send_command<T: Serialize + std::fmt::Debug>(
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
        debug!("(Sync) Preparing to send command: {:?}", command);
        let json_string = serde_json::to_string(&command).map_err(ObsError::Serialization)?;
        info!("(Sync) Preparing to send command: {:?}", json_string);
        self.send_json_string(json_string)
    }

    /// Sends a pre-serialized JSON string command (Synchronous).
    /// Blocks if the command channel buffer is full.
    fn send_json_string(&self, json_string: String) -> Result<(), ObsError> {
        debug!("(Sync) Sending command JSON: {}", json_string);
        // send() might block if the buffer is full
        self.command_sender
            .send(json_string)
            .map_err(|e| ObsError::CommandSend(format!("(Sync) Command channel closed or error: {}", e)))?;
        Ok(())
    }

    /// Shuts down the ascent-obs process and associated communication threads gracefully (Synchronous).
    ///
    /// This function BLOCKS until all threads have joined and the process has exited.
    pub fn shutdown(mut self) -> Result<(), ObsError> {
        info!("(Sync) Shutting down ObsClient (PID: {:?})", self.child.id());

        // Drop the command sender. This signals the writer thread to exit
        // because command_receiver.recv() will return Err.
        drop(self.command_sender);

        // Wait for the writer thread to finish. This blocks.
        match self.writer_handle.join() {
            Ok(Ok(())) => info!("(Sync) Writer thread finished gracefully."),
            Ok(Err(e)) => error!("(Sync) Writer thread finished with error: {}", e),
            Err(panic_payload) => error!("(Sync) Writer thread panicked: {:?}", panic_payload), // Join returns Err on panic
        }

        // Attempt to terminate the process (optional, writer closing stdin might be enough)
        info!("(Sync) Attempting to terminate ascent-obs process...");
        if let Err(e) = self.child.kill() {
             // Log error, but proceed. Maybe it already exited.
             error!("(Sync) Failed to kill ascent-obs process: {}. It might have already exited.", e);
        } else {
             info!("(Sync) Kill signal sent to ascent-obs process.");
        }

        // Wait for the process to exit. This blocks.
        match self.child.wait() {
            Ok(status) => info!("(Sync) ascent-obs process exited with status: {}", status),
            Err(e) => error!("(Sync) Error waiting for ascent-obs process exit: {}", e),
        }

        // Wait for the reader and stderr threads to finish. They should exit
        // when the process stdout/stderr streams are closed after process termination.
        // These join calls block.
        if let Err(panic_payload) = self.reader_handle.join() {
            error!("(Sync) Reader thread panicked: {:?}", panic_payload);
        } else {
             info!("(Sync) Reader thread finished.");
        }
        if let Err(panic_payload) = self.stderr_handle.join() {
            error!("(Sync) Stderr thread panicked: {:?}", panic_payload);
        } else {
             info!("(Sync) Stderr thread finished.");
        }

        info!("(Sync) ObsClient shutdown complete.");
        Ok(())
    }
}

// --- Background Thread Functions ---

fn spawn_writer_thread(
    mut stdin: ChildStdin,
    command_receiver: mpsc::Receiver<String>, // Standard mpsc Receiver
) -> JoinHandle<Result<(), ObsError>> { // Standard JoinHandle
    thread::spawn(move || { // Use std::thread::spawn
        info!("(Sync) Writer thread started.");
        // Loop while the channel is open. recv() blocks until a message arrives or the channel closes.
        while let Ok(mut json_string) = command_receiver.recv() {
            if !json_string.ends_with('\n') {
                json_string.push('\n');
            }
            debug!("(Sync) Writer thread sending: {}", json_string.trim_end());

            // write_all and flush are blocking operations
            if let Err(e) = stdin.write_all(json_string.as_bytes()) {
                error!("(Sync) Writer thread failed to write to stdin: {}", e);
                // Don't drop stdin here explicitly, let the thread exit naturally
                return Err(ObsError::PipeError(format!("Write error: {}", e)));
            }
             if let Err(e) = stdin.flush() {
                 error!("(Sync) Writer thread failed to flush stdin: {}", e);
                 return Err(ObsError::PipeError(format!("Flush error: {}", e)));
             }

            // Synchronous sleep
            thread::sleep(Duration::from_secs(1));
            debug!("(Sync) Writer thread completed 1-second sleep after send");
        }

        // recv() returned Err, meaning the sender was dropped (shutdown signal)
        info!("(Sync) Writer thread command channel closed. Exiting.");
        // Dropping stdin happens implicitly when the thread scope ends
        Ok(())
    })
}

fn spawn_reader_thread(
    stdout: ChildStdout,
    event_sender: mpsc::SyncSender<Result<EventNotification, ObsError>>, // Standard mpsc Sender
) -> JoinHandle<()> { // Standard JoinHandle
    thread::spawn(move || { // Use std::thread::spawn
        info!("(Sync) Reader thread started.");
        // Use std::io::BufReader
        let mut reader = BufReader::new(stdout);
        let mut read_buf = vec![0u8; 4096]; // Buffer for reading directly from stdout
        let mut process_buf = Vec::with_capacity(8192); // Buffer to accumulate data for parsing

        loop {
            // read() is a blocking call from std::io::Read
            match reader.read(&mut read_buf) {
                Ok(0) => {
                    info!("(Sync) Reader thread reached EOF on stdout. Exiting.");
                     if !process_buf.is_empty() {
                        warn!(
                            "(Sync) Reader thread exited with unprocessed data in buffer ({} bytes): '{}'",
                            process_buf.len(),
                            String::from_utf8_lossy(&process_buf).chars().take(100).collect::<String>()
                        );
                    }
                    break; // End of stream
                }
                Ok(n) => {
                    // Append newly read data to the processing buffer
                    process_buf.extend_from_slice(&read_buf[..n]);
                }
                Err(e) => {
                    error!("(Sync) Reader thread failed to read from stdout: {}", e);
                    let err_res = Err(ObsError::PipeError(format!("Read error: {}", e)));
                    // Try sending the error, ignore if channel is closed
                    let _ = event_sender.send(err_res);
                    break; // Exit on IO read error
                }
            }

            // --- Process the accumulated buffer ---
            let mut bytes_consumed_in_buffer = 0;
            loop {
                let current_slice = &process_buf[bytes_consumed_in_buffer..];
                 if current_slice.is_empty() {
                    break; // Nothing more to process in the buffer for now
                }

                let de = Deserializer::from_slice(current_slice);
                let mut iter = de.into_iter::<EventNotification>(); // serde_json is sync

                match iter.next() {
                    Some(Ok(event)) => {
                        bytes_consumed_in_buffer += iter.byte_offset();
                        debug!("(Sync) Reader thread successfully parsed event: {:?}", event);
                        // send() can block if the event channel buffer is full
                        if event_sender.send(Ok(event)).is_err() {
                            warn!("(Sync) Reader thread failed to send event: receiver dropped.");
                            // Exit thread if we can't send events anymore
                            return;
                        }
                        debug!("(Sync) Reader task successfully sent event to MPSC channel");
                    }
                    Some(Err(e)) => {
                        let error_offset = iter.byte_offset();
                        if e.is_eof() {
                            // Incomplete JSON object in buffer, need more data
                             debug!("(Sync) Parser needs more data (EOF encountered).");
                            break; // Break inner loop to read more data
                        } else {
                            // Syntax or other parse error
                             error!(
                                "(Sync) JSON parse error: {} (at offset {} within current parse attempt, absolute {}) Context: '{}'",
                                e,
                                error_offset,
                                bytes_consumed_in_buffer + error_offset,
                                String::from_utf8_lossy(current_slice).chars().take(80).collect::<String>()
                            );
                            let err_res = Err(ObsError::Deserialization(format!("JSON Parse Error: {}", e)));
                             // Try sending the error, ignore if channel is closed
                            if event_sender.send(err_res).is_err() {
                                warn!("(Sync) Reader thread failed to send parsing error: receiver dropped.");
                                return; // Exit thread
                            }
                            // Consume data up to the error point
                            bytes_consumed_in_buffer += error_offset;
                            break; // Break inner loop, try reading more after draining
                        }
                    }
                     None => {
                        // Iter finished cleanly for this slice
                        bytes_consumed_in_buffer = process_buf.len();
                        break; // Break inner loop
                    }
                }
            } // End inner processing loop

            // Drain consumed bytes
            if bytes_consumed_in_buffer > 0 {
                debug!("Draining {} bytes from process buffer", bytes_consumed_in_buffer);
                process_buf.drain(..bytes_consumed_in_buffer);
            }
        } // End outer read loop

        info!("(Sync) Reader thread finished.");
        // Sender is dropped implicitly when the thread scope ends
    })
}

fn spawn_stderr_thread(stderr: ChildStderr) -> JoinHandle<()> { // Standard JoinHandle
    thread::spawn(move || { // Use std::thread::spawn
        // Use std::io::BufReader
        let mut reader = BufReader::new(stderr);
        let mut line_buf = String::new();
        info!("(Sync) Stderr logging thread started.");
        loop {
            line_buf.clear();
            // read_line() is a blocking call from std::io::BufRead
            match reader.read_line(&mut line_buf) {
                Ok(0) => {
                    info!("(Sync) Stderr thread reached EOF. Exiting.");
                    break;
                }
                Ok(_) => {
                    let trimmed_line = line_buf.trim();
                    if !trimmed_line.is_empty() {
                        warn!("[ascent-obs stderr] {}", trimmed_line); // Log remains the same
                    }
                }
                Err(e) => {
                    error!("(Sync) Stderr thread failed to read: {}", e);
                    break;
                }
            }
        }
        info!("(Sync) Stderr thread finished.");
    })
}