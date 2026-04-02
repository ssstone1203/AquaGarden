"""机械臂摄像头 MJPEG 流（与 agent/camera_preview 共享缓冲）。"""

import asyncio
import sys
from pathlib import Path

from fastapi import APIRouter
from fastapi.responses import StreamingResponse

router = APIRouter(prefix="/camera", tags=["Camera"])

# .../ruisa/backend/app/api/v1/camera_stream.py → parents[4] = ruisa
_RUISA = Path(__file__).resolve().parents[4]
_AGENT_DIR = str(_RUISA / "agent")


def _ensure_agent_path() -> None:
    if _AGENT_DIR not in sys.path:
        sys.path.insert(0, _AGENT_DIR)


@router.get("/mjpeg")
async def camera_mjpeg():
    """multipart MJPEG，供 <img src="..."> 播放。"""

    async def frames():
        _ensure_agent_path()
        import camera_preview  # noqa: WPS433

        boundary = b"--frame\r\n"
        # 无新帧时也周期性输出，避免部分浏览器断连
        placeholder = None
        while True:
            jpg = await asyncio.to_thread(camera_preview.get_latest_jpeg)
            if jpg:
                placeholder = jpg
                payload = jpg
            elif placeholder:
                payload = placeholder
            else:
                await asyncio.sleep(0.1)
                continue
            yield boundary + b"Content-Type: image/jpeg\r\n\r\n" + payload + b"\r\n"
            await asyncio.sleep(0.05)

    return StreamingResponse(
        frames(),
        media_type="multipart/x-mixed-replace; boundary=frame",
    )
