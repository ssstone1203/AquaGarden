from datetime import datetime, timedelta
from random import random
from typing import Annotated, Any

from fastapi import APIRouter, Depends, Header, HTTPException, Query, Response
from sqlalchemy import delete, select
from sqlalchemy.orm import Session

from app.core.config import settings
from app.db.session import get_db
from app.models.sensor_reading import SensorReading
from app.schemas.common import SensorSnapshot
from app.services.logs import hub, sensor_message
from app.services.state import now_ms, state


router = APIRouter()
SENSOR_META = {
    "temp-01": ("water_temp", "C"),
    "air-temp-01": ("air_temp", "C"),
    "humidity-01": ("air_humidity", "%RH"),
    "wqi-01": ("wqi", "index"),
    "soil-moisture-01": ("soil_moisture", "%"),
}


@router.get("/api/sensor/latest")
def latest(sensorId: str = Query()) -> dict:
    snapshot = state.read_sensors()
    ts = state.latest_real_ts if state.has_hardware_snapshot() else now_ms()
    if sensorId not in SENSOR_META:
        return {"code": 400, "msg": f"unknown sensorId: {sensorId}", "data": {"sensorId": sensorId, "value": None, "unit": "", "ts": ts}}
    field, unit = SENSOR_META[sensorId]
    return {"code": 0, "msg": "ok", "data": {"sensorId": sensorId, "value": getattr(snapshot, field), "unit": unit, "ts": ts}}


@router.get("/api/debug/whoami")
def whoami() -> dict:
    return {"service": "fastapi-aquagarden", "port": str(settings.port), "ts": now_ms()}


@router.get("/api/sensors")
def sensors() -> dict:
    fresh = state.has_fresh_hardware_snapshot(settings.sensors_realtime_max_age_ms)
    snapshot = state.read_fresh_or_demo(settings.sensors_realtime_max_age_ms)
    hardware_ts = state.latest_real_ts
    details = state.read_sensor_details() if fresh else {}
    return {
        "water_temp": snapshot.water_temp,
        "air_temp": snapshot.air_temp,
        "air_humidity": snapshot.air_humidity,
        "wqi": snapshot.wqi,
        "soil_moisture": snapshot.soil_moisture,
        "source": "hardware" if fresh else "demo",
        "realtime": fresh,
        "hardwareTs": hardware_ts,
        "ageMs": now_ms() - hardware_ts if fresh and hardware_ts > 0 else -1,
        **details,
    }


@router.get("/api/sensors/history")
def history(db: Annotated[Session, Depends(get_db)], range_value: str = Query(default="24h", alias="range")) -> list[dict[str, Any]]:
    hours_back = 168 if range_value == "7d" else 720 if range_value == "30d" else 24
    since = now_ms() - int(timedelta(hours=hours_back).total_seconds() * 1000)
    rows = db.scalars(select(SensorReading).where(SensorReading.recorded_at > since).order_by(SensorReading.recorded_at.asc())).all()
    if rows:
        return [_reading_to_history(row) for row in rows]
    count = 168 if range_value == "7d" else 240 if range_value == "30d" else 120
    interval = timedelta(hours=1 if range_value == "7d" else 2 if range_value == "30d" else 0.5)
    now = now_ms()
    return [
        {
            "time": datetime.fromtimestamp((now - int(interval.total_seconds() * 1000) * i) / 1000).strftime("%Y/%m/%d %H:%M"),
            "water_temp": round(24.0 + random() * 4 - 2, 1),
            "air_temp": round(26.0 + random() * 6 - 3, 1),
            "air_humidity": round(55.0 + random() * 20 - 10, 1),
            "wqi": round(72.0 + random() * 20 - 10, 0),
            "soil_moisture": round(62.0 + random() * 20 - 10, 0),
        }
        for i in range(count - 1, -1, -1)
    ]


@router.post("/api/sensors/ingest", status_code=200)
async def ingest(body: SensorSnapshot, db: Annotated[Session, Depends(get_db)], response: Response) -> Response:
    ts = now_ms()
    state.update_sensor(body, ts, details={})
    _save_reading(db, body, ts)
    await hub.broadcast(sensor_message(body, ts, source="hardware"))
    response.status_code = 200
    return response


@router.post("/api/sensor/upload")
async def upload(
    body: dict[str, Any],
    db: Annotated[Session, Depends(get_db)],
    x_device_token: str | None = Header(default=None, alias="X-Device-Token"),
) -> dict:
    if not settings.device_upload_token:
        raise HTTPException(status_code=503, detail={"code": 503, "msg": "device upload token is not configured", "data": {}})
    if x_device_token != settings.device_upload_token:
        raise HTTPException(status_code=401, detail={"code": 401, "msg": "invalid device token", "data": {}})
    sensor_id = str(body.get("sensorId", ""))
    if not sensor_id:
        return {"code": 400, "msg": "sensorId is required", "data": {}}
    if sensor_id not in SENSOR_META:
        return {"code": 400, "msg": f"unknown sensorId: {sensor_id}", "data": {"sensorId": sensor_id}}
    value = body.get("value")
    if not isinstance(value, (int, float)):
        return {"code": 400, "msg": "value must be numeric", "data": {}}
    ts = int(body.get("ts") or now_ms())
    current = state.read_sensors().model_copy()
    setattr(current, SENSOR_META[sensor_id][0], float(value))
    state.update_sensor(current, ts, details={})
    _save_reading(db, current, ts)
    await hub.broadcast(sensor_message(current, ts, source="hardware"))
    return {"code": 0, "msg": "ok", "data": {"deviceId": str(body.get("deviceId", "")), "sensorId": sensor_id, "value": value, "ts": ts}}


def _save_reading(db: Session, snapshot: SensorSnapshot, recorded_at_ms: int) -> None:
    db.add(SensorReading(recorded_at=int(recorded_at_ms), **snapshot.model_dump()))
    db.commit()
    limit = settings.sensor_readings_max_pages * settings.sensor_readings_page_size
    ids = db.scalars(select(SensorReading.id).order_by(SensorReading.recorded_at.desc()).offset(limit)).all()
    if ids:
        db.execute(delete(SensorReading).where(SensorReading.id.in_(ids)))
        db.commit()


def _reading_to_history(row: SensorReading) -> dict[str, Any]:
    return {
        "time": datetime.fromtimestamp(int(row.recorded_at) / 1000).strftime("%Y/%m/%d %H:%M"),
        "water_temp": row.water_temp,
        "air_temp": row.air_temp,
        "air_humidity": row.air_humidity,
        "wqi": row.wqi,
        "soil_moisture": row.soil_moisture,
    }
