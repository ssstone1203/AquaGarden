#ifndef __ESP32_WIFI_H
#define __ESP32_WIFI_H


#include <stdio.h>
#include <string.h>
#include "stm32f10x.h"
#include "bsp_usart.h"
#include "delay.h"

//wifi模式配置 如果都关闭，默认使用MODE_AP_STA模式 Wi-Fi mode configuration If both are turned off, the default mode is MODE_AP_STA.
#define MODE_AP 			0   //0关闭 1开启 0 Off 1 On
#define MODE_STA 			0		//0关闭 1开启 0 Off 1 On
#define MODE_AP_STA 	1		//0关闭 1开启 0 Off 1 On



typedef struct  ESP32_AI_Msg_t
{
  int16_t lx; //左上角 Top left corner
  int16_t ly; //左上角 Top left corner
  int16_t rx; //右下角 Bottom right corner
  int16_t ry; //右下角 Bottom right corner
  int16_t cx; //中心点 Center Point
  int16_t cy; //中心点 Center Point
  uint16_t area; //面积 area
  int16_t id;  //人脸id Face ID
}ESP32_AI_Msg;

typedef enum AI_mode_t
{
    Nornal_AI = 0,//不检测 No detection
    Cat_Dog_AI,   //猫狗检测 Cat and Dog Detection
    FACE_AI,      //人脸检测 Face Detection
    COLOR_AI ,   //颜色检测 Color Detection
    REFACE_AI,   //人脸识别 Face Recognition
    QR_AI = 5,   //二维码识别 QR code recognition
    AI_MAX      //最大值 Maximum
}AI_mode;

typedef struct  QR_AI_Msg_t
{
  char QR_msg[50];//QRmsg的处理 QRmsg processing
}QR_AI_Msg;





void SET_ESP_WIFI_MODE(void);
void SET_STA_WIFI(void);
void SET_AP_WIFI(void);
void SET_ESP_AI_MODE(AI_mode Mode);

void Get_STAIP(void);
void Get_APIP(void);
void Get_Version(void);


void Data_Deal(uint8_t RXdata);
void IP_identify(uint8_t* data);
void recv_tcp_data(char tcpdata);
void recv_AI_data(char AIdata) ;
void recv_QR_data(char QRdata);
void recv_face_data(char facedata);
void Get_AI_msg(char *buf);
void Get_faceAI_msg(char *buf);

extern uint8_t newlines;
extern AI_mode runmode;
extern QR_AI_Msg QR_msg;
extern ESP32_AI_Msg esp32_ai_msg;




#endif

