"""
Ruisa Agent Web — 仅服务 agent/agent.py 对话逻辑 + agent/tasks.py 五项任务。
"""
import sys
from contextlib import asynccontextmanager
from pathlib import Path

_backend_root = Path(__file__).resolve().parent.parent
_ruisa_root = _backend_root.parent
if str(_backend_root) not in sys.path:
    sys.path.insert(0, str(_backend_root))

from datetime import datetime, timezone

from fastapi import FastAPI, Request, status
from fastapi.middleware.cors import CORSMiddleware
from fastapi.responses import FileResponse, JSONResponse

from app.api.v1 import api_router
from app.config import settings


@asynccontextmanager
async def lifespan(app: FastAPI):
    print("[Startup] Ruisa Agent Web — agent.py / tasks.py 桥接已就绪")
    yield
    print("[Shutdown] 退出")


def create_app() -> FastAPI:
    app = FastAPI(
        title="Ruisa 小臂 Agent",
        description="Web 端复现 agent/agent.py 持续对话与 agent/tasks.py 五项任务。",
        version="2.0.0",
        docs_url="/docs",
        lifespan=lifespan,
    )

    app.add_middleware(
        CORSMiddleware,
        allow_origins=["*"],
        allow_credentials=True,
        allow_methods=["*"],
        allow_headers=["*"],
    )

    @app.exception_handler(Exception)
    async def _unhandled(request: Request, exc: Exception):
        import logging

        logging.exception("unhandled")
        return JSONResponse(
            status_code=status.HTTP_500_INTERNAL_SERVER_ERROR,
            content={"detail": "内部错误"},
        )

    app.include_router(api_router, prefix=settings.api_v1_prefix)

    frontend_dir = _ruisa_root / "frontend"
    if frontend_dir.is_dir():

        @app.get("/")
        async def serve_index():
            return FileResponse(str(frontend_dir / "index.html"))

        @app.get("/js/{filename}")
        async def serve_js(filename: str):
            from fastapi import HTTPException

            p = frontend_dir / "js" / filename
            if p.is_file():
                return FileResponse(str(p), media_type="application/javascript")
            raise HTTPException(status_code=404)

    @app.get("/health")
    async def health():
        return {
            "status": "ok",
            "service": "ruisa-agent-web",
            "timestamp": datetime.now(timezone.utc).isoformat(),
        }

    return app


app = create_app()

if __name__ == "__main__":
    import uvicorn

    uvicorn.run(
        "app.main:app",
        host=settings.app_host,
        port=settings.app_port,
        reload=settings.is_development,
        log_level=settings.log_level.lower(),
    )
