use ascent_obs_comm::{
    types::{
        AudioSettings, ErrorEventPayload, FileOutputSettings, GameSourceSettings,
        RecordingStartedEventPayload, RecordingStoppedEventPayload, RecorderType, SceneSettings,
        StartCommandPayload, VideoEncoderSettings, VideoSettings,
    },
    ObsError, Recorder,
};
use std::io::{self, Write};
use tokio;

#[tokio::main]
async fn main() {
// Initialize env_logger instead of simple_logger
env_logger::init();
if let Err(e) = run_recorder().await {
    eprintln!("Recorder encountered an error: {}", e);
}
}

const ASCENT_OBS_PATH: &str = "C:/path/to/file";
const FILE_PATH: &str = "C:/output/path";
const ENCODER_ID: &str = "jim_nvenc";
const TARGET_PID: i32 = 22748; // Make sure this PID is correct when you run!

// --- Event Codes for matching ---
const EVT_READY: i32 = 3;
const EVT_RECORDING_STARTED: i32 = 4;
const EVT_RECORDING_STOPPED: i32 = 6;
const EVT_ERR: i32 = 2;
// ---

async fn run_recorder() -> Result<(), ObsError> {
    println!("Starting recorder...");
    // NOTE: Consider using a unique channel ID if running multiple instances
    // or stick to stdio if ASCENT_OBS_PATH is used directly without Overwolf manager
    let (recorder, mut event_receiver) = Recorder::start(ASCENT_OBS_PATH, 128).await?;
    println!("Recorder started (ow-obs process should be running).");

    // Spawn a task to listen for events
    let event_handle = tokio::spawn(async move {
        println!("Event listener task started.");
        while let Some(event_result) = event_receiver.recv().await {
            match event_result {
                Ok(notification) => {
                    let event_code_raw = notification.event; // Get the raw i32 code
                    let event_code: Option<i32> =
                        serde_json::from_value(serde_json::Value::from(event_code_raw)).ok();

                    println!(
                        "Received Event -> Code: {} ({:?}), ID: {:?}, Payload: {:?}",
                        event_code_raw, event_code, notification.identifier, notification.payload
                    );

                    // --- Process specific events ---
                    match event_code_raw {
                        EVT_READY => {
                            if let Some(id) = notification.identifier {
                                println!("Capture (id: {}) is READY.", id);
                            } else {
                                println!("Capture is READY (no id).")
                            }
                        }
                        EVT_RECORDING_STARTED => {
                            if let Some(id) = notification.identifier {
                                println!(">>> Recording (id: {}) STARTED. <<<", id); // Highlight this
                                match notification
                                    .deserialize_payload::<RecordingStartedEventPayload>()
                                {
                                    Ok(Some(payload)) => {
                                        println!("  Visible source: {}", payload.source)
                                    }
                                    Ok(None) => println!("  (No payload data)"),
                                    Err(e) => {
                                        eprintln!("  Error parsing RECORDING_STARTED payload: {}", e)
                                    }
                                }
                            }
                        }
                        EVT_RECORDING_STOPPED => {
                            if let Some(id) = notification.identifier {
                                println!("<<< Recording (id: {}) STOPPED. >>>", id); // Highlight this
                                match notification
                                    .deserialize_payload::<RecordingStoppedEventPayload>()
                                {
                                    Ok(Some(payload)) => println!(
                                        "  Code: {}, Duration: {}ms, Error: {:?}",
                                        payload.code, payload.duration, payload.last_error
                                    ),
                                    Ok(None) => println!("  (No payload data)"),
                                    Err(e) => {
                                        eprintln!("  Error parsing RECORDING_STOPPED payload: {}", e)
                                    }
                                }
                            }
                        }
                        EVT_ERR => {
                            eprintln!("!!! Received ERROR event! ID: {:?} !!!", notification.identifier);
                            match notification.deserialize_payload::<ErrorEventPayload>() {
                                Ok(Some(payload)) => eprintln!(
                                    "  Code: {}, Desc: {:?}",
                                    payload.code, payload.desc
                                ),
                                Ok(None) => eprintln!("  (No error payload data)"),
                                Err(e) => eprintln!("  Error parsing ERR payload: {}", e),
                            }
                        }
                        // Add cases for other events you care about
                        _ => {
                            // Optionally log unhandled events or their raw payload
                            // if let Some(payload_value) = notification.payload {
                            //     println!("  Payload: {}", payload_value);
                            // }
                        }
                    }
                }
                Err(e) => {
                    eprintln!("Error receiving event from channel: {}", e);
                    // Consider if the client should shut down on channel errors
                    break;
                }
            }
        }
        println!("Event listener task finished.");
    });

    println!("\n>>> You have 5 seconds to Alt+Tab to League of Legends (PID: {}) <<<", TARGET_PID);
    println!(">>> Recording will automatically start after the countdown...");
    io::stdout().flush().unwrap();
    
    // Countdown display
    for i in (1..=5).rev() {
        print!("\rStarting recording in {} seconds...", i);
        io::stdout().flush().unwrap();
        tokio::time::sleep(tokio::time::Duration::from_secs(1)).await;
    }
    println!("\rProceeding to send start command...");


    // --- Define Start Payload ---
    let start_id = 101;
    let video_settings = VideoSettings {
        video_encoder: VideoEncoderSettings {
            encoder_id: ENCODER_ID.to_string(),
            ..Default::default()
        },
        game_cursor: Some(true),
        fps: Some(60), // Match your previous tests
        base_width: Some(1920),
        base_height: Some(1080),
        output_width: Some(1920),
        output_height: Some(1080),
        ..Default::default()
    };
    let audio_settings = AudioSettings {
        sample_rate: Some(48000), // Match Overwolf logs
        ..Default::default()
    };
    let scene_settings = SceneSettings {
        game: Some(GameSourceSettings {
            process_id: Some(TARGET_PID),
            foreground: Some(true), // **** CHANGED TO TRUE ****
            allow_transparency: Some(false),
            ..Default::default()
        }),
        ..Default::default()
    };
    let file_settings = FileOutputSettings {
        filename: Some(FILE_PATH.to_string()),
        ..Default::default()
    };

    let start_payload = StartCommandPayload {
        recorder_type: RecorderType::Video,
        video_settings: Some(video_settings),
        audio_settings: Some(audio_settings),
        file_output: Some(file_settings),
        scene_settings: Some(scene_settings),
        ..Default::default()
    };

    let json = serde_json::to_string_pretty(&start_payload).unwrap();
    println!("Sending Start payload (id: {}):\n{}", start_id, json);

    if let Err(e) = recorder.start_capture(start_id, start_payload).await {
        eprintln!("Failed to send start command: {}", e);
    } else {
        println!("Start command sent (id: {})", start_id);
    }

    // Let it run for a bit (longer delay to be safe)
    println!("Waiting 20 seconds for recording...");
    tokio::time::sleep(tokio::time::Duration::from_secs(20)).await;

    // --- Example: Stop the recording ---
    println!("Sending Stop command (id: {})...", start_id);
    if let Err(e) = recorder.stop_capture(start_id, RecorderType::Video).await {
        eprintln!("Failed to send stop command: {}", e);
    } else {
        println!("Stop command sent (id: {})", start_id);
    }

    // Allow time for stop event processing
    tokio::time::sleep(tokio::time::Duration::from_secs(4)).await;

    // --- Shutdown ---
    println!("Shutting down recorder...");
    if let Err(e) = recorder.shutdown().await {
        eprintln!("Error during shutdown: {}", e);
    } else {
        println!("Recorder shutdown message sent.");
    }

    // Wait for the event listener task to finish (or a timeout)
     println!("Waiting for event listener task to potentially finish...");
    // Wait longer to ensure all messages might have been processed
    tokio::time::sleep(tokio::time::Duration::from_secs(5)).await;
    // The handle itself might finish earlier if the receiver channel closes upon shutdown
    // let _ = event_handle.await; // You can optionally await it fully

    println!("Rust script finished.");

    Ok(())
}