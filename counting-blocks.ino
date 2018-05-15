/////////////////////////////////////////////////////////////
// #include
/////////////////////////////////////////////////////////////
#include <stdio.h>

#include <SPI.h>
#include <SD.h>

#include <Adafruit_VS1053.h>
#include <Adafruit_NeoPixel.h>

/////////////////////////////////////////////////////////////
// Constants
/////////////////////////////////////////////////////////////

bool DEBUG = true;
bool DEBUG_STARTUP_BEEP = false;

#define VOLUME      100   // 0 (loud) -> 10 (quiet)
#define BRIGHTNESS  64  // 0->255

/////////////////////////////////////////////////////////////
// Initialize NeoPixel
/////////////////////////////////////////////////////////////
#define PIN_ONBOARD_LED 0
#define PIN_LED_ARRAY 14
#define ledStrip_COUNT 2

Adafruit_NeoPixel ledStrip = Adafruit_NeoPixel(ledStrip_COUNT, PIN_LED_ARRAY);

/////////////////////////////////////////////////////////////
// Initialize AudioPlayer
/////////////////////////////////////////////////////////////
#define VS1053_RESET   -1     // VS1053 reset pin (not used!)
#define VS1053_CS      16     // VS1053 chip select pin (output)
#define VS1053_DCS     15     // VS1053 Data/command select pin (output)
#define CARDCS          2     // Card chip select pin
#define VS1053_DREQ     0     // VS1053 Data request, ideally an Interrupt pin

Adafruit_VS1053_FilePlayer audioPlayer =
  Adafruit_VS1053_FilePlayer(VS1053_RESET, VS1053_CS, VS1053_DCS, VS1053_DREQ, CARDCS);


/////////////////////////////////////////////////////////////
// LedAction
/////////////////////////////////////////////////////////////
struct LedAction {
  int arrayIndex;
  unsigned long startTime;
  unsigned long endTime;
};

/////////////////////////////////////////////////////////////
// Cycles
/////////////////////////////////////////////////////////////
LedAction testCycle[10][10] = {
  { {0, 2000, 3000}, {0, 4000, 5000} },   { {1, 3000, 4000}, {1, 6000, 6500}, {1, 7000, 7500}, {1, 8000, 8500} },
  { {2, 2000, 3000}, {0, 4000, 5000} },   { {3, 2000, 3000}, {0, 4000, 5000} },
  { {4, 2000, 3000}, {0, 4000, 5000} },   { {5, 2000, 3000}, {0, 4000, 5000} },
  { {6, 2000, 3000}, {0, 4000, 5000} },   { {7, 1234, 5324}, {0, 4000, 5000} },
  { {8, 2000, 3000}, {0, 4000, 5000} },   { {9, 2000, 3000}, {0, 4000, 5000} },
};


/////////////////////////////////////////////////////////////
// Setup
/////////////////////////////////////////////////////////////

void setup() {
  if (DEBUG) {
    Serial.begin(115200);
  }

  debug("\n----------------------------");
  debug("Setup Starting");
  debug("----------------------------\n");

  ledSetup();
  audioSetup();

  debug("\nActions Test:");
  debugLedAction(testCycle[0][0]);
  debugLedAction(testCycle[1][2]);
  debugLedAction(testCycle[7][0]);
  debugLedAction(testCycle[7][2]);
  
  debug("\n----------------------------");
  debug("Setup Completed Successfully");
  debug("----------------------------");
}

/////////////////////////////////////////////////////////////
// Loop
/////////////////////////////////////////////////////////////

void loop() {
  unsigned long currentMillis = millis();

  delay(10000);
}


/////////////////////////////////////////////////////////////
// Logic
/////////////////////////////////////////////////////////////

void playTrack(const char *trackName) {
  audioPlayer.startPlayingFile(trackName);
}


void updateStrip(unsigned long currentTime, unsigned long trackStart) {
  ledStrip.setPixelColor(0, 0, 255, 255);
  ledStrip.setPixelColor(1, 0, 0, 0);
  ledStrip.show();

  delay(1000);

  ledStrip.setPixelColor(0, 0, 0, 0);
  ledStrip.setPixelColor(1, 255, 0, 255);
  ledStrip.show();

  delay(1000);

  ledStrip.setPixelColor(1, 0, 0, 0);
  ledStrip.show();
}

void ledSetup() {
  ledStrip.begin();
  ledStrip.setBrightness(127); // 0->255
  ledStrip.show(); // Initialize all pixels to 'off'
}

void audioSetup() {
  // Wait for serial port to be opened, remove this line for 'standalone' operation
  if (DEBUG) {
    while (!Serial) {
      delay(1);
    }
  }

  // Init Audio
  if (audioPlayer.begin()) { // initialise the music player
    debug(F("VS1053 found"));
  } else {
    debug(F("Couldn't find VS1053, do you have the right pins defined?"));
    while (1);
  }

  // Init SD Card
  if (SD.begin(CARDCS)) {
    debug(F("SD OK!"));
  } else {
    debug(F("SD failed, or not present"));
    while (1);  // don't do anything more
  }

  // Setup the player instance
  audioPlayer.setVolume(VOLUME, VOLUME); // Set volume for left, right channels. lower numbers == louder volume!
  audioPlayer.useInterrupt(VS1053_FILEPLAYER_PIN_INT);  // DREQ int

  // Play startup Beep
  if (DEBUG_STARTUP_BEEP) {
    audioPlayer.sineTest(0x44, 500);    // Make a tone to indicate VS1053 is working
  }
}


void debug(String message) {
  if (DEBUG) {
    Serial.println(message);
  }
}

void debugInline(String message) {
  if (DEBUG) {
    Serial.print(message);
  }
}

void debugLedAction(LedAction action) {
  debug((String) action.arrayIndex + " | " + action.startTime + " -- " + action.endTime);
}


