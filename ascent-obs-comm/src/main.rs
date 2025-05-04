use ascent_obs_comm::{
    errors::ObsError, Recorder, RecordingConfig
};
use log::debug;
use std::io::{self, Write};
use tokio;
use mp4ameta; // Import the mp4ameta crate

#[tokio::main]
async fn main() {
    env_logger::Builder::from_env(env_logger::Env::default().default_filter_or("info")).init();
    if let Err(e) = run_recorder() {
        eprintln!("Recorder encountered an error: {}", e);
    }
}

const ASCENT_OBS_PATH: &str =
    "/Users/judeb/AppData/Local/Ascent/libraries/ascent-obs/bin/64bit/ascent-obs.exe";
const FILE_PATH: &str = "C:/Users/judeb/Desktop/output_refactored.mp4";
const REPLAY_FILE_PATH: &str = "C:/Users/judeb/Desktop/automatic_replay_buffer.mp4";
const TARGET_PID: i32 = 13840; // !! Make sure this PID is correct when you run! !!

fn run_recorder() -> Result<(), ObsError> {
    println!("Configuring recorder...");

    println!("Recorder process started.");

    // --- Example: Query Machine Info ---
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
        .with_encoder("obs_x264")       // Default is jim_nvenc anyway
        .with_encoder_preset(Some("ultrafast".to_string()))
        // .with_fps(60)                    // Default is 60
        // .with_resolution(1920, 1080)     // Default is 1920x1080
        // .with_cursor(true)               // Default is true
        // .with_sample_rate(48000)         // Default is 48000
        .with_replay_buffer(Some(30))
        ;
        // --- Create the recorder directly with new() ---
    let recorder = Recorder::new(ASCENT_OBS_PATH, config, TARGET_PID, None)?;
    
    let recording_id = match recorder.start_recording() {
        Ok(id) => {
            println!("Start recording command sent (id: {})", id);
            id
        },
        Err(e) => {
            eprintln!("Failed to start recording: {}", e);
            return Err(e);
        }
    };
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

    // --- Shutdown ---
    println!("Shutting down recorder...");
    recorder.request_shutdown()?; // Consume the recorder instance
    println!("Recorder shutdown message sent.");

    // --- Add metadata to the recording ---
    println!("Adding metadata to the recording...");
    if let Err(e) = add_metadata_to_recording(FILE_PATH) {
        eprintln!("Failed to add metadata to recording: {}", e);
    }

    // --- Add metadata to the replay buffer recording ---
    println!("Adding metadata to the replay buffer recording...");
    if let Err(e) = add_metadata_to_recording(REPLAY_FILE_PATH) {
        eprintln!("Failed to add metadata to replay buffer: {}", e);
    }

    println!("Rust script finished.");
    Ok(())
}

// Function to add metadata to an MP4 file
fn add_metadata_to_recording(file_path: &str) -> Result<(), String> {
    match mp4ameta::Tag::read_from_path(file_path) {
        Ok(mut tag) => {
            // Add metadata
            let comment = format!("Recording created by Ascent OBS");
            
            tag.set_comment(comment);
            
            // You can add more metadata if needed
            tag.set_title("Game Recording");
            tag.set_artist("Ascent OBS Recorder");
            
            // Write the changes back to the file
            if let Err(e) = tag.write_to_path(file_path) {
                return Err(format!("Failed to write metadata: {}", e));
            }
            
            println!("Successfully added metadata to: {}", file_path);
            Ok(())
        },
        Err(e) => {
            Err(format!("Failed to read MP4 file: {}", e))
        }
    }
}