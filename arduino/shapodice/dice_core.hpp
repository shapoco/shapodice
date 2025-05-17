#pragma once

#include <stdint.h>
#include "xoroshiro128plusplus.hpp"

enum class DiceEvent : uint8_t {
  NONE = 0,
  ROLL = 1,
  STOP = 2,
};

class DiceCore {
public:
  // 目の数
  static constexpr uint8_t PERIOD = 6;

  // サイコロ回転中の変化スピード (回/秒)
  static constexpr uint8_t ROLLING_SPEED_HZ = 20;

  // サイコロ回転スピードの精度 (大きいほどスイッチ開放から停止までの時間が延びる)
  static constexpr uint8_t ROLLING_SPEED_PREC = 2;

  // サイコロ回転タイマーの周期
  static constexpr uint16_t ROLLING_TIMER_PERIOD = 32768;

  Xoroshiro128plusplus rng;     // 乱数生成器
  bool buttonPressed = 0;     // ボタン押下状態
  uint16_t rollingSpeed = 0;  // 回転スピード
  uint16_t rollingTimer = 0;  // 回転タイマー
  uint8_t number = 0;         // 現在の数字

  // 現在の数字
  uint8_t last() const {
    return number;
  }

  // 回転中か否か
  bool isRolling() const {
    return (rollingSpeed != 0);
  }

  void begin() {
    // nothing to do
  }

  // 乱数生成器の内部状態へのポインタを返す
  uint8_t *getRngStatePtr(uint8_t *size) {
    *size = Xoroshiro128plusplus::STATE_BYTES;
    return (uint8_t *)rng.state;
  }

  void startRolling() {
    buttonPressed = true;
    rollingSpeed = (uint32_t)ROLLING_TIMER_PERIOD * (ROLLING_SPEED_HZ << ROLLING_SPEED_PREC) / 1000;
  }

  void startSlowdown() {
    buttonPressed = false;
    number = rng.next() % PERIOD;
    rollingSpeed = (uint32_t)ROLLING_TIMER_PERIOD * ((ROLLING_SPEED_HZ / 2) << ROLLING_SPEED_PREC) / 1000;
  }

  DiceEvent update() {
    if (!buttonPressed) {
      // 目を予測できないよう、押してない間に乱数生成器を空回りさせる
      rng.next();
    }

    if (rollingSpeed == 0) {
      // 回転スピードゼロ --> 停止
      rollingTimer = 0;
      return DiceEvent::NONE;
    }

    if (!buttonPressed) {
      // 徐々に減速
      rollingSpeed--;
      if (rollingSpeed == 0) {
        // 停止 --> 点滅を開始
        return DiceEvent::STOP;
      }
    }

    // 回転スピードをタイマーカウンタに加算
    rollingTimer += (rollingSpeed >> ROLLING_SPEED_PREC);
    if (rollingTimer >= ROLLING_TIMER_PERIOD) {
      // タイマーカウンタが溢れたら数字を更新
      rollingTimer -= ROLLING_TIMER_PERIOD;
      if (++number >= PERIOD) number = 0;
      return DiceEvent::ROLL;
    }

    return DiceEvent::NONE;
  }
};
