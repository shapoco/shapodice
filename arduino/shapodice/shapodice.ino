#define ENABLE_DEBUG_SERIAL (0)

#include <stdint.h>
#include <stdbool.h>

#include <EEPROM.h>
#include <avr/sleep.h>
#include <avr/interrupt.h>

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

// ノイズ生成用ポート
static constexpr uint8_t NOISE_SENSE_PORT = 2;
static constexpr uint8_t NOISE_ADC_CHANNEL = 1;

#if !(ENABLE_DEBUG_SERIAL)
// 起動メロディ
static constexpr uint8_t STARTUP_SOUND[] = {
  BUZZER_NOTE(O0, G, 24),
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
#else
Buzzer<BUZZER_PORT> buzzer;
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
#if ENABLE_DEBUG_SERIAL
      if (startupTimerMs == 0) {
        debug.println("Started up.");
      }
#endif
    } else {
      startupTimerMs = STARTUP_DELAY_MS;
    }
  } else {
    switch (btn) {
      case ButtonState::DOWN_EDGE:
        // スイッチ押下 --> サイコロ回転開始
        dice.startRolling();
        leds.stopBlink();
#if ENABLE_DEBUG_SERIAL
        debug.println("Button pushed-down.");
#else
        buzzer.play(ROLL_SOUND);
#endif
        break;

      case ButtonState::UP_EDGE:
        // スイッチ開放 --> サイコロ減速
        dice.startSlowdown();
#if ENABLE_DEBUG_SERIAL
        debug.println("Button released.");
#endif
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
#if !(ENABLE_DEBUG_SERIAL)
      // サイコロ回転 --> 回転音を再生
      buzzer.play(ROLL_SOUND);
#endif
      break;

    case DiceEvent::STOP:
      // サイコロ停止 --> 点滅開始, 停止メロディ再生
#if !(ENABLE_DEBUG_SERIAL)
      buzzer.play(STOP_SOUND);
#endif
      leds.startBlink();
      break;
  }

  if (evt != DiceEvent::NONE) {
    // 数字を LED 表示状態に反映
    uint8_t number = dice.last();
    leds.put(number);
#if ENABLE_DEBUG_SERIAL
    debug.print("Event#");
    debug.print((uint8_t)evt);
    debug.print(", Dice number: ");
    debug.println(number);
#endif
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
  debug.println();
#else
  buzzer.begin();
#endif

#if !(ENABLE_DEBUG_SERIAL)
  buzzer.play(STARTUP_SOUND);
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
#if ENABLE_DEBUG_SERIAL
    debug.println("*W: RNG State all zero.");
#endif
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
#if ENABLE_DEBUG_SERIAL
  debug.println("RNG state saved.");
#endif
}

#if ENABLE_DEBUG_SERIAL
void dumpRngState() {
  uint8_t rngStateSize = 0;
  uint8_t* rngState = dice.getRngStatePtr(&rngStateSize);
  debug.print("RNG State: ");
  for (uint8_t i = 0; i < rngStateSize; i++) {
    debug.print(rngState[i], HEX);
    debug.print(' ');
  }
  debug.println();
}
#endif

// LED 消灯
void ledReset() {
  // 全 LED ポートを入力 (Hi-Z) に指定
  DDRB &= ~LED_PORT_MASK;
  PORTB &= ~LED_PORT_MASK;
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
    PORTB |= out & LED_PORT_MASK;
    DDRB |= dir & LED_PORT_MASK;
  } else {
    for (uint8_t i = 0; i < NUM_LED_PORTS; i++) {
      uint8_t pin = LED_PORTS[i];
      pinMode(pin, (sreg & 1) ? OUTPUT : INPUT);
      sreg >>= 1;
    }
    sreg >>= 1;
    for (uint8_t i = 0; i < NUM_LED_PORTS; i++) {
      uint8_t pin = LED_PORTS[i];
      digitalWrite(pin, sreg & 1);
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
  pinMode(WAKEUP_PORT, OUTPUT);
  digitalWrite(WAKEUP_PORT, HIGH);
  pinMode(WAKEUP_PORT, INPUT_PULLUP);
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
