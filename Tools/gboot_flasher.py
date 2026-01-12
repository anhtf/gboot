import serial
import time
import argparse
import os
import sys

# Protocol Constants
CMD_SYNC        = 0x5A
GBOOT_ACK       = 0x79
GBOOT_NACK      = 0x1F

CMD_GET_INFO    = 0x50
CMD_ERASE_APP   = 0x02
CMD_WRITE_PAGE  = 0x03
CMD_RESET_DEV   = 0x04
CMD_JUMP_APP    = 0x05

APP_START_ADDR  = 0x08004000
PAGE_SIZE       = 1024

class GBootFlasher:
    def __init__(self, port, baudrate=460800):
        self.ser = serial.Serial(
            port=port,
            baudrate=baudrate,
            parity=serial.PARITY_NONE,
            stopbits=serial.STOPBITS_ONE,
            bytesize=serial.EIGHTBITS,
            timeout=1
        )
        print(f"[GBoot] Opened {port} @ {baudrate}")

    def close(self):
        self.ser.close()

    def sync(self):
        print("[GBoot] Attempting to Sync...")
        # Send SYNC byte
        self.ser.write(bytes([CMD_SYNC]))
        # We don't necessarily get an ACK immediately on Sync, 
        # usually we send Sync then Command.
        # But our protocol implementation might expect [SYNC][CMD] or just [CMD] after idle.
        # Based on gboot.c: It handles IDLE.
        # So we can just send commands. But wait, gboot.c code says:
        # "if (this->rx_len < 1) return;"
        # Just sending the command is enough as long as it's a single packet.
        pass

    def get_info(self):
        cmd = bytes([CMD_GET_INFO])
        self.ser.write(cmd)
        
        ack = self.ser.read(1)
        if len(ack) > 0 and ack[0] == GBOOT_ACK:
            data = self.ser.read(2)
            print(f"[GBoot] Device Connected. Version: {data[0]}.{data[1]}")
            return True
        else:
            print("[GBoot] Failed to get info (No ACK)")
            return False

    def erase(self):
        print("[GBoot] Erasing App Flash... (this may take a few seconds)")
        cmd = bytes([CMD_ERASE_APP])
        self.ser.write(cmd)
        
        # Erase takes time, increase timeout temporarily
        old_timeout = self.ser.timeout
        self.ser.timeout = 10 
        
        ack = self.ser.read(1)
        self.ser.timeout = old_timeout
        
        if len(ack) > 0 and ack[0] == GBOOT_ACK:
            print("[GBoot] Erase Successful.")
            return True
        else:
            print("[GBoot] Erase Failed or Timed out.")
            return False

    def write_flash(self, bin_path):
        if not os.path.exists(bin_path):
            print(f"Error: File {bin_path} not found.")
            return False
            
        file_size = os.path.getsize(bin_path)
        print(f"[GBoot] Flashing {bin_path} ({file_size} bytes)")
        
        with open(bin_path, 'rb') as f:
            data = f.read()
            
        num_pages = (file_size + PAGE_SIZE - 1) // PAGE_SIZE
        
        for i in range(num_pages):
            offset = i * PAGE_SIZE
            chunk = data[offset : offset + PAGE_SIZE]
            
            # Pad chunk if needed
            if len(chunk) < PAGE_SIZE:
                chunk += b'\xFF' * (PAGE_SIZE - len(chunk))
                
            current_addr = APP_START_ADDR + offset
            
            # Prepare packet: [CMD][Addr:4][Data:1024]
            packet = bytearray()
            packet.append(CMD_WRITE_PAGE)
            packet.append((current_addr >> 0) & 0xFF)
            packet.append((current_addr >> 8) & 0xFF)
            packet.append((current_addr >> 16) & 0xFF)
            packet.append((current_addr >> 24) & 0xFF)
            packet.extend(chunk)
            
            print(f"\r[GBoot] Writing Page {i+1}/{num_pages} @ 0x{current_addr:08X}...", end='', flush=True)
            
            self.ser.write(packet)
            ack = self.ser.read(1)
            
            if len(ack) == 0 or ack[0] != GBOOT_ACK:
                print(f"\n[GBoot] Write Failed at Page {i+1}!")
                return False
                
        print("\n[GBoot] Flash Write Complete.")
        return True

    def jump_to_app(self):
        print("[GBoot] Jumping to Application...")
        cmd = bytes([CMD_JUMP_APP])
        self.ser.write(cmd)
        ack = self.ser.read(1) # Optional wait

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='gboot Flasher Tool')
    parser.add_argument('port', help='Serial Port (e.g. /dev/ttyUSB0, COM3)')
    parser.add_argument('file', help='Path to binary (.bin) file')
    
    args = parser.parse_args()
    
    flasher = GBootFlasher(args.port)
    
    # Simple flow
    if flasher.get_info():
        if flasher.erase():
            if flasher.write_flash(args.file):
                flasher.jump_to_app()
                print("[GBoot] Done.")
            else:
                print("[GBoot] Flashing Failed.")
        else:
            print("[GBoot] Erase Failed.")
    else:
        print("[GBoot] Could not connect to Bootloader. (Is device in Boot mode?)")
        
    flasher.close()
