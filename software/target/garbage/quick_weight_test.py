#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
quick_weight_test.py

快速验证本地 YOLOv8 权重在 USB 摄像头上的效果。

默认权重：
  ./yolov8n-waste-12cls-best.pt

用法：
  python quick_weight_test.py
  python quick_weight_test.py --model "C:/path/to/yolov8n-waste-12cls-best.pt" --camera 0 --conf 0.35
"""

import argparse
import sys
from pathlib import Path

import cv2


def build_parser() -> argparse.ArgumentParser:
    script_dir = Path(__file__).resolve().parent
    parser = argparse.ArgumentParser(description="YOLOv8 本地权重 USB 摄像头快速测试")
    parser.add_argument(
        "--model",
        default=str(script_dir / "yolov8n-waste-12cls-best.pt"),
        help="本地 .pt/.onnx 权重路径",
    )
    parser.add_argument("--camera", type=int, default=1, help="USB 摄像头索引，默认 0")
    parser.add_argument("--conf", type=float, default=0.35, help="置信度阈值，默认 0.35")
    parser.add_argument("--width", type=int, default=640, help="画面宽，默认 640")
    parser.add_argument("--height", type=int, default=480, help="画面高，默认 480")
    return parser


def main() -> int:
    args = build_parser().parse_args()

    model_path = Path(args.model).expanduser().resolve()
    if not model_path.is_file():
        print(f"[ERROR] 模型文件不存在: {model_path}")
        return 1

    try:
        from ultralytics import YOLO
    except ImportError:
        print("[ERROR] 缺少依赖，请先安装: pip install ultralytics opencv-python")
        return 1

    print(f"[INFO] 模型: {model_path}")
    print(f"[INFO] 摄像头: {args.camera}  分辨率: {args.width}x{args.height}  conf: {args.conf}")
    print("[INFO] 按 q 或 ESC 退出")

    model = YOLO(str(model_path))

    cap = cv2.VideoCapture(args.camera)
    cap.set(cv2.CAP_PROP_FRAME_WIDTH, args.width)
    cap.set(cv2.CAP_PROP_FRAME_HEIGHT, args.height)
    cap.set(cv2.CAP_PROP_BUFFERSIZE, 1)
    if not cap.isOpened():
        print(f"[ERROR] 无法打开摄像头 index={args.camera}")
        return 1

    window = "quick_weight_test (q=quit)"
    cv2.namedWindow(window, cv2.WINDOW_NORMAL)

    try:
        while True:
            ok, frame = cap.read()
            if not ok:
                continue

            results = model.predict(source=frame, conf=args.conf, verbose=False)
            r = results[0]
            names = r.names if r.names else {}

            if r.boxes is not None and len(r.boxes):
                for box in r.boxes:
                    cls_id = int(box.cls[0])
                    score = float(box.conf[0])
                    x1, y1, x2, y2 = map(int, box.xyxy[0].cpu().numpy())
                    label = names.get(cls_id, str(cls_id))
                    text = f"{label} {score:.2f}"

                    cv2.rectangle(frame, (x1, y1), (x2, y2), (40, 255, 40), 2)
                    cv2.putText(
                        frame,
                        text,
                        (x1, max(0, y1 - 8)),
                        cv2.FONT_HERSHEY_SIMPLEX,
                        0.6,
                        (0, 0, 255),
                        2,
                        cv2.LINE_AA,
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
