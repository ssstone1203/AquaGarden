"""
AquaGarden 后端应用入口
机械臂智能控制系统后端，基于 FastAPI + RA6M5 串口协议
"""
import sys
from pathlib import Path

# 直接执行 `python app/main.py` 时，解释器会把 app/ 当作 sys.path 根目录，
# `from app.xxx` 会找不到包。将 backend 根目录插入 path 后与普通 `python -m app.main` 一致。
_backend_root = Path(__file__).resolve().parent.parent
if str(_backend_root) not in sys.path:
    sys.path.insert(0, str(_backend_root))

import asyncio
import uuid
from contextlib import asynccontextmanager
from datetime import datetime, timezone

from fastapi import FastAPI, Request, status
from fastapi.middleware.cors import CORSMiddleware
from fastapi.responses import JSONResponse, FileResponse

from app.api.v1 import api_router
from app.config import settings
from app.database import engine
from app.redis_client import RedisManager


# ── 全局异常处理器 ──────────────────────────────────────────────────────────
async def global_exception_handler(request: Request, exc: Exception) -> JSONResponse:
    """统一异常处理"""
    request_id = str(uuid.uuid4())
    import logging
    logging.error(
        f"Unhandled exception | request_id={request_id} | path={request.url.path}",
        exc_info=True,
    )
    return JSONResponse(
        status_code=status.HTTP_500_INTERNAL_SERVER_ERROR,
        content={
            "error": {
                "code": "INTERNAL_ERROR",
                "message": "服务器内部错误，请稍后重试",
                "request_id": request_id,
            }
        },
    )


# ── 生命周期 ──────────────────────────────────────────────────────────────
@asynccontextmanager
async def lifespan(app: FastAPI):
    """应用启动与关闭"""
    # ── 启动 ──
    await RedisManager.init()
    print("[Startup] Redis 连接已初始化")

    if settings.is_development:
        from app.database import init_db, async_session_maker
        from sqlalchemy import select
        from app.models.user import User
        from app.core.security import hash_password

        await init_db()
        print("[Startup] 数据库表已创建（开发模式）")

        # 创建默认管理员账号（如果不存在）
        async with async_session_maker() as session:
            stmt = select(User).where(User.username == "admin")
            result = await session.execute(stmt)
            if result.scalar_one_or_none() is None:
                admin = User(
                    username="admin",
                    hashed_password=hash_password("admin123"),
                    role="admin",
                    is_active=True,
                )
                session.add(admin)
                await session.commit()
                print("[Startup] 已创建默认管理员账号: admin / admin123")

    print("[Startup] 后端服务已就绪")

    yield  # ← 应用在此运行

    # ── 关闭 ──
    await RedisManager.close()
    print("[Shutdown] Redis 连接已关闭")

    await engine.dispose()
    print("[Shutdown] 数据库引擎已关闭")


# ── FastAPI 应用 ─────────────────────────────────────────────────────────────
def create_app() -> FastAPI:
    app = FastAPI(
        title="AquaGarden 机械臂智能控制系统",
        description=(
            "基于 FastAPI 构建的机械臂控制系统后端 API。\n\n"
            "功能：RA6M5 串口协议通信、示教标定、颜色检测引导夹取分拣、"
            "彩色物块分拣任务执行、操作日志记录。"
        ),
        version="1.0.0",
        docs_url="/docs",
        redoc_url="/redoc",
        openapi_url="/openapi.json",
        lifespan=lifespan,
    )

    # ── CORS（允许前端访问）────────────────────────────────────────────
    cors_origins = list(settings.cors_origins) + [
        "http://127.0.0.1:5500",
        "http://localhost:5500",
        "http://127.0.0.1:8000",
        "http://localhost:8000",
    ]
    app.add_middleware(
        CORSMiddleware,
        allow_origins=["*"],
        allow_credentials=True,
        allow_methods=["*"],
        allow_headers=["*"],
    )

    # ── 全局异常处理 ──
    app.add_exception_handler(Exception, global_exception_handler)

    # ── 注册路由 ──
    app.include_router(api_router, prefix=settings.api_v1_prefix)

    # ── 前端静态页面（开发模式直接返回前端页面）──────────────────────────
    frontend_dir = Path(__file__).resolve().parent.parent.parent / "frontend"
    if not frontend_dir.exists():
        frontend_dir = Path(__file__).resolve().parent.parent.parent / "前端"
    if frontend_dir.exists():
        @app.get("/")
        async def serve_index():
            return FileResponse(str(frontend_dir / "index.html"))

        @app.get("/index.html")
        async def serve_index_html():
            return FileResponse(str(frontend_dir / "index.html"))

        @app.get("/login.html")
        async def serve_login():
            return FileResponse(str(frontend_dir / "login.html"))

        @app.get("/register.html")
        async def serve_register():
            return FileResponse(str(frontend_dir / "register.html"))

        @app.get("/js/{filename}")
        async def serve_js(filename: str):
            fpath = frontend_dir / "js" / filename
            if fpath.exists():
                return FileResponse(str(fpath), media_type="application/javascript")
            from fastapi import HTTPException
            raise HTTPException(status_code=404, detail="Not Found")

        @app.get("/css/{filename}")
        async def serve_css(filename: str):
            fpath = frontend_dir / "css" / filename
            if fpath.exists():
                return FileResponse(str(fpath), media_type="text/css")
            from fastapi import HTTPException
            raise HTTPException(status_code=404, detail="Not Found")

    # ── 健康检查 ──
    @app.get("/health", tags=["健康检查"])
    async def health_check():
        redis_ok = await RedisManager.ping()
        return {
            "status": "healthy",
            "timestamp": datetime.now(timezone.utc).isoformat(),
            "redis": "connected" if redis_ok else "disconnected",
            "version": "1.0.0",
        }

    # ── 根路径 ──
    @app.get("/", tags=["首页"])
    async def root():
        return {
            "Name": "AquaGarden Arm API",
            "version": "1.0.0",
            "docs": "/docs",
            "redoc": "/redoc",
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
