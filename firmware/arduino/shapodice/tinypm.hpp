#pragma once

#include <stdint.h>
#include <Arduino.h>
#include <avr/sleep.h>

#define TINYPM_INLINE inline __attribute__((always_inline))

namespace tinypm {

static TINYPM_INLINE void powerDown() {
  // https://www.nongnu.org/avr-libc/user-manual/group__avr__sleep.html
  set_sleep_mode(SLEEP_MODE_PWR_DOWN);
  noInterrupts();
  sleep_enable();
  sleep_bod_disable();
  interrupts();
  sleep_cpu();
  sleep_disable();
}

}
