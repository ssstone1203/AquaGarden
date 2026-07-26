#!/usr/bin/env python3
# encoding: utf-8

import json
import os
import traceback
import threading
import time
from http import HTTPStatus
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from urllib.parse import urlparse

import cv2
import numpy as np
import rclpy
from rclpy.callback_groups import ReentrantCallbackGroup
from rclpy.executors import MultiThreadedExecutor
from rclpy.node import Node
from ros_robot_controller_msgs.msg import ServoPosition as BusServoPosition
from ros_robot_controller_msgs.msg import ServosPosition as BusServosPosition
from ros_robot_controller_msgs.srv import MoveStepper
from sensor_msgs.msg import CameraInfo, Image

HOME_POSE = {1: 220, 2: 489, 3: 130, 4: 842, 5: 836, 6: 509}

# The replacement servo 4 reaches the old calibrated angle 63 pulses lower.
# Keep all task poses in the original coordinate system and compensate once
# at the hardware command boundary.
SERVO_COMMAND_OFFSETS = {4: -63}

FISH_POSES = [
    {1: 304, 2: 506, 3: 331, 4: 770, 5: 267, 6: 509},  # P1
    {1: 304, 2: 497, 3: 281, 4: 885, 5: 705, 6: 518},  # P2
    {1: 304, 2: 528, 3: 342, 4: 726, 5: 401, 6: 299},  # P3: drop point
]

LOOSEN_POSES = [
    {1: 223, 2: 497, 3: 221, 4: 712, 5: 605, 6: 487},
    {1: 223, 2: 515, 3: 181, 4: 548, 5: 268, 6: 504},
    # Digging action pose.
    {1: 222, 2: 516, 3: 244, 4: 548, 5: 182, 6: 502},
    {1: 222, 2: 516, 3: 240, 4: 542, 5: 230, 6: 509},
    {1: 223, 2: 515, 3: 133, 4: 542, 5: 345, 6: 507},
]
LOOSEN_ACTION_POSE_INDEX = 3
GRIPPER_OPEN = 220
FISH_GRIPPER_OPEN = 304
FISH_GRIPPER_CLOSE = 675
RAIL_MIN = 0
RAIL_MAX = 5200
# 作业滑轨位置（与机械臂标定一致）：投食在远端；挖土/裁剪在近端
RAIL_POS_FEED = RAIL_MAX
RAIL_POS_DIG_PRUNE = RAIL_MIN
# 官方 color_sorting_stepper：time.sleep(abs(steps)/1000)；已初始化时服务立即返回，须按步数等待
RAIL_MOVE_MARGIN_SEC = 0.25
RAIL_INIT_EXTRA_SETTLE_SEC = 0.35
RAIL_SERVICE_TIMEOUT = 65.0
# 断电后位置未知：向近端大幅回退以碰限位，再视为 0（作业行程 0~5200）
RAIL_HOME_SEEK_STEPS = -6000

DEPTH_MIN = 120
DEPTH_MAX = 2200


def _json_bytes(payload):
    return json.dumps(payload, ensure_ascii=False).encode('utf-8')


def _pose_items(pose, include_gripper=True, gripper=None):
    items = []
    for sid in range(1, 7):
        if sid == 1:
            if not include_gripper:
                continue
            value = pose[1] if gripper is None else int(gripper)
        else:
            value = int(pose[sid])
        items.append((sid, value))
    return tuple(items)


class AquaBridgeNode(Node):
    def __init__(self):
        super().__init__('aqua_bridge')
        self.declare_parameter('host', '0.0.0.0')
        self.declare_parameter('port', 18080)
        self.declare_parameter('prune_yolo_model', '')
        self.declare_parameter('prune_preview_only', False)

        self._host = str(self.get_parameter('host').value)
        self._port = int(self.get_parameter('port').value)

        self._state_lock = threading.Lock()
        self._camera_lock = threading.Lock()
        self._task_lock = threading.Lock()
        self._rail_lock = threading.Lock()
        self._rail_ready_event = threading.Event()
        self._stop_event = threading.Event()

        self._current_task = 'idle'
        self._phase = 'starting'
        self._busy = False
        self._last_error = ''
        self._started_at = time.time()
        self._rail_pos = None
        self._task_seq = 0

        self._last_rgb = None
        self._last_depth = None
        self._rgb_K = None
        self._rgb_D = None
        self._rgb_jpeg = None
        self._depth_jpeg = None
        self._camera_seq = 0
        self._camera_at = 0.0
        self._camera_size = None
        self._last_servo_pose = dict(HOME_POSE)
        self._prune_executor = None
        self._last_prune_preview = ''

        cb_group = ReentrantCallbackGroup()
        # 与 init_arm_pose.py 一致：直连总线舵机话题（经 /servo_controller 链路在部分环境下不到硬件）
        self._bus_servo_pub = self.create_publisher(
            BusServosPosition,
            '/ros_robot_controller/bus_servo/set_position',
            10,
        )
        self._rail_client = self.create_client(MoveStepper, '/ros_robot_controller/move_stepper', callback_group=cb_group)

        self.create_subscription(Image, '/depth_cam/rgb/image_raw', self._on_rgb, 5)
        self.create_subscription(Image, '/depth_cam/depth/image_raw', self._on_depth, 5)
        self.create_subscription(CameraInfo, '/depth_cam/rgb/camera_info', self._on_camera_info, 5)

        self._httpd = None
        self._server_thread = None
        self._start_http_server()
        preview_only = bool(self.get_parameter('prune_preview_only').value)
        if preview_only:
            self._rail_pos = RAIL_MIN
            self._rail_ready_event.set()
            self._set_phase('idle')
            self.get_logger().info(
                f'AquaBridge http://{self._host}:{self._port} '
                '(裁剪预览模式：启动时不移动滑轨/机械臂)'
            )
        else:
            self._set_phase('rail_home')
            self.get_logger().info(f'AquaBridge http://{self._host}:{self._port} (滑轨启动归零中...)')
            threading.Thread(target=self._startup_rail_home_thread, daemon=True).start()

    def _start_http_server(self):
        node = self

        class Handler(BaseHTTPRequestHandler):
            server_version = 'AquaBridge/0.1'

            def log_message(self, fmt, *args):
                node.get_logger().debug(fmt % args)

            def _headers(self, status=HTTPStatus.OK, content_type='application/json'):
                self.send_response(int(status))
                self.send_header('Access-Control-Allow-Origin', '*')
                self.send_header('Access-Control-Allow-Methods', 'GET,POST,OPTIONS')
                self.send_header('Access-Control-Allow-Headers', 'Content-Type,Authorization')
                self.send_header('Content-Type', content_type)
                self.end_headers()

            def _json(self, payload, status=HTTPStatus.OK):
                body = _json_bytes(payload)
                self.send_response(int(status))
                self.send_header('Access-Control-Allow-Origin', '*')
                self.send_header('Access-Control-Allow-Methods', 'GET,POST,OPTIONS')
                self.send_header('Access-Control-Allow-Headers', 'Content-Type,Authorization')
                self.send_header('Content-Type', 'application/json; charset=utf-8')
                self.send_header('Content-Length', str(len(body)))
                self.end_headers()
                self.wfile.write(body)

            def do_OPTIONS(self):
                self._headers(HTTPStatus.NO_CONTENT)

            def do_GET(self):
                path = urlparse(self.path).path
                if path == '/api/status':
                    self._json(node.status_snapshot())
                elif path == '/api/camera/status':
                    self._json(node.camera_status())
                elif path == '/video/rgb.mjpg':
                    self._mjpeg('rgb')
                elif path == '/video/depth.mjpg':
                    self._mjpeg('depth')
                else:
                    self._json({'ok': False, 'message': 'not found'}, HTTPStatus.NOT_FOUND)

            def do_POST(self):
                path = urlparse(self.path).path
                if path.startswith('/api/task/'):
                    task = path.rsplit('/', 1)[-1]
                    if task == 'stop':
                        self._json(node.request_stop())
                    else:
                        result, status = node.start_task(task)
                        self._json(result, status)
                elif path == '/api/arm/home':
                    result, status = node.start_manual('arm_home', {})
                    self._json(result, status)
                elif path == '/api/arm/pose':
                    payload, err = self._read_json()
                    if err is not None:
                        self._json(err, HTTPStatus.BAD_REQUEST)
                        return
                    result, status = node.start_manual('arm_pose', payload)
                    self._json(result, status)
                elif path == '/api/arm/gripper':
                    payload, err = self._read_json()
                    if err is not None:
                        self._json(err, HTTPStatus.BAD_REQUEST)
                        return
                    result, status = node.start_manual('arm_gripper', payload)
                    self._json(result, status)
                elif path == '/api/rail/position':
                    payload, err = self._read_json()
                    if err is not None:
                        self._json(err, HTTPStatus.BAD_REQUEST)
                        return
                    result, status = node.start_manual('rail_position', payload)
                    self._json(result, status)
                else:
                    self._json({'ok': False, 'message': 'not found'}, HTTPStatus.NOT_FOUND)

            def _read_json(self):
                length = int(self.headers.get('Content-Length', '0') or '0')
                if length <= 0:
                    return {}, None
                raw = self.rfile.read(length)
                try:
                    payload = json.loads(raw.decode('utf-8'))
                except Exception:
                    return None, {'ok': False, 'message': 'invalid JSON body'}
                if not isinstance(payload, dict):
                    return None, {'ok': False, 'message': 'JSON body must be an object'}
                return payload, None

            def _mjpeg(self, kind):
                self.send_response(HTTPStatus.OK)
                self.send_header('Access-Control-Allow-Origin', '*')
                self.send_header('Cache-Control', 'no-cache, no-store, must-revalidate')
                self.send_header('Pragma', 'no-cache')
                self.send_header('Content-Type', 'multipart/x-mixed-replace; boundary=frame')
                self.end_headers()
                try:
                    while True:
                        frame = node.get_jpeg(kind)
                        self.wfile.write(b'--frame\r\n')
                        self.wfile.write(b'Content-Type: image/jpeg\r\n')
                        self.wfile.write(f'Content-Length: {len(frame)}\r\n\r\n'.encode('ascii'))
                        self.wfile.write(frame)
                        self.wfile.write(b'\r\n')
                        self.wfile.flush()
                        time.sleep(0.05)
                except (BrokenPipeError, ConnectionResetError, OSError):
                    return

        self._httpd = ThreadingHTTPServer((self._host, self._port), Handler)
        self._server_thread = threading.Thread(target=self._httpd.serve_forever, daemon=True)
        self._server_thread.start()

    def destroy_node(self):
        if self._httpd is not None:
            self._httpd.shutdown()
            self._httpd.server_close()
        super().destroy_node()

    def _set_phase(self, phase, error=''):
        with self._state_lock:
            self._phase = phase
            if error:
                self._last_error = error

    def status_snapshot(self):
        with self._state_lock:
            return {
                'ok': True,
                'connected': True,
                'busy': self._busy,
                'currentTask': self._current_task,
                'phase': self._phase,
                'railPosition': self._rail_pos,
                'railReady': self._rail_ready_event.is_set(),
                'prunePreviewPath': self._last_prune_preview or None,
                'lastError': self._last_error,
                'taskSeq': self._task_seq,
                'uptimeSec': int(time.time() - self._started_at),
                'servoPulse': dict(self._last_servo_pose),
                'camera': self.camera_status(),
            }

    def camera_status(self):
        with self._camera_lock:
            age = None if not self._camera_at else round(time.time() - self._camera_at, 3)
            return {
                'hasRgb': self._rgb_jpeg is not None,
                'hasDepth': self._depth_jpeg is not None,
                'seq': self._camera_seq,
                'ageSec': age,
                'size': self._camera_size,
            }

    def start_task(self, task):
        if task not in ('feed', 'loosen', 'prune'):
            return {'ok': False, 'message': f'unknown task: {task}'}, HTTPStatus.BAD_REQUEST
        if not self._rail_ready_event.is_set():
            return {
                'ok': False,
                'busy': True,
                'message': '滑轨启动归零中，请稍后再试',
            }, HTTPStatus.CONFLICT
        with self._task_lock:
            if self._busy:
                return {'ok': False, 'busy': True, 'message': 'task already running'}, HTTPStatus.CONFLICT
            self._busy = True
            self._current_task = task
            self._last_error = ''
            self._stop_event.clear()
            self._task_seq += 1
            seq = self._task_seq
        threading.Thread(target=self._run_task_thread, args=(task, seq), daemon=True).start()
        return {'ok': True, 'accepted': True, 'task': task, 'taskSeq': seq}, HTTPStatus.ACCEPTED

    def request_stop(self):
        self._stop_event.set()
        self._set_phase('stopping')
        return {'ok': True, 'message': 'stop requested'}

    def start_manual(self, action, payload):
        if action not in ('arm_home', 'arm_pose', 'arm_gripper', 'rail_position'):
            return {'ok': False, 'message': f'unknown manual action: {action}'}, HTTPStatus.BAD_REQUEST
        if action == 'rail_position' and not self._rail_ready_event.is_set():
            return {
                'ok': False,
                'busy': True,
                'message': '滑轨启动归零中，请稍后再试',
            }, HTTPStatus.CONFLICT
        with self._task_lock:
            if self._busy:
                return {'ok': False, 'busy': True, 'message': 'task already running'}, HTTPStatus.CONFLICT
            self._busy = True
            self._current_task = action
            self._last_error = ''
            self._stop_event.clear()
            self._task_seq += 1
            seq = self._task_seq
        threading.Thread(target=self._run_manual_thread, args=(action, payload, seq), daemon=True).start()
        return {'ok': True, 'accepted': True, 'task': action, 'taskSeq': seq}, HTTPStatus.ACCEPTED

    def _run_manual_thread(self, action, payload, seq):
        ok = False
        try:
            if action == 'arm_home':
                self._set_phase('manual_arm_home')
                self._home_arm()
            elif action == 'arm_pose':
                self._manual_arm_pose(payload)
            elif action == 'arm_gripper':
                self._manual_gripper(payload)
            elif action == 'rail_position':
                self._manual_rail(payload)
            ok = True
        except Exception as exc:
            self.get_logger().error(f'manual action {action} failed: {exc}\n{traceback.format_exc()}')
            self._set_phase('error', str(exc))
        finally:
            with self._task_lock:
                self._busy = False
                self._current_task = 'idle'
                if self._phase != 'error':
                    self._phase = 'idle' if ok else 'failed'
            self.get_logger().info(f'manual action {action} seq={seq} finished ok={ok}')

    def _as_duration(self, payload, default, minimum=0.05, maximum=8.0):
        value = float(payload.get('duration', default))
        return max(minimum, min(maximum, value))

    def _as_pulse(self, value, sid='servo'):
        pulse = int(value)
        if pulse < 0 or pulse > 1000:
            raise RuntimeError(f'{sid} pulse out of range [0,1000]: {pulse}')
        return pulse

    def _as_bool(self, value, default=False):
        if value is None:
            return default
        if isinstance(value, bool):
            return value
        if isinstance(value, str):
            return value.strip().lower() in ('1', 'true', 'yes', 'on')
        return bool(value)

    def _normalize_pose(self, payload):
        pose_data = payload.get('pose', payload)
        if not isinstance(pose_data, dict):
            raise RuntimeError('pose must be an object')
        pose = dict(HOME_POSE)
        provided = False
        for sid in range(1, 7):
            raw = pose_data.get(str(sid), pose_data.get(sid))
            if raw is None:
                continue
            pose[sid] = self._as_pulse(raw, sid=f'servo{sid}')
            provided = True
        if not provided:
            raise RuntimeError('pose must include at least one servo value')
        return pose

    def _manual_arm_pose(self, payload):
        pose = self._normalize_pose(payload)
        duration = self._as_duration(payload, default=1.0)
        include_gripper = self._as_bool(payload.get('includeGripper'), default=True)
        gripper = payload.get('gripper')
        gripper_pulse = None if gripper is None else self._as_pulse(gripper, sid='gripper')
        self._set_phase('manual_arm_pose')
        self._move_pose(
            pose,
            duration=duration,
            include_gripper=include_gripper,
            gripper=gripper_pulse,
        )

    def _manual_gripper(self, payload):
        if 'pulse' not in payload:
            raise RuntimeError('missing pulse for gripper')
        pulse = self._as_pulse(payload['pulse'], sid='gripper')
        duration = self._as_duration(payload, default=0.45)
        self._set_phase('manual_gripper')
        self._set_gripper(pulse, duration=duration)

    def _manual_rail(self, payload):
        if 'position' not in payload:
            raise RuntimeError('missing position for rail')
        target = int(payload['position'])
        if target < RAIL_MIN or target > RAIL_MAX:
            raise RuntimeError(f'rail position out of range [{RAIL_MIN},{RAIL_MAX}]: {target}')
        self._set_phase('manual_rail')
        self._ensure_rail(target)

    def _run_task_thread(self, task, seq):
        ok = False
        try:
            if task == 'feed':
                ok = self._task_feed()
            elif task == 'loosen':
                ok = self._task_loosen()
            elif task == 'prune':
                ok = self._task_prune()
        except Exception as exc:
            self.get_logger().error(f'task {task} failed: {exc}\n{traceback.format_exc()}')
            self._set_phase('error', str(exc))
            if not self._stop_event.is_set():
                try:
                    self._home_arm()
                except Exception as home_exc:
                    self.get_logger().warn(f'home after failure also failed: {home_exc}')
        finally:
            with self._task_lock:
                self._busy = False
                self._current_task = 'idle'
                if self._phase != 'error':
                    self._phase = 'idle' if ok else 'failed'
            self.get_logger().info(f'task {task} seq={seq} finished ok={ok}')

    def _check_stop(self):
        if self._stop_event.is_set() or not rclpy.ok():
            raise RuntimeError('task stopped')

    def _call_service(self, client, req, timeout):
        fut = client.call_async(req)
        ev = threading.Event()
        fut.add_done_callback(lambda _: ev.set())
        if not ev.wait(timeout=timeout):
            return None
        return fut.result()

    def _wait_dependencies(self):
        self._set_phase('checking_dependencies')
        if not self._rail_client.wait_for_service(timeout_sec=5.0):
            raise RuntimeError('move_stepper service unavailable')

    def _official_travel_wait_sec(self, steps: int) -> float:
        if steps == 0:
            return 0.0
        return abs(int(steps)) / 1000.0 + RAIL_MOVE_MARGIN_SEC

    def _rail_move_steps(self, steps: int, phase: str):
        """发送相对步数；已初始化时服务立即返回，按官方公式等待到位。"""
        steps = int(steps)
        if steps == 0:
            return
        self._set_phase(phase)
        req = MoveStepper.Request()
        req.steps = steps
        res = self._call_service(self._rail_client, req, timeout=RAIL_SERVICE_TIMEOUT)
        if res is None or not res.success:
            msg = 'rail move service timeout' if res is None else res.message
            raise RuntimeError(msg)
        self.get_logger().info(f'rail move {steps:+d}: {res.message}')
        self._wait_rail_settle(self._official_travel_wait_sec(steps), f'{phase}_settle')

    def _wait_rail_startup_ready(self):
        """滑轨手动移动/任务前须等启动归零完成。"""
        if self._rail_ready_event.is_set():
            return
        if not self._rail_ready_event.wait(timeout=120.0):
            raise RuntimeError('rail startup homing timeout')

    def _startup_rail_home_thread(self):
        """Bridge 启动后归零；_start.sh 已归零时仅同步软件坐标。"""
        try:
            if not self._rail_client.wait_for_service(timeout_sec=90.0):
                self.get_logger().warn('startup rail home skipped: move_stepper unavailable')
                return
            with self._rail_lock:
                if os.environ.get('AQUA_RAIL_HOMED') == '1':
                    self._rail_pos = RAIL_MIN
                    self.get_logger().info('startup rail: reuse _start.sh homing, software pos=0')
                else:
                    self._rail_force_home()
            self.get_logger().info('startup rail home finished')
        except Exception as exc:
            self.get_logger().error(f'startup rail home failed: {exc}')
        finally:
            self._rail_ready_event.set()
            self._set_phase('idle')
            self.get_logger().info(f'AquaBridge ready (rail at {self._rail_pos})')

    def _rail_force_home(self):
        """物理归零：向近端寻限位（-6000），软件坐标置 0。每次任务前也会调用。"""
        self._set_phase('rail_home')
        self.get_logger().info(f'rail force home: seek {RAIL_HOME_SEEK_STEPS} steps')
        self._rail_move_steps(RAIL_HOME_SEEK_STEPS, 'rail_home_seek')
        self._rail_pos = RAIL_MIN
        self._wait_rail_settle(RAIL_INIT_EXTRA_SETTLE_SEC, 'rail_home_settle')

    def _wait_rail_settle(self, duration, phase):
        self._set_phase(phase)
        end = time.time() + float(duration)
        while time.time() < end:
            self._check_stop()
            time.sleep(0.05)

    def _ensure_rail(self, target, *, force_home=True):
        """移动到绝对坐标 target（0~5200）。

        force_home=True：先 -6000 寻限位再定位（断电后恢复用）。
        force_home=False：已在目标位则不动；否则按相对步数移动（裁剪与预览摆放一致）。
        """
        self._wait_rail_startup_ready()
        target = max(RAIL_MIN, min(RAIL_MAX, int(target)))
        self._wait_dependencies()
        with self._rail_lock:
            cur = self._rail_pos
            if cur == target:
                self.get_logger().info(f'rail already at {target}, skip move')
                return True
            if force_home:
                self._rail_force_home()
                if target != RAIL_MIN:
                    self._rail_move_steps(target, f'rail_0_to_{target}')
            else:
                delta = target - (cur if cur is not None else RAIL_MIN)
                if delta != 0:
                    self._rail_move_steps(delta, f'rail_{cur}_to_{target}')
            self._rail_pos = target
        return True

    def _move_servo(self, items, duration):
        self._check_stop()
        dur = float(duration)
        logical_items = tuple((int(sid), int(pulse)) for sid, pulse in items)
        command_items = tuple(
            (
                sid,
                max(0, min(1000, pulse + SERVO_COMMAND_OFFSETS.get(sid, 0))),
            )
            for sid, pulse in logical_items
        )
        msg = BusServosPosition()
        msg.duration = dur
        for sid, pulse in command_items:
            sp = BusServoPosition()
            sp.id = sid
            # ros_robot_controller_msgs/ServoPosition：id/position 均为 uint16，必须为 int
            sp.position = pulse
            msg.position.append(sp)
        self._bus_servo_pub.publish(msg)
        time.sleep(0.08)
        self._bus_servo_pub.publish(msg)
        for sid, pulse in logical_items:
            self._last_servo_pose[sid] = pulse
        end = time.time() + dur + 0.15
        while time.time() < end:
            self._check_stop()
            time.sleep(0.05)

    def _move_pose(self, pose, duration=1.0, include_gripper=True, gripper=None):
        self._move_servo(_pose_items(pose, include_gripper=include_gripper, gripper=gripper), duration)

    def _set_gripper(self, pulse, duration=0.45):
        self._move_servo(((1, int(pulse)),), duration)

    def _home_arm(self):
        self._set_phase('arm_home')
        self._move_pose(HOME_POSE, duration=1.5, include_gripper=True, gripper=HOME_POSE[1])

    def _task_feed(self):
        self._set_phase('feed_prepare_rail')
        self._ensure_rail(RAIL_POS_FEED)
        self._set_phase('feed_running')
        self._set_gripper(FISH_GRIPPER_OPEN)
        self._move_pose(FISH_POSES[0], duration=1.0, include_gripper=False)
        self._set_gripper(FISH_GRIPPER_CLOSE)
        for pose in FISH_POSES[1:]:
            self._move_pose(pose, duration=1.0, include_gripper=False)
        self._set_gripper(FISH_GRIPPER_OPEN)
        self._home_arm()
        return True

    def _task_loosen(self):
        self._set_phase('loosen_prepare_rail')
        self._ensure_rail(RAIL_POS_DIG_PRUNE)
        self._home_arm()

        # Sequence: power-on HOME -> five calibrated poses (dig at pose3) -> HOME.
        for idx, pose in enumerate(LOOSEN_POSES, start=1):
            self._set_phase(f'loosen_pose_{idx}')
            self._move_pose(pose, duration=1.0)
            if idx == LOOSEN_ACTION_POSE_INDEX:
                self._set_phase('loosen_10s')
                end = time.time() + 10.0
                is_open = True
                while time.time() < end:
                    target = 200 if is_open else 540
                    self._set_gripper(target, duration=0.35)
                    is_open = not is_open

        self._home_arm()
        return True

    def _get_prune_executor(self):
        if self._prune_executor is None:
            from aqua_bridge.prune_executor import PruneExecutor
            model_path = str(self.get_parameter('prune_yolo_model').value).strip()
            self._prune_executor = PruneExecutor(self, model_path=model_path)
        return self._prune_executor

    def _task_prune(self):
        preview_only = bool(self.get_parameter('prune_preview_only').value)
        if preview_only:
            self._set_phase('prune_preview')
            self.get_logger().info(
                '裁剪预览模式：不移动机械臂/滑轨，请在初始位姿前放置黄叶后触发'
            )
            found, path = self._get_prune_executor().preview_inference_only()
            self._last_prune_preview = path
            self._set_phase('prune_preview_done')
            self.get_logger().info(
                f'预览完成 detected={found} image={path or "(保存失败)"}'
            )
            return True

        # 滑轨 0 → HOME 观测 → 准备位姿 → 五区标定夹取 → HOME
        self.get_logger().info('[prune] 任务开始：滑轨→HOME观测→准备位姿→五区夹取→回HOME')
        self._set_phase('prune_rail')
        # 与预览一致：已在近端 0 时不重复寻零，避免植株相对镜头位移
        self._ensure_rail(RAIL_POS_DIG_PRUNE, force_home=False)
        self._set_phase('prune_home')
        self.get_logger().info(f'[prune] 回 HOME 脉宽 {HOME_POSE}')
        self._move_pose(HOME_POSE, duration=1.5, include_gripper=True, gripper=HOME_POSE[1])
        time.sleep(0.5)
        self._set_phase('prune_observe')
        ok = self._get_prune_executor().run_once()
        if not ok:
            self._set_phase('prune_fail_home')
            self._home_arm()
            raise RuntimeError('prune: no target or grasp failed')
        self._set_phase('prune_return')
        self._home_arm()
        return True

    def _on_camera_info(self, msg):
        with self._camera_lock:
            self._rgb_K = np.array(msg.k).reshape(3, 3)
            self._rgb_D = np.array(msg.d)

    def _on_rgb(self, msg):
        try:
            arr = np.frombuffer(msg.data, dtype=np.uint8).reshape(msg.height, msg.width, -1).copy()
            if msg.encoding in ('rgb8', 'rgb'):
                arr = cv2.cvtColor(arr, cv2.COLOR_RGB2BGR)
            ok, jpg = cv2.imencode('.jpg', arr, [int(cv2.IMWRITE_JPEG_QUALITY), 80])
            if not ok:
                return
            with self._camera_lock:
                self._last_rgb = arr
                self._rgb_jpeg = jpg.tobytes()
                self._camera_seq += 1
                self._camera_at = time.time()
                self._camera_size = [int(msg.width), int(msg.height)]
        except Exception as exc:
            self.get_logger().warn(f'rgb frame decode failed: {exc}')

    def _on_depth(self, msg):
        try:
            if msg.encoding in ('32FC1', '32FC'):
                depth = np.frombuffer(msg.data, dtype=np.float32).reshape(msg.height, msg.width).copy()
                depth_mm = np.nan_to_num(depth * 1000.0).astype(np.uint16)
            else:
                depth_mm = np.frombuffer(msg.data, dtype=np.uint16).reshape(msg.height, msg.width).copy()
            vis = np.clip(depth_mm, DEPTH_MIN, DEPTH_MAX)
            vis = ((vis - DEPTH_MIN) * 255.0 / max(1, DEPTH_MAX - DEPTH_MIN)).astype(np.uint8)
            vis = cv2.applyColorMap(vis, cv2.COLORMAP_JET)
            ok, jpg = cv2.imencode('.jpg', vis, [int(cv2.IMWRITE_JPEG_QUALITY), 80])
            if not ok:
                return
            with self._camera_lock:
                self._last_depth = depth_mm
                self._depth_jpeg = jpg.tobytes()
        except Exception as exc:
            self.get_logger().warn(f'depth frame decode failed: {exc}')

    def get_jpeg(self, kind):
        with self._camera_lock:
            frame = self._rgb_jpeg if kind == 'rgb' else self._depth_jpeg
        if frame is not None:
            return frame
        return self._placeholder_jpeg(f'waiting for {kind} frame')

    def _placeholder_jpeg(self, text):
        img = np.zeros((480, 640, 3), dtype=np.uint8)
        cv2.putText(img, 'AquaBridge', (24, 64), cv2.FONT_HERSHEY_SIMPLEX, 1.1, (80, 220, 80), 2)
        cv2.putText(img, text, (24, 120), cv2.FONT_HERSHEY_SIMPLEX, 0.8, (200, 200, 200), 2)
        ok, jpg = cv2.imencode('.jpg', img)
        return jpg.tobytes() if ok else b''


def main():
    rclpy.init()
    node = AquaBridgeNode()
    executor = MultiThreadedExecutor()
    executor.add_node(node)
    try:
        executor.spin()
    except KeyboardInterrupt:
        pass
    finally:
        executor.shutdown()
        node.destroy_node()
        if rclpy.ok():
            rclpy.shutdown()


if __name__ == '__main__':
    main()
