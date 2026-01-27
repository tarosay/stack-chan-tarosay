#ifndef SPI_BUS_LOCK_HPP
#define SPI_BUS_LOCK_HPP

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

extern SemaphoreHandle_t gSpiMutex;

// 必ず一度呼ぶ（setupの早い段階）
void ensure_spi_mutex();

// RAII
struct SpiGuard {
  bool locked = false;

  SpiGuard() {
    ensure_spi_mutex();
    if (gSpiMutex) {
      xSemaphoreTakeRecursive(gSpiMutex, portMAX_DELAY);
      locked = true;
    }
  }
  ~SpiGuard() {
    if (locked && gSpiMutex) {
      xSemaphoreGiveRecursive(gSpiMutex);
    }
  }

  SpiGuard(const SpiGuard&) = delete;
  SpiGuard& operator=(const SpiGuard&) = delete;
};

#endif  // SPI_BUS_LOCK_HPP