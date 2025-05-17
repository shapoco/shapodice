#define ENABLE_DEBUG_SERIAL (0)

#define SHAPODICE_INLINE inline __attribute__((always_inline))

#include <stdint.h>
#include <stdbool.h>

#include <EEPROM.h>
#include <avr/sleep.h>
#include <avr/interrupt.h>

#include "tinyio.hpp"
#include "dice_core.hpp"
#include "dice_leds.hpp"
#include "button.hpp"
#include "buzzer.hpp"

#if ENABLE_DEBUG_SERIAL
#include <SoftwareSerial.h>
#endif

// ピン配置
//        RESETn dW PCINT5 ADC0 PB5 -|￣￣￣￣|- VCC
// XTAL1 CLKI OC1Bn PCINT3 ADC3 PB3 -|　　　　|- PB2 ADC1      PCINT2 SCK USCK SCL T0 INT0
// XTAL2 CLKO OC1B  PCINT4 ADC2 PB4 -|　　　　|- PB1      AIN1 PCINT1 MISO DO     OC0B OC1A
//                              GND -|＿＿＿＿|- PB0 AREF AIN0 PCINT0 MOSI DI SDA OC0A OC1An

// ポート番号
static constexpr uint8_t LED_PORT_X = 0;
static constexpr uint8_t LED_PORT_Y = 3;
static constexpr uint8_t LED_PORT_Z = 4;
static constexpr uint8_t BUTTON_PORT = 2;
static constexpr uint8_t BUZZER_PORT = 1;
static constexpr uint8_t RESET_PORT = 5;

#if !defined(ADC_INTERNAL1V1)
#define ADC_INTERNAL1V1 (0x0C)
#endif
static constexpr uint8_t BATTERY_ADC = ADC_INTERNAL1V1;

// バッテリー定電圧閾値 (mV)
static constexpr uint16_t LOW_BATTERY_THRESH_MV = 3.3 * 1000;

// バッテリー定電圧閾値 (ADC値)
static constexpr uint16_t LOW_BATTERY_THRESH_ADC = 1100.0 * 1024 / LOW_BATTERY_THRESH_MV;

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

// 起動の遅延時間
static constexpr uint8_t STARTUP_DELAY_MS = 100;

// スリープまでの時間
static constexpr uint8_t POWER_DOWN_DELAY_SEC = 60;

// 電源電圧測定間隔
static constexpr uint8_t BATTERY_CHECK_INTERVAL_SEC = 10;

// EEPROM アドレス
static constexpr uint16_t EEPROM_ADDR_RNG_STATE = 0;

DiceCore dice;
DiceLeds<LED_PORT_X, LED_PORT_Y, LED_PORT_Z> leds;
Button<BUTTON_PORT> button;

#if ENABLE_DEBUG_SERIAL
static constexpr uint32_t DEBUG_BAUDRATE = 115200;
SoftwareSerial debug(RESET_PORT, BUZZER_PORT);
#else
Buzzer<BUZZER_PORT> buzzer;
#endif

uint8_t startupTimerMs = STARTUP_DELAY_MS;
uint16_t milliSecCounter = 0;
uint8_t powerDownTimerSec = POWER_DOWN_DELAY_SEC;
uint8_t batteryCheckTimerSec = 0;
bool lowBattery = false;

static SHAPODICE_INLINE void buzzer_play(const uint8_t* sound) {
#if !(ENABLE_DEBUG_SERIAL)
  buzzer.play(sound);
#endif
}

static SHAPODICE_INLINE void buzzer_stop() {
#if !(ENABLE_DEBUG_SERIAL)
  buzzer.stop();
#endif
}

// clang-format off
#if ENABLE_DEBUG_SERIAL
#define DEBUG_PRINT(val) debug.print(val);
#define DEBUG_PRINTLN(val) debug.println(val);
#else
#define DEBUG_PRINT(val) do { } while (false)
#define DEBUG_PRINTLN(val) do { } while (false)
#endif
// clang-format on

static void DEBUG_PRINTHEX(uint8_t val) {
#if ENABLE_DEBUG_SERIAL
  char buf[3];
  uint8_t tmp = val;
  uint8_t i = 3;
  buf[--i] = '\0';
  do {
    uint8_t digit = tmp & 0xf;
    buf[--i] = ((digit < 10) ? '0' : ('a' - 10)) + digit;
    tmp >>= 4;
  } while (i != 0);
  debug.print(buf);
#endif
}

// 初期設定
void setup() {
  startup();
  leds.put(0);
  loadRngState();
  dice.rng.next();
  saveRngState();
}

// 起動直後の処理
void startup() {
  // ペリフェラル設定
  button.begin();
  leds.begin();
#if ENABLE_DEBUG_SERIAL
  debug.begin(DEBUG_BAUDRATE);
  debug.print("\x1b[!p");  // DECSTR
  debug.println();
  tinyio::asOutput(BUZZER_PORT);
#else
  buzzer.begin();
#endif

  // ADC 有効化 + 空読み
  ADCSRA |= (1 << ADEN) | (0x7 << ADPS0);
  for (uint8_t i = 3; i != 0; i--) {
    analogRead(BATTERY_ADC);
  }

#if !(ENABLE_DEBUG_SERIAL)
  buzzer_play(STARTUP_SOUND);
#endif

  // 起動直後はボタンが開放されるまでボタンに応答しない
  startupTimerMs = STARTUP_DELAY_MS;

  // 各種タイマの初期化
  resetPowerDownTimer();
  resetBatteryCheckTimer();
  milliSecCounter = 0;
}

// ループ処理
void loop() {
  // 秒毎の処理
  bool pulse1sec = (milliSecCounter == 0);
  if (pulse1sec) {
    milliSecCounter = 999;
  } else {
    milliSecCounter--;
  }

  // パワーダウン制御
  powerDownControl(pulse1sec);

  // バッテリーチェック
  batteryCheck(pulse1sec);

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
        buzzer_play(ROLL_SOUND);
        break;

      case ButtonState::UP_EDGE:
        // スイッチ開放 --> サイコロ減速
        dice.startSlowdown();
        DEBUG_PRINTLN("Button released.");
        break;
    }
  }

  if (btn != ButtonState::UP || dice.isRolling()) {
    // ボタンが押されている間と回転痛はパワーダウンまでの時間を延長
    resetPowerDownTimer();
  }

  auto evt = dice.update();
  switch (evt) {
    case DiceEvent::ROLL:
      // サイコロ回転 --> 回転音を再生
      buzzer_play(ROLL_SOUND);
      break;

    case DiceEvent::STOP:
      // サイコロ停止 --> 点滅開始, 停止メロディ再生
      buzzer_play(STOP_SOUND);
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
  leds.setUserLed(lowBattery && !(milliSecCounter & 0x200));
  leds.update();

// サウンド再生
#if !(ENABLE_DEBUG_SERIAL)
  buzzer.update();
#endif

  delay(1);
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
  DEBUG_PRINTLN("RNG state loaded.");
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
}

// スリープ遅延カウンタリセット
void resetPowerDownTimer() {
  powerDownTimerSec = POWER_DOWN_DELAY_SEC;
}

// パワーダウン
void powerDownControl(bool pulse1sec) {
  if (powerDownTimerSec != 0) {
    if (pulse1sec) {
      DEBUG_PRINT("Power down timer: ");
      DEBUG_PRINTLN(powerDownTimerSec);
      powerDownTimerSec--;
    }
    return;
  }
  powerDownTimerSec = 0;

  // 全ポートを内部プルアップに設定
  tinyio::multi::asInput(0xff, tinyio::Pull::UP);

  // 乱数の内部状態を保存
  saveRngState();

#if ENABLE_DEBUG_SERIAL
  debug.println("Power down...");
  debug.end();
#endif

  // ADC 無効化
  ADCSRA &= ~((1 << ADEN) | (0x7 << ADPS0));

  // 外部割り込み設定
  attachInterrupt(BUTTON_PORT, wakeup, RISING);
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
  detachInterrupt(BUTTON_PORT);

  // ペリフェラル再初期化
  startup();

#if ENABLE_DEBUG_SERIAL
  debug.begin(DEBUG_BAUDRATE);
  debug.println("Woke up.");
#endif
}

void wakeup() {
}

void resetBatteryCheckTimer() {
  batteryCheckTimerSec = 0;
}

// 電源電圧測定
void batteryCheck(bool pulse1sec) {
  if (batteryCheckTimerSec != 0) {
    if (pulse1sec) {
      DEBUG_PRINT("Battery check timer: ");
      DEBUG_PRINTLN(batteryCheckTimerSec);
      batteryCheckTimerSec--;
    }
    return;
  }
  batteryCheckTimerSec = BATTERY_CHECK_INTERVAL_SEC;

  // 電源電圧に対する内部基準電圧 1.1V の値を測る
  uint16_t adcVal = analogRead(BATTERY_ADC);

  // 1.1V の ADC 値が閾値以上なら定電圧判定
  lowBattery = adcVal >= LOW_BATTERY_THRESH_ADC;

#if ENABLE_DEBUG_SERIAL
  uint16_t milliVolt = (uint32_t)(1.1 * 1024 * 1000) / adcVal;
  DEBUG_PRINT("Battery ADC value: ");
  DEBUG_PRINT(adcVal);
  DEBUG_PRINT(" (");
  DEBUG_PRINT(milliVolt);
  DEBUG_PRINT("mV)");
  if (lowBattery) {
    DEBUG_PRINTLN(" LOW BATTERY !!");
  } else {
    DEBUG_PRINTLN();
  }
#endif
}