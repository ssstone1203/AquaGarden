from fastapi import FastAPI, Request
from fastapi.middleware.cors import CORSMiddleware
from fastapi.responses import JSONResponse

from app.api import ai, aqua, auth, mcu, mode, robot, sensors, users, video, websocket
from app.core.config import settings
from app.db.session import init_db
from app.services.hardware_serial import hardware_serial


def create_app() -> FastAPI:
    app = FastAPI(title="AquaGarden FastAPI Backend", version="1.0.0")

    app.add_middleware(
        CORSMiddleware,
        allow_origins=settings.cors_origins,
        allow_origin_regex=settings.cors_origin_regex,
        allow_credentials=False,
        allow_methods=["GET", "POST", "PUT", "DELETE", "OPTIONS", "PATCH"],
        allow_headers=["*"],
    )

    @app.on_event("startup")
    def on_startup() -> None:
        init_db()
        hardware_serial.start()

    @app.on_event("shutdown")
    def on_shutdown() -> None:
        hardware_serial.stop()

    @app.exception_handler(Exception)
    async def unhandled_error(_: Request, exc: Exception) -> JSONResponse:
        app.logger.exception("Unhandled request error", exc_info=exc) if hasattr(app, "logger") else None
        return JSONResponse(status_code=500, content={"detail": "Internal server error"})

    app.include_router(auth.router)
    app.include_router(users.router)
    app.include_router(sensors.router)
    app.include_router(robot.router)
    app.include_router(mode.router)
    app.include_router(aqua.router)
    app.include_router(video.router)
    app.include_router(mcu.router)
    app.include_router(ai.router)
    app.include_router(websocket.router)
    return app


app = create_app()
