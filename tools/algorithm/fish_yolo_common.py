#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""鱼类 YOLO 脚本共用的数据集路径解析（D:/Code/data/<name>/）。"""

from __future__ import annotations

import random
from pathlib import Path

DEFAULT_DATA_ROOT = Path("D:/Code/data")

IMAGE_SUFFIXES = frozenset({".jpg", ".jpeg", ".png", ".bmp", ".webp", ".tif", ".tiff"})


def _validate_dataset_name(name: str) -> str:
    n = (name or "").strip()
    if not n:
        raise ValueError("数据集文件夹名不能为空")
    if n in (".", "..") or "/" in n or "\\" in n:
        raise ValueError(f"非法数据集名: {name!r}")
    return n


def dataset_root(data_root: Path, name: str) -> Path:
    return (data_root / _validate_dataset_name(name)).resolve()


def dataset_yaml(data_root: Path, name: str) -> Path:
    path = dataset_root(data_root, name) / "data.yaml"
    if not path.is_file():
        raise FileNotFoundError(f"找不到 data.yaml: {path}")
    return path


def val_images_dir(data_root: Path, name: str) -> Path:
    """标准布局：{data_root}/{name}/dataset/images/val"""
    path = dataset_root(data_root, name) / "dataset" / "images" / "val"
    if not path.is_dir():
        raise FileNotFoundError(f"找不到 val 图片目录: {path}")
    return path


def list_val_images(data_root: Path, name: str) -> list[Path]:
    val_dir = val_images_dir(data_root, name)
    images = [p for p in val_dir.iterdir() if p.is_file() and p.suffix.lower() in IMAGE_SUFFIXES]
    if not images:
        raise FileNotFoundError(f"val 目录下没有图片: {val_dir}")
    return images


def pick_random_val_image(data_root: Path, name: str, seed: int | None = None) -> Path:
    images = list_val_images(data_root, name)
    rng = random.Random(seed)
    return rng.choice(images)
