// src/recorder.rs
use crate::communication::ObsClient;
use crate::errors::ObsError;
use crate::types::{AudioSettings, BrbSourceSettings, EventNotification, GameFocusChangedCommandPayload, GameSourceSettings, RecorderType, SetVolumeCommandPayload, StartCommandPayload, StartReplayCaptureCommandPayload, StopCommandPayload, TobiiSourceSettings, CMD_ADD_GAME_SOURCE, CMD_GAME_FOCUS_CHANGED, CMD_QUERY_MACHINE_INFO, CMD_SET_BRB, CMD_SET_VOLUME, CMD_SHUTDOWN, CMD_SPLIT_VIDEO, CMD_START, CMD_START_REPLAY_CAPTURE, CMD_STOP, CMD_STOP_REPLAY_CAPTURE, CMD_TOBII_GAZE};
use log::{debug, error, info};
use std::path::Path;
use std::sync::atomic::{AtomicI32, Ordering};
use tokio::sync::mpsc;

// Counter for generating simple identifiers when needed (e.g., for queries)
static NEXT_IDENTIFIER: AtomicI32 = AtomicI32::new(1);

/// A high-level client for controlling an ascent-obs recording process.
#[derive(Debug)]
pub struct Recorder {
    client: ObsClient,
    // We don't store the receiver here; it's returned by start()
    // Add internal state tracking if needed later (e.g., is_recording)
}

// Type alias for the event receiver channel returned by start()
pub type EventReceiver = mpsc::Receiver<Result<EventNotification, ObsError>>;

impl Recorder {
    /// Starts the ascent-obs.exe process and returns a `Recorder` instance
    /// along with a channel receiver for events.
    ///
    /// # Arguments
    /// * `ow_obs_path` - Path to the ascent-obs.exe executable.
    /// * `buffer_size` - Size of the internal channel buffer for incoming events.
    pub async fn start(
        ow_obs_path: impl AsRef<Path>,
        buffer_size: usize,
    ) -> Result<(Self, EventReceiver), ObsError> {
        // For stdio communication, channel_id is typically None.
        let (client, event_receiver) = ObsClient::start(ow_obs_path, None, buffer_size).await?;
        Ok((
            Self {
                client,
                // Initialize any internal state here if added later
            },
            event_receiver,
        ))
    }

    /// Sends a command to start recording, streaming, or replay buffering.
    ///
    /// # Arguments
    /// * `identifier` - A unique identifier for this start request. The corresponding
    ///   `READY`, `STARTED`, or `ERR` event will include this identifier.
    /// * `settings` - The complete settings structure for the start command.
    pub async fn start_capture(
        &self,
        identifier: i32,
        settings: StartCommandPayload,
    ) -> Result<(), ObsError> {
        info!(
            "Sending START command (id: {}, type: {:?})",
            identifier, settings.recorder_type
        );
        self.client
            .send_command(CMD_START, Some(identifier), settings)
            .await
    }

    /// Sends a command to stop a specific recording, stream, or replay buffer.
    ///
    /// # Arguments
    /// * `identifier` - The identifier originally used to start the capture.
    /// * `recorder_type` - The type of capture to stop.
    pub async fn stop_capture(
        &self,
        identifier: i32,
        recorder_type: RecorderType,
    ) -> Result<(), ObsError> {
        info!(
            "Sending STOP command (id: {}, type: {:?})",
            identifier, recorder_type
        );
        let payload = StopCommandPayload { recorder_type };
        self.client
            .send_command(CMD_STOP, Some(identifier), payload)
            .await
    }

    /// Sends a request to query machine information (encoders, audio devices).
    /// Returns the identifier sent with the command, which can be used to
    /// match the corresponding `EVT_QUERY_MACHINE_INFO` event.
    pub async fn query_machine_info(&self) -> Result<i32, ObsError> {
        // Generate a simple unique ID for this query
        let identifier = NEXT_IDENTIFIER.fetch_add(1, Ordering::Relaxed);
        info!("Sending QUERY_MACHINE_INFO command (id: {})", identifier);
        self.client
            .send_simple_command(CMD_QUERY_MACHINE_INFO, Some(identifier))
            .await?;
        Ok(identifier)
    }

    /// Sends a command to set audio volumes.
    ///
    /// # Arguments
    /// * `audio_settings` - Contains the desired volume levels for input/output.
    ///   Only the `volume` fields within the nested `AudioDeviceSettings` are typically used.
    pub async fn set_volume(&self, audio_settings: AudioSettings) -> Result<(), ObsError> {
        info!("Sending SET_VOLUME command");
        // The C++ command expects the volumes nested within an 'audio_settings' object
        let payload = SetVolumeCommandPayload {
            audio_settings: Some(audio_settings),
        };
        // SetVolume doesn't typically need an identifier
        self.client.send_command(CMD_SET_VOLUME, None, payload).await
    }

    /// Informs ascent-obs about a change in game focus.
    ///
    /// # Arguments
    /// * `game_foreground` - True if the game window is now in the foreground.
    /// * `is_minimized` - Optional: True if the game window is minimized.
    pub async fn game_focus_changed(
        &self,
        game_foreground: bool,
        is_minimized: Option<bool>,
    ) -> Result<(), ObsError> {
        info!(
            "Sending GAME_FOCUS_CHANGED command (foreground: {})",
            game_foreground
        );
        let payload = GameFocusChangedCommandPayload {
            game_foreground,
            is_minimized,
        };
        // GameFocusChanged doesn't typically need an identifier
        self.client
            .send_command(CMD_GAME_FOCUS_CHANGED, None, payload)
            .await
    }

    /// Adds or updates the primary game capture source settings.
    ///
    /// # Arguments
    /// * `settings` - The configuration for the game source (e.g., process ID).
    pub async fn add_game_source(&self, settings: GameSourceSettings) -> Result<(), ObsError> {
        info!(
            "Sending ADD_GAME_SOURCE command (pid: {:?})",
            settings.process_id
        );
        // AddGameSource doesn't typically need an identifier for the command itself
        // The payload *is* the settings struct
        self.client
            .send_command(CMD_ADD_GAME_SOURCE, None, settings)
            .await
    }

    /// Starts capturing a replay from the active replay buffer.
    ///
    /// # Arguments
    /// * `identifier` - The identifier originally used to start the replay *buffer*.
    /// * `payload` - Contains details like the output path and desired duration from the buffer head.
    pub async fn start_replay_capture(
        &self,
        identifier: i32, // Identifier of the replay *buffer*
        payload: StartReplayCaptureCommandPayload,
    ) -> Result<(), ObsError> {
        info!(
            "Sending START_REPLAY_CAPTURE command (id: {}, path: {})",
            identifier, payload.path
        );
        // This command uses the *replay buffer's* identifier
        self.client
            .send_command(CMD_START_REPLAY_CAPTURE, Some(identifier), payload)
            .await
    }

    /// Stops an ongoing replay *capture* (saving process).
    ///
    /// # Arguments
    /// * `identifier` - The identifier originally used to start the replay *buffer*.
    pub async fn stop_replay_capture(&self, identifier: i32) -> Result<(), ObsError> {
        info!("Sending STOP_REPLAY_CAPTURE command (id: {})", identifier);
        // This command uses the *replay buffer's* identifier
        self.client
            .send_simple_command(CMD_STOP_REPLAY_CAPTURE, Some(identifier))
            .await
    }

    /// Updates the Tobii gaze overlay source settings (e.g., visibility).
    ///
    /// # Arguments
    /// * `settings` - The new settings for the Tobii source.
    pub async fn update_tobii_gaze(&self, settings: TobiiSourceSettings) -> Result<(), ObsError> {
        info!("Sending TOBII_GAZE command (visible: {:?})", settings.visible);
        // UpdateTobiiGaze doesn't typically need an identifier
        self.client
            .send_command(CMD_TOBII_GAZE, None, settings)
            .await
    }

    /// Updates the "Be Right Back" (BRB) source settings.
    ///
    /// # Arguments
    /// * `settings` - The new settings for the BRB source (path, color).
    pub async fn set_brb(&self, settings: BrbSourceSettings) -> Result<(), ObsError> {
        info!("Sending SET_BRB command (path: {:?})", settings.path);
        // SetBrb doesn't typically need an identifier
        self.client.send_command(CMD_SET_BRB, None, settings).await
    }

    /// Manually triggers a video split for an active recording.
    /// The recording must have been started with `enable_on_demand_split_video: true`.
    ///
    /// # Arguments
    /// * `identifier` - The identifier originally used to start the recording.
    pub async fn split_video(&self, identifier: i32) -> Result<(), ObsError> {
        info!("Sending SPLIT_VIDEO command (id: {})", identifier);
        // SplitVideo uses the recording's identifier
        self.client
            .send_simple_command(CMD_SPLIT_VIDEO, Some(identifier))
            .await
    }

    /// Sends the shutdown command to ascent-obs and waits for the process and tasks to terminate.
    /// Consumes the `Recorder` instance.
    pub async fn shutdown(self) -> Result<(), ObsError> {
        info!("Sending SHUTDOWN command");
        // Send the command first, don't strictly wait for confirmation
        // as the shutdown method will kill the process anyway.
        let _ = self
            .client
            .send_simple_command(CMD_SHUTDOWN, None) // Shutdown doesn't need ID
            .await;
        // Now call the client's shutdown which handles process termination etc.
        self.client.shutdown().await
    }

    // Potential future additions:
    // - Methods to get internal state (e.g., is_recording(identifier))
    // - Methods to simplify common setting configurations
}