#!/usr/bin/env python3
"""
photo_qa_demo.py

上传本地图片到阿里云百炼（DashScope 兼容 OpenAI 接口）进行识别问答，
并把结果打印到终端。

用法示例:
  python photo_qa_demo.py --image "C:/path/to/photo.jpg" --question "这张图里有什么？"
"""

import argparse
import base64
import mimetypes
import os
import sys
from pathlib import Path
from typing import Optional

from openai import OpenAI
from dotenv import load_dotenv


DEFAULT_BASE_URL = "https://dashscope.aliyuncs.com/compatible-mode/v1"
DEFAULT_MODEL = "qwen-vl-plus"
DEFAULT_QUESTION = "这张图片里有什么？"


def get_api_key(cli_key: Optional[str]) -> str:
    if cli_key:
        return cli_key
    env_key = os.getenv("DASHSCOPE_API_KEY") or os.getenv("QWEN_API_KEY")
    if env_key:
        return env_key
    raise ValueError("未找到 API Key。请使用 --api-key 或设置 DASHSCOPE_API_KEY/QWEN_API_KEY。")


def image_to_data_url(image_path: Path) -> str:
    if not image_path.exists() or not image_path.is_file():
        raise FileNotFoundError(f"图片不存在: {image_path}")

    mime, _ = mimetypes.guess_type(str(image_path))
    if not mime:
        mime = "image/jpeg"

    raw = image_path.read_bytes()
    b64 = base64.b64encode(raw).decode("ascii")
    return f"data:{mime};base64,{b64}"


def ask_with_image(
    api_key: str,
    image_path: Path,
    question: str,
    model: str = DEFAULT_MODEL,
    base_url: str = DEFAULT_BASE_URL,
) -> str:
    client = OpenAI(api_key=api_key, base_url=base_url)
    image_url = image_to_data_url(image_path)

    response = client.chat.completions.create(
        model=model,
        messages=[
            {
                "role": "system",
                "content": "你是一个图像识别助手，请用简体中文、简洁回答。",
            },
            {
                "role": "user",
                "content": [
                    {"type": "text", "text": question},
                    {"type": "image_url", "image_url": {"url": image_url}},
                ],
            },
        ],
        temperature=0.2,
    )
    return response.choices[0].message.content or ""


def build_parser() -> argparse.ArgumentParser:
    env_model = os.getenv("QWEN_VL_MODEL", DEFAULT_MODEL)
    env_base_url = os.getenv("QWEN_BASE_URL", DEFAULT_BASE_URL)
    env_image = os.getenv("QWEN_IMAGE_PATH")
    env_question = os.getenv("QWEN_QUESTION", DEFAULT_QUESTION)
    parser = argparse.ArgumentParser(description="阿里云多模态图片问答 Demo")
    parser.add_argument("--image", default=env_image, help="本地图片路径，默认读取 .env 的 QWEN_IMAGE_PATH")
    parser.add_argument("--question", default=env_question, help="提问文本，默认读取 .env 的 QWEN_QUESTION")
    parser.add_argument("--model", default=env_model, help=f"模型名，默认读取 .env 的 QWEN_VL_MODEL（回退 {DEFAULT_MODEL}）")
    parser.add_argument("--base-url", default=env_base_url, help="兼容接口地址，默认读取 .env 的 QWEN_BASE_URL")
    parser.add_argument("--api-key", default=None, help="可选，直接传入 API Key")
    return parser


def main() -> int:
    # 自动读取当前目录下的 .env
    script_dir = Path(__file__).resolve().parent
    load_dotenv(script_dir / ".env", override=False)

    parser = build_parser()
    args = parser.parse_args()

    try:
        if not args.image:
            raise ValueError("未提供图片路径。请设置 .env 中的 QWEN_IMAGE_PATH，或通过 --image 传入。")

        api_key = get_api_key(args.api_key)
        image_path = Path(args.image).expanduser().resolve()

        print(f"[INFO] 模型: {args.model}")
        print(f"[INFO] 图片: {image_path}")
        print(f"[INFO] 问题: {args.question}")
        print("[INFO] 正在请求阿里云模型，请稍候...\n")

        answer = ask_with_image(
            api_key=api_key,
            image_path=image_path,
            question=args.question,
            model=args.model,
            base_url=args.base_url,
        )
        print("=== 模型回答 ===")
        print(answer.strip())
        return 0
    except Exception as exc:
        print(f"[ERROR] 调用失败: {exc}")
        return 1


if __name__ == "__main__":
    sys.exit(main())
