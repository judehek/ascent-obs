use crate::event_handler::{EventManager};
use crate::types::{CommandRequest, SimpleCommandRequest, EventNotification};
use crate::errors::ObsError;
use log::{debug, error, info, trace, warn};
use serde::de::DeserializeOwned;
use serde::{Deserialize, Serialize};
use serde_json::Deserializer;
use std::io::{BufRead, BufReader, Read, Write};
use std::os::windows::process::CommandExt;
use std::path::Path;
use std::process::{Child, ChildStdin, ChildStdout, ChildStderr, Command, Stdio};
use std::sync::mpsc;
use std::thread::{self, JoinHandle};
use std::time::Duration;

/// Handles communication with a running ascent-obs.exe process.
///
/// This client provides both synchronous command sending and the ability to
/// wait for responses to specific commands or register callbacks for event types.
pub struct ObsClient {
    child: Child,
    writer_handle: JoinHandle<Result<(), ObsError>>,
    reader_handle: JoinHandle<()>,
    stderr_handle: JoinHandle<()>,
    command_sender: mpsc::SyncSender<String>,
    
    // New field: Event manager for handling responses and callbacks
    event_manager: EventManager,
}

const CREATE_NO_WINDOW: u32 = 0x08000000;

// Public interface
impl ObsClient {
    /// Starts the ascent-obs.exe process and establishes communication channels.
    ///
    /// This function blocks until the process is started and threads are spawned.
    pub fn start(
        ascent_obs_path: impl AsRef<Path>,
        channel_id: Option<&str>,
        buffer_size: usize,
    ) -> Result<Self, ObsError> {
        let ascent_obs_path_str = ascent_obs_path
            .as_ref()
            .to_str()
            .ok_or(ObsError::InvalidPath)?;

        info!("Starting ascent-obs process: {}", ascent_obs_path_str);

        let mut command = Command::new(ascent_obs_path_str);
        command
            .creation_flags(CREATE_NO_WINDOW)
            .stdin(Stdio::piped())
            .stdout(Stdio::piped())
            .stderr(Stdio::piped());

        if let Some(id) = channel_id {
            warn!("Channel ID provided ('{}'), but Rust client primarily uses stdio. Ensure ascent-obs handles this.", id);
            command.arg("--channel").arg(id);
        } else {
            info!("Using stdio communication mode.");
        }

        let mut child = command.spawn().map_err(|e| ObsError::ProcessStart(e.to_string()))?;
        info!("ascent-obs process started (PID: {:?})", child.id());

        // Take stdin/out/err
        let stdin = child.stdin.take().ok_or(ObsError::PipeError(
            "Failed to take stdin".to_string(),
        ))?;
        let stdout = child.stdout.take().ok_or(ObsError::PipeError(
            "Failed to take stdout".to_string(),
        ))?;
        let stderr = child.stderr.take().ok_or(ObsError::PipeError(
            "Failed to take stderr".to_string(),
        ))?;

        // Create channels
        let (command_sender, command_receiver) = mpsc::sync_channel::<String>(buffer_size);
        let (event_sender, event_receiver) = mpsc::sync_channel::<Result<EventNotification, ObsError>>(buffer_size);

        // Spawn standard threads
        let writer_handle = spawn_writer_thread(stdin, command_receiver);
        let reader_handle = spawn_reader_thread(stdout, event_sender.clone());
        let stderr_handle = spawn_stderr_thread(stderr);
        
        // Create the event manager with the event receiver
        let event_manager = EventManager::new(event_receiver, buffer_size);

        Ok(Self {
            child,
            writer_handle,
            reader_handle,
            stderr_handle,
            command_sender,
            event_manager,
        })
    }

    /// Sends a command without a specific payload.
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
        let json_string = serde_json::to_string(&command)?;
        self.send_json_string(json_string)
    }

    /// Sends a command with a specific payload structure.
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
        debug!("Preparing to send command: {:?}", command);
        let json_string = serde_json::to_string(&command)?;
        info!("Preparing to send command: {}", json_string);
        self.send_json_string(json_string)
    }

    /// Sends a pre-serialized JSON string command.
    /// Blocks if the command channel buffer is full.
    fn send_json_string(&self, json_string: String) -> Result<(), ObsError> {
        debug!("Sending command JSON: {}", json_string);
        self.command_sender
            .send(json_string)
            .map_err(|e| ObsError::CommandSend(format!("Command channel closed or error: {}", e)))?;
        Ok(())
    }
    
    /// Sends a command and waits for the response.
    /// This is useful for commands that expect a specific response event.
    pub fn send_command_and_wait<T, P, F>(
        &self,
        cmd_code: i32,
        identifier: i32,
        payload: P,
        timeout: Duration,
        expected_event_type: i32,
        error_event_types: Vec<i32>,
        deserializer: F,
    ) -> Result<T, ObsError>
    where
        P: Serialize + std::fmt::Debug,
        T: DeserializeOwned + Send + 'static,
        F: FnOnce(&EventNotification) -> Result<Option<T>, ObsError> + Send + 'static,
    {
        // Send the command with the provided identifier
        self.send_command(cmd_code, Some(identifier.clone()), payload)?;
        
        // Wait for the response using the event manager
        self.event_manager
            .wait_for_response(identifier, timeout, expected_event_type, error_event_types, deserializer)
    }
    
    /// Register a callback for a specific event type.
    /// The callback will be called whenever an event of the specified type is received.
    pub fn register_event_callback<F>(&self, event_type: i32, callback: F)
    where
        F: Fn(&EventNotification) + Send + Sync + 'static,
    {
        self.event_manager.register_event_callback(event_type, callback);
    }

    /// Shuts down the ascent-obs process and associated communication threads gracefully.
    ///
    /// This function BLOCKS until all threads have joined and the process has exited.
    pub fn shutdown(mut self) -> Result<(), ObsError> {
        info!("Shutting down ObsClient (PID: {:?})", self.child.id());

        // First, shutdown the event manager to stop processing events
        self.event_manager.shutdown();
        
        // Drop the command sender - this signals the writer thread to exit
        drop(self.command_sender);

        // Wait for the writer thread to finish
        match self.writer_handle.join() {
            Ok(Ok(())) => info!("Writer thread finished gracefully."),
            Ok(Err(e)) => error!("Writer thread finished with error: {}", e),
            Err(panic_payload) => error!("Writer thread panicked: {:?}", panic_payload),
        }

        // Attempt to terminate the process
        info!("Attempting to terminate ascent-obs process...");
        if let Err(e) = self.child.kill() {
            error!("Failed to kill ascent-obs process: {}. It might have already exited.", e);
        } else {
            info!("Kill signal sent to ascent-obs process.");
        }

        // Wait for the process to exit
        match self.child.wait() {
            Ok(status) => info!("ascent-obs process exited with status: {}", status),
            Err(e) => error!("Error waiting for ascent-obs process exit: {}", e),
        }

        // Wait for the reader and stderr threads to finish
        if let Err(panic_payload) = self.reader_handle.join() {
            error!("Reader thread panicked: {:?}", panic_payload);
        } else {
            info!("Reader thread finished.");
        }
        if let Err(panic_payload) = self.stderr_handle.join() {
            error!("Stderr thread panicked: {:?}", panic_payload);
        } else {
            info!("Stderr thread finished.");
        }

        info!("ObsClient shutdown complete.");
        Ok(())
    }
}

// --- Background Thread Functions ---

// The thread functions remain the same as in your original code
fn spawn_writer_thread(
    mut stdin: ChildStdin,
    command_receiver: mpsc::Receiver<String>,
) -> JoinHandle<Result<(), ObsError>> {
    thread::spawn(move || {
        info!("Writer thread started.");
        while let Ok(mut json_string) = command_receiver.recv() {
            if !json_string.ends_with('\n') {
                json_string.push('\n');
            }
            debug!("Writer thread sending: {}", json_string.trim_end());

            if let Err(e) = stdin.write_all(json_string.as_bytes()) {
                error!("Writer thread failed to write to stdin: {}", e);
                return Err(ObsError::PipeError(format!("Write error: {}", e)));
            }
            if let Err(e) = stdin.flush() {
                error!("Writer thread failed to flush stdin: {}", e);
                return Err(ObsError::PipeError(format!("Flush error: {}", e)));
            }

            thread::sleep(Duration::from_secs(1));
            debug!("Writer thread completed 1-second sleep after send");
        }

        info!("Writer thread command channel closed. Exiting.");
        Ok(())
    })
}

fn spawn_reader_thread(
    stdout: ChildStdout,
    event_sender: mpsc::SyncSender<Result<EventNotification, ObsError>>,
) -> JoinHandle<()> {
    thread::spawn(move || {
        info!("Reader thread started.");
        let mut reader = BufReader::new(stdout);
        let mut read_buf = vec![0u8; 4096];
        let mut process_buf = Vec::with_capacity(8192);

        loop {
            match reader.read(&mut read_buf) {
                Ok(0) => {
                    info!("Reader thread reached EOF on stdout. Exiting.");
                    if !process_buf.is_empty() {
                        warn!(
                            "Reader thread exited with unprocessed data in buffer ({} bytes): '{}'",
                            process_buf.len(),
                            String::from_utf8_lossy(&process_buf).chars().take(100).collect::<String>()
                        );
                    }
                    break;
                }
                Ok(n) => {
                    process_buf.extend_from_slice(&read_buf[..n]);
                }
                Err(e) => {
                    error!("Reader thread failed to read from stdout: {}", e);
                    let err_res = Err(ObsError::PipeError(format!("Read error: {}", e)));
                    let _ = event_sender.send(err_res);
                    break;
                }
            }

            let mut bytes_consumed_in_buffer = 0;
            loop {
                let current_slice = &process_buf[bytes_consumed_in_buffer..];
                if current_slice.is_empty() {
                    break;
                }

                let de = Deserializer::from_slice(current_slice);
                let mut iter = de.into_iter::<EventNotification>();

                match iter.next() {
                    Some(Ok(event)) => {
                        bytes_consumed_in_buffer += iter.byte_offset();
                        debug!("Reader thread successfully parsed event: {:?}", event);
                        if event_sender.send(Ok(event)).is_err() {
                            warn!("Reader thread failed to send event: receiver dropped.");
                            return;
                        }
                        debug!("Reader task successfully sent event to MPSC channel");
                    }
                    Some(Err(e)) => {
                        let error_offset = iter.byte_offset();
                        if e.is_eof() {
                            debug!("Parser needs more data (EOF encountered).");
                            break;
                        } else {
                            error!(
                                "JSON parse error: {} (at offset {} within current parse attempt, absolute {}) Context: '{}'",
                                e,
                                error_offset,
                                bytes_consumed_in_buffer + error_offset,
                                String::from_utf8_lossy(current_slice).chars().take(80).collect::<String>()
                            );
                            let err_res = Err(ObsError::Deserialization(format!("JSON Parse Error: {}", e)));
                            if event_sender.send(err_res).is_err() {
                                warn!("Reader thread failed to send parsing error: receiver dropped.");
                                return;
                            }
                            bytes_consumed_in_buffer += error_offset;
                            break;
                        }
                    }
                    None => {
                        bytes_consumed_in_buffer = process_buf.len();
                        break;
                    }
                }
            }

            if bytes_consumed_in_buffer > 0 {
                debug!("Draining {} bytes from process buffer", bytes_consumed_in_buffer);
                process_buf.drain(..bytes_consumed_in_buffer);
            }
        }

        info!("Reader thread finished.");
    })
}

fn spawn_stderr_thread(stderr: ChildStderr) -> JoinHandle<()> {
    thread::spawn(move || {
        let mut reader = BufReader::new(stderr);
        let mut line_buf = String::new();
        info!("Stderr logging thread started.");
        loop {
            line_buf.clear();
            match reader.read_line(&mut line_buf) {
                Ok(0) => {
                    info!("Stderr thread reached EOF. Exiting.");
                    break;
                }
                Ok(_) => {
                    let trimmed_line = line_buf.trim();
                    if !trimmed_line.is_empty() {
                        warn!("[ascent-obs stderr] {}", trimmed_line);
                    }
                }
                Err(e) => {
                    error!("Stderr thread failed to read: {}", e);
                    break;
                }
            }
        }
        info!("Stderr thread finished.");
    })
}