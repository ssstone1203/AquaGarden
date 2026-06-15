from dataclasses import dataclass
from datetime import datetime, timezone
from random import randint
from threading import Lock
from time import time

from app.schemas.common import SensorSnapshot


DEMO_SNAPSHOT = SensorSnapshot(
    water_temp=24.5,
    air_temp=26.0,
    air_humidity=58.0,
    wqi=76.0,
    soil_moisture=62.0,
)


@dataclass
class PendingCommand:
    cmd: int
    power: int
    enqueue_time_ms: int

    @property
    def cmd_name(self) -> str:
        return {1: "stop", 2: "start", 3: "set_pwm"}.get(self.cmd, "unknown")


class SystemState:
    def __init__(self) -> None:
        self._lock = Lock()
        self.mode = "demo"
        self.robot_position = {"x": 0, "y": 0, "z": 0}
        self.latest_real: SensorSnapshot | None = None
        self.latest_real_ts = 0
        self.pending_command: PendingCommand | None = None

    def update_sensor(self, snapshot: SensorSnapshot, timestamp_ms: int | None = None) -> None:
        with self._lock:
            self.latest_real = snapshot
            self.latest_real_ts = timestamp_ms if timestamp_ms and timestamp_ms > 0 else now_ms()

    def has_hardware_snapshot(self) -> bool:
        return self.latest_real is not None

    def has_fresh_hardware_snapshot(self, max_age_ms: int) -> bool:
        return self.latest_real is not None and self.latest_real_ts > 0 and now_ms() - self.latest_real_ts <= max(0, max_age_ms)

    def read_sensors(self) -> SensorSnapshot:
        return self.latest_real or DEMO_SNAPSHOT

    def read_fresh_or_demo(self, max_age_ms: int) -> SensorSnapshot:
        return self.latest_real if self.has_fresh_hardware_snapshot(max_age_ms) else DEMO_SNAPSHOT

    def move_robot(self, direction: str) -> dict[str, int]:
        delta = {
            "up": (0, 0, 1),
            "down": (0, 0, -1),
            "left": (-1, 0, 0),
            "right": (1, 0, 0),
            "forward": (0, 1, 0),
            "backward": (0, -1, 0),
        }.get(direction, (0, 0, 0))
        with self._lock:
            self.robot_position["x"] += delta[0]
            self.robot_position["y"] += delta[1]
            self.robot_position["z"] += delta[2]
            return dict(self.robot_position)

    def servo_angles(self) -> list[int]:
        return [max(0, min(180, base + randint(-2, 2))) for base in [90, 45, 120, 60, 90, 30]]

    def enqueue_pump(self, cmd: int, power: int) -> None:
        with self._lock:
            self.pending_command = PendingCommand(cmd=cmd, power=max(0, min(100, power)), enqueue_time_ms=now_ms())

    def drain_pump(self) -> PendingCommand | None:
        with self._lock:
            pending = self.pending_command
            self.pending_command = None
            return pending

    def peek_pump(self) -> PendingCommand | None:
        return self.pending_command


def now_ms() -> int:
    return int(time() * 1000)


def iso_now() -> str:
    return datetime.now(timezone.utc).isoformat().replace("+00:00", "Z")


state = SystemState()
