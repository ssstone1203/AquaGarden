#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
鱼类检测推理脚本（支持图片或视频）。

依赖：
  pip install "ultralytics>=8.3.0"

示例：
  # 图片推理
  python infer_yolo11n_fish.py --source "D:/Code/data/fish/test.jpg"

  # 视频推理
  python infer_yolo11n_fish.py --source "D:/Code/data/fish/test.mp4"

  # 指定权重与保存目录
  python infer_yolo11n_fish.py --model "D:/Code/AquaGarden/model/yolo_fish/runs/yolo11n_fish/weights/best.pt" ^
    --source "D:/Code/data/fish/test.mp4" --project "D:/Code/AquaGarden/model/yolo_fish/infer" --name "exp1"
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

SCRIPT_DIR = Path(__file__).resolve().parent
REPO_ROOT = SCRIPT_DIR.parents[1]

DEFAULT_MODEL = REPO_ROOT / "model" / "yolo_fish" / "runs" / "yolo11n_fish" / "weights" / "best.pt"
DEFAULT_PROJECT = REPO_ROOT / "model" / "yolo_fish" / "infer"


def _resolve_path(path_str: str) -> Path:
    p = Path(path_str).expanduser()
    if not p.is_absolute():
        p = (REPO_ROOT / p).resolve()
    else:
        p = p.resolve()
    return p


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="YOLO11n 鱼类检测推理（图片/视频）")
    parser.add_argument(
        "--model",
        default=str(DEFAULT_MODEL),
        help="模型权重路径（.pt/.onnx 等），默认使用训练得到的 best.pt",
    )
    parser.add_argument(
        "--source",
        required=True,
        help="输入源：图片路径或视频路径",
    )
    parser.add_argument("--conf", type=float, default=0.25, help="置信度阈值")
    parser.add_argument("--iou", type=float, default=0.7, help="NMS IoU 阈值")
    parser.add_argument("--imgsz", type=int, default=640, help="推理尺寸")
    parser.add_argument(
        "--device",
        default="0",
        help="推理设备，如 0 或 cpu",
    )
    parser.add_argument(
        "--project",
        default=str(DEFAULT_PROJECT),
        help="结果输出目录（Ultralytics project）",
    )
    parser.add_argument("--name", default="yolo11n_fish_infer", help="本次推理 run 名称")
    parser.add_argument("--show", action="store_true", help="实时显示推理画面")
    parser.add_argument("--save", action="store_true", default=True, help="保存可视化结果（默认开启）")
    parser.add_argument("--save-txt", action="store_true", help="保存 txt 检测结果")
    parser.add_argument("--save-conf", action="store_true", help="保存 txt 时附带 conf")
    parser.add_argument("--line-width", type=int, default=8, help="检测框线宽（像素，高分辨率可适当加大）")
    return parser


def main() -> int:
    args = build_parser().parse_args()

    model_path = _resolve_path(args.model)
    source_path = _resolve_path(args.source)
    project_dir = _resolve_path(args.project)

    if not model_path.is_file():
        print(f"[ERROR] 模型文件不存在: {model_path}")
        print("        请先完成训练，或通过 --model 指定正确权重路径。")
        return 1

    if not source_path.exists():
        print(f"[ERROR] 输入源不存在: {source_path}")
        return 1

    try:
        from ultralytics import YOLO
    except ImportError:
        print("[ERROR] 请先安装: pip install \"ultralytics>=8.3.0\"")
        return 1

    project_dir.mkdir(parents=True, exist_ok=True)

    print(f"[INFO] 模型: {model_path}")
    print(f"[INFO] 输入源: {source_path}")
    print(f"[INFO] 输出目录: {project_dir / args.name}")

    model = YOLO(str(model_path))
    model.predict(
        source=str(source_path),
        conf=args.conf,
        iou=args.iou,
        imgsz=args.imgsz,
        device=args.device,
        project=str(project_dir),
        name=args.name,
        exist_ok=True,
        save=args.save,
        show=args.show,
        save_txt=args.save_txt,
        save_conf=args.save_conf,
        line_width=args.line_width,
        verbose=True,
    )

    print(f"[INFO] 推理完成，结果位于: {project_dir / args.name}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
