/*
 雾化器模块
 https://sourl.cn/eVK3GC
*/
int LED = 3;  //定义LED引脚
void setup() {
  pinMode(LED, OUTPUT);//设置引脚为输出
}

void loop() {
  //开启雾化器1000ms             
  digitalWrite(LED, HIGH);   
  delay(1000);     
         
  //关闭雾化器1000ms          
  digitalWrite(LED, LOW);   
  delay(1000);   
}
