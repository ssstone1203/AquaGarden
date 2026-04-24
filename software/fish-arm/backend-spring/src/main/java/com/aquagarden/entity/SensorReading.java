package com.aquagarden.entity;

import jakarta.persistence.*;
import java.time.Instant;

@Entity
@Table(name = "sensor_readings",
       indexes = @Index(name = "idx_sensor_ts", columnList = "recorded_at"))
public class SensorReading {

    @Id
    @GeneratedValue(strategy = GenerationType.IDENTITY)
    private Long id;

    @Column(name = "recorded_at", nullable = false)
    private Instant recordedAt;

    @Column(name = "water_temp")    private double waterTemp;
    @Column(name = "air_temp")      private double airTemp;
    @Column(name = "air_humidity")  private double airHumidity;
    @Column(name = "wqi")           private double wqi;
    @Column(name = "soil_moisture") private double soilMoisture;

    public SensorReading() {}

    public SensorReading(Instant recordedAt, double waterTemp, double airTemp,
                         double airHumidity, double wqi, double soilMoisture) {
        this.recordedAt   = recordedAt;
        this.waterTemp    = waterTemp;
        this.airTemp      = airTemp;
        this.airHumidity  = airHumidity;
        this.wqi          = wqi;
        this.soilMoisture = soilMoisture;
    }

    public Long    getId()            { return id; }
    public Instant getRecordedAt()    { return recordedAt; }
    public double  getWaterTemp()     { return waterTemp; }
    public double  getAirTemp()       { return airTemp; }
    public double  getAirHumidity()   { return airHumidity; }
    public double  getWqi()           { return wqi; }
    public double  getSoilMoisture()  { return soilMoisture; }
}
