#pragma once

#include <stdint.h>
#include <Arduino.h>

#define PORTS_INLINE inline __attribute__((always_inline))

namespace tinyio {

enum class Pull : uint8_t {
  KEEP,
  OFF,
  UP,
};

}

namespace tinyio::multi {

static PORTS_INLINE void asInput(uint8_t mask, Pull pu = Pull::KEEP) {
  DDRB &= ~mask;
  switch (pu) {
    case Pull::OFF:
      PORTB &= ~mask;
      break;
    case Pull::UP:
      PORTB |= mask;
      break;
  }
}

static PORTS_INLINE void asOutput(uint8_t mask) {
  DDRB |= mask;
}

static PORTS_INLINE void putL(uint8_t mask) {
  PORTB &= ~mask;
}

static PORTS_INLINE void putH(uint8_t mask) {
  PORTB |= mask;
}

static PORTS_INLINE bool isAllL(uint8_t mask) {
  return !(PINB & mask);
}

}

namespace tinyio {

static PORTS_INLINE void asInput(uint8_t port, Pull pu = Pull::KEEP) {
  multi::asInput(1 << port, pu);
}

static PORTS_INLINE void asOutput(uint8_t port) {
  multi::asOutput(1 << port);
}

static PORTS_INLINE void putL(uint8_t port) {
  multi::putL(1 << port);
}

static PORTS_INLINE void putH(uint8_t port) {
  multi::putH(1 << port);
}

static PORTS_INLINE bool isL(uint8_t port) {
  return multi::isAllL(1 << port);
}

static PORTS_INLINE bool isH(uint8_t port) {
  return !isL(1 << port);
}

}
