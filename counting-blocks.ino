/////////////////////////////////////////////////////////////
// #include
/////////////////////////////////////////////////////////////

#include <SPI.h>
#include <SD.h>

#include <Adafruit_VS1053.h>
#include <Adafruit_NeoPixel.h>

/////////////////////////////////////////////////////////////
// Constants
/////////////////////////////////////////////////////////////
#define DEBUG_LOG           true
#define DEBUG_SILENCE       false
#define DEBUG_STARTUP_BEEP  true
#define DEBUG_STARTUP_FLASH true

#define VOLUME              100   // 0 (loud) -> 10 (quiet)
#define BRIGHTNESS          64    // 0->255
#define NUMBER_BLOCK_COUNT  12
#define PIXELS_PER_BLOCK    3
#define ACTION_LIMIT        200
#define SONG_DONE_DELAY     15000

#define PIN_ONBOARD_LED     0
#define PIN_LED_ARRAY       4
#define LED_COUNT           4

#define ARRAY_LENGTH(A) (sizeof(A) / sizeof(A[0]))

String NO_AUDIO_TRACK = "||";

/////////////////////////////////////////////////////////////
// Operating Structs
/////////////////////////////////////////////////////////////
struct LedSequence {
  int arrayIndex;
  unsigned long startTime;
  unsigned long endTime;
};

typedef LedSequence LedSequences[];

struct LedAction {
  int arrayIndex;
  unsigned long actionTime;
};

struct PlayerState {
  bool  isPlaying;  
  int   actionCount;
  unsigned long startTime;
  bool  loadingShown;
  
  int         onIndex;
  LedAction   onActions[ACTION_LIMIT];
  int         offIndex;
  LedAction   offActions[ACTION_LIMIT];
};

/////////////////////////////////////////////////////////////
// State Variables
/////////////////////////////////////////////////////////////

bool m_displayLoading = false;

PlayerState m_playerState = {false, 0, 0, false};

/////////////////////////////////////////////////////////////
// Initialize NeoPixel
/////////////////////////////////////////////////////////////

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
LedSequence loadingFlash[] = {
  {0,0,200}, {0,400,600}, {0,800,1000}
};

LedSequence seq12Rap[] = {
  {0, 23, 216}, {1, 223, 417}, {2, 419, 651}, {3, 651, 968}, 
  {4, 968, 1300}, {5, 1325, 1558}, {6, 1558, 1836}, {7, 1890, 2106},
  {8, 2106, 2411}, {9, 2432, 2762}, {10, 2863, 3339}, {11, 3354, 3665}
};


// HACK: Inflate all pixels by the adjustment factor below
//       workaround so I can identify empty values from intentional zeros
#define MAGIC_LED_ADJUSTMENT     1000
int ledMapLogicalToPhysical[NUMBER_BLOCK_COUNT][PIXELS_PER_BLOCK] = {
  {1000},{1001},{1002},{1003},
  {1002},{1001},{1000},{1001},
  {1002},{1003},{1002},{1001}
};

/////////////////////////////////////////////////////////////
// Setup
/////////////////////////////////////////////////////////////

void setup() {
  if (DEBUG_LOG) {
    Serial.begin(115200);
  }

  debug("\n----------------------------");
  debug("Setup Starting");
  debug("----------------------------\n");

  ledSetup();
  if (!DEBUG_SILENCE) {
    audioSetup();
  }

  // Setup for an initial loading flash
  if (DEBUG_STARTUP_FLASH) { m_displayLoading = true; }

  debug("\n----------------------------");
  debug("Setup Completed Successfully");
  debug("----------------------------");
}

/////////////////////////////////////////////////////////////
// Loop
/////////////////////////////////////////////////////////////

void loop() {
  unsigned long currentMillis = millis();

  if (!m_playerState.isPlaying) {
    if (m_displayLoading) {
      play(NO_AUDIO_TRACK, loadingFlash, ARRAY_LENGTH(loadingFlash), currentMillis);  
      m_displayLoading = false;
    } else {
      play("12RAP.MP3", seq12Rap, ARRAY_LENGTH(seq12Rap), currentMillis);
      if (DEBUG_STARTUP_FLASH) { m_displayLoading = true; }
    }
  }
  
  updateProgress(currentMillis);
}

/////////////////////////////////////////////////////////////
// Player Operations
/////////////////////////////////////////////////////////////

void play(String filename, LedSequence *sequences, int sequenceCount, unsigned long currentTime) {  
  loadPlayerWithSequence(sequences, sequenceCount);

  if (filename != NO_AUDIO_TRACK && !DEBUG_SILENCE) {
    playTrack(filename);    
  }

  m_playerState.startTime = currentTime;
  m_playerState.isPlaying = true;

  debug((String) "Playing " + filename + " with " + sequenceCount + " sequences\n");
}

void loadPlayerWithSequence(LedSequence sequences[], int actionCount) {
  if (actionCount > ACTION_LIMIT) {
    debug("\n\n----------------------");
    debug("WARNING: You have exceeded the total action limit, it's probably fine to bump up the ACTION_LIMIT constant");
    debug((String) actionCount + " > " + ACTION_LIMIT);
    debug("----------------------");

    actionCount = ACTION_LIMIT;
  }

  resetPlayer();
  m_playerState.actionCount = actionCount;
    
  for (int i=0; i<actionCount; i++) {
    m_playerState.onActions[i] = {sequences[i].arrayIndex, sequences[i].startTime};
    m_playerState.offActions[i] = {sequences[i].arrayIndex, sequences[i].endTime};
  }

  sortLedActions(m_playerState.onActions, actionCount);
  sortLedActions(m_playerState.offActions, actionCount);
}

void resetPlayer() {
  m_playerState.isPlaying = false;
  m_playerState.startTime = 0;
  m_playerState.actionCount = 0;
  m_playerState.onIndex = 0;
  m_playerState.offIndex = 0;
  m_playerState.loadingShown = false;
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
// Logic
/////////////////////////////////////////////////////////////

void playTrack(String trackName) {
 audioPlayer.startPlayingFile(trackName.c_str());
}


void updateProgress(unsigned long currentTime) {
  if (!m_playerState.isPlaying || m_playerState.actionCount == 0) {
    return;
  } else if (m_playerState.onIndex >= m_playerState.actionCount && m_playerState.offIndex >= m_playerState.actionCount) {
    m_playerState.isPlaying = false;
    debug((String)"Song finished. Pausing for: " + SONG_DONE_DELAY + "ms");
    if (m_displayLoading) {
      delay(SONG_DONE_DELAY);  
    }
    return;
  }
  
  bool updateNeeded = false;
  unsigned long relativeTime = currentTime - m_playerState.startTime;

  // Turn OFF LEDs
  while (m_playerState.offIndex < m_playerState.actionCount && m_playerState.offActions[m_playerState.offIndex].actionTime <= relativeTime) {
    int blockIndex = m_playerState.offActions[m_playerState.offIndex].arrayIndex;
    
    for (int i=0; i<PIXELS_PER_BLOCK; i++) {
      int pixelIndex = ledMapLogicalToPhysical[blockIndex][i] - MAGIC_LED_ADJUSTMENT;
      if (pixelIndex >= 0) {
        ledStrip.setPixelColor(pixelIndex, 0, 0, 0);
        debug((String) "OFF:   [" + blockIndex + "] " + pixelIndex);
      }
    }
    
    m_playerState.offIndex += 1;
    updateNeeded = true;
  }


  // Turn ON LEDs
  while (m_playerState.onIndex < m_playerState.actionCount && m_playerState.onActions[m_playerState.onIndex].actionTime <= relativeTime) {
    int blockIndex = m_playerState.onActions[m_playerState.onIndex].arrayIndex;
    
    for (int i=0; i<PIXELS_PER_BLOCK; i++) {
      int pixelIndex = ledMapLogicalToPhysical[blockIndex][i] - MAGIC_LED_ADJUSTMENT;
      if (pixelIndex >= 0) {
        ledStrip.setPixelColor(pixelIndex, 0, 64, 64);
        debug((String) "ON:    [" + blockIndex + "] " + pixelIndex);
      }
    }

    m_playerState.onIndex += 1;
    updateNeeded = true;
  }

  if (updateNeeded) {
    ledStrip.show(); 
    
    debug((String) "SHOWN: " + relativeTime + "ms");
    debug("...\n");
  } 
}

void ledSetup() {
  ledStrip.begin();
  ledStrip.setBrightness(127); // 0->255
  ledStrip.show(); // Initialize all pixels to 'off'
}

void audioSetup() {
  // Wait for serial port to be opened, remove this line for 'standalone' operation
  if (DEBUG_LOG) {
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
  if (DEBUG_LOG) {
    Serial.println(message);
  }
}

void debugInline(String message) {
  if (DEBUG_LOG) {
    Serial.print(message);
  }
}
