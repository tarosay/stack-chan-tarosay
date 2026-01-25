#ifndef SPI_BUS_LOCK_HPP
#define SPI_BUS_LOCK_HPP

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

extern SemaphoreHandle_t gSpiMutex;

struct SpiGuard {
  SpiGuard() {
    xSemaphoreTake(gSpiMutex, portMAX_DELAY);
  }
  ~SpiGuard() {
    xSemaphoreGive(gSpiMutex);
  }
};

#endif  // SPI_BUS_LOCK_HPP
