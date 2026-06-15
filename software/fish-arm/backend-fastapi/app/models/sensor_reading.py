from sqlalchemy import BigInteger, Float, Index, Integer
from sqlalchemy.orm import Mapped, mapped_column

from app.db.session import Base


class SensorReading(Base):
    __tablename__ = "sensor_readings"
    __table_args__ = (Index("idx_sensor_ts", "recorded_at"),)

    id: Mapped[int] = mapped_column(Integer, primary_key=True, autoincrement=True)
    recorded_at: Mapped[int] = mapped_column(BigInteger, default=lambda: 0)
    water_temp: Mapped[float] = mapped_column(Float, default=24.0)
    air_temp: Mapped[float] = mapped_column(Float, default=26.0)
    air_humidity: Mapped[float] = mapped_column(Float, default=55.0)
    wqi: Mapped[float] = mapped_column(Float, default=70.0)
    soil_moisture: Mapped[float] = mapped_column(Float, default=62.0)
