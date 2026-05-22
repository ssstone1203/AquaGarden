#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
黄叶语义分割训练脚本（YOLO11n-seg + 预训练权重）

依赖：pip install ultralytics>=8.3.0

数据集目录约定（根目录默认 D:/Code/data）：
  {data_root}/yellow_leaves/
    data.yaml          # 含 path / train / val / names，可选 masks_dir
    dataset/images/train|val
    dataset/labels/train|val   # 多边形：class x1 y1 x2 y2 ...（归一化）

用法：
  python train_yolo11n_yellow_leaves.py yellow_leaves
  python train_yolo11n_yellow_leaves.py yellow_leaves --epochs 100 --batch 8
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

SCRIPT_DIR = Path(__file__).resolve().parent
if str(SCRIPT_DIR) not in sys.path:
    sys.path.insert(0, str(SCRIPT_DIR))

from fish_yolo_common import DEFAULT_DATA_ROOT, dataset_yaml

REPO_ROOT = SCRIPT_DIR.parents[1]
DEFAULT_TRAIN_PROJECT = REPO_ROOT / "model" / "yolo_yellow_leaves" / "runs"

# 分割任务须使用 -seg 预训练权重，不能用 yolo11n.pt
PRETRAINED_WEIGHTS = "yolo11n-seg.pt"


def build_parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(description="YOLO11n-seg 黄叶语义分割训练")
    p.add_argument(
        "dataset",
        nargs="?",
        default="yellow_leaves",
        help="D:/Code/data 下的文件夹名，默认 yellow_leaves",
    )
    p.add_argument(
        "--data-root",
        type=Path,
        default=DEFAULT_DATA_ROOT,
        help=f"数据集根目录，默认 {DEFAULT_DATA_ROOT}",
    )
    p.add_argument(
        "--data",
        default=None,
        help="直接指定 data.yaml 路径（覆盖 dataset 自动解析）",
    )
    p.add_argument("--model", default=PRETRAINED_WEIGHTS, help="预训练起点，默认 yolo11n-seg.pt")
    p.add_argument("--epochs", type=int, default=100)
    p.add_argument("--batch", type=int, default=8, help="分割显存占用较大，OOM 请减小")
    p.add_argument("--imgsz", type=int, default=640)
    p.add_argument("--device", default="0", help="cuda 设备，如 0；无 GPU 可设 cpu")
    p.add_argument(
        "--project",
        type=str,
        default=str(DEFAULT_TRAIN_PROJECT),
        help="Ultralytics project 目录",
    )
    p.add_argument(
        "--name",
        type=str,
        default=None,
        help="本次 run 名称，默认 yolo11n_seg_{dataset}",
    )
    p.add_argument("--workers", type=int, default=8, help="Windows 报错可改为 0")
    p.add_argument("--patience", type=int, default=50, help="早停 patience，0 关闭早停")
    p.add_argument("--seed", type=int, default=42)
    return p


def main() -> int:
    args = build_parser().parse_args()
    data_root = Path(args.data_root).expanduser().resolve()
    run_name = args.name or f"yolo11n_seg_{args.dataset}"

    if args.data:
        data_yaml_path = Path(args.data).expanduser().resolve()
        if not data_yaml_path.is_file():
            print(f"[ERROR] 找不到数据集配置: {data_yaml_path}")
            return 1
    else:
        try:
            data_yaml_path = dataset_yaml(data_root, args.dataset)
        except (ValueError, FileNotFoundError) as e:
            print(f"[ERROR] {e}")
            print(f"        请确认目录存在: {data_root / args.dataset}")
            return 1

    try:
        from ultralytics import YOLO
    except ImportError:
        print('[ERROR] 请先安装: pip install "ultralytics>=8.3.0"')
        return 1

    project_dir = Path(args.project).expanduser().resolve()
    project_dir.mkdir(parents=True, exist_ok=True)

    print(f"[INFO] 任务: 语义分割 (segment)")
    print(f"[INFO] 数据集根目录: {data_root}")
    print(f"[INFO] 数据集: {args.dataset}")
    print(f"[INFO] data.yaml: {data_yaml_path}")
    print(f"[INFO] 预训练模型: {args.model}")
    print(f"[INFO] 输出目录: {project_dir / run_name}")

    model = YOLO(args.model)
    kwargs: dict = {
        "data": str(data_yaml_path),
        "epochs": args.epochs,
        "imgsz": args.imgsz,
        "batch": args.batch,
        "device": args.device,
        "project": str(project_dir),
        "name": run_name,
        "exist_ok": True,
        "workers": args.workers,
        "seed": args.seed,
    }
    if args.patience > 0:
        kwargs["patience"] = args.patience

    model.train(**kwargs)
    print(
        "[INFO] 训练结束。最优权重通常在:\n"
        f"       {project_dir / run_name / 'weights' / 'best.pt'}"
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
