#include <SPI.h>
#include <nRF24L01.h>
#include <RF24.h>

#define CE_PIN 4
#define CSN_PIN 5

RF24 radio(CE_PIN, CSN_PIN);

const byte rxAddr[6] = "NODE1";
const byte txAddr[6] = "NODE2";

unsigned long lastReceivedTime = 0;
const unsigned long TIMEOUT = 5000; // 5秒未收到判为断联

bool isConnected = false;

void setup() {
  Serial.begin(115200);
  SPI.begin(18, 19, 23);

  if (!radio.begin()) {
    Serial.println("❌ NRF24 初始化失败，检查接线或电源！");
    while (1);
  }

  radio.setPALevel(RF24_PA_LOW);
  radio.setDataRate(RF24_1MBPS);
  radio.setChannel(76);
  radio.openReadingPipe(1, rxAddr);
  radio.openWritingPipe(txAddr);
  radio.startListening();

  Serial.println("📡 接收端启动完成，等待数据...");
  lastReceivedTime = millis();
}

void loop() {
  if (radio.available()) {
    char data[32] = "";
    radio.read(&data, sizeof(data));

    Serial.print("📥 收到数据：");
    Serial.println(data);

    lastReceivedTime = millis();

    // 连接状态首次建立时提示
    if (!isConnected) {
      Serial.println("✅ 已建立通信连接！");
      isConnected = true;
    }

    // 自动回应 handshake
    if (strcmp(data, "PING") == 0) {
      radio.stopListening();
      const char pong[] = "PONG";
      bool ok = radio.write(&pong, sizeof(pong));
      Serial.println(ok ? "🔁 回应 PONG 成功" : "⚠️ 回应失败！");
      radio.startListening();
    }
  }

  // 检测是否断联
  if (millis() - lastReceivedTime > TIMEOUT && isConnected) {
    Serial.println("⛔ 对方已断开或掉电！");
    isConnected = false;
  }
}