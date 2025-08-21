// TTTTT  EEEEE  SSSS  TTTTT   CCCC   OOO   DDDD  EEEEE
//   T    E     S        T    C      O   O  D   D E    
//   T    EEEE   SSS     T    C      O   O  D   D EEEE 
//   T    E         S    T    C      O   O  D   D E    
//   T    EEEEE SSSS     T     CCCC   OOO   DDDD  EEEEE

// #include <SPI.h>
// #include <nRF24L01.h>
// #include <RF24.h>

// #define CE_PIN 4
// #define CSN_PIN 5
// #define SCK_PIN 18
// #define MISO_PIN 19
// #define MOSI_PIN 23

// // Must match transmitter settings
// static const uint16_t TIMEOUT_MS       = 150;  // Fail-safe: stop if no data for 150ms
// static const uint16_t CONTROL_DEADBAND = 5;    // Optional: deadband for smoother control

// RF24 radio(CE_PIN, CSN_PIN);

// const byte rxAddr[6] = "CTRL1"; // Address to receive data (transmitter must use openWritingPipe("CTRL1"))
// const byte txAddr[6] = "BASE1"; // Optional symmetry, not used for ACK payload in this version

// struct ControlPacket {
//   int16_t throttle;
//   int16_t steering;
//   uint8_t flags;
// } __attribute__((packed));

// unsigned long lastReceive = 0;
// bool isConnected = false;

// // ====== Your motor/actuator interface (implement as needed) ======
// void motorStopAll() {
//   // TODO: Immediate stop / cut PWM / apply brake — core of Fail-safe
//   // analogWrite(pinL, 0); analogWrite(pinR, 0); or send zero throttle to ESC
// }
// void motorApply(int16_t throttle, int16_t steering) {
//   // TODO: Mixing logic: convert throttle/steering into left/right wheel PWM
//   // Example: left = throttle + steering; right = throttle - steering
//   // Be sure to clamp values from [-512,512] to [0..PWM_MAX]
// }

// void setup() {
//   Serial.begin(115200);
//   SPI.begin(SCK_PIN, MISO_PIN, MOSI_PIN);

//   if (!radio.begin()) {
//     Serial.println("❌ NRF24 initialization failed");
//     while (1) {}
//   }

//   radio.setPALevel(RF24_PA_LOW);
//   radio.setDataRate(RF24_1MBPS);
//   radio.setChannel(90);
//   radio.setRetries(5, 15);
//   radio.setCRCLength(RF24_CRC_16);
//   radio.enableDynamicPayloads();
//   radio.setAutoAck(true);            // **Important: Enable hardware ACK**

//   radio.openReadingPipe(1, rxAddr);
//   radio.openWritingPipe(txAddr);     // Reserved for potential ACK payload usage
//   radio.startListening();

//   lastReceive = millis();
//   motorStopAll();                    // Ensure safe state on power-up
//   Serial.println("🚗 Receiver ready (Fail-safe ≤150ms)");
// }

// void loop() {
//   // —— Receive and execute —— 
//   while (radio.available()) {
//     ControlPacket pkt{};
//     radio.read(&pkt, sizeof(pkt));

//     lastReceive = millis();
//     if (!isConnected) {
//       isConnected = true;
//       Serial.println("✅ Control signal connected (frame received)");
//     }

//     // Optional: basic deadband smoothing
//     if (abs(pkt.throttle) <= CONTROL_DEADBAND) pkt.throttle = 0;
//     if (abs(pkt.steering) <= CONTROL_DEADBAND) pkt.steering = 0;

//     // Execute action (you implement motorApply)
//     motorApply(pkt.throttle, pkt.steering);

//     // Serial debug output
//     Serial.print("🕹 throttle="); Serial.print(pkt.throttle);
//     Serial.print(" steering=");   Serial.print(pkt.steering);
//     Serial.print(" flags=");      Serial.println(pkt.flags);
//   }

//   // —— Low-latency Fail-safe —— 
//   const unsigned long now = millis();
//   if (isConnected && (now - lastReceive > TIMEOUT_MS)) {
//     isConnected = false;
//     Serial.println("⛔ Control signal lost (>150ms) — entering safe state");
//     motorStopAll();  // Stop immediately
//   }
// }






//--------------------------------------------------------------------------
//--------------------------------------------------------------------------
//--------------------------------------------------------------------------

// MM   MM  OOO  TTTTT  OOO  RRRR        CCCC  OOO  DDDD  EEEEE
// MMM MMM O   O   T   O   O R   R      C     O   O D   D E    
// MM M MM O   O   T   O   O RRRR       C     O   O D   D EEEE 
// MM   MM O   O   T   O   O R  R       C     O   O D   D E    
// MM   MM  OOO    T    OOO  R   R       CCCC  OOO  DDDD  EEEEE


// === Snowcat_ESC_SerialControl.ino ===
// 硬件: ESP32 + 航模ESC + 2212电机，ESC信号脚→GPIO25
// 供电: 电池->ESC；ESP32 由 BEC(5V) 或独立降压供电；务必共地(GND)






// #include <Arduino.h>

// // ---------- 基本参数 ----------
// const int   ESC_PIN   = 25;
// const int   PWM_CH    = 0;
// const int   PWM_FREQ  = 50;   // 50Hz
// const int   PWM_RES   = 16;   // 16-bit
// const int   US_MIN    = 1000; // µs
// const int   US_MAX    = 2000; // µs
// const int   US_IDLE   = 1000; // 解锁/最小
// const int   US_SLOW   = 1250; // 默认慢速

// // 软启动
// const int   RAMP_STEP_US = 2;
// const int   RAMP_DELAY_MS= 8;

// int currentUs = US_IDLE;

// // ---------- 工具 ----------
// uint32_t usToDuty(int us){
//   long duty = map(us, 1000, 2000, 3277, 6553); // 5%~10%
//   duty = constrain(duty, 0, 65535);
//   return (uint32_t)duty;
// }
// void escWriteUs(int us){
//   currentUs = constrain(us, US_MIN, US_MAX);
//   ledcWrite(PWM_CH, usToDuty(currentUs));
// }
// void escArm(){
//   escWriteUs(US_IDLE);
//   delay(2000);
// }
// void escRampTo(int targetUs){
//   targetUs = constrain(targetUs, US_MIN, US_MAX);
//   int step = (targetUs >= currentUs) ? RAMP_STEP_US : -RAMP_STEP_US;
//   for(int us = currentUs; (step>0)? us<=targetUs : us>=targetUs; us += step){
//     escWriteUs(us);
//     delay(RAMP_DELAY_MS);
//   }
//   escWriteUs(targetUs);
// }
// void printHelp(){
//   Serial.println(F("\n命令："));
//   Serial.println(F("  p <0-100>     设定百分比油门，如: p 30"));
//   Serial.println(F("  u <1000-2000> 设定脉宽(µs)，如: u 1300"));
//   Serial.println(F("  r <µs>        软启动到目标脉宽，如: r 1400"));
//   Serial.println(F("  s             急停(最小油门)"));
//   Serial.println(F("  g             慢速运行(1250µs)"));
//   Serial.println(F("  h             帮助\n"));
//   Serial.print(F("当前输出: ")); Serial.print(currentUs); Serial.println(F(" us"));
// }

// // 解析一行命令
// void handleLine(String line){
//   line.trim();
//   if(line.length() == 0) return;
//   // 拆分
//   int sp = line.indexOf(' ');
//   String cmd = (sp<0)? line : line.substring(0, sp);
//   String arg = (sp<0)? ""   : line.substring(sp+1);
//   cmd.toLowerCase(); arg.trim();

//   if(cmd == "p"){                       // 百分比
//     int pct = arg.toInt();
//     pct = constrain(pct, 0, 100);
//     int us = US_MIN + (int)(pct * 10);  // 0..100 -> 1000..2000
//     escWriteUs(us);
//     Serial.print(F("[OK] 油门 ")); Serial.print(pct); Serial.print(F("%) -> "));
//     Serial.print(us); Serial.println(F(" us"));
//   } else if(cmd == "u"){                // 微秒
//     int us = arg.toInt();
//     us = constrain(us, US_MIN, US_MAX);
//     escWriteUs(us);
//     Serial.print(F("[OK] 设定 ")); Serial.print(us); Serial.println(F(" us"));
//   } else if(cmd == "r"){                // 软启动
//     int us = arg.toInt();
//     us = constrain(us, US_MIN, US_MAX);
//     Serial.print(F("[RAMP] ")); Serial.print(currentUs); Serial.print(F(" -> "));
//     Serial.print(us); Serial.println(F(" us"));
//     escRampTo(us);
//   } else if(cmd == "s"){                // 急停
//     escWriteUs(US_IDLE);
//     Serial.println(F("[STOP] 最小油门(1000us)"));
//   } else if(cmd == "g"){                // 慢速
//     escRampTo(US_SLOW);
//     Serial.println(F("[GO] 慢速运行(1250us)"));
//   } else if(cmd == "h" || cmd == "help" || cmd == "?"){
//     printHelp();
//   } else {
//     Serial.println(F("[ERR] 未知命令。输入 h 查看帮助。"));
//   }
// }

// void setup(){
//   Serial.begin(115200);
//   delay(200);

//   ledcSetup(PWM_CH, PWM_FREQ, PWM_RES);
//   ledcAttachPin(ESC_PIN, PWM_CH);

//   Serial.println(F("ESC 串口调速程序启动"));
//   Serial.println(F("注意: 电池->ESC，ESP32 与 ESC 必须共地(GND)。"));
//   Serial.println(F("先解锁 ESC..."));
//   escArm();

//   Serial.println(F("软启动到慢速..."));
//   escRampTo(US_SLOW);
//   printHelp();
// }

// void loop(){
//   // 串口行读取
//   static String buf;
//   while(Serial.available()){
//     char c = (char)Serial.read();
//     if(c == '\r') continue;
//     if(c == '\n'){
//       handleLine(buf);
//       buf = "";
//     } else {
//       buf += c;
//       if(buf.length() > 64) buf = ""; // 防溢出
//     }
//   }
//   delay(10);
// }

// //用法小结：

// // 打开串口监视器（115200，无自动换行或设“Both NL & CR”均可），输入：

// // p 30 → 30% 油门（约 1300 µs）

// // u 1200 → 1200 µs

// // r 1400 → 软启动到 1400 µs

// // s → 急停（1000 µs）

// // g → 慢速（1250 µs）

// // h → 帮助

// // 想更慢/更快，改 US_SLOW 或直接用命令调。





//--------------------------------------------------------------------------
//--------------------------------------------------------------------------
//--------------------------------------------------------------------------

//  SSSS  EEEEE RRRR  V   V  OOO       CCCC  OOO  DDDD  EEEEE
// S      E     R   R V   V O   O     C     O   O D   D E    
//  SSS   EEEE  RRRR  V   V O   O     C     O   O D   D EEEE 
//     S  E     R  R   V V  O   O     C     O   O D   D E    
// SSSS   EEEEE R   R   V    OOO       CCCC  OOO  DDDD  EEEEE



