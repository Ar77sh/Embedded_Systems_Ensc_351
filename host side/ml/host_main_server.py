#!/usr/bin/env python3
import socket
import subprocess
import sys
import os

# Ensure we run from the Project root: /home/gavinbell/Project
PROJECT_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
os.chdir(PROJECT_ROOT)

LISTEN_IP = "0.0.0.0"
LISTEN_PORT = 6000  # Beagle sends "start" here

def run_cmd(cmd):
    print(f"[host_main_server] Running: {' '.join(cmd)}")
    result = subprocess.run(cmd)
    if result.returncode != 0:
        print(f"[host_main_server] ERROR: command failed with code {result.returncode}")
        return False
    return True

def main():
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.bind((LISTEN_IP, LISTEN_PORT))
    print(f"[host_main_server] Listening for 'start' on UDP {LISTEN_PORT}...")

    while True:
        data, addr = sock.recvfrom(1024)
        msg = data.decode(errors="ignore").strip().lower()
        print(f"[host_main_server] Received '{msg}' from {addr}")

        if msg == "start":
            print("[host_main_server] Triggering capture + ML + UDP send...")

            # 1) Take 3 photos
            if not run_cmd(["python", "ml/capture_three_photos_vm.py"]):
                print("[host_main_server] Skipping ML step due to capture error.")
                continue

            # 2) Run prediction + send result to Beagle
            #    This assumes predict_and_send_udp.py:
            #      - loads paper_plastic_model_2.keras
            #      - does best-of-3 on Project/images
            #      - sends 'paper' or 'plastic' to Beagle:5005
            if not run_cmd(["python", "ml/predict_and_send_udp.py"]):
                print("[host_main_server] ML/UDP send step failed.")
                continue

            print("[host_main_server] Pipeline finished for this start request.")
        else:
            print("[host_main_server] Ignoring unknown command")

if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        print("\n[host_main_server] Shutting down.")
        sys.exit(0)
