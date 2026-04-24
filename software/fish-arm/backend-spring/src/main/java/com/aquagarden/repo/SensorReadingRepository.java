package com.aquagarden.repo;

import com.aquagarden.entity.SensorReading;
import org.springframework.data.jpa.repository.JpaRepository;
import org.springframework.data.jpa.repository.Modifying;
import org.springframework.data.jpa.repository.Query;
import org.springframework.data.repository.query.Param;

import java.time.Instant;
import java.util.List;

public interface SensorReadingRepository extends JpaRepository<SensorReading, Long> {
    /** 查询指定时间点之后的所有记录，按时间升序（供历史图表使用）。 */
    List<SensorReading> findByRecordedAtAfterOrderByRecordedAtAsc(Instant after);

    /** 批量删除早于 cutoff 的记录（供定时保留策略使用）。返回删除行数。 */
    int deleteByRecordedAtBefore(Instant cutoff);

    /** 保留最近 retainCount 条记录，删除更老的数据（用于详细记录页页数上限）。 */
    @Modifying
    @Query(value = """
        DELETE FROM sensor_readings
        WHERE id IN (
            SELECT id
            FROM sensor_readings
            ORDER BY recorded_at DESC, id DESC
            LIMIT -1 OFFSET :retainCount
        )
        """, nativeQuery = true)
    int deleteOlderKeepingLatest(@Param("retainCount") int retainCount);
}
