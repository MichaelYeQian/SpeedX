#include <Arduino.h>
#include <nRF24L01.h>
#include <RF24.h>

// 设置CE和CSN引脚
#define CE_PIN 15
#define CSN_PIN 2


RF24 radio(CE_PIN, CSN_PIN);

void setup() {
  Serial.begin(115200);

  // 初始化NRF24L01
  if (!radio.begin()) {
    Serial.println("NRF24L01初始化失败，请检查连接。");
    while (1);  // 停止执行
  }

  Serial.println("NRF24L01初始化成功！");
  radio.printDetails();

  // 配置发送参数
  radio.setPALevel(RF24_PA_LOW);
  radio.setChannel(76);
  radio.openWritingPipe(0xF0F0F0F0E1LL); // 设置发送管道

  // 检查芯片是否连接
  if (radio.isChipConnected()) {
    Serial.println("NRF24L01芯片连接正常。");
  } else {
    Serial.println("NRF24L01芯片连接异常。");
  }
}

void loop() {
  // // 可以添加循环测试，例如发送一个简单的数据包
  // const char testData[] = "Hello";
  // if (radio.write(&testData, sizeof(testData))) {
  //   Serial.println("数据发送成功！");
  // } else {
  //   Serial.println("数据发送失败！");
  // }
  
  // delay(1000);  // 每隔1秒发送一次
}