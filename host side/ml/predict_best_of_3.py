#!/usr/bin/env python3

import os
from collections import Counter

import numpy as np
import tensorflow as tf
from tensorflow.keras.applications import efficientnet

# EfficientNet preprocessing
preprocess_input = efficientnet.preprocess_input

# Paths
THIS_DIR = os.path.dirname(os.path.abspath(__file__))
PROJECT_ROOT = os.path.dirname(THIS_DIR)

IMAGE_DIR = os.path.join(PROJECT_ROOT, "images")
TFLITE_PATH = os.path.join(THIS_DIR, "paper_plastic_model.tflite")
OUTPUT_PATH = os.path.join(THIS_DIR, "prediction_output.txt")

# Class order – flip if backwards
class_names = ["paper", "plastic"]


def load_interpreter():
    interpreter = tf.lite.Interpreter(model_path=TFLITE_PATH)
    interpreter.allocate_tensors()
    return interpreter


def preprocess_image(img_path, input_shape):
    """
    input_shape: e.g. (1, 96, 96, 3)
    Returns numpy array matching input_shape with EfficientNet preprocessing.
    """
    _, h, w, c = input_shape
    img = tf.keras.utils.load_img(img_path, target_size=(h, w))
    img_array = tf.keras.utils.img_to_array(img)
    img_array = tf.expand_dims(img_array, axis=0)  # (1, h, w, 3)

    # EfficientNet preprocessing (same as training)
    img_array = preprocess_input(img_array)

    return img_array.numpy()


def predict_single(interpreter, img_path):
    input_details = interpreter.get_input_details()
    output_details = interpreter.get_output_details()

    input_shape = input_details[0]["shape"]
    input_dtype = input_details[0]["dtype"]

    x = preprocess_image(img_path, input_shape)

    # Handle float vs quantized input
    if np.issubdtype(input_dtype, np.floating):
        x_in = x.astype(input_dtype)
    else:
        scale, zero_point = input_details[0].get("quantization", (1.0, 0))
        x_in = x / scale + zero_point
        x_in = x_in.astype(input_dtype)

    interpreter.set_tensor(input_details[0]["index"], x_in)
    interpreter.invoke()

    y = interpreter.get_tensor(output_details[0]["index"])[0]

    # Dequantize output if needed
    if not np.issubdtype(y.dtype, np.floating):
        scale, zero_point = output_details[0].get("quantization", (1.0, 0))
        y = scale * (y.astype(np.float32) - zero_point)

    # Softmax for safety
    probs = tf.nn.softmax(y).numpy()
    idx = int(np.argmax(probs))
    label = class_names[idx]
    conf = float(np.max(probs) * 100.0)

    return label, conf, probs


def main():
    interpreter = load_interpreter()

    # Grab images
    exts = (".jpg", ".jpeg", ".png", ".bmp")
    images = [f for f in os.listdir(IMAGE_DIR) if f.lower().endswith(exts)]
    images.sort()

    if len(images) < 3:
        raise SystemExit(f"Need at least 3 images in {IMAGE_DIR}, found {len(images)}")

    image_paths = [os.path.join(IMAGE_DIR, f) for f in images[:3]]

    print("\nImages used:")
    for p in image_paths:
        print(" ", os.path.basename(p))

    lines = []
    lines.append(f"TFLITE_PATH: {TFLITE_PATH}")
    lines.append(f"IMAGE_DIR: {IMAGE_DIR}")
    lines.append("")
    lines.append("Per-image predictions:")

    votes = []

    for p in image_paths:
        label, conf, probs = predict_single(interpreter, p)
        votes.append(label)
        probs_str = ", ".join(
            f"{class_names[i]}={probs[i]:.3f}" for i in range(len(class_names))
        )
        line = f"{os.path.basename(p)} -> {label} ({conf:.2f}%)   [{probs_str}]"
        print(line)
        lines.append(line)

    winner = Counter(votes).most_common(1)[0][0]
    lines.append("")
    lines.append(f"Best-of-3 result: {winner}")

    # Save to txt so we can inspect later
    with open(OUTPUT_PATH, "w") as f:
        f.write("\n".join(lines))

    print("\nBest-of-3 result:", winner)
    print(f"Details written to {OUTPUT_PATH}")


if __name__ == "__main__":
    main()
