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

    @Value("${aquagarden.sensor-readings-max-pages:100}")
    private int maxPages;

    @Value("${aquagarden.sensor-readings-page-size:20}")
    private int pageSize;

    public SensorReadingRetentionService(SensorReadingRepository readingRepo) {
        this.readingRepo = readingRepo;
    }

    @Transactional
    public void purgeOldReadings() {
        if (retentionDays < 1) {
            log.warn("aquagarden.sensor-readings-retention-days is {}; skip purge", retentionDays);
        } else {
            Instant cutoff = Instant.now().minus(retentionDays, ChronoUnit.DAYS);
            int removed = readingRepo.deleteByRecordedAtBefore(cutoff);
            if (removed > 0) {
                log.info("Purged {} sensor_readings older than {} days (before {})", removed, retentionDays, cutoff);
            }
        }
        enforceMaxRecordWindow();
    }

    /**
     * 详细记录页每页 20 条，最多保留 100 页，对应 2000 条最新采样。
     * 新数据写入后或定时任务运行时都会触发该裁剪，避免数据库无限增长。
     */
    @Transactional
    public void enforceMaxRecordWindow() {
        int maxRecords = maxPages * pageSize;
        if (maxRecords < 1) {
            log.warn("sensor history maxRecords is {}; skip count-based purge", maxRecords);
            return;
        }
        long total = readingRepo.count();
        if (total <= maxRecords) return;

        int removed = readingRepo.deleteOlderKeepingLatest(maxRecords);
        if (removed > 0) {
            log.info("Purged {} sensor_readings beyond {} pages (keeping latest {} rows)", removed, maxPages, maxRecords);
        }
    }

    /**
     * 按配置保留最近若干天的传感器采样，并额外限制详细记录页最多显示 100 页的数据。
     * 默认每天 03:15 执行；cron 与各项保留参数见 application.properties。
     */
    @Scheduled(cron = "${aquagarden.sensor-readings-purge-cron:0 15 3 * * *}")
    public void scheduledPurgeOldReadings() {
        purgeOldReadings();
    }
}
