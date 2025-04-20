use ascent_obs_comm::{
    errors::ObsError, // Import errors directly
    recorder::{EventReceiver, Recorder}, // Import Recorder and EventReceiver
    types::{
        // Import only event types needed for handling
        ErrorEventPayload, EventNotification, QueryMachineInfoEventPayload,
        RecordingStartedEventPayload, RecordingStoppedEventPayload,
        // Event codes
        EVT_ERR, EVT_QUERY_MACHINE_INFO, EVT_READY, EVT_RECORDING_STARTED, EVT_RECORDING_STOPPED,
    },
};
use log::debug;
use std::io::{self, Write};
use tokio;

#[tokio::main]
async fn main() {
    env_logger::Builder::from_env(env_logger::Env::default().default_filter_or("info")).init(); // Default to info level
    if let Err(e) = run_recorder().await {
        eprintln!("Recorder encountered an error: {}", e);
        // Consider adding specific error handling based on ObsError variants
        // if let ObsError::Configuration(msg) = e {
        //     eprintln!("Configuration Error: {}", msg);
        // }
    }
}

const ASCENT_OBS_PATH: &str =
    "/Users/judeb/AppData/Local/Ascent/OBS_Organized_Build/bin/64bit/ascent-obs.exe";
const FILE_PATH: &str = "C:/Users/judeb/Desktop/output_refactored.mp4";
const TARGET_PID: i32 = 23404; // !! Make sure this PID is correct when you run! !!

async fn run_recorder() -> Result<(), ObsError> {
    println!("Configuring recorder...");

    // --- Use RecorderBuilder to start the process ---
    let (recorder, mut event_receiver) = Recorder::builder(ASCENT_OBS_PATH)
        // .event_buffer_size(256) // Optional: Set buffer size
        .build()
        .await?;

    println!("Recorder process started.");

    // --- Spawn Event Listener Task (largely unchanged) ---
    let event_handle = tokio::spawn(async move {
        println!("Event listener task started.");
        while let Some(event_result) = event_receiver.recv().await {
            match event_result {
                Ok(notification) => {
                    handle_event(notification); // Delegate to separate function
                }
                Err(e) => {
                    eprintln!("Error receiving event from channel: {}", e);
                    break; // Stop listening on channel error
                }
            }
        }
        println!("Event listener task finished.");
    });

    // --- Example: Query Machine Info ---
    let query_id = recorder.query_machine_info().await?;
    println!("\nSent Query Machine Info command (id: {})", query_id);
    // You would look for EVT_QUERY_MACHINE_INFO with this ID in the event handler

    tokio::time::sleep(tokio::time::Duration::from_secs(1)).await; // Give time for query response

    // --- Start Recording using the Builder ---
    println!(
        "\n>>> You have 5 seconds to Alt+Tab to League of Legends (PID: {}) <<<",
        TARGET_PID
    );
    println!(">>> Recording will automatically start after the countdown...");
    io::stdout().flush().unwrap();

    // Countdown display
    for i in (1..=5).rev() {
        print!("\rStarting recording in {} seconds...", i);
        io::stdout().flush().unwrap();
        tokio::time::sleep(tokio::time::Duration::from_secs(1)).await;
    }
    println!("\rSending start recording command...          "); // Clear countdown line

    let recording_id = recorder
        .start_video_recording() // Get the builder
        .output_file(FILE_PATH) // Set required path
        .capture_game(TARGET_PID) // Set required PID
        // Optional settings:
        // .video_encoder("jim_nvenc") // Default is jim_nvenc anyway
        // .fps(60)                     // Default is 60
        // .resolution(1920, 1080)      // Default is 1920x1080
        // .show_game_cursor(true)      // Default is true
        // .audio_sample_rate(48000)    // Default is 48000
        .start() // Build payload and send command
        .await?; // Await the send operation

    println!("Start recording command sent (id: {})", recording_id);

    // Let it run
    println!("Waiting 10 seconds for recording...");
    tokio::time::sleep(tokio::time::Duration::from_secs(10)).await;

    // --- Stop the recording ---
    println!("Sending Stop command (id: {})...", recording_id);
    recorder.stop_recording(recording_id).await?; // Use the specific stop method
    println!("Stop command sent (id: {})", recording_id);

    // Allow time for stop event processing
    tokio::time::sleep(tokio::time::Duration::from_secs(4)).await;

    // --- Shutdown ---
    println!("Shutting down recorder...");
    recorder.shutdown().await?; // Consume the recorder instance
    println!("Recorder shutdown message sent.");

    // Wait for the event listener task to potentially finish
    println!("Waiting a few seconds for event listener cleanup...");
    // Wait slightly longer to ensure shutdown messages might have been processed
    let _ = tokio::time::timeout(tokio::time::Duration::from_secs(5), event_handle).await;
    // Handle timeout or join error if needed

    println!("Rust script finished.");
    Ok(())
}

// --- Event Handling Function ---
fn handle_event(notification: EventNotification) {
    let event_code = notification.event;
    let id_str = notification
        .identifier
        .map(|id| id.to_string())
        .unwrap_or_else(|| "None".to_string());

    debug!(
        "Received Event -> Code: {}, ID: {}, Raw Payload: {:?}",
        event_code,
        id_str,
        notification.payload // Log raw payload only at debug level
    );

    match event_code {
        EVT_READY => {
            println!("---> Event: READY (ID: {}) <---", id_str);
        }
        EVT_RECORDING_STARTED => {
            println!(
                "---> Event: RECORDING_STARTED (ID: {}) <---",
                id_str
            );
            match notification.deserialize_payload::<RecordingStartedEventPayload>() {
                Ok(Some(payload)) => {
                    println!("     Source: {}, Window Capture: {:?}", payload.source, payload.is_window_capture)
                }
                Ok(None) => println!("     (No payload data)"),
                Err(e) => eprintln!("     Error parsing RECORDING_STARTED payload: {}", e),
            }
        }
        EVT_RECORDING_STOPPED => {
            println!(
                "---> Event: RECORDING_STOPPED (ID: {}) <---",
                id_str
            );
            match notification.deserialize_payload::<RecordingStoppedEventPayload>() {
                Ok(Some(payload)) => println!(
                    "     Code: {}, Duration: {}ms, Error: {:?}, Res: {:?}x{:?}, Stats: {:?}",
                    payload.code,
                    payload.duration,
                    payload.last_error,
                    payload.output_width,
                    payload.output_height,
                    payload.stats_data.is_some() // Just indicate if stats exist
                ),
                Ok(None) => println!("     (No payload data)"),
                Err(e) => eprintln!("     Error parsing RECORDING_STOPPED payload: {}", e),
            }
        }
        EVT_QUERY_MACHINE_INFO => {
             println!(
                "---> Event: QUERY_MACHINE_INFO (ID: {}) <---",
                id_str
            );
             match notification.deserialize_payload::<QueryMachineInfoEventPayload>() {
                Ok(Some(payload)) => {
                    println!("     Audio In: {} devices", payload.audio_input_devices.len());
                    println!("     Audio Out: {} devices", payload.audio_output_devices.len());
                    println!("     Video Encoders:");
                    for enc in payload.video_encoders {
                         println!("       - ID: {}, Desc: {}, Valid: {}", enc.encoder_type, enc.description, enc.valid);
                    }
                     println!("     WinRT Capture Supported: {}", payload.winrt_capture_supported);
                }
                Ok(None) => println!("     (No payload data)"),
                Err(e) => eprintln!("     Error parsing QUERY_MACHINE_INFO payload: {}", e),
             }
        }
        EVT_ERR => {
            eprintln!("!!! ---> Event: ERROR (ID: {}) <--- !!!", id_str);
            match notification.deserialize_payload::<ErrorEventPayload>() {
                Ok(Some(payload)) => {
                    eprintln!("     Code: {}, Desc: {:?}", payload.code, payload.desc);
                    if let Some(data) = payload.data {
                         eprintln!("     Data: {}", data);
                    }
                }
                Ok(None) => eprintln!("     (No error payload data)"),
                Err(e) => eprintln!("     Error parsing ERR payload: {}", e),
            }
        }
        // Add cases for other events if needed (e.g., EVT_RECORDING_STOPPING)
        _ => {
             println!("---> Event: UNHANDLED (Code: {}, ID: {}) <---", event_code, id_str);
             // Optionally log the payload for debugging unhandled events
             // if let Some(payload_value) = notification.payload {
             //     println!("     Payload: {}", payload_value);
             // }
        }
    }
}