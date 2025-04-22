use ascent_obs_comm::{
    errors::ObsError, Recorder, RecordingConfig // Import errors directly
};
use log::debug;
use std::io::{self, Write};
use tokio;

#[tokio::main]
async fn main() {
    env_logger::Builder::from_env(env_logger::Env::default().default_filter_or("info")).init(); // Default to info level
    if let Err(e) = run_recorder() {
        eprintln!("Recorder encountered an error: {}", e);
        // Consider adding specific error handling based on ObsError variants
        // if let ObsError::Configuration(msg) = e {
        //     eprintln!("Configuration Error: {}", msg);
        // }
    }
}

const ASCENT_OBS_PATH: &str =
    "/Users/judeb/AppData/Local/Ascent/libraries/ascent-obs/bin/64bit/ascent-obs.exe";
const FILE_PATH: &str = "C:/Users/judeb/Desktop/output_refactored.mp4";
const REPLAY_FILE_PATH: &str = "C:/Users/judeb/Desktop/automatic_replay_buffer.mp4";
const TARGET_PID: i32 = 32780; // !! Make sure this PID is correct when you run! !!

fn run_recorder() -> Result<(), ObsError> {
    println!("Configuring recorder...");

    println!("Recorder process started.");

    // --- Example: Query Machine Info ---
    // Note: Our new API just returns an ID but we can't actually listen for the response
    match ascent_obs_comm::query_machine_info(ASCENT_OBS_PATH) {
    Ok(machine_info) => {
        println!("\nQuery Machine Info completed successfully!");
        
        // Print the entire struct with pretty formatting
        println!("Machine Info: {:#?}", machine_info);
        
        // You can still keep the summary if you want
        println!("Found {} video encoders and {} audio devices:", 
            machine_info.video_encoders.len(),
            machine_info.audio_input_devices.len() + machine_info.audio_output_devices.len());
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
        std::thread::sleep(std::time::Duration::from_secs(1));
    }
    println!("\rSending start recording command...          "); // Clear countdown line

    // Create a RecordingConfig and start recording
    let config = RecordingConfig::new(FILE_PATH)
        // Optional settings:
        .with_encoder("h264_texture_amf")       // Default is jim_nvenc anyway
        // .with_fps(60)                    // Default is 60
        // .with_resolution(1920, 1080)     // Default is 1920x1080
        // .with_cursor(true)               // Default is true
        // .with_sample_rate(48000)         // Default is 48000
        .with_replay_buffer(Some(30))
        ;
        // --- Create the recorder directly with new() ---
    let recorder = Recorder::new(ASCENT_OBS_PATH, config, TARGET_PID, None)?;
    
    let recording_id = recorder.start_recording()?;
    println!("Start recording command sent (id: {})", recording_id);

    // Let it run
    println!("Waiting 30 seconds for recording...");
    std::thread::sleep(std::time::Duration::from_secs(30));

    println!("saving replay buffer...");
    recorder.save_replay_buffer(REPLAY_FILE_PATH)?;
    println!("saved replay buffer");

    std::thread::sleep(std::time::Duration::from_secs(10));

    // --- Stop the recording ---
    println!("Sending Stop command (id: {})...", recording_id);
    recorder.stop_recording()?;
    println!("Stop command sent (id: {})", recording_id);

    // Allow time for stop event processing 
    // (though we aren't receiving events in this version)
    std::thread::sleep(std::time::Duration::from_secs(1));

    // --- Shutdown ---
    println!("Shutting down recorder...");
    recorder.shutdown()?; // Consume the recorder instance
    println!("Recorder shutdown message sent.");

    // No need to wait for event listener since we're not handling events
    println!("Rust script finished.");
    Ok(())
}