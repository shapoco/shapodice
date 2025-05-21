#pragma once

#include <stdint.h>
#include <Arduino.h>

#define TINYPM_INLINE inline __attribute__((always_inline))

namespace tinyadc {

static TINYPM_INLINE void enable() {
  ADCSRA |= (1 << ADEN) | (0x7 << ADPS0);
}

static TINYPM_INLINE void disable() {
  ADCSRA &= ~(1 << ADEN);
}

}
