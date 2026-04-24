package com.aquagarden.repo;

import com.aquagarden.entity.SensorReading;
import org.springframework.data.jpa.repository.JpaRepository;

import java.time.Instant;
import java.util.List;

public interface SensorReadingRepository extends JpaRepository<SensorReading, Long> {
    /** 查询指定时间点之后的所有记录，按时间升序（供历史图表使用）。 */
    List<SensorReading> findByRecordedAtAfterOrderByRecordedAtAsc(Instant after);

    /** 批量删除早于 cutoff 的记录（供定时保留策略使用）。返回删除行数。 */
    int deleteByRecordedAtBefore(Instant cutoff);
}
