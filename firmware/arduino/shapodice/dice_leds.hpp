#pragma once

#include <stdint.h>
#include "tinyio.hpp"

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

template<uint8_t PORT_X, uint8_t PORT_Y, uint8_t PORT_Z>
class DiceLeds {
public:
  static constexpr uint8_t NUM_ELEMENTS = 6;

  static constexpr uint8_t NUM_PORTS = 3;
  static constexpr uint8_t PORT_MASK = (1 << PORT_X) | (1 << PORT_Y) | (1 << PORT_Z);
  static constexpr bool PORT_IS_SEQUENTIAL = (PORT_Y == PORT_X + 1) && (PORT_Z == PORT_X + 2);

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
    switch (number) {
      case 0: state = 0b10000; break;  // 1
      case 1: state = 0b00001; break;  // 2
      case 2: state = 0b01001; break;  // 3
      case 3: state = 0b00011; break;  // 4
      case 4: state = 0b01011; break;  // 5
      case 5: state = 0b00111; break;  // 6
    }
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

    uint8_t sreg = 0;
    if ((tmp >> idx) & 1) {
      switch (idx) {
        case 0: sreg = 0b00010011; break;  // a
        case 1: sreg = 0b00100011; break;  // b
        case 2: sreg = 0b00100110; break;  // c
        case 3: sreg = 0b01000110; break;  // d
        case 4: sreg = 0b00010101; break;  // e
        case 5: sreg = 0b01000101; break;  // f
      }
    }

    // いったん消灯
    tinyio::multi::asInput(PORT_MASK, tinyio::Pull::OFF);

    // LED (a...f) に応じたポート設定を取得
    if (PORT_IS_SEQUENTIAL) {
      // ポート番号が連続している場合はまとめて設定
      uint8_t dir = sreg << PORT_X;
      uint8_t out = sreg >> (4 - PORT_X);
      tinyio::multi::putH(out & PORT_MASK);
      tinyio::multi::asOutput(dir & PORT_MASK);
    } else {
      for (uint8_t i = 0; i < NUM_PORTS; i++) {
        uint8_t pin;
        switch (i) {
          case 0: pin = PORT_X; break;
          case 1: pin = PORT_Y; break;
          case 2: pin = PORT_Z; break;
        }
        if (sreg & 0x1) {
          tinyio::asOutput(pin);
        } else {
          tinyio::asInput(pin);
        }
        if (sreg & 0x10) {
          tinyio::putH(pin);
        } else {
          tinyio::putL(pin);
        }
        sreg >>= 1;
      }
    }

    return 0;
  }
};
