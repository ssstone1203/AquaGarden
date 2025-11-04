#include "esp32_wifi.h"


#if MODE_AP_STA
#define WIFI_MODE '2'

#elif MODE_STA
#define WIFI_MODE '1'

#elif MODE_AP 
#define WIFI_MODE '0'

#else
#define WIFI_MODE '2'
#endif


extern AI_mode runmode = Nornal_AI;
QR_AI_Msg QR_msg;
ESP32_AI_Msg esp32_ai_msg;//二维码以外的结构体 Structures other than QR codes

#define STAIP "sta_ip"  
#define STA_WIFI_SSID "Yahboom2"  		//wifi名称 Wi-Fi Name
#define STA_WIFI_PD   "yahboom890729" //wifi密码 Wifi password

#define APIP "ap_ip" 
#define AP_WIFI_SSID "ESP32_WIFI_TEST"  		//wifi名称 Wi-Fi Name
#define AP_WIFI_PD   "" //wifi密码 -无密码 也可在双引号添加 Wifi password - no password can also be added in double quotes

uint8_t send_buf[35]={0}; //发送命令的buf Send command buf
uint8_t recv_buf[50]={0}; //接收buf Receive buf
uint8_t data_buff[50]={0}; //备份buf Backup buf

uint8_t cmd_flag = 0;//发送命令的标志 0:没发送  1:以发送  2:进入网络透传 Send command flag 0: Not sent 1: Sent 2: Enter network transparent transmission

//设置sta模式的wifi Set up wifi in sta mode
void SET_STA_WIFI(void)
{
	//发送ssid Send SSID
	sprintf((char*)send_buf,"sta_ssid:%s",STA_WIFI_SSID);
	USART2_Send_ArrayU8(send_buf,strlen((char*)send_buf));
	memset(send_buf,0,sizeof(send_buf));
	delay_ms(300);
	
	//发送pd Send pd
	sprintf((char*)send_buf,"sta_pd:%s",STA_WIFI_PD);
	USART2_Send_ArrayU8(send_buf,strlen((char*)send_buf));
	memset(send_buf,0,sizeof(send_buf));
	
	FUN_delay_s(2);//等待复位重启成功 Wait for reset to restart successfully
	
}

//设置ap模式的wifi Set up WiFi in AP mode
void SET_AP_WIFI(void)
{
	//发送ssid Send SSID
	sprintf((char*)send_buf,"ap_ssid:%s",AP_WIFI_SSID);
	USART2_Send_ArrayU8(send_buf,strlen((char*)send_buf));
	memset(send_buf,0,sizeof(send_buf));
	delay_ms(300);
	
	//发送pd Send pd
	sprintf((char*)send_buf,"ap_pd:%s",AP_WIFI_PD);
	USART2_Send_ArrayU8(send_buf,strlen((char*)send_buf));
	memset(send_buf,0,sizeof(send_buf));
	
	FUN_delay_s(2);//等待复位重启成功 Wait for reset to restart successfully
	
}

void SET_ESP_WIFI_MODE(void) //设置模式选择，需要把esp32-wifi断电复位 To set the mode, you need to power off and reset esp32-wifi
{
	//选择模式STA+AP模式共存 Select mode STA+AP mode coexistence
	sprintf((char*)send_buf,"wifi_mode:%c",WIFI_MODE);
	USART2_Send_ArrayU8(send_buf,strlen((char*)send_buf));
	memset(send_buf,0,sizeof(send_buf));
	
	FUN_delay_s(2);//等待复位重启成功 Wait for reset to restart successfully
}

void SET_ESP_AI_MODE(AI_mode Mode) //设置AI模式 Setting AI Mode
{
//  printf("2------------");
	sprintf((char*)send_buf,"ai_mode:%d",Mode);
  USART2_Send_ArrayU8(send_buf,strlen((char*)send_buf));
  memset(send_buf,0,sizeof(send_buf));
  runmode = Mode;
//	printf("2--");
  FUN_delay_s(2);//等待复位重启成功 Wait for reset to restart successfully
}



//查询sta模式的ip Query the ip of sta mode
void Get_STAIP(void)
{
	sprintf((char*)send_buf,STAIP);
	USART2_Send_ArrayU8(send_buf,strlen((char*)send_buf));
	memset(send_buf,0,sizeof(send_buf));
	cmd_flag = 1;
}

//查询ap模式的ip Query the ip of AP mode
void Get_APIP(void)
{
	sprintf((char*)send_buf,APIP);
	USART2_Send_ArrayU8(send_buf,strlen((char*)send_buf));
	memset(send_buf,0,sizeof(send_buf));
	cmd_flag = 1;
}

void Get_Version(void)
{
	//查询固件版本号 Query the firmware version number
	sprintf((char*)send_buf,"wifi_ver");
	USART2_Send_ArrayU8(send_buf,strlen((char*)send_buf));
	memset(send_buf,0,sizeof(send_buf));
	cmd_flag = 1;
}


//-------------------------------------------------接收数据处理 Receive data processing------------------------------------------////


uint8_t end_falg = 0;
uint8_t i_index= 0; //数组索引 Array Indexing
int i = 0;

//处理串口接收到的信息 Process the information received by the serial port
void Data_Deal(uint8_t RXdata)
{
	//查询wifi的ip地址情况 Query the wifi ip address
//	 printf("%d",cmd_flag);
	if(cmd_flag == 1)
	{
		recv_buf[i_index] = RXdata;
		
		//当接收到换行符,一包的数据接收完成 When a newline character is received, a packet of data is received.
		if(RXdata == 0x0D)
		{
			end_falg = 1;
		}
		
		if(end_falg == 1 && RXdata == 0x0A)
		{		
				cmd_flag = 0;
				end_falg = 0;
				memcpy(data_buff,recv_buf,i_index);
				memset(recv_buf,0,sizeof(recv_buf));//接收数据缓存清0 Receive data buffer cleared
				i_index = 0;//索引清0 Index cleared to 0
		}
		else
			i_index ++;
		
	}
	
	//网络透传的数据 Data transmitted over the network
	else if(cmd_flag == 2 )
	{
		recv_tcp_data(RXdata);
	}
	else if(cmd_flag == 3)
  {
    recv_face_data(RXdata);//人脸识别数据解析 Face recognition data analysis
  }
  else if(cmd_flag == 4)
  {
    recv_QR_data(RXdata);//二维码数据解析 QR code data analysis
  }
  else
  {
    recv_AI_data(RXdata);//AI模式解析 AI Mode Analysis
//		 printf("%c",RXdata);
  }
}




//判断数据是STA的ip还是AP的ip Determine whether the data is the STA's IP or the AP's IP
void IP_identify(uint8_t* data)
{
	if(memcmp(data,"sta_ip",6)==0)
	{
		printf("%s",data);
	}
	else if(memcmp(data,"ap_ip",5)==0)
	{
		printf("%s",data);
	}
	else if(memcmp(data,"YAHBOOM VerSion",15)==0)
	{
		printf("%s",data); //版本号 Version Number
	}
	
	
	memset(data_buff,0,sizeof(data_buff)); 
}

uint8_t g_new_flag = 0;
uint8_t g_index = 0;
uint8_t newlines = 0; //1:接收新的数据 0:没合适数据 1: Receive new data 0: No suitable data

//接收app数据透传的数据  Receive data transmitted by app data
void recv_tcp_data(char tcpdata)
{
  // ESPWIFISerial.print(tcpdata);
  if (tcpdata == '$' && g_new_flag == 0)
  {
    g_new_flag = 1;
    memset(recv_buf, 0, sizeof(recv_buf)); // 清除旧数据 Clear old data
    return;
  }
  if(g_new_flag == 1)
  {
    if (tcpdata == '#')
    {
      g_new_flag = 0;
      g_index = 0;
      //newlines = 1; //新数据接收完毕 New data received
      memcpy(data_buff,recv_buf,sizeof(recv_buf));
      //处理
      memset(recv_buf, 0, sizeof(recv_buf)); // 清除旧数据 Clear old data
    }
    
    else if (tcpdata == '$')//中途出现丢包 Packet loss occurs midway
    {
      g_index = 0;
      g_new_flag = 0;
      memset(recv_buf, 0, sizeof(recv_buf)); //清除旧数据 Clear old data
    }

    else 
    {
      recv_buf[g_index++] = tcpdata;
    }
    
    if(g_index > 50) //数组溢出  Array Overflow
    {
      g_index = 0;
      g_new_flag = 0;
      memset(recv_buf, 0, sizeof(recv_buf)); //清除旧数据 Clear old data
    }
    
  }
}


//检测和颜色识别的协议处理 Protocol processing for detection and color recognition
void recv_AI_data(char AIdata) //协议基本是$xxx,yyy,zzz,hhh,#
{
//  printf("%c",AIdata);
	
  if (AIdata == '$' && g_new_flag == 0)
  {
    g_new_flag = 1;
    memset(recv_buf, 0, sizeof(recv_buf)); // 清除旧数据 Clear old data
    // ESPWIFISerial.print(g_new_flag);
    return;
  }
  
  if(g_new_flag == 1)
  {
    if (AIdata == '#')
    {
      // ESPWIFISerial.print(cmd_flag);
      g_new_flag = 0;
      g_index = 0;
      memcpy(data_buff,recv_buf,sizeof(recv_buf));
      //处理
//      printf("x: %d\n",data_buff);
      Get_AI_msg(data_buff);
      newlines = 1; //新数据接收完毕 New data received
      memset(recv_buf, 0, sizeof(recv_buf)); // 清除旧数据 Clear old data
    }
    
    else if (AIdata == '$')//中途出现丢包 Packet loss occurs midway
    {
      g_index = 0;
      g_new_flag = 0;
      memset(recv_buf, 0, sizeof(recv_buf)); //清除旧数据 Clear old data
    }

    else 
    {
      recv_buf[g_index++] = AIdata;
    }
    
    if(g_index > 50) //数组溢出  Array Overflow
    { 
      g_index = 0;
      g_new_flag = 0;
      memset(recv_buf, 0, sizeof(recv_buf)); //清除旧数据 Clear old data
    }
    
  }
}

//解析检测AI数据  Analyzing and detecting AI data
void Get_AI_msg(char *buf)
{
  char databuf[3] ={"\0"};
  uint8_t len = 0;
  
  if(strlen(buf)!=16) //当长度不满足协议 When the length does not meet the protocol
  {
    //ESPWIFISerial.println(strlen(buf));
    return ;  
  }
   if(buf[3]!=',' && buf[7]!=','&& buf[11]!=','&& buf[15]!=',')//不是逗号 Not a comma
  {
      return;
  }
   
  for(uint8_t i =0;i<16;i++)
  {
    if(buf[i] == ',')
    len ++;  
  }
  if(len != 4) //逗号数量不为于4 The number of commas is not more than 4
  {
    return; 
  }

  esp32_ai_msg.lx = (buf[0] - 48)*100 + (buf[1] - 48)*10 +(buf[2] - 48);

	esp32_ai_msg.ly = (buf[4] - 48)*100 + (buf[5] - 48)*10 +(buf[6] - 48);

	esp32_ai_msg.rx  = (buf[8] - 48)*100 + (buf[9] - 48)*10 +(buf[10] - 48);

	esp32_ai_msg.ry = (buf[12] - 48)*100 + (buf[13] - 48)*10 +(buf[14] - 48);

//	printf("x: %d\n",esp32_ai_msg.lx);
//	printf("y: %d\n",esp32_ai_msg.ly );
//	printf("x: %d\n",esp32_ai_msg.rx);
//	printf("y: %d\n",esp32_ai_msg.ry);
  memset(buf,0,sizeof(buf));

  if((esp32_ai_msg.lx > esp32_ai_msg.rx)||(esp32_ai_msg.ly > esp32_ai_msg.ry))//当坐标非法
  {
      return;
  }

  //中心点x,y Center point x,y
  esp32_ai_msg.cx = (esp32_ai_msg.rx-esp32_ai_msg.lx)/2+esp32_ai_msg.lx;
  esp32_ai_msg.cy = (esp32_ai_msg.ry-esp32_ai_msg.ly)/2+esp32_ai_msg.ly;

  //面积缩10倍吧  因为ardniuo是个16位的东西,这个65535是上限  The area is reduced by 10 times. Because ardniuo is a 16-bit thing, 65535 is the upper limit.
  esp32_ai_msg.area = (esp32_ai_msg.rx-esp32_ai_msg.lx)/10*(esp32_ai_msg.ry-esp32_ai_msg.ly);
  
	printf("x: %d\n",esp32_ai_msg.cx);
	printf("y: %d\n",esp32_ai_msg.cy);
	printf("area: %d\n",esp32_ai_msg.area);
	
}


//-----------------------------------------------------------------------------------------------//
//二维码协议 QR Code Protocol
void recv_QR_data(char QRdata) //协议基本是$二维码数据# The protocol is basically $QR code data#
{
  
  if (QRdata == '$' && g_new_flag == 0)
  {
    g_new_flag = 1;
    memset(recv_buf, 0, sizeof(recv_buf)); // 清除旧数据 Clear old data
    return;
  }
  if(g_new_flag == 1)
  {
    
    if (QRdata == '#')
    {
      
      g_new_flag = 0;
      g_index = 0;
      newlines = 1; //新数据接收完毕 New data received
      memcpy(data_buff,recv_buf,sizeof(recv_buf));
			printf("%s\n",data_buff);
			
      memcpy(QR_msg.QR_msg,data_buff,sizeof(QR_msg.QR_msg));//处理赋值到有效数据 Processing assignment to valid data

      memset(recv_buf, 0, sizeof(recv_buf)); // 清除旧数据 Clear old data
      memset(data_buff, 0, sizeof(data_buff)); // 清除旧数据 Clear old data
      
    }
    
    else if (QRdata == '$')//中途出现丢包  Packet loss occurs midway
    {
      g_index = 0;
      g_new_flag = 0;
      memset(recv_buf, 0, sizeof(recv_buf)); //清除旧数据 Clear old data
    }

    else 
    {
      recv_buf[g_index++] = QRdata;
    }

    if(g_index > 50) //数组溢出  Array Overflow
    { 
      g_index = 0;
      g_new_flag = 0;
      memset(recv_buf, 0, sizeof(recv_buf)); //清除旧数据 Clear old data
    }
    
  }
}


//---------------------------------------------------------------------------------------//
//人脸识别协议 Face recognition protocol
void recv_face_data(char facedata) //协议基本是$xxx,yyy,zzz,hhh,#@ID:1!\r\n
{
  // ESPWIFISerial.print(facedata); 
  if (facedata == '$' && g_new_flag == 0)
  {
    g_new_flag = 1;
    memset(recv_buf, 0, sizeof(recv_buf)); // 清除旧数据 Clear old data
    return;
  }
  if(g_new_flag == 1)
  {
    if (facedata == '!')
    {
      g_new_flag = 0;
      g_index = 0;
      memcpy(data_buff,recv_buf,sizeof(recv_buf));
			
//      printf("%s",data_buff);
      //处理
      Get_faceAI_msg(data_buff);
      newlines = 1; //新数据接收完毕 New data received
      memset(data_buff, 0, sizeof(data_buff)); // 清除旧数据 Clear old data
      memset(recv_buf, 0, sizeof(recv_buf)); // 清除旧数据 Clear old data
    }
    
    else if (facedata == '$')//中途出现丢包 Packet loss occurs midway
    {
      g_index = 0;
      g_new_flag = 0;
      memset(recv_buf, 0, sizeof(recv_buf)); //清除旧数据 Clear old data
    }

    else 
    {
      if((facedata != '#') && (facedata != '@') )//去掉#和@号，只要有效数据  Remove the # and @ signs, only valid data
        recv_buf[g_index++] = facedata;
    }
    
    if(g_index > 50) //数组溢出  Array Overflow
    { 
      g_index = 0;
      g_new_flag = 0;
      memset(recv_buf, 0, sizeof(recv_buf)); //清除旧数据 Clear old data
    }
  }
}

//获取人脸数据  Get face data
void Get_faceAI_msg(char *buf)
{
	
  char databuf[3] ={"\0"};
  uint8_t len = 0;
  for(uint8_t i =0;i<16;i++)
  {
    if(buf[i] == ',')
    len ++;  
  }
  if(len != 4) //逗号数量不为于4 The number of commas is not more than 4
  {
    return; 
  }
  

  if(buf[3]!=',' && buf[7]!=','&& buf[11]!=','&& buf[15]!=',')//不是逗号 Not a comma
  {
      return;
  }
		
		esp32_ai_msg.lx = (buf[0] - 48)*100 + (buf[1] - 48)*10 +(buf[2] - 48);
	  esp32_ai_msg.ly = (buf[4] - 48)*100 + (buf[5] - 48)*10 +(buf[6] - 48);
	  esp32_ai_msg.rx  = (buf[8] - 48)*100 + (buf[9] - 48)*10 +(buf[10] - 48);
	  esp32_ai_msg.ry = (buf[12] - 48)*100 + (buf[13] - 48)*10 +(buf[14] - 48);

  len = strlen(buf);
  
  if((esp32_ai_msg.lx > esp32_ai_msg.rx)||(esp32_ai_msg.ly > esp32_ai_msg.ry))//当坐标非法 When the coordinates are illegal
  {
      return;
  }

  //中心点x,y
  esp32_ai_msg.cx = (esp32_ai_msg.rx-esp32_ai_msg.lx)/2+esp32_ai_msg.lx;
  esp32_ai_msg.cy = (esp32_ai_msg.ry-esp32_ai_msg.ly)/2+esp32_ai_msg.ly;
 
  //面积缩10倍吧  因为ardniuo是个16位的东西,这个65535是上限  The area is reduced by 10 times. Because ardniuo is a 16-bit thing, 65535 is the upper limit.
  esp32_ai_msg.area = (esp32_ai_msg.rx-esp32_ai_msg.lx)/10*(esp32_ai_msg.ry-esp32_ai_msg.ly);

	
	char idbuf[2]={'\0'};
  //解析id Parsing ID
  if(buf[19] == '-')//为负数 Negative
  {
      for(uint8_t i = 0;i<1;i++)
      {
          idbuf[i] = buf[20+i];
      }
      esp32_ai_msg.id =idbuf[0] - 48;
  }
  else //为正数 Is a positive number
  {
      for(uint8_t i = 0;i<1;i++)
      {
        idbuf[i] = buf[19+i];
      }
      esp32_ai_msg.id = idbuf[0] -48;
  }
    
	printf("x: %d\n",esp32_ai_msg.cx);
	printf("y: %d\n",esp32_ai_msg.cy);
	printf("id: %d\n",esp32_ai_msg.id );
	
    
}








