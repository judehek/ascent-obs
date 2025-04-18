#[tokio::main] // Or use Tauri's async runtime context
async fn main() {
    // Initialize env_logger instead of simple_logger
    env_logger::init();

    if let Err(e) = run_recorder().await {
        eprintln!("Recorder encountered an error: {}", e);
    }
}

use ascent_obs_comm::{types::{AudioDeviceSettings, AudioSettings, ErrorEventPayload, FileOutputSettings, MonitorSourceSettings, RecorderType, RecordingStartedEventPayload, RecordingStoppedEventPayload, SceneSettings, StartCommandPayload, VideoEncoderSettings, VideoSettings}, ObsError, Recorder};

const OW_OBS_PATH: &str = "C:/Users/judeb/Desktop/OBS_Organized_Build/bin/64bit/ascent-obs.exe";

async fn run_recorder() -> Result<(), ObsError> {
    println!("Starting recorder...");
    let (recorder, mut event_receiver) = Recorder::start(OW_OBS_PATH, 128).await?;
    println!("Recorder started.");

    // Spawn a task to listen for events
    let event_handle = tokio::spawn(async move {
        println!("Event listener task started.");
        while let Some(event_result) = event_receiver.recv().await {
            match event_result {
                Ok(notification) => {
                    println!(
                        "Received Event -> Code: {}, ID: {:?}, Payload: {:?}",
                        notification.event, notification.identifier, notification.payload
                    );
                    // --- Process specific events ---
                    match notification.event {
                        EVT_READY => {
                            if let Some(id) = notification.identifier {
                                println!("Capture (id: {}) is READY.", id);
                            } else {
                                println!("Capture is READY (no id).")
                            }
                        },
                        EVT_RECORDING_STARTED => {
                            if let Some(id) = notification.identifier {
                                println!("Recording (id: {}) STARTED.", id);
                                // Example: Deserialize payload
                                match notification.deserialize_payload::<RecordingStartedEventPayload>() {
                                    Ok(Some(payload)) => println!("  Visible source: {}", payload.source),
                                    Ok(None) => println!("  (No payload data)"),
                                    Err(e) => eprintln!("  Error parsing RECORDING_STARTED payload: {}", e),
                                }
                            }
                        },
                         EVT_RECORDING_STOPPED => {
                             if let Some(id) = notification.identifier {
                                 println!("Recording (id: {}) STOPPED.", id);
                                 match notification.deserialize_payload::<RecordingStoppedEventPayload>() {
                                     Ok(Some(payload)) => println!("  Code: {}, Duration: {}ms", payload.code, payload.duration),
                                     Ok(None) => println!("  (No payload data)"),
                                     Err(e) => eprintln!("  Error parsing RECORDING_STOPPED payload: {}", e),
                                 }
                             }
                         }
                        EVT_ERR => {
                            eprintln!("Received ERROR event!");
                             match notification.deserialize_payload::<ErrorEventPayload>() {
                                 Ok(Some(payload)) => eprintln!("  Code: {}, Desc: {:?}", payload.code, payload.desc),
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

    // --- Example: Start a simple video recording ---
    let start_id = 101;
    let video_settings = VideoSettings { video_encoder: VideoEncoderSettings { encoder_id: "jim_nvenc".to_string(), ..Default::default() }, game_cursor: Some(true), ..Default::default() };
    let audio_settings = AudioSettings { sample_rate: Some(44100), ..Default::default() };
    let scene_settings = SceneSettings { monitor: Some(MonitorSourceSettings { enable: Some(true), ..Default::default()}), ..Default::default()}; // Capture primary monitor
    let file_settings = FileOutputSettings { filename: Some("C:/Users/judeb/Desktop/OBS_Organized_Build/bin/64bit/test_recording.mp4".to_string()), ..Default::default() };

    let start_payload = StartCommandPayload {
        recorder_type: RecorderType::Video,
        video_settings: Some(video_settings),
        audio_settings: Some(audio_settings),
        file_output: Some(file_settings),
        scene_settings: Some(scene_settings),
        ..Default::default()
    };

    let json = serde_json::to_string_pretty(&start_payload).unwrap();
    println!("Sending payload: {}", json);

    if let Err(e) = recorder.start_capture(start_id, start_payload).await {
        eprintln!("Failed to send start command: {}", e);
    } else {
        println!("Start command sent (id: {})", start_id);
    }

    // Let it run for a bit
    tokio::time::sleep(tokio::time::Duration::from_secs(10)).await;

    // --- Example: Stop the recording ---
    if let Err(e) = recorder.stop_capture(start_id, RecorderType::Video).await {
         eprintln!("Failed to send stop command: {}", e);
    } else {
         println!("Stop command sent (id: {})", start_id);
    }

    // Allow time for stop event processing
    tokio::time::sleep(tokio::time::Duration::from_secs(2)).await;

    // --- Shutdown ---
    println!("Shutting down recorder...");
    if let Err(e) = recorder.shutdown().await {
        eprintln!("Error during shutdown: {}", e);
    } else {
        println!("Recorder shutdown complete.");
    }

    // Wait for the event listener task to finish
    let _ = event_handle.await;
    println!("All tasks finished.");

    Ok(())
}