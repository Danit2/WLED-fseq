#pragma once

#include "wled.h"
#include "usermod_fseq.h"  // Obsahuje FSEQ playback logiku a getter metódy pre piny

#ifdef WLED_USE_SD_SPI
  #include <SPI.h>
  #include <SD.h>
#elif defined(WLED_USE_SD_MMC)
  #include "SD_MMC.h"
#endif

#include <AsyncUDP.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson-v6.h>

// ----- Minimal WriteBufferingStream Implementation -----
class WriteBufferingStream : public Stream {
  public:
    WriteBufferingStream(Stream &upstream, size_t capacity) : _upstream(upstream) {
      _capacity = capacity;
      _buffer = (uint8_t*)malloc(capacity);
      _offset = 0;
      if (!_buffer) {
        DEBUG_PRINTLN(F("[WBS] ERROR: Buffer allocation failed"));
      }
    }
    ~WriteBufferingStream() {
      flush();
      if (_buffer) free(_buffer);
    }
    size_t write(const uint8_t *buffer, size_t size) override {
      size_t total = 0;
      while (size > 0) {
        size_t space = _capacity - _offset;
        size_t toCopy = (size < space) ? size : space;
        memcpy(_buffer + _offset, buffer, toCopy);
        _offset += toCopy;
        buffer += toCopy;
        size -= toCopy;
        total += toCopy;
        if (_offset == _capacity) flush();
      }
      return total;
    }
    size_t write(uint8_t b) override {
      return write(&b, 1);
    }
    void flush() override {
      if (_offset > 0) {
        _upstream.write(_buffer, _offset);
        _offset = 0;
      }
      _upstream.flush();
    }
    int available() override { return _upstream.available(); }
    int read() override { return _upstream.read(); }
    int peek() override { return _upstream.peek(); }
  private:
    Stream &_upstream;
    uint8_t *_buffer = nullptr;
    size_t _capacity = 0;
    size_t _offset = 0;
};
// ----- End WriteBufferingStream -----

#define FILE_UPLOAD_BUFFER_SIZE (563 * 7)

// Definície pre UDP (FPP) synchronizáciu
#define CTRL_PKT_SYNC   1
#define CTRL_PKT_PING   4
#define CTRL_PKT_BLANK  3

// UDP port pre FPP discovery/synchronizáciu
const uint16_t UDP_SYNC_PORT = 32320;

// Inline funkcie pre zápis 16-bitových a 32-bitových hodnôt
static inline void write16(uint8_t* dest, uint16_t value) {
  dest[0] = (value >> 8) & 0xff;
  dest[1] = value & 0xff;
}

static inline void write32(uint8_t* dest, uint32_t value) {
  dest[0] = (value >> 24) & 0xff;
  dest[1] = (value >> 16) & 0xff;
  dest[2] = (value >> 8) & 0xff;
  dest[3] = value & 0xff;
}

// Štruktúra synchronizačného paketu (prispôsobte podľa potreby)
struct FPPMultiSyncPacket {
  uint8_t header[4];          // napr. "FPPD"
  uint8_t packet_type;        // napr. CTRL_PKT_SYNC
  uint16_t data_len;          // dĺžka dát
  uint8_t sync_action;        // akcia: start, stop, sync, open, atď.
  uint8_t sync_type;          // typ synchronizácie, napr. 0 pre FSEQ
  uint32_t frame_number;      // aktuálny rámec
  float seconds_elapsed;      // uplynuté sekundy
  char filename[64];          // názov prehrávaného súboru
  uint8_t raw[128];           // surové dáta paketu
};

class UsermodFPP : public Usermod {
  private:
    AsyncUDP udp;
    bool udpStarted = false;
    const IPAddress multicastAddr = IPAddress(239, 70, 80, 80);
    const uint16_t udpPort = UDP_SYNC_PORT; // UDP port pre FPP discovery

    // Premenné pre upload FSEQ súborov
    File currentUploadFile;
    String currentUploadFileName = "";
    unsigned long uploadStartTime = 0;
    WriteBufferingStream* uploadStream = nullptr;

    String getDeviceName() {
      return String(serverDescription);
    }

    // Sestaví JSON so systémovými informáciami
    String buildSystemInfoJSON() {
      DynamicJsonDocument doc(1024);
      doc["fppd"] = "running";
      String devName = getDeviceName();
      doc["HostName"] = devName;
      doc["HostDescription"] = devName;
      doc["Platform"] = "ESPixelStick";
      doc["Variant"] = "ESPixelStick-ESP32";
      doc["Mode"] = 8;
      doc["Version"] = "4.x-dev";
      doc["majorVersion"] = 4;
      doc["minorVersion"] = 0;
      doc["typeId"] = 195;
      JsonObject utilization = doc.createNestedObject("Utilization");
      utilization["MemoryFree"] = ESP.getFreeHeap();
      utilization["Uptime"] = millis();
      doc["rssi"] = WiFi.RSSI();
      JsonArray ips = doc.createNestedArray("IPS");
      ips.add(WiFi.localIP().toString());
      String json;
      serializeJson(doc, json);
      return json;
    }

    // Sestaví JSON so systémovým statusom
    String buildSystemStatusJSON() {
      DynamicJsonDocument doc(1024);
      doc["fppd"] = "running";
      String devName = getDeviceName();
      doc["HostName"] = devName;
      doc["HostDescription"] = devName;
      doc["Platform"] = "ESPixelStick";
      doc["Variant"] = "ESPixelStick-ESP32";
      doc["Mode"] = 8;
      doc["Version"] = "4.x-dev";
      doc["majorVersion"] = 4;
      doc["minorVersion"] = 0;
      doc["typeId"] = 195;
      doc["current_sequence"] = "";
      doc["playlist"] = "";
      doc["seconds_elapsed"] = 0;
      doc["seconds_played"] = 0;
      doc["seconds_remaining"] = 0;
      doc["time_elapsed"] = "00:00";
      doc["time_remaining"] = "00:00";
      doc["status"] = 0;
      doc["status_name"] = "idle";
      String json;
      serializeJson(doc, json);
      return json;
    }

    // Sestaví JSON pre FPP multi-sync systémy
    String buildFppdMultiSyncSystemsJSON() {
      DynamicJsonDocument doc(512);
      String devName = getDeviceName();
      doc["hostname"] = devName;
      doc["id"] = devName;
      doc["ip"] = WiFi.localIP().toString();
      doc["version"] = "4.x-dev";
      doc["hardwareType"] = "ESPixelStick-ESP32";
      doc["type"] = 195;
      doc["num_chan"] = 2052;
      doc["NumPixelPort"] = 5;
      doc["NumSerialPort"] = 0;
      String json;
      serializeJson(doc, json);
      return json;
    }

    // Odoslanie ping paketu cez UDP
    void sendPingPacket(IPAddress destination = IPAddress(255,255,255,255)) {
      uint8_t buf[301];
      memset(buf, 0, sizeof(buf));
      buf[0] = 'F'; buf[1] = 'P'; buf[2] = 'P'; buf[3] = 'D';
      buf[4] = 0x04; // ping typ
      uint16_t dataLen = 294;
      buf[5] = (dataLen >> 8) & 0xFF;
      buf[6] = dataLen & 0xFF;
      buf[7] = 0x03;
      buf[8] = 0x00;
      buf[9] = 0xC3;
      uint16_t versionMajor = 0, versionMinor = 16;
      buf[10] = (versionMajor >> 8) & 0xFF;
      buf[11] = versionMajor & 0xFF;
      buf[12] = (versionMinor >> 8) & 0xFF;
      buf[13] = versionMinor & 0xFF;
      buf[14] = 0x08;
      IPAddress ip = WiFi.localIP();
      buf[15] = ip[0]; buf[16] = ip[1]; buf[17] = ip[2]; buf[18] = ip[3];
      String hostName = getDeviceName();
      if(hostName.length() > 32) hostName = hostName.substring(0,32);
      for (int i = 0; i < 32; i++) {
        buf[19 + i] = (i < hostName.length()) ? hostName[i] : 0;
      }
      String verStr = "4.x-dev";
      for (int i = 0; i < 16; i++) {
        buf[51 + i] = (i < verStr.length()) ? verStr[i] : 0;
      }
      String hwType = "ESPixelStick-ESP32";
      for (int i = 0; i < 16; i++) {
        buf[67 + i] = (i < hwType.length()) ? hwType[i] : 0;
      }
      udp.writeTo(buf, sizeof(buf), destination, udpPort);
    }

    // Odoslanie synchronizačnej správy
    void sendSyncMessage(uint8_t action, const String &fileName, uint32_t currentFrame, float secondsElapsed) {
      FPPMultiSyncPacket syncPacket;
      // Vyplníme hlavičku "FPPD"
      syncPacket.header[0] = 'F';
      syncPacket.header[1] = 'P';
      syncPacket.header[2] = 'P';
      syncPacket.header[3] = 'D';
      syncPacket.packet_type = CTRL_PKT_SYNC;
      // Nastavíme dĺžku dát – využijeme veľkosť celej štruktúry
      write16((uint8_t*)&syncPacket.data_len, sizeof(syncPacket));
      syncPacket.sync_action = action;
      syncPacket.sync_type = 0; // FSEQ synchronizácia
      write32((uint8_t*)&syncPacket.frame_number, currentFrame);
      syncPacket.seconds_elapsed = secondsElapsed;
      // Skopírujeme názov súboru (ak je dlhší, bude orezaný)
      strncpy(syncPacket.filename, fileName.c_str(), sizeof(syncPacket.filename)-1);
      syncPacket.filename[sizeof(syncPacket.filename)-1] = 0x00;

      // Odosielame paket na broadcast aj multicast adresy
      udp.writeTo(syncPacket.raw, sizeof(syncPacket), IPAddress(255,255,255,255), udpPort);
      udp.writeTo(syncPacket.raw, sizeof(syncPacket), multicastAddr, udpPort);
    }

    // Spracovanie prijatého UDP paketu
    void processUdpPacket(AsyncUDPPacket packet) {
      if(packet.length() < 4) return;
      if(packet.data()[0] != 'F' || packet.data()[1] != 'P' ||
         packet.data()[2] != 'P' || packet.data()[3] != 'D')
        return;
      uint8_t packetType = packet.data()[4];
      switch(packetType) {
        case CTRL_PKT_SYNC: {
          FPPMultiSyncPacket* syncPacket = reinterpret_cast<FPPMultiSyncPacket*>(packet.data());
          DEBUG_PRINTLN(F("[FPP] Received UDP sync packet"));
          // Voláme synchronizačnú funkciu pre FSEQ playback, ak ju implementujete
          // Napríklad: FSEQPlayer::syncFromUdp(syncPacket->filename, syncPacket->seconds_elapsed);
          break;
        }
        case CTRL_PKT_PING:
          DEBUG_PRINTLN(F("[FPP] Received UDP ping packet"));
          sendPingPacket(packet.remoteIP());
          break;
        case CTRL_PKT_BLANK:
          DEBUG_PRINTLN(F("[FPP] Received UDP blank packet"));
          // Prípadne zastavte prehrávanie
          break;
        default:
          DEBUG_PRINTLN(F("[FPP] Unknown UDP packet type"));
          break;
      }
    }

  public:
    static const char _name[];

    void setup() {
      DEBUG_PRINTF("[%s] FPP Usermod loaded\n", _name);
      
#ifdef WLED_USE_SD_SPI
      int8_t csPin   = UsermodFseq::getCsPin();
      int8_t sckPin  = UsermodFseq::getSckPin();
      int8_t misoPin = UsermodFseq::getMisoPin();
      int8_t mosiPin = UsermodFseq::getMosiPin();
      if (!SD.begin(csPin)) {
        DEBUG_PRINTF("[%s] ERROR: SD.begin() failed with CS pin %d!\n", _name, csPin);
      } else {
        DEBUG_PRINTF("[%s] SD card initialized (SPI) with CS pin %d\n", _name, csPin);
      }
#elif defined(WLED_USE_SD_MMC)
      if (!SD_MMC.begin()) {
        DEBUG_PRINTF("[%s] ERROR: SD_MMC.begin() failed!\n", _name);
      } else {
        DEBUG_PRINTF("[%s] SD card initialized (MMC)\n", _name);
      }
#endif

      // API endpointy
      server.on("/api/system/info", HTTP_GET, [this](AsyncWebServerRequest *request) {
        String json = buildSystemInfoJSON();
        request->send(200, "application/json", json);
      });
      server.on("/api/system/status", HTTP_GET, [this](AsyncWebServerRequest *request) {
        String json = buildSystemStatusJSON();
        request->send(200, "application/json", json);
      });
      server.on("/api/fppd/multiSyncSystems", HTTP_GET, [this](AsyncWebServerRequest *request) {
        String json = buildFppdMultiSyncSystemsJSON();
        request->send(200, "application/json", json);
      });
      server.on("/api/playlists", HTTP_GET, [this](AsyncWebServerRequest *request) {
        request->send(200, "application/json", "{\"playlists\": []}");
      });
      server.on("/api/cape", HTTP_GET, [this](AsyncWebServerRequest *request) {
        request->send(200, "application/json", "{\"cape\": \"none\"}");
      });
      server.on("/api/proxies", HTTP_GET, [this](AsyncWebServerRequest *request) {
        request->send(200, "application/json", "{\"proxies\": []}");
      });
      server.on("/api/channel/output/co-pixelStrings", HTTP_GET, [this](AsyncWebServerRequest *request) {
        request->send(200, "application/json", "{\"pixelStrings\": []}");
      });
      server.on("/api/channel/output/channelOutputsJSON", HTTP_GET, [this](AsyncWebServerRequest *request) {
        request->send(200, "application/json", "{\"channels\": []}");
      });
      server.on("/api/channel/output/co-other", HTTP_GET, [this](AsyncWebServerRequest *request) {
        request->send(200, "application/json", "{\"other\": []}");
      });
      server.on("/proxy/", HTTP_GET, [this](AsyncWebServerRequest *request) {
        request->send(200, "application/json", "{}");
      });
      server.on("/fpp/discover", HTTP_GET, [this](AsyncWebServerRequest *request) {
        String json = buildSystemInfoJSON();
        request->send(200, "application/json", json);
      });

      // Endpoint pre upload súborov (raw dáta, application/octet-stream)
      server.on("/fpp", HTTP_POST,
        [this](AsyncWebServerRequest *request) {
          if (uploadStream != nullptr) {
            uploadStream->flush();
            delete uploadStream;
            uploadStream = nullptr;
          }
          if (currentUploadFile) {
            currentUploadFile.close();
          }
          currentUploadFileName = "";
          request->send(200, "text/plain", "Upload complete");
        },
        NULL,
        [this](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
          if (index == 0) {
            DEBUG_PRINTLN("[FPP] Received parameters:");
            for (uint8_t i = 0; i < request->params(); i++) {
              AsyncWebParameter* p = request->getParam(i);
              DEBUG_PRINTF("[FPP] Param %s = %s\n", p->name().c_str(), p->value().c_str());
            }
            String fileParam = "";
            if (request->hasParam("filename")) {
              fileParam = request->arg("filename");
            }
            DEBUG_PRINTF("[FPP] fileParam = %s\n", fileParam.c_str());
            if (fileParam != "") {
              currentUploadFileName = fileParam.startsWith("/") ? fileParam : "/" + fileParam;
            } else {
              currentUploadFileName = "/default.fseq";
            }
            DEBUG_PRINTF("[FPP] Using filename: %s\n", currentUploadFileName.c_str());
            if (SD.exists(currentUploadFileName.c_str())) {
              SD.remove(currentUploadFileName.c_str());
            }
            currentUploadFile = SD.open(currentUploadFileName.c_str(), FILE_WRITE);
            if (!currentUploadFile) {
              DEBUG_PRINTLN(F("[FPP] ERROR: Failed to open file for writing"));
              return;
            }
            uploadStream = new WriteBufferingStream(currentUploadFile, FILE_UPLOAD_BUFFER_SIZE);
            uploadStartTime = millis();
          }
          if (uploadStream != nullptr) {
            uploadStream->write(data, len);
          }
          if (index + len >= total) {
            if (uploadStream != nullptr) {
              uploadStream->flush();
              delete uploadStream;
              uploadStream = nullptr;
            }
            currentUploadFile.close();
            currentUploadFileName = "";
          }
        }
      );

      server.on("/fpp/connect", HTTP_GET, [this](AsyncWebServerRequest *request) {
        if (!request->hasArg("file")) {
          request->send(400, "text/plain", "Missing 'file' parameter");
          return;
        }
        String filepath = request->arg("file");
        if (!filepath.startsWith("/")) {
          filepath = "/" + filepath;
        }
        // Opravené: používa sa FSEQPlayer namiesto FSEQFile
        FSEQPlayer::loadRecording(filepath.c_str(), 0, strip.getLength());
        request->send(200, "text/plain", "FPP connect started: " + filepath);
      });
      server.on("/fpp/stop", HTTP_GET, [this](AsyncWebServerRequest *request) {
        // Opravené: FSEQPlayer namiesto FSEQFile
        FSEQPlayer::clearLastPlayback();
        realtimeLock(10, REALTIME_MODE_INACTIVE);
        request->send(200, "text/plain", "FPP connect stopped");
      });
      
      // Inicializácia UDP listenera pre synchronizáciu a ping
      if (!udpStarted && (WiFi.status() == WL_CONNECTED)) {
        if (udp.listenMulticast(multicastAddr, udpPort)) {
          udpStarted = true;
          udp.onPacket([this](AsyncUDPPacket packet) {
            processUdpPacket(packet);
          });
        }
      }
    }

    void loop() {
      if (!udpStarted && (WiFi.status() == WL_CONNECTED)) {
        if (udp.listenMulticast(multicastAddr, udpPort)) {
          udpStarted = true;
          udp.onPacket([this](AsyncUDPPacket packet) {
            processUdpPacket(packet);
          });
        }
      }
      // Spracovanie prehrávania FSEQ súborov pomocou FSEQPlayer
      FSEQPlayer::handlePlayRecording();
    }

    uint16_t getId() {
      return USERMOD_ID_SD_CARD;
    }
    void addToConfig(JsonObject &root) { }
    bool readFromConfig(JsonObject &root) { return true; }
};

const char UsermodFPP::_name[] PROGMEM = "FPP Connect";