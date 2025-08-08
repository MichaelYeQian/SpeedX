#include <SPI.h>
#include <nRF24L01.h>
#include <RF24.h>

#define CE_PIN 4
#define CSN_PIN 5

RF24 radio(CE_PIN, CSN_PIN);
const byte rxAddr[6] = "CTRL1"; // 接收遥控器的数据
const byte txAddr[6] = "BASE1"; // 回复确认信号

unsigned long lastReceive = 0;
const unsigned long TIMEOUT = 3000; // 超过3秒无数据就认为断联
bool isConnected = false;

struct ControlPacket {
  int16_t throttle;  // 前进后退（-512~512）
  int16_t steering;  // 左右控制（-512~512）
  uint8_t flags;     // 扩展功能位，例如灯光、音效等
};

void setup() {
  Serial.begin(115200);
  SPI.begin(18, 19, 23);

  if (!radio.begin()) {
    Serial.println("NRF24 初始化失败");
    while (1);
  }

  radio.setPALevel(RF24_PA_LOW);
  radio.setDataRate(RF24_1MBPS);
  radio.setChannel(90);
  radio.openReadingPipe(1, rxAddr);
  radio.openWritingPipe(txAddr);
  radio.startListening();

  Serial.println("🚗 接收端启动完成，等待指令...");
}

void loop() {
  if (radio.available()) {
    ControlPacket packet;
    radio.read(&packet, sizeof(packet));

    lastReceive = millis();
    if (!isConnected) {
      Serial.println("✅ 控制信号已连接！");
      isConnected = true;
    }

    // 显示指令内容
    Serial.print("🚀 油门: ");
    Serial.print(packet.throttle);
    Serial.print(" | 转向: ");
    Serial.print(packet.steering);
    Serial.print(" | 标志位: ");
    Serial.println(packet.flags);

    // 可以在这里用 packet.throttle / steering 去控制电机

    // 回复确认
    radio.stopListening();
    const char ack[] = "ACK";
    radio.write(&ack, sizeof(ack));
    radio.startListening();
  }

  // 如果超时未收到
  if (millis() - lastReceive > TIMEOUT && isConnected) {
    Serial.println("❌ 控制信号丢失！");
    isConnected = false;
    // 可以在这里紧急停止电机
  }
}
