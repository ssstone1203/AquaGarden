#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
鱼类单类别检测训练脚本（YOLO11n + COCO 预训练权重）

依赖：pip install ultralytics>=8.3.0
许可：Ultralytics 发布包为 AGPL-3.0，商用请自行合规评估。

数据集需符合 Ultralytics YOLO 检测格式：
  dataset/
    images/train/*.jpg
    images/val/*.jpg
    labels/train/*.txt   # 每行: class xc yc w h（归一化），单类时 class 恒为 0
    labels/val/*.txt
  data.yaml 中配置 path / train / val / names（1 类即可）。

用法：
  1）在下方 「手动填写」 处填写 DATASET_YAML
  2） python train_yolo11n_fish.py
  也可用命令行覆盖： python train_yolo11n_fish.py --data "D:/data/fish.yaml"
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path


# -----------------------------------------------------------------------------
# 手动填写（可留空，改用命令行 --data）
# -----------------------------------------------------------------------------
# 设为 data.yaml 的路径：绝对路径，或相对于「本仓库根目录」的相对路径。
DATASET_YAML: str = ""


SCRIPT_DIR = Path(__file__).resolve().parent
REPO_ROOT = SCRIPT_DIR.parents[1]
# 训练日志与权重默认写入仓库 model/，与 CAD 等资源分子目录存放
DEFAULT_TRAIN_PROJECT = REPO_ROOT / "model" / "yolo_fish" / "runs"

PRETRAINED_WEIGHTS = "yolo11n.pt"


def _resolve_yaml(p: str) -> Path:
    path = Path(p).expanduser()
    if not path.is_absolute():
        path = (REPO_ROOT / path).resolve()
    else:
        path = path.resolve()
    return path


def build_parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(description="使用 YOLO11n 预训练权重训练单类鱼类检测")
    p.add_argument(
        "--data",
        default=None,
        help="覆盖 DATASET_YAML：data.yaml 路径",
    )
    p.add_argument("--model", default=PRETRAINED_WEIGHTS, help="预训练起点，默认 yolo11n.pt")
    p.add_argument("--epochs", type=int, default=100)
    p.add_argument("--batch", type=int, default=16, help="视显存调整，OOM 则减小")
    p.add_argument("--imgsz", type=int, default=640)
    p.add_argument(
        "--device",
        default="0",
        help="cuda 设备，如 0 或 0,1；无 GPU 可设 cpu",
    )
    p.add_argument(
        "--project",
        type=str,
        default=str(DEFAULT_TRAIN_PROJECT),
        help="Ultralytics project 目录（其下再建 run name）",
    )
    p.add_argument("--name", type=str, default="yolo11n_fish", help="本次 run 名称")
    p.add_argument("--workers", type=int, default=8, help="dataloader workers，Windows 报错可改为 0")
    p.add_argument("--patience", type=int, default=50, help="早停 patience，0 关闭早停")
    p.add_argument("--seed", type=int, default=42)
    return p


def main() -> int:
    args = build_parser().parse_args()

    data_src = args.data if args.data is not None else DATASET_YAML
    data_src = (data_src or "").strip()
    if not data_src:
        print(
            "[ERROR] 请在脚本顶部设置 DATASET_YAML，或通过 --data 指定 data.yaml。\n"
            "  data.yaml 示例片段：\n"
            "    path: D:/datasets/fish    # 数据集根目录\n"
            "    train: images/train\n"
            "    val: images/val\n"
            "    names:\n"
            "      0: fish\n"
        )
        return 1

    data_yaml = _resolve_yaml(data_src)
    if not data_yaml.is_file():
        print(f"[ERROR] 找不到数据集配置: {data_yaml}")
        return 1

    try:
        from ultralytics import YOLO
    except ImportError:
        print("[ERROR] 请先安装: pip install \"ultralytics>=8.3.0\"")
        return 1

    project_dir = Path(args.project).expanduser().resolve()
    project_dir.mkdir(parents=True, exist_ok=True)

    print(f"[INFO] 仓库根目录: {REPO_ROOT}")
    print(f"[INFO] data.yaml: {data_yaml}")
    print(f"[INFO] 预训练模型: {args.model}")
    print(f"[INFO] 输出目录: {project_dir / args.name}")

    model = YOLO(args.model)
    kwargs: dict = {
        "data": str(data_yaml),
        "epochs": args.epochs,
        "imgsz": args.imgsz,
        "batch": args.batch,
        "device": args.device,
        "project": str(project_dir),
        "name": args.name,
        "exist_ok": True,
        "workers": args.workers,
        "seed": args.seed,
    }
    if args.patience > 0:
        kwargs["patience"] = args.patience

    model.train(**kwargs)
    print(
        "[INFO] 训练结束。最优权重通常在:\n"
        f"       {project_dir / args.name / 'weights' / 'best.pt'}"
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
