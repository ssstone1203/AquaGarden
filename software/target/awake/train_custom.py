#!/usr/bin/env python3
"""
train_custom.py  ——  自定义唤醒词录音 + 训练指引（多人版）
依赖：pip install openwakeword pyaudio numpy

步骤：
  1. 运行本脚本，三名录音人依次录制唤醒词音频
  2. 脚本调用 openWakeWord 训练工具生成 .onnx 模型
  3. 将生成的模型路径填入 awake.py 的 CUSTOM_MODEL_PATH

注意：
  openWakeWord 自定义训练需要 ~50-200 条正样本音频，
  本脚本帮你完成录音部分，训练命令见末尾说明。
  三人录音会分别保存在各自子目录，训练时合并使用。
"""

import os
import time
import wave
import pyaudio
import argparse

# ================================================================
#  配置
# ================================================================
SAMPLE_RATE    = 16000
CHANNELS       = 1
AUDIO_FORMAT   = pyaudio.paInt16
RECORD_SECONDS = 2       # 每条录音时长（秒）
OUTPUT_DIR     = "./recordings"   # 录音输出根目录

# 三位录音人姓名（可通过 --persons 参数覆盖）
DEFAULT_PERSONS = ["person1", "person2", "person3"]


# ================================================================
#  录音函数
# ================================================================
def record_clip(audio: pyaudio.PyAudio, index: int, output_dir: str, word: str) -> str:
    """录制一条唤醒词音频，返回保存路径"""
    os.makedirs(output_dir, exist_ok=True)
    filepath = os.path.join(output_dir, f"wake_{index:04d}.wav")

    print(f"  [{index}] 准备好后按 Enter 开始录音（{RECORD_SECONDS}秒）... ", end="", flush=True)
    input()
    print(f"  [{index}] 录音中，请说「{word}」...")

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

    with wave.open(filepath, "wb") as wf:
        wf.setnchannels(CHANNELS)
        wf.setsampwidth(audio.get_sample_size(AUDIO_FORMAT))
        wf.setframerate(SAMPLE_RATE)
        wf.writeframes(b"".join(frames))

    print(f"  [{index}] 已保存: {filepath}")
    return filepath


# ================================================================
#  单人录音流程
# ================================================================
def record_person(audio: pyaudio.PyAudio, person_name: str, count: int,
                  root_dir: str, word: str) -> list:
    """为一个人录制全部音频，返回已保存文件列表"""
    person_dir = os.path.join(root_dir, person_name)
    print()
    print(f"  >>> 现在开始录制：{person_name}  （共 {count} 条）")
    print(f"      保存目录：{person_dir}")
    print(f"      提示：请自然说出「{word}」，可变换语速、音量、语气。")
    print()

    saved = []
    try:
        for i in range(1, count + 1):
            path = record_clip(audio, i, person_dir, word)
            saved.append(path)
            if i < count:
                time.sleep(0.3)
    except KeyboardInterrupt:
        print(f"\n  {person_name} 录音被中断，已保存 {len(saved)} 条。")

    print(f"\n  {person_name} 录音完成，共 {len(saved)} 条。")
    return saved


# ================================================================
#  打印训练说明
# ================================================================
def print_training_guide(word: str, outdir: str) -> None:
    merged_dir = os.path.join(outdir, "merged")
    print()
    print("=" * 52)
    print("  下一步：合并录音并训练自定义唤醒词模型")
    print("=" * 52)
    print()
    print("  【合并三人录音】")
    print(f"  将三个子目录下的 .wav 文件统一复制到 {merged_dir}/")
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
    print(f"""
     target_phrase: "{word}"
     positive_data_dir: {merged_dir}
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


# ================================================================
#  主流程
# ================================================================
def main():
    parser = argparse.ArgumentParser(description="多人录制自定义唤醒词训练音频")
    parser.add_argument("--word",    default="小鱼",
                        help="唤醒词文本（用于提示）")
    parser.add_argument("--count",   type=int, default=50,
                        help="每人录制条数，建议 50~200")
    parser.add_argument("--outdir",  default=OUTPUT_DIR,
                        help="录音输出根目录")
    parser.add_argument("--persons", nargs="+", default=DEFAULT_PERSONS,
                        help="录音人姓名列表，默认 person1 person2 person3")
    args = parser.parse_args()

    persons = args.persons[:3]   # 最多取三人

    print("=" * 52)
    print("  自定义唤醒词录音工具（多人版）train_custom.py")
    print("=" * 52)
    print(f"  唤醒词    : 「{args.word}」")
    print(f"  录音人数  : {len(persons)} 人  →  {', '.join(persons)}")
    print(f"  每人条数  : {args.count} 条")
    print(f"  总计条数  : {len(persons) * args.count} 条")
    print(f"  输出根目录: {args.outdir}")
    print(f"  每条时长  : {RECORD_SECONDS} 秒")
    print()
    print("  流程：依次完成每位录音人的录音，")
    print("        完成一人后按提示换人继续。")
    print("=" * 52)

    audio = pyaudio.PyAudio()
    all_saved = {}

    try:
        for idx, person in enumerate(persons):
            if idx > 0:
                print()
                print(f"  *** 请换 {person} 就位，准备好后按 Enter 继续 ***", end="", flush=True)
                input()
            all_saved[person] = record_person(
                audio, person, args.count, args.outdir, args.word
            )
    except KeyboardInterrupt:
        print("\n\n录音全程中断。")
    finally:
        audio.terminate()

    total = sum(len(v) for v in all_saved.values())
    print()
    print("=" * 52)
    print(f"  录音汇总：共 {total} 条")
    for person, files in all_saved.items():
        person_dir = os.path.join(args.outdir, person)
        print(f"    {person}: {len(files)} 条  →  {person_dir}/")
    print("=" * 52)

    print_training_guide(args.word, args.outdir)


if __name__ == "__main__":
    main()
