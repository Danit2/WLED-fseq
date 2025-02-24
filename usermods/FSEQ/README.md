### Configure PlatformIO

Add the following configuration to your `platformio_override.ini` (or `platformio.ini`) to enable the necessary build flags and define the usermod:

```ini
[env:esp32dev_V4]
build_flags = 
  -D WLED_DEBUG
  -D WLED_USE_SD_SPI
  -D USERMOD_FPP
  -D USERMOD_FSEQ
  -I wled00/src/dependencies/json

### 3. Update the WLED Web Interface / optional

To integrate the new FSEQ functionality into the WLED UI, add a new button in your wled00/data/index.htm file. For example, insert the following button into the navigation area:

<!-- New button for SD & FSEQ Manager -->
			<button onclick="window.location.href=getURL('/sd/ui');"><i class="icons">&#xe0d2;</i><p class="tab-label">Fseq</p></button>



### 1. Modify `usermods_list.cpp`

Add the following lines to register the FSEQ usermod. Make sure that you **do not** include any conflicting modules (e.g. `usermod_sd_card.h`):

```cpp
#ifdef USERMOD_FSEQ
  #include "../usermods/FSEQ/usermod_fseq.h"
#endif

#ifdef USERMOD_FSEQ
  UsermodManager::add(new UsermodFseq());
#endif

delete or uncoment
  //#include "../usermods/sd_card/usermod_sd_card.h"

  //#ifdef SD_ADAPTER
  //UsermodManager::add(new UsermodSdCard());
  //#endif





Below is a list of the HTTP endpoints (commands) included in the code along with their descriptions and usage:
	•	GET /sd/ui
Description: Returns an HTML page with a user interface for managing SD files and FSEQ files.
Usage: Open this URL in a browser to view the SD & FSEQ Manager interface.
	•	GET /sd/list
Description: Displays an HTML page listing all files on the SD card. It also includes options to delete files and a form to upload new files.
Usage: Access this endpoint via a browser or an HTTP GET request to list the SD card files.
	•	POST /sd/upload
Description: Handles file uploads to the SD card. The file is sent as part of a multipart/form-data POST request.
Usage: Use a file upload form or an HTTP client to send a POST request with the file data to this endpoint.
	•	GET /sd/delete
Description: Deletes a specified file from the SD card. It requires a query parameter named path which specifies the file to delete.
Usage: Example: /sd/delete?path=/example.fseq to delete the file named example.fseq.
	•	GET /fseq/list
Description: Returns an HTML page listing all FSEQ files (files ending with .fseq or .FSEQ) available on the SD card. Each file is accompanied by a button to play it.
Usage: Navigate to this URL in a browser to view and interact with the FSEQ file list.
	•	GET /fseq/start
Description: Starts the playback of a selected FSEQ file. This endpoint requires a file query parameter for the file path and optionally a t parameter (in seconds) to specify a starting time offset.
Usage: Example: /fseq/start?file=/animation.fseq&t=10 starts the playback of animation.fseq from 10 seconds into the sequence.
	•	GET /fseq/stop
Description: Stops the current FSEQ playback and clears the active playback session.
Usage: Simply send an HTTP GET request to this endpoint to stop the playback.











# SD & FSEQ Usermod for WLED

This usermod adds support for playing FSEQ files from an SD card and provides a web interface for managing SD files and FSEQ playback. Follow the instructions below to install and use this usermod.

---

## Installation

### 1. Modify `usermods_list.cpp`

Add the following lines to register the FSEQ usermod. Make sure that you **do not** include any conflicting modules (e.g. `usermod_sd_card.h`):

```cpp
#ifdef USERMOD_FSEQ
  #include "../usermods/FSEQ/usermod_fseq.h"
#endif

#ifdef USERMOD_FSEQ
  UsermodManager::add(new UsermodFseq());
#endif

delete or uncoment
  //#include "../usermods/sd_card/usermod_sd_card.h"

  //#ifdef SD_ADAPTER
  //UsermodManager::add(new UsermodSdCard());
  //#endif


### 2. Configure PlatformIO

Add the following configuration to your platformio_override.ini (or platformio.ini) to enable the necessary build flags and define the usermod:


[env:esp32dev_V4]
build_flags = 
  -D WLED_DEBUG
  -D WLED_USE_SD_SPI
  -D WLED_PIN_SCK=18    ; CLK
  -D WLED_PIN_MISO=19   ; Data Out (POCI)
  -D WLED_PIN_MOSI=23   ; Data In (PICO)
  -D WLED_PIN_SS=5      ; Chip Select (CS)
  -D USERMOD_FSEQ
  -std=gnu++17



### 3. Update the WLED Web Interface

To integrate the new FSEQ functionality into the WLED UI, add a new button in your index.htm file. For example, insert the following button into the navigation area:

<!-- New button for SD & FSEQ Manager -->
<button onclick="window.location.href=getURL('/sd/ui');">
  <i class="icons">&#xe0d2;</i>
  <p class="tab-label">Fseq</p>
</button>


This button will take you to the SD & FSEQ Manager interface.



###Usage
	•	Web Interface:
Access the SD & FSEQ Manager by clicking the Fseq button in the main UI. The interface allows you to view, upload, and delete SD card files as well as control FSEQ playback.
	•	File Management:
The SD file manager displays all files on the SD card. You can upload new files via the provided form and delete files using the red “Delete” button. 
	•	FSEQ Playback:
Once an FSEQ file is loaded, the usermod will play the sequence on the LED strip. Use the provided web interface to start and stop playback.





