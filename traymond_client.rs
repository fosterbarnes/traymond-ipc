use std::io::{Read, Write};
use std::mem::size_of;

#[repr(C)]
#[derive(Debug)]
struct IPCMessage {
    command: i32,
    window_handle: usize,
    window_title: [u8; 256],
}

const IPC_MINIMIZE_CURRENT: i32 = 1;
const IPC_MINIMIZE_BY_HANDLE: i32 = 2;
const IPC_SHOW_ALL: i32 = 3;
const IPC_EXIT: i32 = 4;

fn send_ipc_command(command: i32, window_handle: usize) -> Result<(), Box<dyn std::error::Error>> {
    let mut pipe = std::fs::File::open(r"\\.\pipe\traymond_ipc")?;
    
    let message = IPCMessage {
        command,
        window_handle,
        window_title: [0; 256],
    };
    
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

fn main() -> Result<(), Box<dyn std::error::Error>> {
    println!("Traymond IPC Client");
    println!("Available commands:");
    println!("1. Minimize current window");
    println!("2. Minimize window by handle");
    println!("3. Show all windows");
    println!("4. Exit Traymond");
    
    let mut input = String::new();
    std::io::stdin().read_line(&mut input)?;
    
    let command: i32 = input.trim().parse()?;
    
    match command {
        1 => {
            println!("Sending minimize current window command...");
            send_ipc_command(IPC_MINIMIZE_CURRENT, 0)?;
        }
        2 => {
            println!("Enter window handle (hex):");
            let mut handle_input = String::new();
            std::io::stdin().read_line(&mut handle_input)?;
            let handle = usize::from_str_radix(handle_input.trim(), 16)?;
            println!("Sending minimize window by handle command...");
            send_ipc_command(IPC_MINIMIZE_BY_HANDLE, handle)?;
        }
        3 => {
            println!("Sending show all windows command...");
            send_ipc_command(IPC_SHOW_ALL, 0)?;
        }
        4 => {
            println!("Sending exit command...");
            send_ipc_command(IPC_EXIT, 0)?;
        }
        _ => {
            println!("Invalid command");
        }
    }
    
    println!("Command sent successfully!");
    Ok(())
}
