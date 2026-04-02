"""
tasks.py  ——  四个任务函数
每个函数签名：task_xxx(arm: Arm) -> None
任务内部管理摄像头等资源，执行完毕后释放。
"""

import cv2
import json
import os
import time
import base64
import mimetypes
import numpy as np
from pathlib import Path
from openai import OpenAI

import config
import dialogue
from arm import Arm

# Windows 上优先使用 DirectShow，避免部分机器走 FFMPEG 枚举时报错
_CAM_BACKEND = cv2.CAP_DSHOW if os.name == "nt" else cv2.CAP_ANY


def _open_camera():
    """
    打开可用摄像头，优先 config.CAMERA_INDEX，失败时回退尝试常见索引。
    返回 (cap, index)，失败则返回 (None, None)。
    """
    candidates = [config.CAMERA_INDEX] + [i for i in (0, 1, 2) if i != config.CAMERA_INDEX]
    for idx in candidates:
        cap = cv2.VideoCapture(idx, _CAM_BACKEND)
        if not cap.isOpened():
            cap.release()
            continue
        cap.set(cv2.CAP_PROP_FRAME_WIDTH, 1280)
        cap.set(cv2.CAP_PROP_FRAME_HEIGHT, 720)
        ret, _ = cap.read()
        if ret:
            return cap, idx
        cap.release()
    return None, None

# ================================================================
#  任务1：颜色识别与分拣
# ================================================================

_COLOR_RANGES = {
    'r': [(np.array([0,   80, 80]), np.array([10,  255, 255])),
          (np.array([160, 80, 80]), np.array([180, 255, 255]))],
    'g': [(np.array([40,  60, 60]), np.array([85,  255, 255]))],
    'b': [(np.array([95,  50, 40]), np.array([135, 255, 255]))],
}
_COLOR_BGR  = {'r': (0, 0, 255), 'g': (0, 200, 0), 'b': (255, 80, 0)}
_COLOR_NAME = {'r': '红色', 'g': '绿色', 'b': '蓝色'}


def _detect_block(frame):
    """检测最大彩色物块，返回 (cx, cy, color_key, disp_frame)"""
    hsv  = cv2.cvtColor(frame, cv2.COLOR_BGR2HSV)
    kern = np.ones((5, 5), np.uint8)
    best_area, best = 300, (None, None, None, None)

    for key, ranges in _COLOR_RANGES.items():
        mask = np.zeros(hsv.shape[:2], dtype=np.uint8)
        for lo, hi in ranges:
            mask |= cv2.inRange(hsv, lo, hi)
        mask = cv2.morphologyEx(mask, cv2.MORPH_OPEN,   kern)
        mask = cv2.morphologyEx(mask, cv2.MORPH_DILATE, kern)
        cnts, _ = cv2.findContours(mask, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)
        if not cnts:
            continue
        c    = max(cnts, key=cv2.contourArea)
        area = cv2.contourArea(c)
        if area > best_area:
            M = cv2.moments(c)
            if M["m00"] > 0:
                best_area = area
                best = (int(M["m10"]/M["m00"]), int(M["m01"]/M["m00"]), key, c)

    disp = frame.copy()
    if best[0] is not None:
        cx, cy, key, cnt = best
        cv2.drawContours(disp, [cnt], -1, _COLOR_BGR[key], 2)
        cv2.circle(disp, (cx, cy), 8, (0, 255, 0), -1)
        cv2.putText(disp, _COLOR_NAME[key], (cx+12, cy-12),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.7, _COLOR_BGR[key], 2)
        return cx, cy, key, disp
    return None, None, None, disp


def _pixel_to_robot(u, v, A):
    uv1  = np.array([u, v, 1.0])
    xyzp = A @ uv1
    return (float(xyzp[0]) + config.X_BIAS,
            float(xyzp[1]) + config.Y_BIAS,
            float(xyzp[2]),
            float(xyzp[3]))


def task_clamp(arm: Arm):
    """任务1：颜色识别与分拣"""
    print("[分拣] 加载示教映射...")
    if not config.MAP_FILE.exists():
        print(f"[分拣] 错误：找不到 {config.MAP_FILE}，请先运行 collect_teach.py")
        return

    data    = np.load(config.MAP_FILE)
    A       = data["A"]

    cap, cam_idx = _open_camera()
    if cap is None:
        print(f"[分拣] 错误：未找到可用摄像头。请检查 CAMERA_INDEX={config.CAMERA_INDEX} 和设备连接。")
        return
    print(f"[分拣] 使用摄像头索引: {cam_idx}")

    try:
        # 移到观测位姿
        print("[分拣] 移到观测位姿...")
        arm.move(config.OBS_X, config.OBS_Y, config.OBS_Z, config.OBS_PITCH, dur=1500)

        # 检测物块（按空格确认，Q 放弃）
        print("[分拣] 检测物块 — 空格确认 / Q 放弃")
        bx = by = bz = bp = block_color = None

        while True:
            ret, frame = cap.read()
            if not ret:
                continue
            if config.CAMERA_ROT:
                frame = cv2.rotate(frame, cv2.ROTATE_180)

            cx, cy, color_key, disp = _detect_block(frame)
            if cx is not None:
                rx, ry, rz, rp = _pixel_to_robot(cx, cy, A)
                cv2.putText(disp, f"x={rx:.1f} y={ry:.1f} z={rz:.1f}",
                            (10, 80), cv2.FONT_HERSHEY_SIMPLEX, 0.8, (0, 220, 0), 2)
            cv2.putText(disp, "SPACE=confirm  Q=cancel",
                        (10, disp.shape[0]-12), cv2.FONT_HERSHEY_SIMPLEX, 0.6, (180,180,180), 1)
            cv2.imshow("clamp", disp)
            key = cv2.waitKey(30) & 0xFF

            if key == ord('q'):
                print("[分拣] 用户取消")
                return
            if key == ord(' ') and cx is not None:
                bx, by, bz, bp = _pixel_to_robot(cx, cy, A)
                block_color = color_key
                # 超声波校正 X
                d = arm.dist()
                if d > 0:
                    bx_ultra = config.SENSOR_X_OFFSET + d
                    if abs(bx_ultra - bx) <= config.ULTRA_MAX_DIFF:
                        bx = bx_ultra
                        print(f"[分拣] 超声波校正: {d:.2f}cm → x={bx:.2f}")
                break

        cv2.destroyAllWindows()
        if bx is None:
            return

        zone      = config.ZONES.get(block_color)
        grasp_z   = bz + config.GRASP_Z_LIFT
        above_z   = bz + config.ABOVE_CLEARANCE
        print(f"[分拣] 目标: {_COLOR_NAME.get(block_color,'未知')}  "
              f"x={bx:.2f} y={by:.2f} z={bz:.2f} 夹取 z={grasp_z:.2f}  "
              f"→ {zone['name'] if zone else '未知区'}")

        # 夹取流程
        arm.gripper_open()
        arm.move(bx, by, above_z, bp, dur=1000)
        arm.move(bx, by, grasp_z,  bp, dur=600)
        arm.gripper_close()
        time.sleep(0.4)
        arm.move(bx, by, config.SAFE_Z, bp, dur=800)
        arm.move(config.HORIZ_X, config.HORIZ_Y, config.SAFE_Z, config.HORIZ_PITCH, dur=1000)

        # 放置
        if zone:
            above_zone_z = max(config.SAFE_Z, zone['z'] + 3.0)
            arm.move(zone['x'], zone['y'], above_zone_z, zone['pitch'], dur=1400)
            arm.move(zone['x'], zone['y'], zone['z'],    zone['pitch'], dur=800)
            time.sleep(0.3)
            arm.gripper_open()
            time.sleep(0.4)
            arm.move(zone['x'], zone['y'], above_zone_z, zone['pitch'], dur=700)
        print("[分拣] 完成")

    finally:
        cap.release()
        cv2.destroyAllWindows()


# ================================================================
#  任务2：智能台灯
# ================================================================

def task_led(arm: Arm, preset_key: str = config.LED_DEFAULT):
    """
    任务2：智能台灯
    preset_key: 'off' / 'low' / 'medium' / 'high'，由 dialogue 模块根据用户语音传入
    'off' 直接关灯，不移动机械臂；其余档位先移到台灯位置再点亮。
    """
    preset = config.LED_PRESETS.get(preset_key, config.LED_PRESETS[config.LED_DEFAULT])

    if preset_key == "off":
        print("[台灯] 关闭台灯（亮度归零）")
        arm.led(0, 0, 0, 0)
        print("[台灯] 已关闭")
        return

    print(f"[台灯] 移到台灯位置，亮度档位: {preset_key}")
    arm.move(config.LED_X, config.LED_Y, config.LED_Z, config.LED_PITCH, dur=2000)
    time.sleep(0.3)
    arm.led(preset['r'], preset['g'], preset['b'], preset['bright'])
    print(f"[台灯] 已设置: R={preset['r']} G={preset['g']} B={preset['b']} "
          f"bright={preset['bright']}")


# ================================================================
#  任务3：人脸识别追踪
# ================================================================

_cascade = cv2.CascadeClassifier(
    cv2.data.haarcascades + "haarcascade_frontalface_default.xml"
)


def _detect_face(frame):
    gray  = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)
    faces = _cascade.detectMultiScale(
        gray, scaleFactor=1.1, minNeighbors=5, minSize=(60, 60)
    )
    if len(faces) == 0:
        return None
    x, y, w, h = max(faces, key=lambda f: f[2]*f[3])
    return int(x + w/2), int(y + h/2), w, h


def _scan_waypoints():
    pts, angle = [], config.FACE_ANGLE_CENTER
    for _ in range(config.FACE_SCAN_CYCLES):
        while angle > config.FACE_ANGLE_MIN:
            angle -= config.FACE_SCAN_STEP
            pts.append(max(config.FACE_ANGLE_MIN, angle))
        while angle < config.FACE_ANGLE_MAX:
            angle += config.FACE_SCAN_STEP
            pts.append(min(config.FACE_ANGLE_MAX, angle))
    pts.append(config.FACE_ANGLE_CENTER)
    return pts


def task_face(arm: Arm):
    """任务3：人脸识别追踪（运行至超时或按 Q 退出）"""
    cap, cam_idx = _open_camera()
    if cap is None:
        print(f"[人脸] 错误：未找到可用摄像头。请检查 CAMERA_INDEX={config.CAMERA_INDEX} 和设备连接。")
        return
    print(f"[人脸] 使用摄像头索引: {cam_idx}")
    cap.set(cv2.CAP_PROP_BUFFERSIZE, 1)

    print("[人脸] 移到追踪起始位置...")
    arm.move_angle(config.FACE_ANGLE_CENTER, config.FACE_Z, config.FACE_PITCH, dur=2000)
    time.sleep(2.2)

    current_angle  = config.FACE_ANGLE_CENTER
    last_move_time = 0.0
    state          = "TRACKING"
    scan_pts       = []
    scan_idx       = 0
    deadline       = time.time() + config.FACE_TASK_TIMEOUT

    print(f"[人脸] 开始追踪（最长 {config.FACE_TASK_TIMEOUT}s，按 Q 提前退出）")

    try:
        while time.time() < deadline:
            ret, frame = cap.read()
            if not ret:
                continue
            if config.CAMERA_ROT:
                frame = cv2.rotate(frame, cv2.ROTATE_180)

            h_img, w_img = frame.shape[:2]
            cx_img       = w_img // 2
            face         = _detect_face(frame)

            cv2.line(frame, (cx_img, 0), (cx_img, h_img), (100, 100, 100), 1)

            if face is not None:
                fx, fy, fw, fh = face
                cv2.rectangle(frame, (fx-fw//2, fy-fh//2), (fx+fw//2, fy+fh//2), (0,220,0), 2)
                cv2.circle(frame, (fx, fy), 6, (0,255,0), -1)

                offset_px = fx - cx_img
                state     = "TRACKING"
                scan_pts  = []
                scan_idx  = 0

                now = time.time()
                if abs(offset_px) > config.FACE_DEADZONE and now - last_move_time >= config.FACE_COOLDOWN:
                    d_angle   = offset_px * config.FACE_SCALE * config.FACE_DIRECTION
                    new_angle = max(config.FACE_ANGLE_MIN,
                                   min(config.FACE_ANGLE_MAX, current_angle + d_angle))
                    if abs(new_angle - current_angle) > 0.1:
                        arm.move_angle(new_angle, config.FACE_Z, config.FACE_PITCH,
                                       dur=config.FACE_MOVE_DUR)
                        current_angle  = new_angle
                        last_move_time = time.time()
                        for _ in range(4):
                            cap.grab()

                cv2.putText(frame, f"TRACKING angle={current_angle:.1f}",
                            (10, 40), cv2.FONT_HERSHEY_SIMPLEX, 0.85, (0,220,0), 2)
            else:
                if state == "TRACKING":
                    state    = "SCANNING"
                    scan_pts = _scan_waypoints()
                    scan_idx = 0

                if state == "SCANNING":
                    if scan_idx < len(scan_pts):
                        arm.move_angle(scan_pts[scan_idx], config.FACE_Z, config.FACE_PITCH,
                                       dur=config.FACE_SCAN_DUR)
                        current_angle = scan_pts[scan_idx]
                        scan_idx += 1
                        cv2.putText(frame, f"SCANNING {scan_idx}/{len(scan_pts)}",
                                    (10, 40), cv2.FONT_HERSHEY_SIMPLEX, 0.8, (0,180,255), 2)
                    else:
                        state    = "NO_FACE"
                        scan_idx = 0

                if state == "NO_FACE":
                    cv2.putText(frame, "No Face", (10, 40),
                                cv2.FONT_HERSHEY_SIMPLEX, 1.2, (0,0,255), 3)
                    if not scan_pts:
                        scan_pts = _scan_waypoints()
                    if scan_idx < len(scan_pts):
                        arm.move_angle(scan_pts[scan_idx], config.FACE_Z, config.FACE_PITCH,
                                       dur=config.FACE_SCAN_DUR * 2)
                        current_angle = scan_pts[scan_idx]
                        scan_idx += 1
                    else:
                        scan_idx = 0

            remaining = int(deadline - time.time())
            cv2.putText(frame, f"Q=quit  {remaining}s", (10, h_img-14),
                        cv2.FONT_HERSHEY_SIMPLEX, 0.6, (180,180,180), 1)
            cv2.imshow("face_track", frame)

            if cv2.waitKey(1) & 0xFF == ord('q'):
                break

    finally:
        cap.release()
        cv2.destroyAllWindows()
    print("[人脸] 追踪结束")


# ================================================================
#  任务4：题目解答
# ================================================================

def task_answer(arm: Arm, question: str = "请解答图片中的题目"):
    """
    任务4：题目解答
    移到观测位置 → 拍照 → 调用多模态大模型 → 打印并语音播报分析与解答
    question 由 dialogue 模块根据用户语音传入
    """
    if not config.DASHSCOPE_API_KEY:
        print("[解答] 错误：未配置 DASHSCOPE_API_KEY，请在 config.py 或环境变量中设置")
        return

    print("[解答] 移到拍照位置...")
    arm.move(config.ANSWER_OBS_X, config.ANSWER_OBS_Y,
             config.ANSWER_OBS_Z, config.ANSWER_OBS_PITCH, dur=1500)
    time.sleep(0.5)

    # 拍照
    cap, cam_idx = _open_camera()
    if cap is None:
        print(f"[解答] 错误：未找到可用摄像头。请检查 CAMERA_INDEX={config.CAMERA_INDEX} 和设备连接。")
        return
    print(f"[解答] 使用摄像头索引: {cam_idx}")
    ret, frame = cap.read()
    cap.release()

    if not ret:
        print("[解答] 错误：摄像头读取失败")
        return
    if config.CAMERA_ROT:
        frame = cv2.rotate(frame, cv2.ROTATE_180)

    photo_path = config.PHOTO_PATH
    # Windows 上 cv2.imwrite 对含中文等非 ASCII 的路径常会失败且不写入文件，
    # 后续 read_bytes 会报 FileNotFoundError；用 imencode + write_bytes 可规避。
    ok, enc = cv2.imencode(".jpg", frame, [int(cv2.IMWRITE_JPEG_QUALITY), 90])
    if not ok:
        print("[解答] 错误：图片编码失败")
        return
    photo_path.write_bytes(enc.tobytes())
    print(f"[解答] 已拍照: {photo_path}")
    print(f"[解答] 问题: {question}")
    print("[解答] 正在请求大模型，请稍候...")

    try:
        client    = OpenAI(api_key=config.DASHSCOPE_API_KEY, base_url=config.DASHSCOPE_BASE_URL)
        mime, _   = mimetypes.guess_type(str(photo_path))
        mime      = mime or "image/jpeg"
        b64       = base64.b64encode(photo_path.read_bytes()).decode("ascii")
        image_url = f"data:{mime};base64,{b64}"

        resp = client.chat.completions.create(
            model=config.VL_MODEL,
            messages=[
                {
                    "role": "system",
                    "content": (
                        "你是辅学助手，用简体中文作答，务必简短。"
                        "先用一两句话点明思路或考点，再直接给出结论与必要步骤，不展开赘述；"
                        "全文尽量控制在200字以内，口语化、便于朗读，不用Markdown表格或复杂排版。"
                    ),
                },
                {"role": "user", "content": [
                    {"type": "text",      "text": question},
                    {"type": "image_url", "image_url": {"url": image_url}},
                ]},
            ],
            temperature=0.2,
            max_tokens=400,
        )
        answer = (resp.choices[0].message.content or "").strip()
        dialogue.speak("解答如下。")
        print("\n" + "=" * 50)
        print("【模型解答】")
        print(answer)
        print("=" * 50 + "\n")
        if answer:
            dialogue.tts_only(answer)

    except Exception as e:
        print(f"[解答] 请求失败: {e}")


# ================================================================
#  任务5：动作回放
# ================================================================

def _load_action_file(name: str):
    """从 ACTIONS_DIR 加载关键帧列表，找不到返回 None。"""
    safe = name.replace("/", "_").replace("\\", "_")
    path = Path(config.ACTIONS_DIR) / f"{safe}.json"
    if not path.exists():
        return None
    with open(path, "r", encoding="utf-8") as f:
        data = json.load(f)
    return data.get("keyframes", [])


def list_actions() -> list:
    """返回 ACTIONS_DIR 下所有已录制动作名（不含扩展名）。"""
    d = Path(config.ACTIONS_DIR)
    if not d.exists():
        return []
    return sorted(os.path.splitext(f)[0] for f in os.listdir(d) if f.endswith(".json"))


def task_action(arm: Arm, action_name: str):
    """
    任务5：回放录制好的动作。
    action_name 须与 actions/ 目录中 JSON 文件名（不含 .json）完全一致。
    """
    keyframes = _load_action_file(action_name)
    if keyframes is None:
        print(f"[动作] 找不到动作文件：{action_name}")
        return
    if not keyframes:
        print(f"[动作] 动作文件为空：{action_name}")
        return

    dur_ms      = config.ACTION_PLAYBACK_DUR_MS
    interval_ms = config.ACTION_PLAYBACK_INTERVAL_MS
    pause_thr   = config.ACTION_PAUSE_THRESHOLD_MS

    # 估算总时长（用于日志）
    est_ms = sum(
        kf.get("duration", 200) if kf.get("duration", 200) > pause_thr else interval_ms
        for kf in keyframes
    )
    print(f"[动作] 开始回放【{action_name}】"
          f"  {len(keyframes)} 帧  预计 {est_ms / 1000:.1f}s")

    # 归位等待机械臂就绪
    arm.go_home()
    time.sleep(2.0)

    total = len(keyframes)
    for i, frame in enumerate(keyframes, 1):
        x, y, z, pitch = frame["x"], frame["y"], frame["z"], frame["pitch"]
        stored_dur = frame.get("duration", 200)
        is_pause   = stored_dur > pause_thr
        is_last    = (i == total)

        if is_last or is_pause:
            # 停顿帧或最后一帧用阻塞 MOVE，让舵机完全到位后再继续
            block_dur = int(stored_dur) if is_pause else dur_ms
            arm.move(x, y, z, pitch, dur=block_dur)
        else:
            arm.move_nb(x, y, z, pitch, dur=dur_ms)
            time.sleep(interval_ms / 1000.0)

        print(f"\r  [{i:3d}/{total}] x={x:6.2f} y={y:6.2f} z={z:6.2f} "
              f"pitch={pitch:5.1f}  {'[停顿]' if is_pause else ''}",
              end="", flush=True)

    print(f"\n[动作] 【{action_name}】回放完成")
