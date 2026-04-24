package com.aquagarden.service;

import com.aquagarden.repo.SensorReadingRepository;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.beans.factory.annotation.Value;
import org.springframework.scheduling.annotation.Scheduled;
import org.springframework.stereotype.Service;
import org.springframework.transaction.annotation.Transactional;

import java.time.Instant;
import java.time.temporal.ChronoUnit;

@Service
public class SensorReadingRetentionService {

    private static final Logger log = LoggerFactory.getLogger(SensorReadingRetentionService.class);

    private final SensorReadingRepository readingRepo;

    @Value("${aquagarden.sensor-readings-retention-days:35}")
    private int retentionDays;

    public SensorReadingRetentionService(SensorReadingRepository readingRepo) {
        this.readingRepo = readingRepo;
    }

    /**
     * 按配置保留最近若干天的传感器采样；更早的行从 SQLite 删除，避免表无限增长。
     * 默认每天 03:15 执行；cron 与保留天数见 application.properties。
     */
    @Scheduled(cron = "${aquagarden.sensor-readings-purge-cron:0 15 3 * * *}")
    @Transactional
    public void purgeOldReadings() {
        if (retentionDays < 1) {
            log.warn("aquagarden.sensor-readings-retention-days is {}; skip purge", retentionDays);
            return;
        }
        Instant cutoff = Instant.now().minus(retentionDays, ChronoUnit.DAYS);
        int removed = readingRepo.deleteByRecordedAtBefore(cutoff);
        if (removed > 0) {
            log.info("Purged {} sensor_readings older than {} days (before {})", removed, retentionDays, cutoff);
        }
    }
}
