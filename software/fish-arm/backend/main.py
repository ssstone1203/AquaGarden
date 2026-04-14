"""
AquaGarden Backend Launcher - Uses the new layered FastAPI structure in app/.
Run with: python -m uvicorn app.main:app --reload
or directly: python main.py
"""
from app.main import app
from app.config import settings
import uvicorn
import sys
from pathlib import Path

if __name__ == "__main__":
    print("🚀 Starting AquaGarden with layered FastAPI backend (app/main.py)")
    print(f"Access at http://localhost:{settings.app_port}")
    print(f"Frontend: http://localhost:{settings.app_port}/login.html")
    uvicorn.run(
        "app.main:app",
        host=settings.app_host,
        port=settings.app_port,
        reload=settings.is_development,
        log_level=settings.log_level.lower(),
    )
