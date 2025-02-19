#pragma once

// --- Macro Definitions ---
// These macros must be defined before any other includes.
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

#ifndef REALTIME_MODE_FSEQ
  #define REALTIME_MODE_FSEQ 3
#endif

#ifdef WLED_USE_SD_SPI
  #ifndef SPI_PORT_DEFINED
    inline SPIClass spiPort = SPIClass(VSPI);
    #define SPI_PORT_DEFINED
  #endif
#endif

#include "wled.h"
#ifdef WLED_USE_SD_SPI
  #include <SPI.h>
  #include <SD.h>
#elif defined(WLED_USE_SD_MMC)
  #include "SD_MMC.h"
#endif

// --- FSEQ Playback Logic ---
// This section defines the FSEQFile class which is responsible for reading
// and playing back FSEQ files.
#ifndef RECORDING_REPEAT_LOOP
  #define RECORDING_REPEAT_LOOP -1
#endif
#ifndef RECORDING_REPEAT_DEFAULT
  #define RECORDING_REPEAT_DEFAULT 0
#endif

class FSEQFile {
public:
  // Structure representing the header of an FSEQ file.
  struct file_header_t {
    uint8_t  identifier[4];       // 'PSEQ' or 'FSEQ'
    uint16_t channel_data_offset;   // Offset at which channel data begins
    uint8_t  minor_version;         // Minor version (e.g., 0, 1, 2)
    uint8_t  major_version;         // Major version (e.g., 2)
    uint16_t header_length;         // Length of the header
    uint32_t channel_count;         // Number of channels (e.g., for 300 RGB LEDs = 900)
    uint32_t frame_count;           // Number of frames in the sequence
    uint8_t  step_time;             // Time between frames in milliseconds
    uint8_t  flags;
  };

  // Public methods for FSEQ playback.
  static void handlePlayRecording();
  static void loadRecording(const char* filepath, uint16_t startLed, uint16_t stopLed);
  static void clearLastPlayback();

private:
  FSEQFile() {};  // Private constructor

  // Version constants.
  static const int V1FSEQ_MINOR_VERSION = 0;
  static const int V1FSEQ_MAJOR_VERSION = 1;
  static const int V2FSEQ_MINOR_VERSION = 0;
  static const int V2FSEQ_MAJOR_VERSION = 2;
  static const int FSEQ_DEFAULT_STEP_TIME = 50;

  // Static variables.
  static File     recordingFile;
  static uint8_t  colorChannels;
  static int32_t  recordingRepeats;
  static uint32_t now;
  static uint32_t next_time;
  static uint16_t playbackLedStart;
  static uint16_t playbackLedStop;
  static uint32_t frame;
  static uint16_t buffer_size;
  static file_header_t file_header;

  // Inline functions for reading multi-byte values.
  static inline uint32_t readUInt32() {
    char buffer[4];
    if (recordingFile.readBytes(buffer, 4) < 4) return 0;
    return (uint32_t)buffer[0] | ((uint32_t)buffer[1] << 8) | ((uint32_t)buffer[2] << 16) | ((uint32_t)buffer[3] << 24);
  }
  static inline uint32_t readUInt24() {
    char buffer[3];
    if (recordingFile.readBytes(buffer, 3) < 3) return 0;
    return (uint32_t)buffer[0] | ((uint32_t)buffer[1] << 8) | ((uint32_t)buffer[2] << 16);
  }
  static inline uint16_t readUInt16() {
    char buffer[2];
    if (recordingFile.readBytes(buffer, 2) < 2) return 0;
    return (uint16_t)buffer[0] | ((uint16_t)buffer[1] << 8);
  }
  static inline uint8_t readUInt8() {
    char buffer[1];
    if (recordingFile.readBytes(buffer, 1) < 1) return 0;
    return (uint8_t)buffer[0];
  }

  // Functions to check if a file exists on SD (or FS, though FS is not used here).
  static bool fileOnSD(const char* filepath) {
    uint8_t cardType = SD_ADAPTER.cardType();
    if(cardType == CARD_NONE) return false;
    return SD_ADAPTER.exists(filepath);
  }
  static bool fileOnFS(const char* filepath) {
    return false; // Only using SD
  }

  // Print the FSEQ header information for debugging.
  static void printHeaderInfo() {
    DEBUG_PRINTLN("FSEQ file_header:");
    DEBUG_PRINTF(" channel_data_offset = %d\n", file_header.channel_data_offset);
    DEBUG_PRINTF(" minor_version       = %d\n", file_header.minor_version);
    DEBUG_PRINTF(" major_version       = %d\n", file_header.major_version);
    DEBUG_PRINTF(" header_length       = %d\n", file_header.header_length);
    DEBUG_PRINTF(" channel_count       = %d\n", file_header.channel_count);
    DEBUG_PRINTF(" frame_count         = %d\n", file_header.frame_count);
    DEBUG_PRINTF(" step_time           = %d\n", file_header.step_time);
    DEBUG_PRINTF(" flags               = %d\n", file_header.flags);
  }

  // Process a single frame of data.
  static void processFrameData() {
    uint16_t packetLength = file_header.channel_count;
    uint16_t lastLed = min(playbackLedStop, uint16_t(playbackLedStart + (packetLength / 3)));
    char frame_data[buffer_size];
    CRGB* crgb = reinterpret_cast<CRGB*>(frame_data);
    uint16_t bytes_remaining = packetLength;
    uint16_t index = playbackLedStart;
    while (index < lastLed && bytes_remaining > 0) {
      uint16_t length = min(bytes_remaining, buffer_size);
      recordingFile.readBytes(frame_data, length);
      bytes_remaining -= length;
      for (uint16_t offset = 0; offset < length / 3; offset++) {
        setRealtimePixel(index, crgb[offset].r, crgb[offset].g, crgb[offset].b, 0);
        if (++index > lastLed) break;
      }
    }
    strip.show();
    realtimeLock(3000, REALTIME_MODE_FSEQ);
    next_time = now + file_header.step_time;
  }

  // Checks whether the playback has reached the end of the file.
  static bool stopBecauseAtTheEnd() {
    if (!recordingFile.available()) {
      if (recordingRepeats == RECORDING_REPEAT_LOOP) {
        recordingFile.seek(0);
      } else if (recordingRepeats > 0) {
        recordingFile.seek(0);
        recordingRepeats--;
        DEBUG_PRINTF("Repeat recording again for: %d\n", recordingRepeats);
      } else {
        DEBUG_PRINTLN("Finished playing recording, disabling realtime mode");
        realtimeLock(10, REALTIME_MODE_INACTIVE);
        recordingFile.close();
        clearLastPlayback();
        return true;
      }
    }
    return false;
  }

  // Advances to the next frame.
  static void playNextRecordingFrame() {
    if (stopBecauseAtTheEnd()) return;
    uint32_t offset = file_header.channel_count * frame++;
    offset += file_header.channel_data_offset; // Correct offset for channel data
    if (!recordingFile.seek(offset)) {
      if (recordingFile.position() != offset) {
        DEBUG_PRINTLN("Failed to seek to proper offset for channel data!");
        return;
      }
    }
    processFrameData();
  }
};

// Definition of static variables
File     FSEQFile::recordingFile;
uint8_t  FSEQFile::colorChannels = 3;
int32_t  FSEQFile::recordingRepeats = RECORDING_REPEAT_DEFAULT;
uint32_t FSEQFile::now = 0;
uint32_t FSEQFile::next_time = 0;
uint16_t FSEQFile::playbackLedStart = 0;
uint16_t FSEQFile::playbackLedStop = uint16_t(-1);
uint32_t FSEQFile::frame = 0;
uint16_t FSEQFile::buffer_size = 48;
FSEQFile::file_header_t FSEQFile::file_header;

// Public methods of FSEQFile
void FSEQFile::handlePlayRecording() {
  now = millis();
  if (realtimeMode != REALTIME_MODE_FSEQ) return;
  if (now < next_time) return;
  playNextRecordingFrame();
}

void FSEQFile::loadRecording(const char* filepath, uint16_t startLed, uint16_t stopLed) {
  if (recordingFile.available()) {
    clearLastPlayback();
    recordingFile.close();
  }
  playbackLedStart = startLed;
  playbackLedStop = stopLed;
  if (playbackLedStart == uint16_t(-1) || playbackLedStop == uint16_t(-1)) {
    Segment sg = strip.getSegment(-1);
    playbackLedStart = sg.start;
    playbackLedStop = sg.stop;
  }
  DEBUG_PRINTF("FSEQ load animation on LED %d to %d\n", playbackLedStart, playbackLedStop);
  if (fileOnSD(filepath)) {
    DEBUG_PRINTF("Read file from SD: %s\n", filepath);
    recordingFile = SD_ADAPTER.open(filepath, "rb");
  } else if (fileOnFS(filepath)) {
    DEBUG_PRINTF("Read file from FS: %s\n", filepath);
    // FS not used in this example
  } else {
    DEBUG_PRINTF("File %s not found (%s)\n", filepath, USED_STORAGE_FILESYSTEMS);
    return;
  }
  if ((uint64_t)recordingFile.available() < sizeof(file_header)) {
    DEBUG_PRINTF("Invalid file size: %d\n", recordingFile.available());
    recordingFile.close();
    return;
  }
  for (int i = 0; i < 4; i++) {
    file_header.identifier[i] = readUInt8();
  }
  file_header.channel_data_offset = readUInt16();
  file_header.minor_version = readUInt8();
  file_header.major_version = readUInt8();
  file_header.header_length = readUInt16();
  file_header.channel_count = readUInt32();
  file_header.frame_count = readUInt32();
  file_header.step_time = readUInt8();
  file_header.flags = readUInt8();
  printHeaderInfo();
  if (file_header.identifier[0] != 'P' || file_header.identifier[1] != 'S' ||
      file_header.identifier[2] != 'E' || file_header.identifier[3] != 'Q') {
    DEBUG_PRINTF("Error reading FSEQ file %s header, invalid identifier\n", filepath);
    recordingFile.close();
    return;
  }
  if (((uint64_t)file_header.channel_count * (uint64_t)file_header.frame_count) + file_header.header_length > UINT32_MAX) {
    DEBUG_PRINTF("Error reading FSEQ file %s header, file too long (max 4gb)\n", filepath);
    recordingFile.close();
    return;
  }
  if (file_header.step_time < 1) {
    DEBUG_PRINTF("Invalid step time %d, using default %d instead\n", file_header.step_time, FSEQ_DEFAULT_STEP_TIME);
    file_header.step_time = FSEQ_DEFAULT_STEP_TIME;
  }
  if (realtimeOverride == REALTIME_OVERRIDE_ONCE) {
    realtimeOverride = REALTIME_OVERRIDE_NONE;
  }
  recordingRepeats = RECORDING_REPEAT_DEFAULT;
  playNextRecordingFrame();
}

void FSEQFile::clearLastPlayback() {
  for (uint16_t i = playbackLedStart; i < playbackLedStop; i++) {
    setRealtimePixel(i, 0, 0, 0, 0);
  }
  frame = 0;
}

// --- Web UI for FSEQ Playback ---
// This section adds web endpoints for managing SD files and controlling FSEQ playback.
class UsermodFseq : public Usermod {
private:
  bool sdInitDone = false;

#ifdef WLED_USE_SD_SPI
  int8_t configPinSourceSelect = 5;
  int8_t configPinSourceClock   = 18;
  int8_t configPinPoci          = 19;
  int8_t configPinPico          = 23;

  void init_SD_SPI() {
    if(sdInitDone) return;
    PinManagerPinType pins[4] = {
      { configPinSourceSelect, true },
      { configPinSourceClock, true },
      { configPinPoci, false },
      { configPinPico, true }
    };
    if (!PinManager::allocateMultiplePins(pins, 4, PinOwner::UM_SdCard)) {
      DEBUG_PRINTF("[%s] SPI pin allocation failed!\n", _name);
      return;
    }
    spiPort.begin(configPinSourceClock, configPinPoci, configPinPico, configPinSourceSelect);
    if(!SD_ADAPTER.begin(configPinSourceSelect, spiPort)) {
      DEBUG_PRINTF("[%s] SPI begin failed!\n", _name);
      return;
    }
    sdInitDone = true;
    DEBUG_PRINTF("[%s] SD SPI initialized\n", _name);
  }
  void deinit_SD_SPI() {
    if(!sdInitDone) return;
    SD_ADAPTER.end();
    PinManager::deallocatePin(configPinSourceSelect, PinOwner::UM_SdCard);
    PinManager::deallocatePin(configPinSourceClock,  PinOwner::UM_SdCard);
    PinManager::deallocatePin(configPinPoci,         PinOwner::UM_SdCard);
    PinManager::deallocatePin(configPinPico,         PinOwner::UM_SdCard);
    sdInitDone = false;
    DEBUG_PRINTF("[%s] SD SPI deinitalized\n", _name);
  }
  void reinit_SD_SPI() {
    deinit_SD_SPI();
    init_SD_SPI();
  }
#endif

#ifdef WLED_USE_SD_MMC
  void init_SD_MMC() {
    if(sdInitDone) return;
    if(SD_ADAPTER.begin()) {
      sdInitDone = true;
      DEBUG_PRINTF("[%s] SD MMC initialized\n", _name);
    } else {
      DEBUG_PRINTF("[%s] SD MMC begin failed!\n", _name);
    }
  }
#endif

  // Web UI: List files on SD card
  void listFiles(const char* dirname, String &result) {
    DEBUG_PRINTF("[%s] Listing directory: %s\n", _name, dirname);
    File root = SD_ADAPTER.open(dirname);
    if (!root) {
      result += "<li>Failed to open directory: " + String(dirname) + "</li>";
      return;
    }
    if (!root.isDirectory()){
      result += "<li>Not a directory: " + String(dirname) + "</li>";
      return;
    }
    File file = root.openNextFile();
    while (file) {
      result += "<li>" + String(file.name()) + " (" + String(file.size()) + " bytes) ";
      result += "<a href='/sd/delete?path=" + String(file.name()) + "'>Delete</a></li>";
      DEBUG_PRINTF("[%s] Found file: %s, size: %d bytes\n", _name, file.name(), file.size());
      file.close();
      file = root.openNextFile();
    }
    root.close();
  }

  // Web UI: Handle file uploads
  void handleUploadFile(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
    static File uploadFile;
    String path = filename;
    if (!filename.startsWith("/")) {
      path = "/" + filename;
    }
    if(index == 0) {
      DEBUG_PRINTF("[%s] Starting upload for file: %s\n", _name, path.c_str());
      uploadFile = SD_ADAPTER.open(path.c_str(), FILE_WRITE);
      if (!uploadFile) {
        DEBUG_PRINTF("[%s] Failed to open file for writing: %s\n", _name, path.c_str());
      } else {
        DEBUG_PRINTF("[%s] File opened successfully for writing: %s\n", _name, path.c_str());
      }
    }
    if(uploadFile) {
      size_t written = uploadFile.write(data, len);
      DEBUG_PRINTF("[%s] Writing %d bytes to file: %s (written: %d bytes)\n", _name, len, path.c_str(), written);
    } else {
      DEBUG_PRINTF("[%s] Cannot write, file not open: %s\n", _name, path.c_str());
    }
    if(final) {
      if(uploadFile) {
        uploadFile.close();
        DEBUG_PRINTF("[%s] Upload complete and file closed: %s\n", _name, path.c_str());
        if(SD_ADAPTER.exists(path.c_str())) {
          DEBUG_PRINTF("[%s] File exists on SD card: %s\n", _name, path.c_str());
        } else {
          DEBUG_PRINTF("[%s] File does NOT exist on SD card after upload: %s\n", _name, path.c_str());
        }
      } else {
        DEBUG_PRINTF("[%s] Upload complete but file was not open: %s\n", _name, path.c_str());
      }
    }
  }

public:
  static const char _name[];

  void setup() {
    DEBUG_PRINTF("[%s] Usermod loaded\n", _name);
#ifdef WLED_USE_SD_SPI
    init_SD_SPI();
#elif defined(WLED_USE_SD_MMC)
    init_SD_MMC();
#endif
    if(sdInitDone) {
      DEBUG_PRINTF("[%s] SD initialization successful.\n", _name);
    } else {
      DEBUG_PRINTF("[%s] SD initialization FAILED.\n", _name);
    }
    
    // --- Web Endpoints for SD & FSEQ Management ---
    
    // Endpoint: /sd/ui - Main menu for SD and FSEQ
    server.on("/sd/ui", HTTP_GET, [this](AsyncWebServerRequest *request) {
      String html = "<html><head><meta charset='utf-8'><title>SD & FSEQ Manager</title>";
      html += "<style>";
      html += "body { font-family: sans-serif; font-size: 24px; color: #00FF00; background-color: #000; margin: 0; padding: 20px; }";
      html += "h1 { margin-top: 0; }";
      html += "ul { list-style: none; padding: 0; margin: 0 0 20px 0; }";
      html += "li { margin-bottom: 10px; }";
      html += "a, button {";
      html += "  display: inline-block;";
      html += "  font-size: 24px;";
      html += "  color: #00FF00;";
      html += "  border: 2px solid #00FF00;";
      html += "  background-color: transparent;";
      html += "  padding: 10px 20px;";
      html += "  margin: 5px;";
      html += "  text-decoration: none;";
      html += "}";
      html += "a:hover, button:hover { background-color: #00FF00; color: #000; }";
      html += "</style></head><body>";
      html += "<h1>SD & FSEQ Manager</h1>";
      html += "<ul>";
      html += "<li><a href='/sd/list'>SD Files</a></li>";
      html += "<li><a href='/fseq/list'>FSEQ Files</a></li>";
      html += "</ul>";
      html += "<a href='/'>BACK</a>";
      html += "</body></html>";
      request->send(200, "text/html", html);
    });
    
    // Endpoint: /sd/list - SD File Manager
    server.on("/sd/list", HTTP_GET, [this](AsyncWebServerRequest *request) {
      DEBUG_PRINTF("[%s] /sd/list endpoint requested\n", _name);
      String html = "<html><head><meta charset='utf-8'><title>SD Card Files</title>";
      html += "<style>";
      html += "body { font-family: sans-serif; font-size: 24px; color: #00FF00; background-color: #000; margin: 0; padding: 20px; }";
      html += "h1 { margin-top: 0; }";
      html += "ul { list-style: none; margin: 0; padding: 0; }";
      html += "li { margin-bottom: 10px; }";
      // Common style for links and buttons
      html += "a, button { display: inline-block; font-size: 24px; color: #00FF00; border: 2px solid #00FF00; background-color: transparent; padding: 10px 20px; margin: 5px; text-decoration: none; }";
      html += "a:hover, button:hover { background-color: #00FF00; color: #000; }";
      // Delete button style: red border and text
      html += ".deleteLink { border-color: #FF0000; color: #FF0000; }";
      html += ".deleteLink:hover { background-color: #FF0000; color: #000; }";
      // Back link style: add a border and some padding
      html += ".backLink { border: 2px solid #00FF00; padding: 10px 20px; }";
      html += "</style></head><body>";
      html += "<h1>SD Card Files</h1><ul>";
    
      // List files on SD card (with inline deletion links)
      File root = SD_ADAPTER.open("/");
      if (root && root.isDirectory()) {
        File file = root.openNextFile();
        while(file) {
          String name = file.name();
          html += "<li>" + name + " (" + String(file.size()) + " bytes) ";
          html += "<a href='#' class='deleteLink' onclick=\"deleteFile('" + name + "'); return false;\">Delete</a>";
          html += "</li>";
          file.close();
          file = root.openNextFile();
        }
      }
      root.close();
    
      html += "</ul>";
      
      // Upload form with AJAX submission
      html += "<h2>Upload File</h2>";
      html += "<form id='uploadForm' enctype='multipart/form-data'>";
      html += "Select file: <input type='file' name='upload'><br><br>";
      html += "<input type='submit' value='Upload'>";
      html += "</form>";
      html += "<div id='uploadStatus'></div>";
    
      // Back link stays on the same window
      html += "<p><a href='/sd/ui'>Back</a></p>";
      
      // JavaScript to handle AJAX for file upload and deletion
      html += "<script>";
      html += "document.getElementById('uploadForm').addEventListener('submit', function(e) {";
      html += "  e.preventDefault();";
      html += "  var formData = new FormData(this);";
      html += "  document.getElementById('uploadStatus').innerText = 'Uploading...';";
      html += "  fetch('/sd/upload', { method: 'POST', body: formData })";
      html += "    .then(response => response.text())";
      html += "    .then(data => {";
      html += "      document.getElementById('uploadStatus').innerText = data;";
      html += "      setTimeout(function() { location.reload(); }, 1000);";  // Refresh list after 1 second
      html += "    })";
      html += "    .catch(err => {";
      html += "      document.getElementById('uploadStatus').innerText = 'Upload failed';";
      html += "    });";
      html += "});";
      html += "function deleteFile(filename) {";
      html += "  if (!confirm('Are you sure you want to delete ' + filename + '?')) return;";
      html += "  fetch('/sd/delete?path=' + encodeURIComponent(filename))";
      html += "    .then(response => response.text())";
      html += "    .then(data => { alert(data); location.reload(); })";
      html += "    .catch(err => { alert('Delete failed'); });";
      html += "}";
      html += "</script>";
      
      html += "</body></html>";
      request->send(200, "text/html", html);
    });
    
    // Endpoint: /sd/upload - Handle file uploads
    server.on("/sd/upload", HTTP_POST, [this](AsyncWebServerRequest *request) {
      DEBUG_PRINTF("[%s] /sd/upload HTTP_POST endpoint requested\n", _name);
      request->send(200, "text/plain", "Upload complete");
    }, [this](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
      handleUploadFile(request, filename, index, data, len, final);
    });
    
    // Endpoint: /sd/delete - Delete file on SD card
    server.on("/sd/delete", HTTP_GET, [this](AsyncWebServerRequest *request) {
      DEBUG_PRINTF("[%s] /sd/delete endpoint requested\n", _name);
      if (!request->hasArg("path")) {
        request->send(400, "text/plain", "Missing 'path' parameter");
        return;
      }
      String path = request->arg("path");
      

      if (!path.startsWith("/")) {
        path = "/" + path;
      }
    
      bool res = SD_ADAPTER.remove(path.c_str());
      DEBUG_PRINTF("[%s] Delete file request: %s, result: %d\n", _name, path.c_str(), res);
      
      String msg = res ? "File deleted" : "Delete failed";
      request->send(200, "text/plain", msg);
    });
    
    
    // Endpoint: /fseq/list - List FSEQ files with Play/Stop buttons
server.on("/fseq/list", HTTP_GET, [this](AsyncWebServerRequest *request) {
  DEBUG_PRINTF("[%s] /fseq/list endpoint requested\n", _name);
  String html = "<html><head><meta charset='utf-8'><title>FSEQ Files</title>";
  html += "<style>";
  html += "body { font-family: sans-serif; font-size: 24px; color: #00FF00; background-color: #000; margin: 0; padding: 20px; }";
  html += "h1 { margin-top: 0; }";
  html += "ul { list-style: none; margin: 0; padding: 0; }";
  html += "li { margin-bottom: 10px; }";
  html += "a, button { display: inline-block; font-size: 24px; color: #00FF00; border: 2px solid #00FF00; background-color: transparent; padding: 10px 20px; margin: 5px; text-decoration: none; }";
  html += "a:hover, button:hover { background-color: #00FF00; color: #000; }";
  html += "</style></head><body>";
  html += "<h1>FSEQ Files</h1><ul>";
  
  File root = SD_ADAPTER.open("/");
  if(root && root.isDirectory()){
    File file = root.openNextFile();
    while(file){
      String name = file.name();
      if(name.endsWith(".fseq") || name.endsWith(".FSEQ")){
        html += "<li>" + name + " ";
        html += "<button id='btn_" + name + "' onclick=\"toggleFseq('" + name + "')\">Play</button>";
        html += "</li>";
      }
      file.close();
      file = root.openNextFile();
    }
  }
  root.close();
  
  html += "</ul>";
  html += "<p><a href='/sd/ui' class='backLink'>BACK</a></p>";
  html += "<script>";
  html += "function toggleFseq(file){";
  html += "  var btn = document.getElementById('btn_' + file);";
  html += "  if(btn.innerText === 'Play'){";
  html += "    fetch('/fseq/start?file=' + encodeURIComponent(file))";
  html += "      .then(response => response.text())";
  html += "      .then(data => { btn.innerText = 'Stop'; });";
  html += "  } else {";
  html += "    fetch('/fseq/stop?file=' + encodeURIComponent(file))";
  html += "      .then(response => response.text())";
  html += "      .then(data => { btn.innerText = 'Play'; });";
  html += "  }";
  html += "}";
  html += "</script>";
  html += "</body></html>";
  request->send(200, "text/html", html);
});
    
    // Endpoint: /fseq/start - Start FSEQ playback
    server.on("/fseq/start", HTTP_GET, [this](AsyncWebServerRequest *request) {
      if (!request->hasArg("file")) {
        request->send(400, "text/plain", "Missing 'file' parameter");
        return;
      }
      String filepath = request->arg("file");
      if (!filepath.startsWith("/")) {
        filepath = "/" + filepath;
      }
      uint16_t startLed = 0;
      uint16_t stopLed  = uint16_t(-1);
      DEBUG_PRINTF("[%s] Starting FSEQ file: %s\n", _name, filepath.c_str());
      FSEQFile::loadRecording(filepath.c_str(), startLed, stopLed);
      request->send(200, "text/plain", "FSEQ started: " + filepath);
    });
    
    // Endpoint: /fseq/stop - Stop FSEQ playback
    server.on("/fseq/stop", HTTP_GET, [this](AsyncWebServerRequest *request) {
      FSEQFile::clearLastPlayback();
      realtimeLock(10, REALTIME_MODE_INACTIVE);
      DEBUG_PRINTF("[%s] FSEQ playback stopped\n", _name);
      request->send(200, "text/plain", "FSEQ stopped");
    });
  }

  void loop() {
    // Call the FSEQ playback handler periodically
    FSEQFile::handlePlayRecording();
  }

  uint16_t getId() {
    return USERMOD_ID_SD_CARD;  // Ensure you have a unique ID
  }
  void addToConfig(JsonObject &root) { }
  bool readFromConfig(JsonObject &root) { return true; }
};

const char UsermodFseq::_name[] PROGMEM = "SD & FSEQ Web";