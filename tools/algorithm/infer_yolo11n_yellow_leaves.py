#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
黄叶语义分割推理脚本（YOLO11n-seg）。

依赖：pip install ultralytics>=8.3.0

示例：
  python infer_yolo11n_yellow_leaves.py
  python infer_yolo11n_yellow_leaves.py yellow_leaves --seed 42
  python infer_yolo11n_yellow_leaves.py --source "D:/Code/data/yellow_leaves/dataset/images/val/378.jpg"
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
DEFAULT_DATASET = "yellow_leaves"
DEFAULT_PROJECT = REPO_ROOT / "model" / "yolo_yellow_leaves" / "infer"


def _resolve_path(path_str: str, base: Path | None = None) -> Path:
    p = Path(path_str).expanduser()
    if not p.is_absolute():
        p = ((base or REPO_ROOT) / p).resolve()
    else:
        p = p.resolve()
    return p


def default_model_for_dataset(dataset: str) -> Path:
    return (
        REPO_ROOT
        / "model"
        / "yolo_yellow_leaves"
        / "runs"
        / f"yolo11n_seg_{dataset}"
        / "weights"
        / "best.pt"
    )


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="YOLO11n-seg 黄叶语义分割推理")
    parser.add_argument(
        "dataset",
        nargs="?",
        default=DEFAULT_DATASET,
        help="D:/Code/data 下的文件夹名，默认 yellow_leaves",
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
        help="权重路径；默认 model/yolo_yellow_leaves/runs/yolo11n_seg_{dataset}/weights/best.pt",
    )
    parser.add_argument(
        "--source",
        default=None,
        help="图片或视频路径；省略则从 val 集随机选取",
    )
    parser.add_argument("--seed", type=int, default=None, help="随机选 val 图时的种子")
    parser.add_argument("--conf", type=float, default=0.25, help="置信度阈值")
    parser.add_argument("--iou", type=float, default=0.7, help="NMS IoU 阈值")
    parser.add_argument("--imgsz", type=int, default=640)
    parser.add_argument("--device", default="0")
    parser.add_argument(
        "--project",
        default=str(DEFAULT_PROJECT),
        help="结果输出目录",
    )
    parser.add_argument("--name", default=None, help="run 名称，默认 infer_{dataset}")
    parser.add_argument("--show", action="store_true", help="实时显示")
    parser.add_argument("--save", action="store_true", default=True, help="保存可视化结果")
    parser.add_argument("--save-txt", action="store_true", help="保存分割多边形 txt")
    parser.add_argument(
        "--retina-masks",
        action="store_true",
        help="高分辨率 mask 叠加（更慢、更清晰）",
    )
    parser.add_argument("--line-width", type=int, default=2, help="轮廓线宽（像素）")
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
        print("        请先训练，或通过 --model 指定权重。")
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

    print(f"[INFO] 任务: 语义分割 (segment)")
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
        retina_masks=args.retina_masks,
        line_width=args.line_width,
        verbose=True,
    )

    print(f"[INFO] 推理完成，结果位于: {project_dir / run_name}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
