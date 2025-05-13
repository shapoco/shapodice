#pragma once

#include <Arduino.h>
#include <stdio.h>

// 音程
static constexpr float BUZZER_C = 1.000000000;
static constexpr float BUZZER_Cp = 1.059463094;
static constexpr float BUZZER_D = 1.122462048;
static constexpr float BUZZER_Dp = 1.189207115;
static constexpr float BUZZER_E = 1.259921050;
static constexpr float BUZZER_F = 1.334839854;
static constexpr float BUZZER_Fp = 1.414213562;
static constexpr float BUZZER_G = 1.498307077;
static constexpr float BUZZER_Gp = 1.587401052;
static constexpr float BUZZER_A = 1.681792831;
static constexpr float BUZZER_Ap = 1.781797436;
static constexpr float BUZZER_B = 1.887748625;

// オクターブ
static constexpr uint8_t BUZZER_O0 = 1;
static constexpr uint8_t BUZZER_O1 = 2;
static constexpr uint8_t BUZZER_O2 = 4;

// ベース音程
static constexpr float BUZZER_C0_FREQ = 440.0 / BUZZER_F;
static constexpr uint8_t BUZZER_BASE_PERIOD = 20e6 / BUZZER_C0_FREQ / 256;

#define BUZZER_NOTE(oct, note, duration) \
  (uint8_t)(BUZZER_BASE_PERIOD / (BUZZER_##oct * BUZZER_##note)), (duration)

#define BUZZER_FINISH() 0

template<uint8_t PORT>
class Buzzer {
public:

  // 音符のポインタ
  const uint8_t *cursor = nullptr;

  // 音符の残り時間
  uint16_t durationRemain = 0;

  void begin() {
    // nothing to do
  }

  // サウンド再生開始
  void play(const uint8_t *ptr) {
    if (ptr) {
      cursor = ptr;        // 音符ポインタ初期化
      durationRemain = 0;  // 音の長さ初期化
      update();            // 最初の音符再生
    } else {
      // 楽譜未指定 --> 演奏停止
      stop();
    }
  }

  // サウンド演奏継続
  void update() {
    if (!cursor) {
      // サウンド停止中
      return;
    }

    if (durationRemain != 0) {
      // 音符再生中
      durationRemain--;
      return;
    }

    // 次の音符を取得
    uint8_t pwmPeriod = *(cursor++);  // PWM周期
    if (!pwmPeriod) {
      // 周期がゼロであれば演奏終了
      stop();
      return;
    }
    uint8_t duration = *(cursor++);  // 音の長さ

    OCR1C = pwmPeriod;                 // PWM 周期設定
    analogWrite(PORT, pwmPeriod / 2);  // Duty = 50%
    durationRemain = duration * 4;     // 音の長さ
  }

  // サウンド演奏停止
  void stop() {
    analogWrite(PORT, 0);
    cursor = nullptr;
  }
};
