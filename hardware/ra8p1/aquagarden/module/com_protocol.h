#ifndef COM_PROTOCOL_H_
#define COM_PROTOCOL_H_

#include <stdbool.h>
#include <stdint.h>
#include "aquagarden_app.h"

#define COM_DOWN_SYNC0          (0x5AU)
#define COM_DOWN_SYNC1          (0xA5U)
#define COM_DOWN_MAX_PAYLOAD    (8U)
#define COM_UP_SYNC0            (0x55U)
#define COM_UP_SYNC1            (0xAAU)
#define COM_UP_VERSION          (0x02U)
#define COM_UP_PAYLOAD_LEN      (30U)
#define COM_UP_FRAME_LEN        (38U)

typedef enum e_com_command
{
    COM_CMD_SET_MANUAL_PUMP = 0x01,
    COM_CMD_SOIL_CONFIG     = 0x02,
    COM_CMD_LINKAGE_CONFIG  = 0x03,
    COM_CMD_PUMP_START      = 0x04,
    COM_CMD_PUMP_STOP       = 0x05,
    COM_CMD_SET_PUMP_PWM    = 0x06,
    COM_CMD_SET_PUMP_AUTO   = 0x07,
    COM_CMD_USB_LIGHT_MODE  = 0x08,
    COM_CMD_ATOMIZER_SET    = 0x09,
} com_command_t;

typedef struct st_com_downlink_frame
{
    uint8_t cmd;
    uint8_t len;
    uint8_t payload[COM_DOWN_MAX_PAYLOAD];
} com_downlink_frame_t;

uint16_t com_crc16_modbus(const uint8_t * p_data, uint16_t len);
bool com_downlink_decode(const uint8_t * p_frame, uint8_t frame_len, com_downlink_frame_t * p_out);
void com_apply_downlink(const com_downlink_frame_t * p_cmd);
void com_build_uplink(uint8_t seq, const aqua_snapshot_t * p_snapshot, uint8_t out_frame[COM_UP_FRAME_LEN]);

#endif /* COM_PROTOCOL_H_ */
