// #include <SPI.h>
// #include <nRF24L01.h>
// #include <RF24.h>

// #define CE_PIN 4
// #define CSN_PIN 5
// #define SCK_PIN 18
// #define MISO_PIN 19
// #define MOSI_PIN 23

// // 与发送端一致（核心两个参数）
// static const uint16_t TIMEOUT_MS       = 150;  // 150ms 无数据 ⇒ Fail-safe
// static const uint16_t CONTROL_DEADBAND = 5;    // 可选：死区平滑控制（按需用）

// RF24 radio(CE_PIN, CSN_PIN);

// const byte rxAddr[6] = "CTRL1"; // 我接收的地址（发送端 openWritingPipe("CTRL1")）
// const byte txAddr[6] = "BASE1"; // 仅保留对称（此方案不回payload）

// struct ControlPacket {
//   int16_t throttle;
//   int16_t steering;
//   uint8_t flags;
// } __attribute__((packed));

// unsigned long lastReceive = 0;
// bool isConnected = false;

// // ====== 你的电机/执行器接口（按需实现） ======
// void motorStopAll() {
//   // TODO: 立即停车/断PWM/刹车 —— Fail-safe 核心
//   // analogWrite(pinL, 0); analogWrite(pinR, 0); 或发给电调零油门
// }
// void motorApply(int16_t throttle, int16_t steering) {
//   // TODO: 混控/差速：把 throttle/steering 映射为左右轮PWM
//   // 示例：左 = throttle + steering；右 = throttle - steering
//   // 并做好限幅 [-512,512]→[0..PWM_MAX]
// }

// void setup() {
//   Serial.begin(115200);
//   SPI.begin(SCK_PIN, MISO_PIN, MOSI_PIN);

//   if (!radio.begin()) {
//     Serial.println("❌ NRF24 初始化失败");
//     while (1) {}
//   }

//   radio.setPALevel(RF24_PA_LOW);
//   radio.setDataRate(RF24_1MBPS);
//   radio.setChannel(90);
//   radio.setRetries(5, 15);
//   radio.setCRCLength(RF24_CRC_16);
//   radio.enableDynamicPayloads();
//   radio.setAutoAck(true);            // **关键：开启硬件ACK**

//   radio.openReadingPipe(1, rxAddr);
//   radio.openWritingPipe(txAddr);     // 保留；若未来用 ACK payload 可直接启用
//   radio.startListening();

//   lastReceive = millis();
//   motorStopAll();                    // 上电默认安全
//   Serial.println("🚗 接收端就绪（Fail-safe ≤150ms）");
// }

// void loop() {
//   // —— 接收并执行 —— 
//   while (radio.available()) {
//     ControlPacket pkt{};
//     radio.read(&pkt, sizeof(pkt));

//     lastReceive = millis();
//     if (!isConnected) {
//       isConnected = true;
//       Serial.println("✅ 控制信号已连接（收到新帧）");
//     }

//     // 可选：简单死区平滑
//     if (abs(pkt.throttle) <= CONTROL_DEADBAND) pkt.throttle = 0;
//     if (abs(pkt.steering) <= CONTROL_DEADBAND) pkt.steering = 0;

//     // 执行动作（你实现 motorApply）
//     motorApply(pkt.throttle, pkt.steering);

//     // 串口调试
//     Serial.print("🕹 throttle="); Serial.print(pkt.throttle);
//     Serial.print(" steering=");   Serial.print(pkt.steering);
//     Serial.print(" flags=");      Serial.println(pkt.flags);
//   }

//   // —— 低延迟 Fail-safe —— 
//   const unsigned long now = millis();
//   if (isConnected && (now - lastReceive > TIMEOUT_MS)) {
//     isConnected = false;
//     Serial.println("⛔ 控制信号中断（>150ms）—— 进入安全状态");
//     motorStopAll();  // 立刻停车
//   }
// }





// === Snowcat_ESC_Test.ino ===
// 硬件: ESP32 + 航模ESC + 2212电机，信号脚接 GPIO25
// 供电: 电池 -> ESC，BEC(5V) 或 独立降压给 ESP32；务必共地(GND)

#include <Arduino.h>

// ---------- 可调参数 ----------
const int   ESC_PIN   = 25;   // 你的 ESC 信号脚
const int   PWM_CH    = 0;    // LEDC 通道 0~15 任意
const int   PWM_FREQ  = 50;   // 50Hz (舵机/ESC标准)
const int   PWM_RES   = 16;   // 16位分辨率(0..65535)
const int   PULSE_MIN = 1000; // µs
const int   PULSE_MAX = 2000; // µs
const int   PULSE_IDLE= 1000; // 解锁/最小油门
const int   PULSE_SLOW= 1250; // 慢速运行 (1200~1300 更慢/更快)
const bool  DO_THROTTLE_CALIBRATION = false; // 若首次使用且ESC需要校准, 设 true

// 软启动设置
const int   RAMP_STEP_US = 2;    // 每步增加的微秒
const int   RAMP_DELAY_MS= 8;    // 每步延时

// ---------- 工具函数 ----------
uint32_t pulseUsToDuty(int us) {
  // 50Hz -> 周期 20000us；16位满量程 65535
  // 常见ESC: 1000~2000us 对应约 5%~10%占空比
  // 直接线性映射到 [3277, 6553] 区间
  long duty = map(us, 1000, 2000, 3277, 6553);
  if (duty < 0) duty = 0;
  if (duty > 65535) duty = 65535;
  return (uint32_t)duty;
}

void escWriteUs(int us) {
  us = constrain(us, PULSE_MIN, PULSE_MAX);
  ledcWrite(PWM_CH, pulseUsToDuty(us));
}

void escArm() {
  // 最小油门解锁
  escWriteUs(PULSE_IDLE);
  delay(2000); // 多数航模ESC 需保持一段时间
}

void escRampTo(int targetUs) {
  // 从当前占空比缓慢拉到目标占空比
  // 先估计当前 duty 对应的 us（简单做法：不追踪，直接从 PULSE_IDLE 开始拉）
  int current = PULSE_IDLE;
  escWriteUs(current);
  delay(50);
  if (targetUs < current) {
    for (int us = current; us >= targetUs; us -= RAMP_STEP_US) {
      escWriteUs(us);
      delay(RAMP_DELAY_MS);
    }
  } else {
    for (int us = current; us <= targetUs; us += RAMP_STEP_US) {
      escWriteUs(us);
      delay(RAMP_DELAY_MS);
    }
  }
  escWriteUs(targetUs);
}

// ---------- 可选：油门校准流程 ----------
void throttleCalibration() {
  // 一些航模ESC需要：
  // 1) 上电后立刻给最大油门(2000us)几秒
  // 2) 再给最小油门(1000us)几秒
  // 校准步骤以你的 ESC 说明书为准！
  Serial.println(F("[校准] 输出最大油门..."));
  escWriteUs(PULSE_MAX);
  delay(3000);

  Serial.println(F("[校准] 切换最小油门..."));
  escWriteUs(PULSE_MIN);
  delay(3000);

  Serial.println(F("[校准] 完成。"));
}

// ---------- 主流程 ----------
void setup() {
  Serial.begin(115200);
  delay(200);

  // 初始化 LEDC PWM
  ledcSetup(PWM_CH, PWM_FREQ, PWM_RES);
  ledcAttachPin(ESC_PIN, PWM_CH);

  Serial.println(F("ESC 测试程序启动"));
  Serial.println(F("请确保: 电池->ESC 接好, 电机连接正确, GND 共地."));
  Serial.println(F("串口输入 's' 可急停。"));

  if (DO_THROTTLE_CALIBRATION) {
    Serial.println(F("进入油门校准模式..."));
    throttleCalibration();
  }

  Serial.println(F("ESC 解锁..."));
  escArm();

  Serial.println(F("软启动到慢速..."));
  escRampTo(PULSE_SLOW);

  Serial.println(F("正在慢速运行。"));
}

void loop() {
  // 保持慢速运行；监听串口急停
  if (Serial.available()) {
    char c = Serial.read();
    if (c == 's' || c == 'S') {
      Serial.println(F("急停: 输出最小油门"));
      escWriteUs(PULSE_IDLE);
      while (1) { delay(100); } // 停在这里，防止误触再次加油门
    }
  }
  delay(20);
}

