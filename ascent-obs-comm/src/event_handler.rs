use crate::errors::ObsError;
use crate::types::EventNotification;
use log::{debug, error, info, trace, warn};
use serde::de::DeserializeOwned;
use std::collections::HashMap;
use std::sync::mpsc::{self, Receiver, Sender, TryRecvError};
use std::sync::{Arc, Mutex}; // Mutex needed for callback registry access from public methods
use std::thread::{self, JoinHandle};
use std::time::{Duration, Instant};

// Type alias for the callback function signature
type Callback = Box<dyn Fn(&EventNotification) + Send + Sync + 'static>;
// Registry mapping Event Type ID -> List of Callbacks
type CallbackRegistry = HashMap<i32, Vec<Callback>>;
// Registry mapping Identifier -> Channel Sender to notify the waiting task
type WaiterRegistry = HashMap<i32, WaiterInfo>;

// Information stored for each waiting task
#[derive(Debug)]
struct WaiterInfo {
    sender: Sender<Result<EventNotification, ObsError>>,
    deadline: Instant,
    expected_event_type: i32, // Store the expected type for better error reporting
    error_event_types: Vec<i32>, // New field: list of event types that indicate errors
}

// Messages sent internally from the public methods to the processing thread
enum ManagerMessage {
    RegisterCallback {
        event_type: i32,
        callback: Callback,
    },
    RegisterWaiter {
        identifier: i32,
        info: WaiterInfo,
    },
    Shutdown,
}

/// Manages incoming events, dispatches them to waiting tasks or registered callbacks.
pub struct EventManager {
    processing_thread: Option<JoinHandle<()>>,
    message_sender: mpsc::SyncSender<ManagerMessage>,
    // Use Arc<Mutex> for callbacks to allow registration from multiple threads
    // if EventManager itself is shared (though typically owned by ObsClient).
    // The processing loop will acquire the lock briefly to add new callbacks.
    callbacks: Arc<Mutex<CallbackRegistry>>,
}

impl EventManager {
    /// Creates a new EventManager and starts its processing thread.
    ///
    /// # Arguments
    /// * `event_receiver` - The channel receiver that gets events from the reader thread.
    /// * `buffer_size` - Size of the internal message channel for requests to the manager thread.
    pub fn new(
        event_receiver: Receiver<Result<EventNotification, ObsError>>,
        buffer_size: usize,
    ) -> Self {
        // Channel for sending messages *to* the manager's processing thread
        let (message_sender, message_receiver) = mpsc::sync_channel::<ManagerMessage>(buffer_size);

        let callbacks = Arc::new(Mutex::new(HashMap::new()));
        let callbacks_clone = Arc::clone(&callbacks);

        let processing_thread = thread::spawn(move || {
            Self::processing_loop(event_receiver, message_receiver, callbacks_clone);
        });

        Self {
            processing_thread: Some(processing_thread),
            message_sender,
            callbacks,
        }
    }

    /// The core loop running in the background thread.
    /// It receives events, processes internal messages, checks timeouts, and dispatches.
    fn processing_loop(
        event_receiver: Receiver<Result<EventNotification, ObsError>>,
        message_receiver: Receiver<ManagerMessage>,
        callbacks_registry: Arc<Mutex<CallbackRegistry>>,
    ) {
        info!("Event Manager processing loop started.");
        let mut waiters: WaiterRegistry = HashMap::new();
        // Store events that arrived but whose waiter hadn't registered yet (less common for req/resp)
        let mut pending_events: HashMap<i32, EventNotification> = HashMap::new();

        loop {
            // --- Process Internal Messages First (Non-blocking) ---
            // This allows registering waiters/callbacks even if no OBS events are flowing
            match message_receiver.try_recv() {
                Ok(ManagerMessage::RegisterCallback { event_type, callback }) => {
                    debug!("Manager thread received RegisterCallback for type {}", event_type);
                    let mut callbacks = callbacks_registry.lock().expect("Callback mutex poisoned");
                    callbacks.entry(event_type).or_default().push(callback);
                    drop(callbacks); // Release lock quickly
                    continue; // Check for more messages immediately
                }
                Ok(ManagerMessage::RegisterWaiter { identifier, info }) => {
                    debug!("Manager thread received RegisterWaiter for id {}", identifier);
                    // Check if the event we are waiting for already arrived
                    if let Some(event) = pending_events.remove(&identifier) {
                        debug!("Event for waiter id {} was pending. Forwarding immediately.", identifier);
                        
                        // Check if this is the expected event type
                        if event.event == info.expected_event_type {
                            let _ = info.sender.send(Ok(event)); // Ignore error if receiver dropped
                        } 
                        // Check if this is an error event type
                        else if info.error_event_types.contains(&event.event) {
                            warn!("Pending event for id {} had error type {}. Sending error.", 
                                  identifier, event.event);
                            let _ = info.sender.send(Err(ObsError::EventManagerError(
                                format!("Received error event type: {}", event.event)
                            )));
                        }
                        // Otherwise, this is an unexpected event - log it and continue waiting
                        else {
                            warn!("Pending event for id {} had type {} but expected {}. Continuing to wait.", 
                                  identifier, event.event, info.expected_event_type);
                            // Put the waiter in the waiting map
                            waiters.insert(identifier, info);
                            // We might want to save this event somewhere if it's important,
                            // but for now, we'll just drop it and wait for the next one
                        }
                    } else {
                        // No pending event, store the waiter
                        waiters.insert(identifier, info);
                    }
                    continue; // Check for more messages immediately
                }
                Ok(ManagerMessage::Shutdown) => {
                    info!("Manager thread received Shutdown message.");
                    break; // Exit the outer loop
                }
                Err(TryRecvError::Empty) => {
                    // No internal messages waiting, proceed to check for OBS events
                }
                Err(TryRecvError::Disconnected) => {
                    info!("Manager thread message channel disconnected. Shutting down loop.");
                    break; // Exit the outer loop
                }
            }

            // --- Check for OBS Events (with timeout to allow periodic checks) ---
            match event_receiver.recv_timeout(Duration::from_millis(100)) {
                Ok(Ok(event)) => { // Successfully received an event
                    info!("Manager received event: {:?}", event);
                    let event_type = event.event;
                    let identifier_opt = event.identifier;

                    // 1. Check if a specific task is waiting for this event's identifier
                    if let Some(identifier) = identifier_opt {
                        if let Some(waiter_info) = waiters.remove(&identifier) {
                            debug!("Found waiter for identifier {}. Processing event.", identifier);
                            
                            // Check if the event type matches expectation
                            if event_type == waiter_info.expected_event_type {
                                // Send the successful event
                                if waiter_info.sender.send(Ok(event.clone())).is_err() {
                                    warn!("Waiter for id {} disconnected before receiving event.", identifier);
                                }
                            }
                            // Check if this is an error event type
                            else if waiter_info.error_event_types.contains(&event_type) {
                                warn!("Received error event type {} for id {}. Sending error.", 
                                      event_type, identifier);
                                let err_res = Err(ObsError::EventManagerError(
                                    format!("Received error event type: {}", event_type)
                                ));
                                if waiter_info.sender.send(err_res).is_err() {
                                    warn!("Waiter for id {} disconnected before receiving error.", identifier);
                                }
                            }
                            // Otherwise, this is an unexpected event - log it and continue waiting
                            else {
                                // Type mismatch but not in error list - log and put the waiter back
                                warn!("Waiter for id {} expected type {} but got {}. Continuing to wait.", 
                                      identifier, waiter_info.expected_event_type, event_type);
                                
                                // Put the waiter back in the map to continue waiting
                                waiters.insert(identifier, waiter_info);
                                
                                // We might want to do something with this event, like save it,
                                // but for now, we'll just continue waiting for the next one
                            }
                        } else {
                            // No one waiting *currently*. Store it in case a waiter registers slightly late.
                            debug!("Received event with identifier {}, but no current waiter. Storing.", identifier);
                            pending_events.insert(identifier, event.clone()); // Clone needed for potential callback use
                        }
                    }

                    // 2. Execute registered general callbacks for this event type
                    let callbacks = callbacks_registry.lock().expect("Callback mutex poisoned");
                    if let Some(callback_list) = callbacks.get(&event_type) {
                        if !callback_list.is_empty() {
                            debug!("Executing {} callback(s) for event type {}", callback_list.len(), event_type);
                            for cb in callback_list {
                                // Consider spawning tasks for long-running callbacks if necessary
                                cb(&event);
                            }
                        }
                    }
                    drop(callbacks); // Release lock
                }
                Ok(Err(obs_error)) => { // Error received from the reader thread itself
                    error!("Manager received error from reader thread: {}", obs_error);
                    // This often indicates a fatal I/O or parsing error.
                    // We could try to inform waiters, but the system might be unstable.
                    // Let's log and continue; the loop might break soon if the reader died.
                    // Or maybe trigger shutdown? Let's break for now.
                    break;
                }
                Err(mpsc::RecvTimeoutError::Timeout) => {
                    // No event received, this is normal. Time to check for timed-out waiters.
                    trace!("Manager event recv timeout.");
                }
                Err(mpsc::RecvTimeoutError::Disconnected) => {
                    info!("Manager event channel disconnected (reader thread likely exited). Shutting down loop.");
                    break; // Exit the loop
                }
            }

            // --- Check for Timed-Out Waiters ---
            let now = Instant::now();
            let mut timed_out_ids = Vec::new();
            for (id, info) in &waiters {
                if now >= info.deadline {
                    timed_out_ids.push(*id);
                }
            }
            for id in timed_out_ids {
                if let Some(info) = waiters.remove(&id) {
                    warn!("Waiter for identifier {} (expecting type {}) timed out.", id, info.expected_event_type);
                    let _ = info.sender.send(Err(ObsError::Timeout("Request timed out".to_string()))); // Using Timeout with a String parameter
                }
                // Also remove from pending events if it arrived just now but waiter timed out
                pending_events.remove(&id);
            }

            // Small sleep to prevent busy-looping if channels are very active
            // Only sleep if we didn't process anything significant? Or always?
            // Let's rely on recv_timeout for now.
            // thread::sleep(Duration::from_millis(1));

        } // End main loop

        info!("Event Manager processing loop shutting down.");
        // Notify any remaining waiters that the manager is shutting down
        for (id, info) in waiters.drain() {
            warn!("Notifying waiter for id {} about manager shutdown.", id);
            let _ = info.sender.send(Err(ObsError::EventManagerError("Manager shutdown".to_string())));
        }
        // Clear pending events
        pending_events.clear();
        // Clear callbacks (though registry lives outside thread)
        let mut callbacks = callbacks_registry.lock().expect("Callback mutex poisoned");
        callbacks.clear();
        drop(callbacks);

        info!("Event Manager processing loop finished cleanup.");
    }

    /// Registers a callback function to be executed whenever an event of the specified type is received.
    ///
    /// The callback is executed in the EventManager's internal thread. Avoid long-blocking operations.
    pub fn register_event_callback<F>(&self, event_type: i32, callback: F)
    where
        F: Fn(&EventNotification) + Send + Sync + 'static,
    {
        let boxed_callback = Box::new(callback);
        let msg = ManagerMessage::RegisterCallback {
            event_type,
            callback: boxed_callback,
        };

        if let Err(e) = self.message_sender.send(msg) {
            error!("Failed to send RegisterCallback request to manager thread (maybe shutdown?): {}", e);
            // Potentially panic or return an error if this is critical
        } else {
            info!("Sent request to register callback for event type {}", event_type);
        }
    }

    /// Waits for a specific event identified by `identifier` and attempts to deserialize its payload.
    ///
    /// This is intended to be used internally by `ObsClient::send_command_and_wait`.
    ///
    /// # Arguments
    /// * `identifier` - The identifier linking the command sent to the expected response event.
    /// * `timeout` - Maximum duration to wait for the response.
    /// * `expected_event_type` - The `event` field value expected in the response `EventNotification`.
    /// * `deserializer` - A function that takes the received `EventNotification` and attempts
    ///   to deserialize the required payload type `T`. It should return `Ok(None)` if the event
    ///   is not the correct *type* for this specific response, allowing the wait to continue
    ///   (though type mismatch is usually handled via `expected_event_type`), or `Err` if
    ///   deserialization fails.
    ///
    /// # Returns
    /// * `Ok(T)` - If the expected event is received within the timeout and deserialization succeeds.
    /// * `Err(ObsError)` - If a timeout occurs, the manager shuts down, deserialization fails,
    ///   or the received event indicates an error.
    pub(crate) fn wait_for_response<T, F>(
        &self,
        identifier: i32,
        timeout: Duration,
        expected_event_type: i32,
        error_event_types: Vec<i32>,
        deserializer: F,
    ) -> Result<T, ObsError>
    where
        T: DeserializeOwned + Send + 'static,
        F: FnOnce(&EventNotification) -> Result<Option<T>, ObsError> + Send + 'static,
    {
        // Channel for *this specific* wait request to get the result back from the manager thread
        let (response_sender, response_receiver) = mpsc::channel();
        let deadline = Instant::now() + timeout;

        let waiter_info = WaiterInfo {
            sender: response_sender,
            deadline,
            expected_event_type,
            error_event_types,
        };

        let msg = ManagerMessage::RegisterWaiter {
            identifier,
            info: waiter_info,
        };

        // Send the request to the manager thread to register us as a waiter
        if self.message_sender.send(msg).is_err() {
            error!("Failed to send RegisterWaiter request to manager thread (already shutdown?).");
            return Err(ObsError::EventManagerError("Manager already shutdown".to_string()));
        }

        // Block waiting for the response from the manager thread
        debug!(
            "Waiting for response for identifier {} (expecting type {}, timeout: {:?})...",
            identifier, expected_event_type, timeout
        );

        // We wait slightly longer than the manager's deadline check to account for channel delays
        let recv_timeout = timeout + Duration::from_millis(200);
        match response_receiver.recv_timeout(recv_timeout) {
            Ok(Ok(event)) => {
                // Manager sent back the EventNotification
                debug!("Received event for identifier {}: type {}", identifier, event.event);

                // Double-check identifier (should always match if logic is correct)
                if event.identifier != Some(identifier) {
                     error!("Internal logic error: Received event for wrong identifier {}!", identifier);
                     return Err(ObsError::EventManagerError("Received event with incorrect identifier".to_string()));
                }

                // Event type should already have been validated by the manager loop before sending Ok(event)
                // Now, try to deserialize it using the provided function
                match deserializer(&event) {
                    Ok(Some(payload)) => {
                        debug!("Successfully deserialized payload for identifier {}", identifier);
                        Ok(payload)
                    },
                    Ok(None) => {
                        // Deserializer decided this wasn't the right event after all (unexpected)
                        error!("Deserializer skipped event type {} for identifier {}", event.event, identifier);
                        Err(ObsError::EventManagerError(
                            format!("Unexpected event type: expected {}, received {}", 
                                expected_event_type, event.event)
                        ))
                    }
                    Err(e) => {
                        error!("Deserialization failed for identifier {}: {}", identifier, e);
                        Err(ObsError::Deserialization(format!("Failed to deserialize event payload: {}", e)))
                    }
                }
            }
            Ok(Err(obs_error)) => {
                // Manager sent back an error (Timeout, ManagerShutdown, EventManagerError)
                error!("Received error from manager while waiting for id {}: {:?}", identifier, obs_error);
                Err(obs_error)
            }
            Err(mpsc::RecvTimeoutError::Timeout) => {
                 error!("Timed out waiting for response channel for identifier {}", identifier);
                // This likely means the manager detected the timeout correctly,
                // but potentially our own timeout was slightly different. Or manager died.
                Err(ObsError::Timeout("Request timed out".to_string()))
            }
            Err(mpsc::RecvTimeoutError::Disconnected) => {
                error!("Response channel disconnected while waiting for identifier {} (manager died?).", identifier);
                Err(ObsError::EventManagerError("Manager shutdown or disconnected".to_string()))
            }
        }
    }

    /// Signals the processing thread to shut down and waits for it to finish.
    pub fn shutdown(&mut self) {
        info!("Shutting down Event Manager...");
        // Send shutdown message, ignore error if already closed
        let _ = self.message_sender.send(ManagerMessage::Shutdown);

        // Wait for the manager thread to finish
        if let Some(handle) = self.processing_thread.take() {
            match handle.join() {
                Ok(_) => info!("Event Manager thread joined successfully."),
                Err(e) => error!("Event Manager thread panicked: {:?}", e),
            }
        } else {
            info!("Event Manager thread already joined or never started.");
        }
    }
}

impl Drop for EventManager {
    fn drop(&mut self) {
        // Ensure shutdown is called if EventManager is dropped implicitly
        if self.processing_thread.is_some() {
            warn!("EventManager dropped without explicit shutdown. Attempting cleanup...");
            self.shutdown();
        }
    }
}