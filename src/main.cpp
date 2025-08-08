#include <SPI.h>
#include <nRF24L01.h>
#include <RF24.h>

#define CE_PIN 4
#define CSN_PIN 5
#define SCK_PIN 18
#define MISO_PIN 19
#define MOSI_PIN 23

// 与发送端一致（核心两个参数）
static const uint16_t TIMEOUT_MS       = 150;  // 150ms 无数据 ⇒ Fail-safe
static const uint16_t CONTROL_DEADBAND = 5;    // 可选：死区平滑控制（按需用）

RF24 radio(CE_PIN, CSN_PIN);

const byte rxAddr[6] = "CTRL1"; // 我接收的地址（发送端 openWritingPipe("CTRL1")）
const byte txAddr[6] = "BASE1"; // 仅保留对称（此方案不回payload）

struct ControlPacket {
  int16_t throttle;
  int16_t steering;
  uint8_t flags;
} __attribute__((packed));

unsigned long lastReceive = 0;
bool isConnected = false;

// ====== 你的电机/执行器接口（按需实现） ======
void motorStopAll() {
  // TODO: 立即停车/断PWM/刹车 —— Fail-safe 核心
  // analogWrite(pinL, 0); analogWrite(pinR, 0); 或发给电调零油门
}
void motorApply(int16_t throttle, int16_t steering) {
  // TODO: 混控/差速：把 throttle/steering 映射为左右轮PWM
  // 示例：左 = throttle + steering；右 = throttle - steering
  // 并做好限幅 [-512,512]→[0..PWM_MAX]
}

void setup() {
  Serial.begin(115200);
  SPI.begin(SCK_PIN, MISO_PIN, MOSI_PIN);

  if (!radio.begin()) {
    Serial.println("❌ NRF24 初始化失败");
    while (1) {}
  }

  radio.setPALevel(RF24_PA_LOW);
  radio.setDataRate(RF24_1MBPS);
  radio.setChannel(90);
  radio.setRetries(5, 15);
  radio.setCRCLength(RF24_CRC_16);
  radio.enableDynamicPayloads();
  radio.setAutoAck(true);            // **关键：开启硬件ACK**

  radio.openReadingPipe(1, rxAddr);
  radio.openWritingPipe(txAddr);     // 保留；若未来用 ACK payload 可直接启用
  radio.startListening();

  lastReceive = millis();
  motorStopAll();                    // 上电默认安全
  Serial.println("🚗 接收端就绪（Fail-safe ≤150ms）");
}

void loop() {
  // —— 接收并执行 —— 
  while (radio.available()) {
    ControlPacket pkt{};
    radio.read(&pkt, sizeof(pkt));

    lastReceive = millis();
    if (!isConnected) {
      isConnected = true;
      Serial.println("✅ 控制信号已连接（收到新帧）");
    }

    // 可选：简单死区平滑
    if (abs(pkt.throttle) <= CONTROL_DEADBAND) pkt.throttle = 0;
    if (abs(pkt.steering) <= CONTROL_DEADBAND) pkt.steering = 0;

    // 执行动作（你实现 motorApply）
    motorApply(pkt.throttle, pkt.steering);

    // 串口调试
    Serial.print("🕹 throttle="); Serial.print(pkt.throttle);
    Serial.print(" steering=");   Serial.print(pkt.steering);
    Serial.print(" flags=");      Serial.println(pkt.flags);
  }

  // —— 低延迟 Fail-safe —— 
  const unsigned long now = millis();
  if (isConnected && (now - lastReceive > TIMEOUT_MS)) {
    isConnected = false;
    Serial.println("⛔ 控制信号中断（>150ms）—— 进入安全状态");
    motorStopAll();  // 立刻停车
  }
}
