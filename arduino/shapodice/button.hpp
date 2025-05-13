#pragma once

#include <Arduino.h>
#include <stdio.h>

enum class ButtonState : uint8_t {
  UP = 0b00,         // 開放中
  DOWN_EDGE = 0b01,  // 開放の瞬間
  DOWN = 0b11,       // 押下中
  UP_EDGE = 0b10,    // 押下の瞬間
};

template<uint8_t PORT>
class Button {
public:
  // チャタリング除去用シフトレジスタ
  uint8_t filter = 0;

  // スイッチ押下状態
  bool isPressed = false;

  // スイッチ状態
  ButtonState switchState = ButtonState::UP;

  void begin() {
    pinMode(PORT, INPUT_PULLUP);
  }

  ButtonState read() {
    bool swRaw = digitalRead(PORT) == LOW;

    // チャタリング除去用シフトレジスタ
    filter <<= 1;
    if (swRaw) {
      // LOW なら押下中
      filter |= 1;
    }
    if (filter == 0x00) {
      isPressed = false;
    } else if (filter == 0xff) {
      isPressed = true;
    }

    // エッジ検出用シフトレジスタ
    uint8_t tmp = static_cast<uint8_t>(switchState);
    tmp <<= 1;
    if (isPressed) {
      tmp |= 1;
    }
    tmp &= 0b11;
    switchState = static_cast<ButtonState>(tmp);

    return switchState;
  }
};
