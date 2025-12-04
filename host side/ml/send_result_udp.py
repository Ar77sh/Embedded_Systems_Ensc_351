#!/usr/bin/env python3
import os
import socket
import subprocess
import sys
import re

BEAGLE_IP = "192.168.7.2"   # change if needed
PORT = 5005                 # must match UDP listener on Beagle

def extract_final_label(stdout_text):
    """
    Extract 'paper' or 'plastic' from the output of test_model.py.
    We look for the line:
        Best-of-3 result: X
    """
    for line in stdout_text.splitlines():
        if "Best-of-3 result:" in line:
            parts = line.split(":")
            if len(parts) == 2:
                return parts[1].strip().lower()
    return None

def main():
    script_dir = os.path.dirname(os.path.abspath(__file__))

    # Run the full TF predictor
    process_output = subprocess.check_output(
        [sys.executable, os.path.join(script_dir, "test_model.py")],
        text=True
    )

    print("Raw model output:\n", process_output)

    # Extract only “paper” or “plastic”
    final_label = extract_final_label(process_output)

    if final_label is None:
        print("ERROR: Could not extract final prediction!")
        return

    print("Clean prediction:", final_label)

    # Send over UDP
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.sendto(final_label.encode(), (BEAGLE_IP, PORT))
    sock.close()

    print(f"Sent to BeagleBone ({BEAGLE_IP}:{PORT}):", final_label)

if __name__ == "__main__":
    main()
