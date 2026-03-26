#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
USB 摄像头垃圾观测（仅识别显示，不分拣、不串口、不 I2C）

该脚本尽量“无自定义分类逻辑”：展示由 YOLO 模型返回的类别名与置信度。
本版本要求**完全使用 Hugging Face 模型**（只接受模型 ID），不再使用本地 .pt/.onnx 权重。

依赖：pip install -r requirements.txt

用法：
  python garbage_observe.py
  set GARBAGE_MODEL=C:/path/best.pt && python garbage_observe.py
"""

import os
import sys
from pathlib import Path

import cv2

# ============ 可改参数 ============
CAMERA_INDEX = int(os.getenv("GARBAGE_CAMERA_INDEX", "0"))
FRAME_WIDTH = int(os.getenv("GARBAGE_FRAME_WIDTH", "640"))
FRAME_HEIGHT = int(os.getenv("GARBAGE_FRAME_HEIGHT", "480"))
CONF_THRESHOLD = float(os.getenv("GARBAGE_CONF", "0.5"))
_DEFAULT_HF_MODEL_ID = "kendrickfff/waste-classification-yolov8-ken"


def _resolve_model_path():
    env = os.getenv("GARBAGE_MODEL")
    if env:
        # 强制按 Hugging Face 模型 ID 使用（例如：kendrickfff/xxx）
        # 如果传了本地路径，直接报错，避免误用。
        p = Path(env).expanduser()
        if p.is_file():
            raise ValueError(
                "你传入的是本地权重路径，但当前脚本要求完全使用 Hugging Face 模型。\n"
                "请把 GARBAGE_MODEL 设置成 Hugging Face 模型 ID（例如：kendrickfff/waste-classification-yolov8-ken）。"
            )
        return env

    return _DEFAULT_HF_MODEL_ID


def main():
    try:
        from ultralytics import YOLO
    except ImportError:
        print("请先安装: pip install ultralytics opencv-python")
        return 1

    model_path = _resolve_model_path()

    print(f"[INFO] HF 模型: {model_path}")
    print(f"[INFO] 摄像头索引: {CAMERA_INDEX}  分辨率: {FRAME_WIDTH}x{FRAME_HEIGHT}")

    model = YOLO(str(model_path))

    cap = cv2.VideoCapture(CAMERA_INDEX)
    cap.set(cv2.CAP_PROP_FRAME_WIDTH, FRAME_WIDTH)
    cap.set(cv2.CAP_PROP_FRAME_HEIGHT, FRAME_HEIGHT)
    cap.set(cv2.CAP_PROP_BUFFERSIZE, 1)

    if not cap.isOpened():
        print(f"[ERROR] 无法打开摄像头 index={CAMERA_INDEX}")
        return 1

    window = "garbage_observe (q=quit)"
    cv2.namedWindow(window, cv2.WINDOW_NORMAL)

    try:
        while True:
            ok, frame = cap.read()
            if not ok:
                continue

            results = model.predict(
                source=frame,
                conf=CONF_THRESHOLD,
                verbose=False,
            )
            r = results[0]
            names = r.names if r.names else {}
            # ultralytics 0-index class id
            if r.boxes is not None and len(r.boxes):
                for box in r.boxes:
                    cls_id = int(box.cls[0])
                    conf = float(box.conf[0])
                    xyxy = box.xyxy[0].cpu().numpy()
                    x1, y1, x2, y2 = map(int, xyxy)

                    label_key = names.get(cls_id, str(cls_id))
                    text = f"{label_key} {conf:.2f}"

                    cv2.rectangle(frame, (x1, y1), (x2, y2), (0, 255, 0), 2)
                    cv2.putText(
                        frame,
                        text,
                        (x1, max(0, y1 - 8)),
                        cv2.FONT_HERSHEY_SIMPLEX,
                        0.5,
                        (0, 0, 255),
                        1,
                        cv2.LINE_AA,
                    )

            cv2.putText(
                frame,
                "observe only | q=quit",
                (8, 24),
                cv2.FONT_HERSHEY_SIMPLEX,
                0.7,
                (255, 255, 0),
                2,
            )
            cv2.imshow(window, frame)

            key = cv2.waitKey(1) & 0xFF
            if key == ord("q") or key == 27:
                break
    finally:
        cap.release()
        cv2.destroyAllWindows()

    return 0


if __name__ == "__main__":
    sys.exit(main())
