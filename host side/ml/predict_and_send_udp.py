#!/usr/bin/env python3
"""
predict_and_send_udp.py

- Runs TFLite best-of-3 on images/ (same logic as your working script).
- Computes majority vote: "paper" or "plastic".
- Sends JUST that word via UDP to the Beagle.

Run from Project/:

    conda activate tfenv
    python ml/predict_and_send_udp.py
"""

import os
from collections import Counter
import socket

import numpy as np
import tensorflow as tf
from tensorflow.keras.applications import efficientnet

# ----------------------- CONFIG -----------------------

# EfficientNet preprocessing (same as training)
preprocess_input = efficientnet.preprocess_input

THIS_DIR = os.path.dirname(os.path.abspath(__file__))
PROJECT_ROOT = os.path.dirname(THIS_DIR)

IMAGE_DIR = os.path.join(PROJECT_ROOT, "images")
TFLITE_PATH = os.path.join(THIS_DIR, "paper_plastic_model.tflite")

# *** CHANGE THIS TO YOUR BEAGLE’S INFO ***
BEAGLE_IP = "192.168.7.2"   # Replace if different
BEAGLE_PORT = 5005          # Must match your Beagle UDP server

# Model output classes
class_names = ["paper", "plastic"]

# -------------------------------------------------------


def load_interpreter():
    interpreter = tf.lite.Interpreter(model_path=TFLITE_PATH)
    interpreter.allocate_tensors()
    return interpreter


def preprocess_image(img_path, input_shape):
    """
    Preprocess one image to match EfficientNet input.
    """
    _, h, w, _ = input_shape
    img = tf.keras.utils.load_img(img_path, target_size=(h, w))
    img_array = tf.keras.utils.img_to_array(img)
    img_array = tf.expand_dims(img_array, axis=0)
    img_array = preprocess_input(img_array)
    return img_array.numpy()


def predict_single(interpreter, img_path):
    input_details = interpreter.get_input_details()
    output_details = interpreter.get_output_details()

    input_shape = input_details[0]["shape"]
    input_dtype = input_details[0]["dtype"]

    x = preprocess_image(img_path, input_shape)

    # Quantized vs float handling
    if np.issubdtype(input_dtype, np.floating):
        x_in = x.astype(input_dtype)
    else:
        scale, zero_point = input_details[0].get("quantization", (1.0, 0))
        x_in = x / scale + zero_point
        x_in = x_in.astype(input_dtype)

    interpreter.set_tensor(input_details[0]["index"], x_in)
    interpreter.invoke()

    y = interpreter.get_tensor(output_details[0]["index"])[0]

    # Dequantize if needed
    if not np.issubdtype(y.dtype, np.floating):
        scale, zero_point = output_details[0].get("quantization", (1.0, 0))
        y = scale * (y.astype(np.float32) - zero_point)

    probs = tf.nn.softmax(y).numpy()
    idx = int(np.argmax(probs))
    label = class_names[idx]
    conf = float(np.max(probs) * 100.0)

    return label, conf, probs


def send_udp(msg: str):
    """Send the final label ('paper' or 'plastic') to Beagle."""
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    try:
        sock.sendto(msg.encode("utf-8"), (BEAGLE_IP, BEAGLE_PORT))
    finally:
        sock.close()


def main():
    interpreter = load_interpreter()

    # Get first 3 images
    images = [
        f for f in os.listdir(IMAGE_DIR)
        if f.lower().endswith((".jpg", ".jpeg", ".png", ".bmp"))
    ]
    images.sort()

    if len(images) < 3:
        raise SystemExit(
            f"Need at least 3 images in {IMAGE_DIR}, found {len(images)}"
        )

    paths = [os.path.join(IMAGE_DIR, f) for f in images[:3]]

    print("\nImages used:")
    for p in paths:
        print(" ", os.path.basename(p))

    print("\nPer-image predictions:")
    print("-----------------------")

    votes = []

    for p in paths:
        label, conf, probs = predict_single(interpreter, p)
        votes.append(label)
        probs_str = ", ".join(
            f"{class_names[i]}={probs[i]:.3f}" for i in range(len(class_names))
        )
        print(f"{os.path.basename(p)} -> {label} ({conf:.2f}%)   [{probs_str}]")

    # Majority vote
    winner = Counter(votes).most_common(1)[0][0]

    print("\nBest-of-3 result:", winner)

    # Send to Beagle
    print(f"Sending '{winner}' to {BEAGLE_IP}:{BEAGLE_PORT} via UDP...")
    send_udp(winner)
    print("Done.\n")


if __name__ == "__main__":
    main()
