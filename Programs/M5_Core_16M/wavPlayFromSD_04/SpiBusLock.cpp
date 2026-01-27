#include "SpiBusLock.hpp"

SemaphoreHandle_t gSpiMutex = nullptr;

void ensure_spi_mutex() {
  if (gSpiMutex == nullptr) {
    gSpiMutex = xSemaphoreCreateRecursiveMutex();
  }
}
