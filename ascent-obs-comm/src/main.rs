use ascent_obs_comm::{
    errors::ObsError, Recorder, RecordingConfig // Import errors directly
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

    // --- Create the recorder directly with new() ---
    let recorder = Recorder::new(ASCENT_OBS_PATH, None).await?;

    println!("Recorder process started.");
    
    // Note: We no longer have an event receiver, so we'll skip event handling

    // --- Example: Query Machine Info ---
    // Note: Our new API just returns an ID but we can't actually listen for the response
    match ascent_obs_comm::query_machine_info(ASCENT_OBS_PATH).await {
        Ok(machine_info) => {
            println!("\nQuery Machine Info completed successfully!");
            println!("Found {} video encoders and {} audio devices:", 
                machine_info.video_encoders.len(),
                machine_info.audio_input_devices.len() + machine_info.audio_output_devices.len());
            
            // Print some details about the encoders
            if !machine_info.video_encoders.is_empty() {
                println!("\nAvailable video encoders:");
                for encoder in &machine_info.video_encoders {
                    println!("  - {}: {} (Valid: {})", 
                        encoder.encoder_type, 
                        encoder.description, 
                        encoder.valid);
                }
            }
            
            // Optionally print audio devices if you want
            // if !machine_info.audio_input_devices.is_empty() {
            //     println!("\nInput audio devices:");
            //     for device in &machine_info.audio_input_devices {
            //         println!("  - {}", device.name);
            //     }
            // }
        },
        Err(e) => {
            println!("\nFailed to query machine info: {}", e);
        }
    }

    // --- Start Recording using RecordingConfig ---
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

    // Create a RecordingConfig and start recording
    let config = RecordingConfig::new(FILE_PATH, TARGET_PID)
        // Optional settings:
        // .with_encoder("jim_nvenc")       // Default is jim_nvenc anyway
        // .with_fps(60)                    // Default is 60
        // .with_resolution(1920, 1080)     // Default is 1920x1080
        // .with_cursor(true)               // Default is true
        // .with_sample_rate(48000)         // Default is 48000
        ;
    
    let recording_id = recorder.start_recording(config).await?;
    println!("Start recording command sent (id: {})", recording_id);

    // Let it run
    println!("Waiting 10 seconds for recording...");
    tokio::time::sleep(tokio::time::Duration::from_secs(10)).await;

    // --- Stop the recording ---
    println!("Sending Stop command (id: {})...", recording_id);
    recorder.stop_recording().await?;
    println!("Stop command sent (id: {})", recording_id);

    // Allow time for stop event processing 
    // (though we aren't receiving events in this version)
    tokio::time::sleep(tokio::time::Duration::from_secs(4)).await;

    // --- Shutdown ---
    println!("Shutting down recorder...");
    recorder.shutdown().await?; // Consume the recorder instance
    println!("Recorder shutdown message sent.");

    // No need to wait for event listener since we're not handling events
    println!("Rust script finished.");
    Ok(())
}