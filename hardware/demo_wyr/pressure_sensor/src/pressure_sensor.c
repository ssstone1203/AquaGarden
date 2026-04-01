#include "pressure_sensor.h"

/* 以下参数根据实际硬件进行修改 */
#define PRESSURE_SENSOR_FULL_SCALE_KG     (5.0f)   /* 传感器量程：5kg */
#define PRESSURE_SENSOR_FULL_SCALE_VOLT   (4.0f)   /* 0~4V 对应 0~满量程 */
#define PRESSURE_SENSOR_ADC_REFERENCE_V   (5.0f)   /* ADC 参考电压，典型 3.3V 或 5V */
#define PRESSURE_SENSOR_ADC_MAX_COUNTS    (4095.0f)/* 12bit ADC 最大计数值 */

/* 假设电路为：VCC ─ R6(10k) ─ 芯片测量电阻(传感器) ─ GND
 *            ADC 采样点在 R6 与传感器之间（见原理图 R6=10k）。
 * 则传感器电阻为：R_sensor = V_adc * R6 / (Vcc - V_adc)
 */
#define PRESSURE_SENSOR_FIXED_RES_OHM     (10000.0f)

/* 从图 3 “压力-电阻曲线”上人工读出的若干校准点（单位：kΩ 与 kg）
 * 压力增加 → 电阻减小，整体呈非线性，下面用线性插值近似。
 */
typedef struct st_pressure_point
{
    float resistance_kohm;
    float pressure_kg;
} pressure_point_t;

static const pressure_point_t g_pressure_curve[] =
{
    { 8.5f, 0.0f },
    { 6.5f, 0.5f },
    { 5.0f, 1.0f },
    { 3.3f, 2.0f },
    { 2.7f, 3.0f },
    { 2.3f, 4.0f },
    { 2.0f, 5.0f },
};

/* 全局变量：在调试窗口中直接观察三个通道的原始值与压力值 */
uint16_t g_adc_raw[PRESSURE_SENSOR_NUM]   = {0};
float    g_pressure[PRESSURE_SENSOR_NUM]  = {0.0f};

fsp_err_t pressure_sensor_init(void)
{
    fsp_err_t err = FSP_SUCCESS;

    /* 打开 ADC0，并配置扫描通道（AN000 → Channel 0） */
    err = g_adc0.p_api->open(g_adc0.p_ctrl, &g_adc0_cfg);
    if (FSP_SUCCESS != err)
    {
        return err;
    }

    err = g_adc0.p_api->scanCfg(g_adc0.p_ctrl, &g_adc0_channel_cfg);
    if (FSP_SUCCESS != err)
    {
        return err;
    }

    /* 单次扫描模式下，每次 read 前都需要手动触发一次扫描；
       如果以后改成连续扫描，这里可以改成 scanStart 一次即可。 */
    return FSP_SUCCESS;
}

float pressure_sensor_convert_to_kg(uint16_t adc_raw)
{
    /* 1. ADC 计数 → 电压 */
    float adc_voltage = ((float) adc_raw) * PRESSURE_SENSOR_ADC_REFERENCE_V / PRESSURE_SENSOR_ADC_MAX_COUNTS;

    if (adc_voltage <= 0.0f)
    {
        return 0.0f;
    }

    if (adc_voltage >= PRESSURE_SENSOR_ADC_REFERENCE_V)
    {
        adc_voltage = PRESSURE_SENSOR_ADC_REFERENCE_V - 0.0001f;
    }

    /* 2. 根据分压电路换算为传感器电阻（单位：kΩ）
     *    R_sensor = V_adc * R_fixed / (Vcc - V_adc)
     */
    float r_sensor_ohm  = (adc_voltage * PRESSURE_SENSOR_FIXED_RES_OHM) /
                          (PRESSURE_SENSOR_ADC_REFERENCE_V - adc_voltage);
    float r_sensor_kohm = r_sensor_ohm / 1000.0f;

    /* 3. 使用图 3 的“压力-电阻曲线”表做线性插值 */
    const uint32_t point_num = (uint32_t)(sizeof(g_pressure_curve) / sizeof(g_pressure_curve[0]));

    /* 电阻最大（基本无压力），直接返回 0kg */
    if (r_sensor_kohm >= g_pressure_curve[0].resistance_kohm)
    {
        return g_pressure_curve[0].pressure_kg;
    }

    /* 电阻最小（接近满量程），直接返回满量程 5kg */
    if (r_sensor_kohm <= g_pressure_curve[point_num - 1U].resistance_kohm)
    {
        return g_pressure_curve[point_num - 1U].pressure_kg;
    }

    /* 中间区间：找到所在的两点，做一次线性插值 */
    for (uint32_t i = 0U; i < (point_num - 1U); i++)
    {
        float r_high = g_pressure_curve[i].resistance_kohm;
        float r_low  = g_pressure_curve[i + 1U].resistance_kohm;

        if ((r_sensor_kohm <= r_high) && (r_sensor_kohm >= r_low))
        {
            float p_high = g_pressure_curve[i].pressure_kg;
            float p_low  = g_pressure_curve[i + 1U].pressure_kg;

            float t = (r_high - r_sensor_kohm) / (r_high - r_low);
            return p_high + t * (p_low - p_high);
        }
    }

    /* 兜底（理论上不会到达这里） */
    return 0.0f;
}

fsp_err_t pressure_sensor_read(uint16_t * p_adc_raw, float * p_pressure_kg)
{
    if ((NULL == p_adc_raw) || (NULL == p_pressure_kg))
    {
        return FSP_ERR_ASSERTION;
    }

    fsp_err_t err = FSP_SUCCESS;

    /* 触发一次扫描（单次扫描模式，FSP 中配置为 Scan mode） */
    err = g_adc0.p_api->scanStart(g_adc0.p_ctrl);
    if (FSP_SUCCESS != err)
    {
        return err;
    }

    /* 等待本次扫描完成（也可以用回调方式） */
    adc_status_t status = {0};
    do
    {
        err = g_adc0.p_api->scanStatusGet(g_adc0.p_ctrl, &status);
        if (FSP_SUCCESS != err)
        {
            return err;
        }
    } while (ADC_STATE_SCAN_IN_PROGRESS == status.state);

    /* 读取 AN000 通道（保留原来的单通道读函数，用于向下兼容） */
    err = g_adc0.p_api->read(g_adc0.p_ctrl, ADC_CHANNEL_0, &g_adc_raw[0]);
    if (FSP_SUCCESS != err)
    {
        return err;
    }

    g_pressure[0] = pressure_sensor_convert_to_kg(g_adc_raw[0]);

    *p_adc_raw     = g_adc_raw[0];
    *p_pressure_kg = g_pressure[0];

    return FSP_SUCCESS;
}

fsp_err_t pressure_sensor_read_all(uint16_t adc_raw[PRESSURE_SENSOR_NUM],
                                   float pressure_kg[PRESSURE_SENSOR_NUM])
{
    if ((NULL == adc_raw) || (NULL == pressure_kg))
    {
        return FSP_ERR_ASSERTION;
    }

    fsp_err_t err = FSP_SUCCESS;

    /* 启动一次多通道扫描 */
    err = g_adc0.p_api->scanStart(g_adc0.p_ctrl);
    if (FSP_SUCCESS != err)
    {
        return err;
    }

    adc_status_t status = {0};
    do
    {
        err = g_adc0.p_api->scanStatusGet(g_adc0.p_ctrl, &status);
        if (FSP_SUCCESS != err)
        {
            return err;
        }
    } while (ADC_STATE_SCAN_IN_PROGRESS == status.state);

    /* 读取 3 个通道：AN000 / AN001 / AN002 */
    const adc_channel_t channel_lut[PRESSURE_SENSOR_NUM] =
    {
        ADC_CHANNEL_0,    /* PS0_AD → AN000 */
        ADC_CHANNEL_1,    /* PS1_AD → AN001 */
        ADC_CHANNEL_2,    /* PS2_AD → AN002 */
    };

    for (uint8_t i = 0U; i < PRESSURE_SENSOR_NUM; i++)
    {
        err = g_adc0.p_api->read(g_adc0.p_ctrl, channel_lut[i], &g_adc_raw[i]);
        if (FSP_SUCCESS != err)
        {
            return err;
        }

        g_pressure[i] = pressure_sensor_convert_to_kg(g_adc_raw[i]);

        adc_raw[i]     = g_adc_raw[i];
        pressure_kg[i] = g_pressure[i];
    }

    return FSP_SUCCESS;
}

fsp_err_t pressure_sensor_read_one(uint8_t index,
                                   uint16_t * p_adc_raw,
                                   float * p_pressure_kg)
{
    if ((index >= PRESSURE_SENSOR_NUM) || (NULL == p_adc_raw) || (NULL == p_pressure_kg))
    {
        return FSP_ERR_ASSERTION;
    }

    uint16_t adc_buf[PRESSURE_SENSOR_NUM]   = {0};
    float    pressure_buf[PRESSURE_SENSOR_NUM] = {0.0f};

    fsp_err_t err = pressure_sensor_read_all(adc_buf, pressure_buf);
    if (FSP_SUCCESS != err)
    {
        return err;
    }

    *p_adc_raw     = adc_buf[index];
    *p_pressure_kg = pressure_buf[index];

    return FSP_SUCCESS;
}

