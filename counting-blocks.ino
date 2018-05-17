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

#define VOLUME              100   // 0 (loud) -> 10 (quiet)
#define BRIGHTNESS          64    // 0->255
#define NUMBER_BLOCK_COUNT  10    
#define ACTION_LIMIT        200
#define SONG_DONE_DELAY     100000

/////////////////////////////////////////////////////////////
// Operating Structs
/////////////////////////////////////////////////////////////
struct LedSequence {
  int arrayIndex;
  unsigned long startTime;
  unsigned long endTime;
};

struct LedAction {
  int arrayIndex;
  unsigned long actionTime;
};

struct PlayerState {
  bool  isPlaying;
  int   actionCount;
  unsigned long startTime;
  
  int         onIndex;
  LedAction   onActions[ACTION_LIMIT];
  int         offIndex;
  LedAction   offActions[ACTION_LIMIT];
};

/////////////////////////////////////////////////////////////
// State Variables
/////////////////////////////////////////////////////////////
bool initialRunComplete = false;

PlayerState   _playerState;

/////////////////////////////////////////////////////////////
// Initialize NeoPixel
/////////////////////////////////////////////////////////////
#define PIN_ONBOARD_LED   0
#define PIN_LED_ARRAY     14
#define LED_COUNT         4

Adafruit_NeoPixel ledStrip = Adafruit_NeoPixel(LED_COUNT, PIN_LED_ARRAY);

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
// Cycles
/////////////////////////////////////////////////////////////
LedSequence testCycle[] = {
  {0, 2000, 3000}, {0, 4000, 5000},   
  {1, 3000, 4000}, {1, 6000, 6500}, {1, 7000, 7500}, {1, 8000, 8500},
  {2, 2000, 3000}, 
  {3, 2000, 3000},
  {4, 2000, 3000},
  {5, 2000, 3000},
  {6, 2000, 3000},
  {7, 1234, 5324},
  {8, 2000, 3000},
  {9, 2000, 3000},
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

  debug("\n----------------------------");
  debug("Setup Completed Successfully");
  debug("----------------------------");
}

void play(String filename, LedSequence sequences[], unsigned long currentTime) {
  loadPlayerWithSequence(_playerState, sequences);

  playTrack(filename);
  _playerState.startTime = currentTime;
  _playerState.isPlaying = true;
}

void loadPlayerWithSequence(PlayerState playerState, LedSequence sequences[]) {
  int actionCount = sizeof(sequences);
  if (actionCount > ACTION_LIMIT) {
    debug("\n\n----------------------");
    debug("WARNING: You have exceeded the total action limit, it's probably fine to bump up the ACTION_LIMIT constant");
    debug((String) actionCount + " > " + ACTION_LIMIT);
    debug("----------------------");

    actionCount = ACTION_LIMIT;
  }

  resetPlayer(playerState);
  playerState.actionCount = actionCount;
  
  for (int i=0; i<actionCount; i++) {
    playerState.onActions[i] = {sequences[i].arrayIndex, sequences[i].startTime};
    playerState.offActions[i] = {sequences[i].arrayIndex, sequences[i].endTime};
  }

  sortLedActions(playerState.onActions, actionCount);
  sortLedActions(playerState.offActions, actionCount);
}

void resetPlayer(PlayerState playerState) {
  playerState.isPlaying = false;
  playerState.startTime = 0;
  playerState.actionCount = 0;
  playerState.onIndex = 0;
  playerState.offIndex = 0;
}

void sortLedActions(LedAction actions[], int count) {
  for (int i = 1; i < count; i++) {
    for (int j = i; j > 0 && actions[j - 1].actionTime > actions[j].actionTime; j--) {
      LedAction tmp = actions[j];
      actions[j] = actions[j - 1];
      actions[j - 1] = tmp;
    }
  }
}

/////////////////////////////////////////////////////////////
// Loop
/////////////////////////////////////////////////////////////

void loop() {
  unsigned long currentMillis = millis();

  if (!_playerState.isPlaying) {
    play((String)"12RAP.mp3", testCycle, currentMillis); 
  }
  
  updateProgress(_playerState, currentMillis);
}


/////////////////////////////////////////////////////////////
// Logic
/////////////////////////////////////////////////////////////

void playTrack(String trackName) {
  audioPlayer.startPlayingFile(trackName.c_str());
}


void updateProgress(PlayerState playerState, unsigned long currentTime) {
  if (!playerState.isPlaying) {
    return;
  } else if (playerState.onIndex >= playerState.actionCount && playerState.offIndex >= playerState.actionCount) {
    playerState.isPlaying = false;
    delay(SONG_DONE_DELAY);
    return;
  }
  
  bool updateNeeded = false;
  unsigned long relativeTime = currentTime - playerState.startTime;
  
  while (playerState.onIndex < playerState.actionCount && playerState.onActions[playerState.onIndex].actionTime <= relativeTime) {
    int arrayIndex = playerState.onActions[playerState.onIndex].arrayIndex;
    ledStrip.setPixelColor(arrayIndex, 0, 255, 255);
    playerState.onIndex += 1;
    updateNeeded = true;
  }

  while (playerState.offIndex < playerState.actionCount && playerState.offActions[playerState.offIndex].actionTime <= relativeTime) {
    int arrayIndex = playerState.onActions[playerState.onIndex].arrayIndex;
    ledStrip.setPixelColor(arrayIndex, 0, 0, 0);
    playerState.offIndex += 1;
    updateNeeded = true;
  }

  if (updateNeeded) {
    ledStrip.show(); 
  } 
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
