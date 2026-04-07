#include <Wire.h>

#define SHT30_ADDRESS 0x44 // SHT30的I2C地址（0x44或0x45，取决于ADDR引脚）

void setup() {
  Serial.begin(9600);
  Wire.begin(); // 初始化I2C
  delay(100);   // 等待传感器稳定

  // 发送软复位命令（可选）
  Wire.beginTransmission(SHT30_ADDRESS);
  Wire.write(0x30);
  Wire.write(0xA2);
  Wire.endTransmission();
  delay(10);
  
  Serial.println("SHT30 温湿度测试");
}

void loop() {
  float temperature, humidity;
  
  if (readSHT30(&temperature, &humidity)) {
    Serial.print("温度: ");
    Serial.print(temperature, 1); // 保留1位小数
    Serial.print(" ℃");
    Serial.print("  |  湿度: ");
    Serial.print(humidity, 1);
    Serial.println(" %");
  } else {
    Serial.println("读取失败！检查传感器连接。");
  }

  delay(1000); // 1秒读取一次
}

// 从SHT30读取温湿度数据
bool readSHT30(float *temp, float *humi) {
  // 发送高精度测量命令（0x2C + 0x06）
  Wire.beginTransmission(SHT30_ADDRESS);
  Wire.write(0x2C);
  Wire.write(0x06);
  if (Wire.endTransmission() != 0) {
    return false; // I2C通信失败
  }

  delay(15); // 等待测量完成（高精度模式需15ms）

  // 读取6字节数据（温度+湿度+CRC）
  Wire.requestFrom(SHT30_ADDRESS, 6);
  if (Wire.available() != 6) {
    return false; // 数据长度错误
  }

  uint8_t data[6];
  for (int i = 0; i < 6; i++) {
    data[i] = Wire.read();
  }

  // 解析温度（16位值，转换为℃）
  uint16_t rawTemp = (data[0] << 8) | data[1];
  *temp = -45 + 175 * (rawTemp / 65535.0);

  // 解析湿度（16位值，转换为%）
  uint16_t rawHumi = (data[3] << 8) | data[4];
  *humi = 100 * (rawHumi / 65535.0);

  return true;
}