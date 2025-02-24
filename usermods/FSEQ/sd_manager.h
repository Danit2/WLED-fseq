#ifndef SD_MANAGER_H
#define SD_MANAGER_H

#include "wled.h"

#ifdef WLED_USE_SD_SPI
  #include <SPI.h>
  #include <SD.h>
#elif defined(WLED_USE_SD_MMC)
  #include "SD_MMC.h"
#endif

class SDManager {
  public:
    SDManager() {}
    bool begin();      // Inicializácia SD
    void end();        // Deinitializácia SD
    String listFiles(const char* dirname);  // Výpis súborov vo forme HTML
    bool deleteFile(const char* path);
};

#endif // SD_MANAGER_H