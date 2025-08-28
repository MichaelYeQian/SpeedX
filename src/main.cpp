// ===== RX (Nano): nRF24 -> 控制 ESC & Servo =====
#include <Arduino.h>
#include <SPI.h>
#include <nRF24L01.h>
#include <RF24.h>
#include <Servo.h>

#define CE_PIN   9
#define CSN_PIN 10
// SPI: SCK=13, MISO=12, MOSI=11

RF24 radio(CE_PIN, CSN_PIN);
const byte txAddr[6] = "CTRL1";
const byte rxAddr[6] = "BASE1";

// 避免与库内宏 RF_CH 冲突，使用 RF_CHANNEL
static const uint8_t RF_CHANNEL = 76;

struct __attribute__((packed)) JoyPacket { uint8_t x, y, seq; };

// 执行器引脚/参数
#define ESC_PIN       3
#define SERVO_PIN     5

static const int THROTTLE_MIN_US  = 1000;
static const int THROTTLE_MAX_US  = 2000;   // 可改 1900
static const int THROTTLE_SAFE_US = 1000;   // 掉线保护

static const int SERVO_MIN_DEG    = 20;
static const int SERVO_MAX_DEG    = 160;
static const int SERVO_CENTER_DEG = (SERVO_MIN_DEG + SERVO_MAX_DEG)/2;

static const int RAMP_STEP_US     = 4;      // ESC 斜率限制
static const int LOOP_DELAY_MS    = 10;     // ~100Hz

static const uint16_t LINK_TIMEOUT_MS     = 800;
static const uint16_t REINIT_COOLDOWN_MS  = 1500;

Servo esc, srv;

unsigned long lastRx=0, lastReinit=0;
bool linkUp=false;
uint8_t lastSeq=0; bool seqInited=false; uint32_t lost=0;

int throttleTarget = THROTTLE_SAFE_US;
int throttleNow    = THROTTLE_SAFE_US;
int servoTargetDeg = SERVO_CENTER_DEG;
int servoNowDeg    = SERVO_CENTER_DEG;

static inline int clamp(int v, int lo, int hi){ return v<lo?lo:(v>hi?hi:v); }
static inline int mapByteToUs(uint8_t b, int usMin, int usMax){
  long out = map((long)b, 1, 255, usMin, usMax);
  return clamp((int)out, usMin, usMax);
}
static inline int mapByteToDeg(uint8_t b, int degMin, int degMax){
  long out = map((long)b, 1, 255, degMin, degMax);
  return clamp((int)out, degMin, degMax);
}
static inline int approach(int current, int target, int step){
  if(current < target) return (current+step>target)?target:(current+step);
  if(current > target) return (current-step<target)?target:(current-step);
  return current;
}

void radioReinitRX(){
  radio.setPALevel(RF24_PA_MIN);
  radio.setDataRate(RF24_250KBPS);
  radio.setChannel(RF_CHANNEL);
  radio.setRetries(15, 15);
  radio.setCRCLength(RF24_CRC_16);
  radio.setAutoAck(true);
  radio.disableDynamicPayloads();
  radio.setPayloadSize(sizeof(JoyPacket));
  radio.openReadingPipe(1, txAddr);
  radio.openWritingPipe(rxAddr);
  radio.startListening();
  radio.flush_rx();
  radio.flush_tx();
}

void setup(){
  Serial.begin(115200);

  esc.attach(ESC_PIN, THROTTLE_MIN_US, 2000);
  srv.attach(SERVO_PIN);
  esc.writeMicroseconds(THROTTLE_SAFE_US);
  srv.write(servoNowDeg);

  Serial.println(F("⚙️ ESC 上电：安全油门，等待自检…"));
  delay(3000);
  Serial.println(F("✅ 自检完成，等待无线数据控制"));

  if(!radio.begin()){ Serial.println(F("❌ NRF24 init fail")); while(1){delay(1000);} }
  radioReinitRX();
  lastRx = millis();

  Serial.println(F("RX ready: X->ESC, Y->Servo"));
}

void loop(){
  // 收包
  while (radio.available()){
    JoyPacket pkt; radio.read(&pkt, sizeof(pkt));
    lastRx = millis();
    if(!linkUp){ linkUp=true; Serial.println(F("✅ 已连接，开始接收控制")); }

    // 丢包统计
    if(!seqInited) seqInited=true;
    else { int d=(int)pkt.seq-(int)lastSeq; if(d<=0) d+=255; if(d>1) lost+=(uint32_t)(d-1); }
    lastSeq = pkt.seq;

    // X -> ESC, Y -> Servo
    throttleTarget = mapByteToUs(pkt.x, THROTTLE_MIN_US, THROTTLE_MAX_US);
    servoTargetDeg = mapByteToDeg(pkt.y, SERVO_MIN_DEG, SERVO_MAX_DEG);

    static unsigned long lastPrint=0;
    if (millis()-lastPrint > 120){
      lastPrint = millis();
      Serial.print(F("RX x=")); Serial.print(pkt.x);
      Serial.print(F(" y="));   Serial.print(pkt.y);
      Serial.print(F(" seq=")); Serial.print(pkt.seq);
      Serial.print(F(" | thrT=")); Serial.print(throttleTarget);
      Serial.print(F("us srvT=")); Serial.print(servoTargetDeg);
      Serial.print(F("deg | lost=")); Serial.println(lost);
    }
  }

  // 失联保护 + 自愈节流
  unsigned long now = millis();
  if (now - lastRx > LINK_TIMEOUT_MS){
    if (linkUp){ linkUp=false; Serial.println(F("⛔ 掉线：安全输出(最小油门+居中)")); }
    throttleTarget = THROTTLE_SAFE_US;
    servoTargetDeg = SERVO_CENTER_DEG;

    if (now - lastReinit > REINIT_COOLDOWN_MS){
      radio.powerDown(); delay(5); radio.powerUp(); delay(5);
      radioReinitRX();
      lastReinit = now;
    }
  }

  // 同步输出（非阻塞）
  throttleNow = approach(throttleNow, throttleTarget, RAMP_STEP_US);
  esc.writeMicroseconds(throttleNow);

  servoNowDeg = approach(servoNowDeg, servoTargetDeg, 2);
  srv.write(servoNowDeg);

  delay(LOOP_DELAY_MS);
}
