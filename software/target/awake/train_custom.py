#!/usr/bin/env python3
"""
train_custom.py  ——  自定义唤醒词录音 + 训练指引
依赖：pip install openwakeword pyaudio numpy

步骤：
  1. 运行本脚本，按提示录制若干条唤醒词音频
  2. 脚本调用 openWakeWord 训练工具生成 .onnx 模型
  3. 将生成的模型路径填入 awake.py 的 CUSTOM_MODEL_PATH

注意：
  openWakeWord 自定义训练需要 ~50-200 条正样本音频，
  本脚本帮你完成录音部分，训练命令见末尾说明。
"""

import os
import time
import wave
import struct
import pyaudio
import argparse

# ================================================================
#  配置
# ================================================================
SAMPLE_RATE    = 16000
CHANNELS       = 1
AUDIO_FORMAT   = pyaudio.paInt16
RECORD_SECONDS = 2       # 每条录音时长（秒）
OUTPUT_DIR     = "./recordings"   # 录音输出目录


# ================================================================
#  录音函数
# ================================================================
def record_clip(audio: pyaudio.PyAudio, index: int, output_dir: str) -> str:
    """录制一条唤醒词音频，返回保存路径"""
    os.makedirs(output_dir, exist_ok=True)
    filepath = os.path.join(output_dir, f"wake_{index:04d}.wav")

    print(f"  [{index}] 准备好后按 Enter 开始录音（{RECORD_SECONDS}秒）...", end="")
    input()
    print(f"  [{index}] 🔴 录音中，请说唤醒词...")

    stream = audio.open(
        format=AUDIO_FORMAT,
        channels=CHANNELS,
        rate=SAMPLE_RATE,
        input=True,
        frames_per_buffer=1024,
    )

    frames = []
    for _ in range(int(SAMPLE_RATE / 1024 * RECORD_SECONDS)):
        frames.append(stream.read(1024, exception_on_overflow=False))

    stream.stop_stream()
    stream.close()

    # 保存 WAV
    with wave.open(filepath, "wb") as wf:
        wf.setnchannels(CHANNELS)
        wf.setsampwidth(audio.get_sample_size(AUDIO_FORMAT))
        wf.setframerate(SAMPLE_RATE)
        wf.writeframes(b"".join(frames))

    print(f"  [{index}] 已保存: {filepath}")
    return filepath


# ================================================================
#  主流程
# ================================================================
def main():
    parser = argparse.ArgumentParser(description="录制自定义唤醒词训练音频")
    parser.add_argument("--word",  default="自定义唤醒词", help="唤醒词文本（用于提示）")
    parser.add_argument("--count", type=int, default=50,   help="录制条数，建议 50~200")
    parser.add_argument("--outdir", default=OUTPUT_DIR,    help="录音输出目录")
    args = parser.parse_args()

    print("=" * 52)
    print("  自定义唤醒词录音工具  train_custom.py")
    print("=" * 52)
    print(f"  唤醒词  : 「{args.word}」")
    print(f"  录制条数: {args.count} 条")
    print(f"  输出目录: {args.outdir}")
    print(f"  每条时长: {RECORD_SECONDS} 秒")
    print()
    print("  提示：每次录音时自然地说一遍唤醒词，")
    print("        可以变换语速、音量、语气，增加多样性。")
    print("=" * 52)

    audio = pyaudio.PyAudio()
    saved = []

    try:
        for i in range(1, args.count + 1):
            path = record_clip(audio, i, args.outdir)
            saved.append(path)
            if i < args.count:
                time.sleep(0.3)
    except KeyboardInterrupt:
        print("\n录音中断")
    finally:
        audio.terminate()

    print(f"\n共录制 {len(saved)} 条音频，保存在 {args.outdir}/")
    print()
    print("=" * 52)
    print("  下一步：训练自定义唤醒词模型")
    print("=" * 52)
    print()
    print("  openWakeWord 目前推荐使用官方 Colab 或本地脚本训练，")
    print("  以下是两种方式：")
    print()
    print("  【方式一】使用 openWakeWord 官方训练脚本（本地）")
    print("  ─────────────────────────────────────────────────")
    print("  1. 安装训练依赖：")
    print("     pip install openwakeword[train]")
    print()
    print("  2. 准备负样本（背景噪音/非唤醒词音频），可从：")
    print("     https://github.com/dscripka/openWakeWord")
    print("     的 data/ 目录下载")
    print()
    print("  3. 创建训练配置 train_config.yaml：")
    print("""
     target_phrase: \"""" + args.word + """\"
     positive_data_dir: """ + args.outdir + """
     negative_data_dir: ./negative_data
     output_dir: ./models
     epochs: 100
    """)
    print("  4. 运行训练：")
    print("     python -m openwakeword.train --config train_config.yaml")
    print()
    print("  5. 训练完成后，将生成的 ./models/*.onnx 路径")
    print("     填入 awake.py 的 CUSTOM_MODEL_PATH 变量即可。")
    print()
    print("  【方式二】Google Colab（无需本地 GPU）")
    print("  ─────────────────────────────────────────────────")
    print("  https://github.com/dscripka/openWakeWord#training-new-models")
    print()


if __name__ == "__main__":
    main()
