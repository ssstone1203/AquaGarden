from time import sleep

from fastapi import APIRouter, HTTPException, Request
from fastapi.responses import Response, StreamingResponse

from app.schemas.common import DetectionPayload
from app.services.video import BOUNDARY, DetectionBox, generated_stream, mjpeg_part, tank_video


router = APIRouter()


@router.get("/api/video/robot")
def robot():
    return StreamingResponse(generated_stream("机械臂", "Camera Feed"), media_type=f"multipart/x-mixed-replace; boundary={BOUNDARY}")


@router.get("/api/video/tank")
def tank():
    return StreamingResponse(_tank_stream(), media_type=f"multipart/x-mixed-replace; boundary={BOUNDARY}")


@router.get("/api/video/tank/snapshot")
def tank_snapshot():
    frame = tank_video.frame_with_overlay()
    if frame is None:
        raise HTTPException(status_code=404, detail="Not found")
    return Response(content=frame, media_type="image/jpeg")


@router.get("/api/video/tank/status")
def tank_status() -> dict:
    return tank_video.status()


@router.post("/api/video/tank/detections")
def ingest_detections(payload: DetectionPayload) -> dict:
    detections: list[DetectionBox] = []
    for item in payload.detections:
        width = item.width if item.width is not None else item.w if item.w is not None else 0
        height = item.height if item.height is not None else item.h if item.h is not None else 0
        score = item.score if item.score is not None else item.confidence if item.confidence is not None else 0
        detections.append(
            DetectionBox(
                label=item.label,
                x=_clamp01(item.x),
                y=_clamp01(item.y),
                width=_clamp01(width),
                height=_clamp01(height),
                score=float(score),
            )
        )
    tank_video.set_detections(detections)
    return {"ok": True, "count": len(detections)}


@router.post("/api/video/tank/ingest")
async def ingest_tank_frame(request: Request) -> dict:
    frame = await request.body()
    if not tank_video.update_frame(frame):
        return {"ok": False, "message": "invalid jpeg frame"}
    return {"ok": True, "seq": tank_video.frame_seq, "bytes": len(frame)}


def _tank_stream():
    last_seq = -1
    while True:
        frame = tank_video.frame_with_overlay()
        if frame is None:
            yield mjpeg_part(_fallback_frame())
            sleep(0.5)
            continue
        if tank_video.frame_seq == last_seq:
            sleep(0.05)
            continue
        yield mjpeg_part(frame)
        last_seq = tank_video.frame_seq
        sleep(0.02)


def _fallback_frame() -> bytes:
    from app.services.video import generated_jpeg

    return generated_jpeg("鱼缸", "Waiting for serial camera frame")


def _clamp01(value: float | int | None) -> float:
    try:
        parsed = float(value)
    except (TypeError, ValueError):
        parsed = 0.0
    return max(0.0, min(1.0, parsed))
