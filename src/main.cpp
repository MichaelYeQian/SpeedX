// Nano 仅ESC测试：上电最小油门解锁→爬到最低能转并保持
#include <Arduino.h>
#include <Servo.h>

#define ESC_PIN 3

const int ESC_US_MIN       = 1000;  // 最小油门（解锁）
const int ESC_US_MIN_SPIN  = 1100;  // 刚好能转（不转就每次+5微调）
const int ESC_ARM_TIME_MS  = 4000;  // 解锁维持时间：部分ESC需要更久
const int RAMP_STEP_US     = 2;     // 爬升步进（更小更柔）
const int RAMP_STEP_DELAY  = 4;     // 每步延时

Servo esc;
int currentUs = ESC_US_MIN;

void rampTo(int targetUs) {
  while (currentUs != targetUs) {
    currentUs += (currentUs < targetUs) ? RAMP_STEP_US : -RAMP_STEP_US;
    esc.writeMicroseconds(currentUs);
    delay(RAMP_STEP_DELAY);
  }
}

void setup() {
  // 可选：Serial.begin(115200);
  esc.attach(ESC_PIN, 1000, 2000);

  // 1) 上电即最小油门，等ESC完成启动自检
  esc.writeMicroseconds(ESC_US_MIN);
  delay(ESC_ARM_TIME_MS);

  // 2) 慢慢爬到“刚好能转”的脉宽，并保持
  rampTo(ESC_US_MIN_SPIN);
}

void loop() {
  // 不做任何变化，保持最低转
}
