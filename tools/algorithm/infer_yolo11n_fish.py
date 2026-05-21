#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
鱼类检测推理脚本（支持图片或视频）。

依赖：
  pip install "ultralytics>=8.3.0"

示例：
  # 从 val 集随机抽一张图推理（最常用）
  python infer_yolo11n_fish.py fish_new

  # 固定随机种子，便于复现同一张 val 图
  python infer_yolo11n_fish.py fish --seed 42

  # 手动指定图片或视频
  python infer_yolo11n_fish.py fish --source "D:/Code/data/fish/test.mp4"

  # 指定权重
  python infer_yolo11n_fish.py fish_new --model "D:/Code/AquaGarden/model/yolo_fish/runs/yolo11n_fish_new/weights/best.pt"
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

SCRIPT_DIR = Path(__file__).resolve().parent
if str(SCRIPT_DIR) not in sys.path:
    sys.path.insert(0, str(SCRIPT_DIR))

from fish_yolo_common import DEFAULT_DATA_ROOT, pick_random_val_image
REPO_ROOT = SCRIPT_DIR.parents[1]

DEFAULT_MODEL = REPO_ROOT / "model" / "yolo_fish" / "runs" / "yolo11n_fish" / "weights" / "best.pt"
DEFAULT_PROJECT = REPO_ROOT / "model" / "yolo_fish" / "infer"


def _resolve_path(path_str: str, base: Path | None = None) -> Path:
    p = Path(path_str).expanduser()
    if not p.is_absolute():
        p = ((base or REPO_ROOT) / p).resolve()
    else:
        p = p.resolve()
    return p


def default_model_for_dataset(dataset: str) -> Path:
    return REPO_ROOT / "model" / "yolo_fish" / "runs" / f"yolo11n_{dataset}" / "weights" / "best.pt"


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="YOLO11n 鱼类检测推理（图片/视频）")
    parser.add_argument(
        "dataset",
        help="D:/Code/data 下的文件夹名；未指定 --source 时从 dataset/images/val 随机选图",
    )
    parser.add_argument(
        "--data-root",
        type=Path,
        default=DEFAULT_DATA_ROOT,
        help=f"数据集根目录，默认 {DEFAULT_DATA_ROOT}",
    )
    parser.add_argument(
        "--model",
        default=None,
        help="模型权重路径；默认 model/yolo_fish/runs/yolo11n_{dataset}/weights/best.pt",
    )
    parser.add_argument(
        "--source",
        default=None,
        help="输入源：图片或视频路径；省略则从 val 集随机选取",
    )
    parser.add_argument(
        "--seed",
        type=int,
        default=None,
        help="随机选 val 图时的种子；不指定则每次不同",
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
    parser.add_argument(
        "--name",
        default=None,
        help="本次推理 run 名称，默认 infer_{dataset}",
    )
    parser.add_argument("--show", action="store_true", help="实时显示推理画面")
    parser.add_argument("--save", action="store_true", default=True, help="保存可视化结果（默认开启）")
    parser.add_argument("--save-txt", action="store_true", help="保存 txt 检测结果")
    parser.add_argument("--save-conf", action="store_true", help="保存 txt 时附带 conf")
    parser.add_argument("--line-width", type=int, default=2, help="检测框线宽（像素），默认 2")
    return parser


def main() -> int:
    args = build_parser().parse_args()
    data_root = Path(args.data_root).expanduser().resolve()
    run_name = args.name or f"infer_{args.dataset}"

    model_path = _resolve_path(
        args.model if args.model else str(default_model_for_dataset(args.dataset))
    )
    project_dir = _resolve_path(args.project)

    if args.source:
        source_path = _resolve_path(args.source)
    else:
        try:
            source_path = pick_random_val_image(data_root, args.dataset, seed=args.seed)
        except (ValueError, FileNotFoundError) as e:
            print(f"[ERROR] {e}")
            return 1

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
        print('[ERROR] 请先安装: pip install "ultralytics>=8.3.0"')
        return 1

    project_dir.mkdir(parents=True, exist_ok=True)

    print(f"[INFO] 数据集: {args.dataset}")
    print(f"[INFO] 模型: {model_path}")
    print(f"[INFO] 输入源: {source_path}")
    print(f"[INFO] 输出目录: {project_dir / run_name}")

    model = YOLO(str(model_path))
    model.predict(
        source=str(source_path),
        conf=args.conf,
        iou=args.iou,
        imgsz=args.imgsz,
        device=args.device,
        project=str(project_dir),
        name=run_name,
        exist_ok=True,
        save=args.save,
        show=args.show,
        save_txt=args.save_txt,
        save_conf=args.save_conf,
        line_width=args.line_width,
        verbose=True,
    )

    print(f"[INFO] 推理完成，结果位于: {project_dir / run_name}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
