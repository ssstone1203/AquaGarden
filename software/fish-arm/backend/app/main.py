"""
AquaGarden FastAPI main application with layered architecture.
Similar structure to ruisa backend.
"""
import sys
from pathlib import Path
from contextlib import asynccontextmanager
import logging

# Add backend to path for imports
_backend_root = Path(__file__).resolve().parent.parent
if str(_backend_root) not in sys.path:
    sys.path.insert(0, str(_backend_root))

from datetime import datetime
from fastapi import FastAPI, Request, status
from fastapi.middleware.cors import CORSMiddleware
from fastapi.responses import FileResponse, JSONResponse, RedirectResponse
from fastapi.staticfiles import StaticFiles

from app.api.v1 import api_router
from app.config import settings
from app.core.database import init_db

# Configure logging
logging.basicConfig(level=logging.INFO)
logger = logging.getLogger(__name__)


@asynccontextmanager
async def lifespan(app: FastAPI):
    """Lifespan events for startup and shutdown."""
    # Initialize database and default admin
    logger.info("Initializing AquaGarden database...")
    init_db()
    logger.info("AquaGarden backend started successfully. Ready for frontend connections.")
    yield
    logger.info("Shutting down AquaGarden backend.")


def create_app() -> FastAPI:
    """Create and configure the FastAPI app."""
    app = FastAPI(
        title="AquaGarden API",
        description="智能水族箱管理系统 - Layered FastAPI backend with frontend",
        version="2.0.0",
        docs_url="/docs",
        lifespan=lifespan,
    )

    # CORS
    app.add_middleware(
        CORSMiddleware,
        allow_origins=settings.cors_origins,
        allow_credentials=True,
        allow_methods=["*"],
        allow_headers=["*"],
    )

    # Exception handler
    @app.exception_handler(Exception)
    async def _unhandled(request: Request, exc: Exception):
        logger.exception("Unhandled exception")
        return JSONResponse(
            status_code=status.HTTP_500_INTERNAL_SERVER_ERROR,
            content={"detail": "内部服务器错误"},
        )

    # Include API routers
    app.include_router(api_router, prefix=settings.api_v1_prefix)

    # Frontend directory (keep as front/ for compatibility with existing HTML/JS)
    # _backend_root is backend/, so frontend is at parent level
    frontend_dir = _backend_root.parent / "front"
    if frontend_dir.exists():
        # Mount static files for CSS, JS
        app.mount("/css", StaticFiles(directory=str(frontend_dir / "css")), name="css")
        app.mount("/js", StaticFiles(directory=str(frontend_dir / "js")), name="js")
        app.mount("/static", StaticFiles(directory=str(frontend_dir)), name="static")

        # Serve HTML pages
        @app.get("/")
        async def root():
            """Root redirects to login."""
            return RedirectResponse(url="/login.html")

        @app.get("/login")
        @app.get("/login.html")
        async def login_page():
            return FileResponse(str(frontend_dir / "login.html"))

        @app.get("/register")
        @app.get("/register.html")
        async def register_page():
            return FileResponse(str(frontend_dir / "register.html"))

        @app.get("/index")
        @app.get("/index.html")
        async def index_page():
            return FileResponse(str(frontend_dir / "index.html"))

        @app.get("/cameras")
        @app.get("/cameras.html")
        async def cameras_page():
            return FileResponse(str(frontend_dir / "cameras.html"))

        @app.get("/history")
        @app.get("/history.html")
        async def history_page():
            return FileResponse(str(frontend_dir / "history.html"))

        @app.get("/robot")
        @app.get("/robot.html")
        async def robot_page():
            return FileResponse(str(frontend_dir / "robot.html"))

        @app.get("/alerts")
        @app.get("/alerts.html")
        async def alerts_page():
            return FileResponse(str(frontend_dir / "alerts.html"))

        @app.get("/settings")
        @app.get("/settings.html")
        async def settings_page():
            return FileResponse(str(frontend_dir / "settings.html"))

        logger.info(f"Frontend served from: {frontend_dir}")
    else:
        logger.warning("Frontend directory not found. Only API available.")

    @app.get("/health")
    async def health():
        """Health check endpoint."""
        return {
            "status": "ok",
            "service": "aquagarden",
            "version": "2.0.0",
            "timestamp": datetime.now().isoformat(),
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
