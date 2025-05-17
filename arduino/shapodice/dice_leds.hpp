#pragma once

#include <stdint.h>

// LED a...e の並び
// (a)       (b)
// (c) (d,e) (c)
// (b)       (a)

// a-d: 白 (Vf=3V)
// e-f: 赤 (Vf=1.5V)

// ポートと LED の接続
// portX           portY           portZ
//   |           a   |   c           |
//   |       +--[>|--+--[>|-----VVV--+
//   +--VVV--|       |               |
//   |       +--|<]--+               |
//   |           b   |               |
//   |               |               |
//   |           a   |   c           |
//   |       +--[>|--+--[>|--+       |
//   +--VVV--|       |       +--VVV--+
//           +--|<]--+--|<]--+
//           |   b       d   |
//           |       e       |
//           +------[>|------+
//           |               |
//           +------|<]------+
//                   f

class DiceLeds {
public:
  static constexpr uint8_t NUM_ELEMENTS = 6;

  uint8_t state = 0;       // LED 点灯状態
  uint8_t scanIndex = 0;   // LED ダイナミック点灯用カウンタ
  uint8_t blinkTimer = 0;  // 点滅用タイマー
  uint8_t blinkCount = 0;  // 点滅残り回数
  bool userLed = false;    // ユーザー LED

  void begin() {
    // nothing to do
  }

  // サイコロの数字を設定
  void put(uint8_t number) {
    // サイコロの目毎の LED 点灯パターン
    // bit 0: a
    // bit 1: b
    // bit 2: c
    // bit 3: d
    // bit 4: e
    // bit 5: f (user defined)
    constexpr uint8_t TABLE[] = {
      0b10000,  // 1
      0b00001,  // 2
      0b01001,  // 3
      0b00011,  // 4
      0b01011,  // 5
      0b00111,  // 6
    };
    state = TABLE[number];
  }

  // ユーザー LED の設定
  void setUserLed(bool val) {
    userLed = val;
  }

  // 点滅開始
  void startBlink() {
    blinkCount = 5;
    blinkTimer = 0xff;
  }

  // 点滅中止
  void stopBlink() {
    blinkCount = 0;
    blinkTimer = 0;
  }

  // LED の点灯状態を更新してポートのドライブ状態を返す
  // bit 0: portX の pinMode の値
  // bit 1: portY の pinMode の値
  // bit 2: portZ の pinMode の値
  // bit 3: reserved
  // bit 4: portX の digitalWrite の値
  // bit 5: portY の digitalWrite の値
  // bit 6: portZ の digitalWrite の値
  // bit 7: reserved
  uint8_t update() {
    uint8_t idx = scanIndex;
    if (++scanIndex >= NUM_ELEMENTS) scanIndex = 0;

    uint8_t tmp = state;

    if (blinkCount) {
      // 点滅中
      if (!blinkTimer) {
        blinkCount--;
      }
      blinkTimer--;
      if (blinkCount & 1) {
        tmp = 0;
      }
    }

    if (userLed) {
      // ユーザーLED
      tmp |= 1 << (NUM_ELEMENTS - 1);
    }

    if ((tmp >> idx) & 1) {
      switch (idx) {
        case 0: return 0b00010011;  // a
        case 1: return 0b00100011;  // b
        case 2: return 0b00100110;  // c
        case 3: return 0b01000110;  // d
        case 4: return 0b00010101;  // e
        case 5: return 0b01000101;  // f
      }
    }

    return 0;
  }
};
