#include <Servo.h>

// Pin Definitions
const int PIN_S0 = 4;
const int PIN_S1 = 5;
const int PIN_S2 = 6;
const int PIN_S3 = 7;
const int PIN_SENSOR_OUT = 8;
const int PIN_RLED = 9;   // 红色LED
const int PIN_GLED = 10;  // 绿色LED
const int PIN_BLED = 11;  // 蓝色LED
const int PIN_SERVO = 3;

Servo motor;
int redValue, greenValue, blueValue = 0;

void setup() {
  initializePins();
  motor.attach(PIN_SERVO, 500, 2500);
  setSensorFrequencyScale();
  Serial.begin(9600);
}

void loop() {
  redValue = readColor(PIN_S2, LOW, PIN_S3, LOW);
  greenValue = readColor(PIN_S2, HIGH, PIN_S3, HIGH);
  blueValue = readColor(PIN_S2, LOW, PIN_S3, HIGH);
  displayColors();
  performActionsBasedOnColor();
}

void initializePins() {
  pinMode(PIN_S0, OUTPUT);
  pinMode(PIN_S1, OUTPUT);
  pinMode(PIN_S2, OUTPUT);
  pinMode(PIN_S3, OUTPUT);
  pinMode(PIN_SENSOR_OUT, INPUT);
  pinMode(PIN_RLED, OUTPUT);
  pinMode(PIN_GLED, OUTPUT);
  pinMode(PIN_BLED, OUTPUT);
}

void setSensorFrequencyScale() {
  digitalWrite(PIN_S0, HIGH);
  digitalWrite(PIN_S1, LOW);
}

int readColor(int pin1, int state1, int pin2, int state2) {
  digitalWrite(pin1, state1);
  digitalWrite(pin2, state2);
  int rawValue = pulseIn(PIN_SENSOR_OUT, LOW);
  int mappedValue = map(rawValue, 50, 130, 0, 255); // 调整映射范围
  delay(100);
  return mappedValue;
}

void displayColors() {
  Serial.print("R= ");
  Serial.print(redValue);
  Serial.print(" G= ");
  Serial.print(greenValue);
  Serial.print(" B= ");
  Serial.println(blueValue);
}

void performActionsBasedOnColor() {
  resetLEDs();
  if (isColorRed(redValue, greenValue, blueValue)) {
    digitalWrite(PIN_RLED, HIGH);  // 红色
    motor.write(22.5);
  } else if (isColorGreen(redValue, greenValue, blueValue)) {
    digitalWrite(PIN_GLED, HIGH);  // 绿色
    motor.write(112.5);
  } else if (isColorBlue(redValue, greenValue, blueValue)) {
    digitalWrite(PIN_BLED, HIGH);  // 蓝色
    motor.write(67.5);
  } else if (isColorYellow(redValue, greenValue, blueValue)) {
    // 如果检测到黄色，同时点亮红色和绿色LED
    digitalWrite(PIN_RLED, HIGH);  // 红色
    digitalWrite(PIN_GLED, HIGH);  // 绿色
    motor.write(157.5);
  }else if ((redValue < 0 || redValue > 255) && (greenValue < 0 || greenValue > 255) && (blueValue < 0 || blueValue > 255)) {
    // 如果所有RGB通道的值都小于0或者大于255，判定为没有检测到颜色
    // 执行相应的操作，例如关闭LED灯或将舵机归位
    digitalWrite(PIN_RLED, LOW);
    digitalWrite(PIN_GLED, LOW);
    // motor.write(90); // 将舵机归位
  }
}

void resetLEDs() {
  digitalWrite(PIN_RLED, LOW);
  digitalWrite(PIN_GLED, LOW);
  digitalWrite(PIN_BLED, LOW);
}

bool isColorRed(int red, int green, int blue) {
  // 判断是否为红色，如果红色通道低于100，就判定为红色
  return red < 100 && green > 100;
}

bool isColorGreen(int red, int green, int blue) {
  // 判断是否为绿色，如果绿色通道低于100，就判定为绿色
  return green < 100 && blue > 100;
}

bool isColorBlue(int red, int green, int blue) {
  // 判断是否为蓝色，如果蓝色通道低于100，就判定为蓝色
  return blue < 100 && red > 100;
}

bool isColorYellow(int red, int green, int blue) {
  // 判断是否为黄色，如果红色和绿色通道都低于100，就判定为黄色
  return red < 0;
}
