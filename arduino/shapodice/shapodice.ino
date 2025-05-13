#define ENABLE_DEBUG_SERIAL (0)

#include <stdint.h>
#include <stdbool.h>

#include <EEPROM.h>
#include <avr/sleep.h>
#include <avr/interrupt.h>

#include "tinyio.hpp"
#include "dice_core.hpp"
#include "dice_leds.hpp"
#include "button.hpp"

#if ENABLE_DEBUG_SERIAL
#include <SoftwareSerial.h>
#else
#include "buzzer.hpp"
#endif

// ピン配置
//        RESETn dW PCINT5 ADC0 PB5 -|￣￣￣￣|- VCC
// XTAL1 CLKI OC1Bn PCINT3 ADC3 PB3 -|　　　　|- PB2 ADC1      PCINT2 SCK USCK SCL T0 INT0
// XTAL2 CLKO OC1B  PCINT4 ADC2 PB4 -|　　　　|- PB1      AIN1 PCINT1 MISO DO     OC0B OC1A
//                              GND -|＿＿＿＿|- PB0 AREF AIN0 PCINT0 MOSI DI SDA OC0A OC1An

// ポート番号
static constexpr uint8_t NUM_LED_PORTS = 3;
static constexpr uint8_t LED_PORTS[NUM_LED_PORTS] = { 2, 3, 4 };
static constexpr uint8_t LED_PORT_MASK =
  (1 << LED_PORTS[0]) | (1 << LED_PORTS[1]) | (1 << LED_PORTS[2]);
static constexpr bool LED_PORT_IS_SEQUENTIAL =
  (LED_PORTS[1] == LED_PORTS[0] + 1) && (LED_PORTS[2] == LED_PORTS[0] + 2);
static constexpr uint8_t BUTTON_PORT = 0;
static constexpr uint8_t BUZZER_PORT = 1;
static constexpr uint8_t WAKEUP_PORT = 2;
static constexpr uint8_t RESET_PORT = 5;

#if !(ENABLE_DEBUG_SERIAL)
// 起動音
static constexpr uint8_t STARTUP_SOUND[] = {
  BUZZER_NOTE(O1, C, 24),
  BUZZER_NOTE(O1, E, 24),
  BUZZER_NOTE(O1, G, 24),
  BUZZER_FINISH(),
};

// サイコロ回転音
static constexpr uint8_t ROLL_SOUND[] = {
  BUZZER_NOTE(O1, C, 8),
  BUZZER_FINISH(),
};

// 停止メロディ
static constexpr uint8_t STOP_SOUND[] = {
  BUZZER_NOTE(O0, G, 24),
  BUZZER_NOTE(O1, C, 24),
  BUZZER_NOTE(O1, E, 24),
  BUZZER_NOTE(O1, G, 24),
  BUZZER_NOTE(O2, C, 24 * 4),
  BUZZER_FINISH(),
};
#endif

// 起動の遅延時間
static constexpr uint8_t STARTUP_DELAY_MS = 100;

// スリープまでの時間
static constexpr uint8_t POWER_DOWN_DELAY_SEC = 5;

// EEPROM アドレス
static constexpr uint16_t EEPROM_ADDR_RNG_STATE = 0;

DiceCore dice;
DiceLeds leds;
Button<BUTTON_PORT> button;

#if ENABLE_DEBUG_SERIAL
static constexpr uint32_t DEBUG_BAUDRATE = 115200;
SoftwareSerial debug(RESET_PORT, BUZZER_PORT);
#define BUZZER_PLAY(sound) do {} while(false)
#define BUZZER_STOP() do {} while(false)
#define DEBUG_PRINT(val) debug.print(val);
#define DEBUG_PRINTLN(val) debug.println(val);
#define DEBUG_PRINTHEX(val) debug.print(val, HEX);
#else
Buzzer<BUZZER_PORT> buzzer;
#define BUZZER_PLAY(sound) buzzer.play(sound)
#define BUZZER_STOP() buzzer.stop()
#define DEBUG_PRINT(val) do {} while(false)
#define DEBUG_PRINTLN(val) do {} while(false)
#define DEBUG_PRINTHEX(val) do {} while(false)
#endif

uint8_t startupTimerMs = STARTUP_DELAY_MS;
uint16_t powerDownTimerMs = 0;
uint8_t powerDownTimerSec = 0;

// 初期設定
void setup() {
  startup();
  leds.put(0);
  loadRngState();
}

// ループ処理
void loop() {
  // パワーダウン制御
  powerDownTimerMs++;
  if (powerDownTimerMs >= 1000) {
    powerDownTimerMs = 0;
    powerDownTimerSec++;
    if (powerDownTimerSec > POWER_DOWN_DELAY_SEC) {
      powerDown();
      return;
    }
  }

  // スイッチの状態読み取り
  ButtonState btn = button.read();

  if (startupTimerMs > 0) {
    if (button.read() == ButtonState::UP) {
      startupTimerMs--;
      if (startupTimerMs == 0) {
        DEBUG_PRINTLN("Started up.");
      }
    } else {
      startupTimerMs = STARTUP_DELAY_MS;
    }
  } else {
    switch (btn) {
      case ButtonState::DOWN_EDGE:
        // スイッチ押下 --> サイコロ回転開始
        dice.startRolling();
        leds.stopBlink();
        DEBUG_PRINTLN("Button pushed-down.");
        BUZZER_PLAY(ROLL_SOUND);
        break;

      case ButtonState::UP_EDGE:
        // スイッチ開放 --> サイコロ減速
        dice.startSlowdown();
        DEBUG_PRINTLN("Button released.");
        break;
    }
  }

  if (btn != ButtonState::UP) {
    // ボタンが押されている間スリープまでの時間を延長
    resetSleepDelay();
  }

  auto evt = dice.update();
  switch (evt) {
    case DiceEvent::ROLL:
      // サイコロ回転 --> 回転音を再生
      BUZZER_PLAY(ROLL_SOUND);
      break;

    case DiceEvent::STOP:
      // サイコロ停止 --> 点滅開始, 停止メロディ再生
      BUZZER_PLAY(STOP_SOUND);
      leds.startBlink();
      break;
  }

  if (evt != DiceEvent::NONE) {
    // 数字を LED 表示状態に反映
    uint8_t number = dice.last();
    leds.put(number);
    DEBUG_PRINT("Event#");
    DEBUG_PRINT((uint8_t)evt);
    DEBUG_PRINT(", Dice number: ");
    DEBUG_PRINTLN(number);
  }

  // LED のダイナミック点灯
  ledScan();

  // サウンド再生
#if !(ENABLE_DEBUG_SERIAL)
  buzzer.update();
#endif

  delay(1);
}

// 起動直後の処理
void startup() {
  // ペリフェラル設定
  ledReset();
  button.begin();
  leds.begin();
#if ENABLE_DEBUG_SERIAL
  debug.begin(DEBUG_BAUDRATE);
  debug.print("\x1b[!p");  // DECSTR
  debug.println();
#else
  buzzer.begin();
#endif

#if !(ENABLE_DEBUG_SERIAL)
  BUZZER_PLAY(STARTUP_SOUND);
#endif

  // 起動直後はボタンが開放されるまでボタンに応答しない
  startupTimerMs = STARTUP_DELAY_MS;

  // パワーダウン遅延タイマの初期化
  resetSleepDelay();
}

// 乱数生成器の状態をロード
void loadRngState() {
  uint8_t rngStateSize = 0;
  uint8_t* rngState = dice.getRngStatePtr(&rngStateSize);
  uint8_t accum = 0;
  for (uint8_t i = 0; i < rngStateSize; i++) {
    uint8_t stateByte = EEPROM.read(EEPROM_ADDR_RNG_STATE + i);
    accum |= stateByte;
    rngState[i] = stateByte;
  }
  if (accum == 0) {
    // ステートがゼロだと乱数にならないので適当に設定する
    for (uint8_t i = 0; i < rngStateSize; i++) {
      rngState[i] = i;
    }
    DEBUG_PRINTLN("*W: RNG State all zero.");
  }
#if ENABLE_DEBUG_SERIAL
  dumpRngState();
#endif
}

// 乱数生成器の状態を保存
void saveRngState() {
#if ENABLE_DEBUG_SERIAL
  dumpRngState();
#endif
  uint8_t rngStateSize = 0;
  uint8_t* rngState = dice.getRngStatePtr(&rngStateSize);
  for (uint8_t i = 0; i < rngStateSize; i++) {
    EEPROM.write(EEPROM_ADDR_RNG_STATE + i, rngState[i]);
  }
  DEBUG_PRINTLN("RNG state saved.");
}

void dumpRngState() {
  uint8_t rngStateSize = 0;
  uint8_t* rngState = dice.getRngStatePtr(&rngStateSize);
  DEBUG_PRINT("RNG State: ");
  for (uint8_t i = 0; i < rngStateSize; i++) {
    DEBUG_PRINTHEX(rngState[i]);
    DEBUG_PRINT(' ');
  }
  DEBUG_PRINTLN();
}

// LED 消灯
void ledReset() {
  // 全 LED ポートを入力 (Hi-Z) に指定
  tinyio::multi::asInput(LED_PORT_MASK, tinyio::Pull::OFF);
}

// LED のダイナミック点灯
void ledScan() {
  // いったん消灯
  ledReset();

  // LED (a...d) に応じたポート設定を取得
  uint8_t sreg = leds.update();
  if (LED_PORT_IS_SEQUENTIAL) {
    // ポート番号が連続している場合はまとめて設定
    uint8_t out = sreg << LED_PORTS[0];
    uint8_t dir = sreg >> (4 - LED_PORTS[0]);
    tinyio::multi::putH(out & LED_PORT_MASK);
    tinyio::multi::asOutput(dir & LED_PORT_MASK);
  } else {
    for (uint8_t i = 0; i < NUM_LED_PORTS; i++) {
      uint8_t pin = LED_PORTS[i];
      if (sreg & 1) {
        tinyio::asOutput(pin);
      } else {
        tinyio::asInput(pin);
      }
      sreg >>= 1;
    }
    sreg >>= 1;
    for (uint8_t i = 0; i < NUM_LED_PORTS; i++) {
      uint8_t pin = LED_PORTS[i];
      if (sreg & 1) {
        tinyio::putH(pin);
      } else {
        tinyio::putL(pin);
      }
      sreg >>= 1;
    }
  }
}

// スリープ遅延カウンタリセット
void resetSleepDelay() {
  powerDownTimerMs = 0;
  powerDownTimerSec = 0;
}

// パワーダウン
void powerDown() {
  // LED 消灯
  ledReset();

  // 乱数の内部状態を保存
  saveRngState();

#if ENABLE_DEBUG_SERIAL
  debug.println("Power Down...");
  debug.end();
#endif

  // 外部割り込み設定
  // プルアップ設定前に一旦 HIGH を出力して端子を充電する
  // これをしないとすぐ復帰する場合がある
  tinyio::asOutput(WAKEUP_PORT);
  tinyio::putH(WAKEUP_PORT);
  tinyio::asInput(WAKEUP_PORT, tinyio::Pull::UP);
  attachInterrupt(WAKEUP_PORT, wakeup, RISING);
  GIMSK |= (1 << INT0);

  // -------- ここから変更禁止 --------
  // https://www.nongnu.org/avr-libc/user-manual/group__avr__sleep.html
  set_sleep_mode(SLEEP_MODE_PWR_DOWN);
  noInterrupts();
  sleep_enable();
  sleep_bod_disable();
  interrupts();
  sleep_cpu();
  sleep_disable();
  // -------- ここまで変更禁止 --------

  // 外部割り込み無効化
  GIMSK &= ~(1 << INT0);
  detachInterrupt(WAKEUP_PORT);

  // ペリフェラル再初期化
  startup();

#if ENABLE_DEBUG_SERIAL
  debug.begin(DEBUG_BAUDRATE);
  debug.println("Woke up.");
#endif
}

void wakeup() {
}
