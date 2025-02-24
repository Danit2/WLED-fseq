#pragma once

// Pôvodné makrá a definície (ako v pôvodnom kóde)
#ifndef USED_STORAGE_FILESYSTEMS
  #ifdef WLED_USE_SD_SPI
    #define USED_STORAGE_FILESYSTEMS "SD SPI, LittleFS"
  #else
    #define USED_STORAGE_FILESYSTEMS "SD MMC, LittleFS"
  #endif
#endif

#ifndef SD_ADAPTER
  #if defined(WLED_USE_SD) || defined(WLED_USE_SD_SPI)
    #ifdef WLED_USE_SD_SPI
      #ifndef WLED_USE_SD
        #define WLED_USE_SD
      #endif
      #ifndef WLED_PIN_SCK
        #define WLED_PIN_SCK SCK
      #endif
      #ifndef WLED_PIN_MISO
        #define WLED_PIN_MISO MISO
      #endif
      #ifndef WLED_PIN_MOSI
        #define WLED_PIN_MOSI MOSI
      #endif
      #ifndef WLED_PIN_SS
        #define WLED_PIN_SS SS
      #endif
      #define SD_ADAPTER SD
    #else
      #define SD_ADAPTER SD_MMC
    #endif
  #endif
#endif

#ifdef WLED_USE_SD_SPI
  #ifndef SPI_PORT_DEFINED
    inline SPIClass spiPort = SPIClass(VSPI);
    #define SPI_PORT_DEFINED
  #endif
#endif

#include "wled.h"
#include "../usermods/FSEQ/fseq_player.h"
#include "../usermods/FSEQ/fseq_player.cpp"
#include "../usermods/FSEQ/sd_manager.h"
#include "../usermods/FSEQ/sd_manager.cpp"
#include "../usermods/FSEQ/web_ui_manager.cpp"
#include "../usermods/FSEQ/web_ui_manager.h"

class UsermodFseq : public Usermod {
  private:
    WebUIManager webUI; // Modul správy web UI s endpointmi
  public:
    void setup() {
      DEBUG_PRINTF("[%s] Usermod loaded\n", FPSTR("SD & FSEQ Web"));
      
      // Inicializácia SD karty cez SDManager
      SDManager sd;
      if (!sd.begin()) {
        DEBUG_PRINTF("[%s] SD initialization FAILED.\n", FPSTR("SD & FSEQ Web"));
      } else {
        DEBUG_PRINTF("[%s] SD initialization successful.\n", FPSTR("SD & FSEQ Web"));
      }
      
      // Registrácia všetkých web endpointov (definovaných v web_ui_manager)
      webUI.registerEndpoints();
    }
    
    void loop() {
      // Spracovanie FSEQ prehrávania
      FSEQPlayer::handlePlayRecording();
    }
    
    uint16_t getId() {
      return USERMOD_ID_SD_CARD;
    }

#ifdef WLED_USE_SD_SPI
    // Pridaj chýbajúce getter metódy a statické premenne pre SD piny
    static int8_t getCsPin()   { return configPinSourceSelect; }
    static int8_t getSckPin()  { return configPinSourceClock; }
    static int8_t getMisoPin() { return configPinPoci; }
    static int8_t getMosiPin() { return configPinPico; }

    static int8_t configPinSourceSelect;
    static int8_t configPinSourceClock;
    static int8_t configPinPoci;
    static int8_t configPinPico;
#endif
};

#ifdef WLED_USE_SD_SPI
int8_t UsermodFseq::configPinSourceSelect = 5;
int8_t UsermodFseq::configPinSourceClock  = 18;
int8_t UsermodFseq::configPinPoci         = 19;
int8_t UsermodFseq::configPinPico         = 23;
#endif