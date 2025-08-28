use std::io::Write;
use std::mem::size_of;

#[repr(C)]
#[derive(Debug, Clone)]
pub struct IPCMessage {
    pub command: i32,
    pub window_handle: usize,
    pub window_title: [u8; 256],
}

#[derive(Debug, Clone, Copy)]
pub enum TraymondCommand {
    MinimizeCurrent,
    MinimizeByHandle(usize),
    ShowAll,
    Exit,
}

impl From<TraymondCommand> for IPCMessage {
    fn from(cmd: TraymondCommand) -> Self {
        match cmd {
            TraymondCommand::MinimizeCurrent => IPCMessage {
                command: 1,
                window_handle: 0,
                window_title: [0; 256],
            },
            TraymondCommand::MinimizeByHandle(handle) => IPCMessage {
                command: 2,
                window_handle: handle,
                window_title: [0; 256],
            },
            TraymondCommand::ShowAll => IPCMessage {
                command: 3,
                window_handle: 0,
                window_title: [0; 256],
            },
            TraymondCommand::Exit => IPCMessage {
                command: 4,
                window_handle: 0,
                window_title: [0; 256],
            },
        }
    }
}

pub struct TraymondClient;

impl TraymondClient {
    /// Send a command to the Traymond application
    pub fn send_command(cmd: TraymondCommand) -> Result<(), Box<dyn std::error::Error>> {
        let mut pipe = std::fs::File::open(r"\\.\pipe\traymond_ipc")?;
        
        let message: IPCMessage = cmd.into();
        
        // Convert struct to bytes
        let bytes = unsafe {
            std::slice::from_raw_parts(
                &message as *const IPCMessage as *const u8,
                size_of::<IPCMessage>()
            )
        };
        
        pipe.write_all(bytes)?;
        pipe.flush()?;
        
        Ok(())
    }
    
    /// Minimize the currently focused window to tray
    pub fn minimize_current() -> Result<(), Box<dyn std::error::Error>> {
        Self::send_command(TraymondCommand::MinimizeCurrent)
    }
    
    /// Minimize a specific window by its handle to tray
    pub fn minimize_by_handle(handle: usize) -> Result<(), Box<dyn std::error::Error>> {
        Self::send_command(TraymondCommand::MinimizeByHandle(handle))
    }
    
    /// Show all hidden windows
    pub fn show_all() -> Result<(), Box<dyn std::error::Error>> {
        Self::send_command(TraymondCommand::ShowAll)
    }
    
    /// Exit the Traymond application
    pub fn exit() -> Result<(), Box<dyn std::error::Error>> {
        Self::send_command(TraymondCommand::Exit)
    }
    
    /// Check if Traymond is running by attempting to connect to the pipe
    pub fn is_running() -> bool {
        std::fs::File::open(r"\\.\pipe\traymond_ipc").is_ok()
    }
}

// Example usage for egui integration
#[cfg(test)]
mod tests {
    use super::*;
    
    #[test]
    fn test_minimize_current() {
        if TraymondClient::is_running() {
            let result = TraymondClient::minimize_current();
            println!("Minimize current result: {:?}", result);
        } else {
            println!("Traymond is not running");
        }
    }
    
    #[test]
    fn test_show_all() {
        if TraymondClient::is_running() {
            let result = TraymondClient::show_all();
            println!("Show all result: {:?}", result);
        } else {
            println!("Traymond is not running");
        }
    }
}
