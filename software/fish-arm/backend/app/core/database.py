"""
Database configuration and utilities for AquaGarden.
"""
from sqlalchemy import create_engine
from sqlalchemy.ext.declarative import declarative_base
from sqlalchemy.orm import sessionmaker
import logging
from pathlib import Path

from app.config import settings

logger = logging.getLogger(__name__)

# Database URL from settings or default
DATABASE_URL = getattr(settings, 'database_url', "sqlite:///./aquagarden.db")

# Create engine
engine = create_engine(
    DATABASE_URL, 
    connect_args={"check_same_thread": False}
)

# Session factory
SessionLocal = sessionmaker(autocommit=False, autoflush=False, bind=engine)

# Base for models
Base = declarative_base()

def init_db():
    """Initialize database: create tables and default admin."""
    from app.models.user import User
    from app.core.security import create_default_admin
    
    # Create tables
    Base.metadata.create_all(bind=engine)
    logger.info("Database tables created")
    
    # Create default admin (need a session)
    db = SessionLocal()
    try:
        create_default_admin(db)
    finally:
        db.close()

def get_db():
    """Dependency for getting DB session."""
    db = SessionLocal()
    try:
        yield db
    finally:
        db.close()
