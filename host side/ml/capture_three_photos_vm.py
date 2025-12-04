import cv2
import os
import time
import glob

# EXACT path for your VM (confirmed from your tree)
IMAGES_DIR = "/home/gavinbell/Project/images"

# 1. Make sure the folder exists
os.makedirs(IMAGES_DIR, exist_ok=True)

# 2. Delete old images
for f in glob.glob(os.path.join(IMAGES_DIR, "*.jpg")):
    os.remove(f)
print("Deleted old images from Project/images/")

# 3. Open Brio 100 camera (try both /dev/video0 and /dev/video1)
def open_brio():
    for idx in (0, 1):
        cam = cv2.VideoCapture(idx)
        if cam.isOpened():
            print(f"Opened Brio 100 on /dev/video{idx}")
            return cam
        cam.release()
    return None

cam = open_brio()
if cam is None:
    print("ERROR: Could not open the Brio 100 webcam. Check VirtualBox camera attachment.")
    raise SystemExit(1)

# Optional: set a consistent resolution
cam.set(cv2.CAP_PROP_FRAME_WIDTH, 1280)
cam.set(cv2.CAP_PROP_FRAME_HEIGHT, 720)

print("Camera ready. Stabilizing exposure...")
# Warm up: read a few frames and ignore them so exposure settles
for _ in range(10):
    cam.read()
    time.sleep(0.1)

print("Capturing 3 photos...")

# 4. Capture 3 frames with per-shot stabilization + brightness boost
for i in range(3):
    # Let the camera adjust between shots
    time.sleep(1.0)

    frame = None
    ret = False

    # Read several frames and keep the last one so exposure is stable
    for _ in range(5):
        ret, frame = cam.read()
        time.sleep(0.1)

    if not ret or frame is None:
        print(f"ERROR: Could not capture image {i+1}")
        continue

    # ---- Brighten the image a bit here ----
    # alpha: scale (>1 = brighter), beta: offset (>0 = brighter)
    frame_out = cv2.convertScaleAbs(frame, alpha=1.3, beta=20)
    # ---------------------------------------

    filename = os.path.join(IMAGES_DIR, f"image_{i+1}.jpg")
    cv2.imwrite(filename, frame_out)
    print(f"Saved {filename}")

cam.release()
print("Done: 3 fresh images saved to ~/Project/images")
