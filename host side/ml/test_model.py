#!/usr/bin/env python3
"""
predict_best_of_3_fulltf.py

Use the full TensorFlow + Keras 3.12 model (paper_plastic_model.keras)
to predict on 3 images and do best-of-3.

Usage (from Project/):
    python ml/predict_best_of_3_fulltf.py
or:
    python ml/predict_best_of_3_fulltf.py images

- Expects a folder with at least 3 images (jpg/png).
- Uses keras.models.load_model to load the Keras 3.12 model.
- Uses EfficientNet preprocess_input (same as training).
- Prints per-image prediction and best-of-3 result.
"""

import os
import sys
import numpy as np

import keras
from keras.applications import efficientnet

# Optional: if you want to use tf ops (not strictly needed here)
import tensorflow as tf

# ---------------------------------------------------------------------
# Paths
# ---------------------------------------------------------------------
PROJECT_DIR = os.path.dirname(os.path.abspath(__file__))  # this is ml/
PROJECT_DIR = os.path.dirname(PROJECT_DIR)                # go up to Project/
ML_DIR       = os.path.join(PROJECT_DIR, "ml")
MODEL_PATH   = os.path.join(ML_DIR, "paper_plastic_model_2.keras")

# Default images dir: Project/images
DEFAULT_IMAGES_DIR = os.path.join(PROJECT_DIR, "images")

# ---------------------------------------------------------------------
# Model + preprocessing
# ---------------------------------------------------------------------
class_names = ["paper", "plastic"]
preprocess_input = efficientnet.preprocess_input

def load_model():
    print(f"Loading Keras 3 model from: {MODEL_PATH}")
    model = keras.models.load_model(MODEL_PATH)
    print("Model loaded.")
    print("input_shape:", model.input_shape)
    print("output_shape:", model.output_shape)
    return model

def load_three_images(images_dir):
    # Collect candidate images (SSjpg/png)
    entries = sorted(
        f for f in os.listdir(images_dir)
        if f.lower().endswith((".jpg", ".jpeg", ".png", ".bmp"))
    )

    if len(entries) < 3:
        raise RuntimeError(f"Need at least 3 images in {images_dir}, found {len(entries)}")

    img_paths = [os.path.join(images_dir, f) for f in entries[:3]]
    print("Images used:")
    for p in img_paths:
        print(" ", os.path.basename(p))
    print()
    return img_paths

def predict_image(model, img_path):
    """Return (pred_label, confidence_pct, raw_probs_array)."""
    # Load and resize to 96x96 (your model input)
    img = keras.utils.load_img(img_path, target_size=(96, 96))
    img_array = keras.utils.img_to_array(img)  # (96, 96, 3)
    img_array = np.expand_dims(img_array, axis=0)  # (1, 96, 96, 3)

    # EfficientNet-style preprocess (same as training)
    img_array = preprocess_input(img_array)

    # Predict
    preds = model.predict(img_array, verbose=0)[0]  # shape (2,)
    pred_idx = int(np.argmax(preds))
    pred_label = class_names[pred_idx]
    confidence = float(preds[pred_idx]) * 100.0

    return pred_label, confidence, preds

def main():
    # Pick images directory
    if len(sys.argv) > 1:
        images_dir = sys.argv[1]
        if not os.path.isabs(images_dir):
            images_dir = os.path.join(PROJECT_DIR, images_dir)
    else:
        images_dir = DEFAULT_IMAGES_DIR

    if not os.path.isdir(images_dir):
        raise RuntimeError(f"Images directory does not exist: {images_dir}")

    model = load_model()
    img_paths = load_three_images(images_dir)

    votes = {name: 0 for name in class_names}
    details = []

    for img_path in img_paths:
        pred_label, confidence, probs = predict_image(model, img_path)
        votes[pred_label] += 1

        # For nice printing:
        paper_prob   = float(probs[0])
        plastic_prob = float(probs[1])

        print(
            f"{os.path.basename(img_path)} -> {pred_label} ({confidence:.2f}%)   "
            f"[paper={paper_prob:.3f}, plastic={plastic_prob:.3f}]"
        )
        details.append((img_path, pred_label, confidence, probs))

    # Decide best-of-3
    paper_votes   = votes["paper"]
    plastic_votes = votes["plastic"]

    if paper_votes > plastic_votes:
        final = "paper"
    elif plastic_votes > paper_votes:
        final = "plastic"
    else:
        # tie-breaker: sum probabilities
        paper_sum   = sum(d[3][0] for d in details)
        plastic_sum = sum(d[3][1] for d in details)
        final = "paper" if paper_sum >= plastic_sum else "plastic"

    print()
    print(f"Best-of-3 result: {final}")

if __name__ == "__main__":
    main()
