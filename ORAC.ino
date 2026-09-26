// ============================================================
// O.R.A.C. v145
// Activity LED + Wi-Fi status indicator build
// Based on v140.1 RAM diagnostic / v139 Wi-Fi scanner build.
// ============================================================

// Arduino IDE 1.8.19 compatibility: define custom types before any functions.
struct FibTile {
  int x;
  int y;
  int w;
  int h;
};

enum SettingsScreen {
  SETTINGS_MENU,
  SETTINGS_CLOCK,
  SETTINGS_DATE,
  SETTINGS_COUNTDOWN,
  SETTINGS_BRIGHTNESS,
  SETTINGS_SOUND,
  SETTINGS_FORMAT,
  SETTINGS_WIFI,
  SETTINGS_DIAGNOSTIC
};


// Wi-Fi settings forward declarations
void drawWifiSettings();
void drawWifiKeyboardScreen();
void handleWifiSettingsTouch(int x, int y);
void handleWifiKeyboardTouch(int x, int y);
void startWifiPasswordEntry();
void startWifiSsidEntry();
void loadOracWifiSettings();
void saveOracWifiSettings();
void drawWifiScanScreen();
void startWifiScan();
void handleWifiScanTouch(int x, int y);
void aiDrawKey(int x, int y, int w, int h, const char *label, uint16_t c);
void drawMemoryDiagnostics();
void handleMemoryDiagnosticsTouch(int x, int y);
void ensureLifeGraphics();
void releaseLifeGraphics();
void releasePKSprite();
void clearWifiScanResults();
bool PSRAMFound();
#include <TFT_eSPI.h>
#include <SPI.h>
#include <Wire.h>
#include <Preferences.h>
#include <SD.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>

// ============================================================
// ORAC CLOCK
// CYD ESP32-2432S028
//
// Fibonacci Clock
// Countdown
// Stopwatch
// Settings
// ============================================================

// ============================================================
// DISPLAY
// ============================================================

TFT_eSPI tft = TFT_eSPI();

#define TFT_BACKLIGHT 21

// ============================================================
// I2C / DS3231
// ============================================================

#define SDA_PIN 27
#define SCL_PIN 22
#define DS3231_ADDRESS 0x68

// ============================================================
// TOUCHSCREEN
// ============================================================

#define TOUCH_CLK  25
#define TOUCH_MISO 39
#define TOUCH_MOSI 32
#define TOUCH_CS   33
#define TOUCH_IRQ  36

SPIClass touchSPI(HSPI);

// Your actual calibration
#define RAW_X_MIN 643
#define RAW_X_MAX 3589
#define RAW_Y_MIN 352
#define RAW_Y_MAX 3506

// ============================================================
// COLOURS
// ============================================================

#define BLACK        TFT_BLACK
#define WHITE        TFT_WHITE

#define RED_CLOCK    0xF800
#define GREEN_CLOCK  0x07E0
#define BLUE_CLOCK   0x001F

#define PALE_YELLOW  0xFFE0

#define DARK_GREY    0x2104
#define MID_GREY     0x4208
#define LIGHT_GREY   0xBDF7

// Extra menu colours
#define ORANGE_CLOCK  0xFD20
#define CYAN_CLOCK    0x07FF
#define PURPLE_CLOCK  0x780F

// CYD onboard speaker / buzzer
#define SPEAKER_PIN   26

// ============================================================
// O.R.A.C. AI / WI-FI
// Personal prototype credentials supplied by the user.
// For a later production build, move the API key behind a proxy.
// ============================================================
#define ORAC_MAX_WIFI_NETWORKS 5
String oracWifiSSIDs[ORAC_MAX_WIFI_NETWORKS];
String oracWifiPasswords[ORAC_MAX_WIFI_NETWORKS];
int oracWifiCount = 0;

// Factory defaults used only on first boot after this Wi-Fi manager is added.
const char *ORAC_DEFAULT_WIFI_SSIDS[] = {
  "Gigaclear_4096",
  "UniFI_Weobley_High_Staff",
  "Andy\'s iPhone"
};
const char *ORAC_DEFAULT_WIFI_PASSWORDS[] = {
  "yf32fxmzc5",
  "R$ylan_Crowd3r!Harl4n",
  "ifhfigas285y"
};
const int ORAC_DEFAULT_WIFI_COUNT = 3;
// Wi-Fi is deliberately non-blocking. A failed network attempt must never
// stop the main O.R.A.C. loop from processing touch, animation or buttons.
const unsigned long ORAC_WIFI_TIMEOUT = 6000UL;
const unsigned long ORAC_WIFI_RETRY_INTERVAL = 15000UL;

bool oracWifiAttemptActive = false;
int oracWifiAttemptIndex = -1;
unsigned long oracWifiAttemptStarted = 0;

const char *OPENAI_API_KEY = "YOUR_OPENAI_API_KEY_HERE";
const char *OPENAI_MODEL = "gpt-5.6-luna";
const char *OPENAI_ENDPOINT = "https://api.openai.com/v1/responses";

bool oracWifiConnected = false;
int oracWifiIndex = -1;
unsigned long oracWifiLastAttempt = 0;
String oracWifiName = "OFFLINE";
String oracAiStatus = "NETWORK STANDBY";
bool oracAiBusy = false;
bool oracBusy = false;

// ============================================================
// SCREEN ENUM
// ============================================================

enum Screen {
  SCREEN_MENU,
  SCREEN_FIBONACCI,
  SCREEN_FLIP,
  SCREEN_MANDELBROT,
  SCREEN_TIME_TOOLS,
  SCREEN_CALCULATOR,
  SCREEN_CONVERTER,
  SCREEN_ORAC,
  SCREEN_AI,
  SCREEN_SETTINGS,
  SCREEN_SNAKE,
  SCREEN_TAMAGOTCHI,
  SCREEN_CELLULAR,
  SCREEN_PK_METER,
  SCREEN_PSYCHIC,
  SCREEN_RANDOM
};

void releaseOracScreenGraphics(Screen oldScreen);

Screen currentScreen = SCREEN_MENU;

// Brightness is declared here because the lid sensor functions below use it.
// Arduino IDE 1.8.19 can otherwise generate their prototypes before the
// later global declaration.
int brightness = 200;
// Speaker volume: 0 = silent, 100 = full DAC amplitude.
int oracVolume = 25;
// Footstep volume is a percentage of the master volume.
int oracFootstepVolume = 25;
bool displayIsOff = false;

// Display sleep/wake functions used by the manual DISPLAY OFF control.
void turnDisplayOff();
void wakeDisplay();
void setBrightness();
void drawCurrentScreen();
void oracBeep(uint16_t frequency, uint16_t duration);
void oracPlaySdRaw(const char *path, int sampleVolume = -1, bool applyMasterVolume = true);
void oracThrongletFootstep(int variant);
void oracThrongletPoot();
void oracPkRadarSweepSound();
void oracPkGhostSound();
void oracMedicineSound();
void lifeDrawButton(int x,int y,int w,int h,const char *text,uint16_t c);
void lifeDrawBar(int y,const char *label,int value,uint16_t colour);
void updatePetStage();
void drawTamagotchiScreen();
void drawAiScreen();
void drawAiKeyboardScreen();
void drawSoundSettings();
void handleSoundSettingsTouch(int x, int y);
void drawOracStatusLed(bool on);
void drawOracButtons();
void drawOracButton(int x,int y,int w,int h,const char *text,uint16_t c);
void scheduleOracGlitch();
void scheduleOracButtonShuffle();

void turnDisplayOff() {
  displayIsOff = true;
  tft.writecommand(0x28);   // Display Off
  tft.writecommand(0x10);   // Sleep In
  delay(20);
  analogWrite(TFT_BACKLIGHT, 0);
}

void wakeDisplay() {
  displayIsOff = false;
  tft.writecommand(0x11);   // Sleep Out
  delay(120);
  tft.writecommand(0x29);   // Display On
  setBrightness();
  drawCurrentScreen();
}

// ============================================================
// RIDICULOUS GAMES
// ============================================================

// Snake: Nokia-inspired grid game.
const int SNAKE_COLS = 20;
const int SNAKE_ROWS = 10;
const int SNAKE_CELL = 16;
const int SNAKE_MAX = SNAKE_COLS * SNAKE_ROWS;
const int SNAKE_ORIGIN_Y = 36;
const unsigned long SNAKE_START_SPEED = 180;

int snakeX[SNAKE_MAX];
int snakeY[SNAKE_MAX];
int snakeLength = 4;
int snakeDir = 0;       // 0 right, 1 down, 2 left, 3 up
int snakeNextDir = 0;
int snakeFoodX = 10;
int snakeFoodY = 5;
uint16_t snakeFoodColour = RED_CLOCK;
uint16_t snakeBodyColour = GREEN_CLOCK;
int snakeScore = 0;
int snakeHighScore = 0;
bool snakeRunning = false;
bool snakeGameOver = false;
bool snakeDemoMode = false;
unsigned long snakeLastMove = 0;
unsigned long snakeMoveInterval = SNAKE_START_SPEED;

// Random utility: dice roller + random number generator.
int randomMode = 0; // 0 dice, 1 number generator
int randomDiceSides = 6;
// Supported RPG dice: D4, D6, D8, D10, D12, D20, D100

int randomResult = 0;
int randomMin = 1;
int randomMax = 100;
int randomNumberMode = 0; // 0 range, 1 = 1-to-N
String randomNInput = "100";


// ============================================================
// TIME TOOLS
// ============================================================

enum TimeTool {
  TOOL_COUNTDOWN,
  TOOL_STOPWATCH
};

TimeTool currentTool = TOOL_COUNTDOWN;

// ============================================================
// SETTINGS SCREENS
// ============================================================

SettingsScreen currentSettings = SETTINGS_MENU;

// Wi-Fi settings editor / live connection status
bool wifiSettingsEditing = false;
bool wifiPasswordEntry = false;
int wifiKeyboardMode = 0;
String wifiEditSSID = "";
String wifiEditPassword = "";
int wifiDeleteIndex = -1;
bool wifiScanShowing = false;
// Manual Wi-Fi control: when true, ORAC stays disconnected until the user
// explicitly chooses CONNECT on a saved network.
bool oracWifiManualHold = false;
String oracWifiResult = "AUTO CONNECTION ENABLED";
int oracWifiManualIndex = -1;
char wifiScanSSIDs[7][33];
int wifiScanRSSI[7];
bool wifiScanOpen[7];
int wifiScanCount = 0;

// ============================================================
// FIBONACCI TILE
// IMPORTANT: declared before functions
// ============================================================

// Exact 320 x 200 Fibonacci rectangle
FibTile tile5  = {120, 0, 200, 200};
FibTile tile3  = {0, 80, 120, 120};
FibTile tile2  = {0, 0, 80, 80};
FibTile tile1a = {80, 0, 40, 40};
FibTile tile1b = {80, 40, 40, 40};

// ============================================================
// PERSISTENT SETTINGS
// ============================================================

Preferences prefs;

bool use24Hour = true;
bool countdownIncludeWeekends = true;

// Countdown target
int countdownYear   = 2026;
int countdownMonth  = 12;
int countdownDay    = 25;
int countdownHour   = 0;
int countdownMinute = 0;
int countdownSecond = 0;

// ============================================================
// EDIT VALUES
// ============================================================

int editYear;
int editMonth;
int editDay;
int editHour;
int editMinute;
int editSecond;

int editCountdownYear;
int editCountdownMonth;
int editCountdownDay;
int editCountdownHour;
int editCountdownMinute;
int editCountdownSecond;

// ============================================================
// STOPWATCH
// ============================================================

bool stopwatchRunning = false;

unsigned long stopwatchStart = 0;
unsigned long stopwatchAccumulated = 0;

// ============================================================
// LIVE DISPLAY TRACKING
// ============================================================

// Fibonacci
int lastFibHour = -1;
int lastFibMinuteUnit = -1;
int lastFibSecond = -1;
int lastFibFooterHour = -1;
int lastFibFooterMinuteUnit = -1;

// Countdown
long long lastCountdownRemaining = -1;

// Stopwatch
unsigned long lastStopwatchDisplay = 0;

// ============================================================
// DS3231 FUNCTIONS
// ============================================================

byte bcdToDec(byte val) {
  return ((val / 16 * 10) + (val % 16));
}

byte decToBcd(byte val) {
  return ((val / 10 * 16) + (val % 10));
}

// ------------------------------------------------------------

void readRTC(
  int &year,
  int &month,
  int &day,
  int &hour,
  int &minute,
  int &second
) {

  Wire.beginTransmission(DS3231_ADDRESS);
  Wire.write(0x00);
  Wire.endTransmission();

  Wire.requestFrom(DS3231_ADDRESS, 7);

  byte sec = Wire.read();
  byte min = Wire.read();
  byte hr  = Wire.read();

  Wire.read(); // day of week

  byte d  = Wire.read();
  byte mo = Wire.read();
  byte yr = Wire.read();

  second = bcdToDec(sec & 0x7F);
  minute = bcdToDec(min & 0x7F);

  if (hr & 0x40) {

    int h = bcdToDec(hr & 0x1F);

    bool pm = hr & 0x20;

    if (pm && h != 12)
      h += 12;

    if (!pm && h == 12)
      h = 0;

    hour = h;

  } else {

    hour = bcdToDec(hr & 0x3F);
  }

  day = bcdToDec(d);
  month = bcdToDec(mo & 0x1F);
  year = 2000 + bcdToDec(yr);
}

// ============================================================
// DAY OF WEEK
// ============================================================

int dayOfWeek(
  int y,
  int m,
  int d
) {

  if (m < 3) {
    m += 12;
    y--;
  }

  int k = y % 100;
  int j = y / 100;

  int h =
    (d +
     (13 * (m + 1)) / 5 +
     k +
     k / 4 +
     j / 4 +
     5 * j) % 7;

  return h;
}

// ============================================================
// WRITE RTC
// ============================================================

void writeRTC(
  int year,
  int month,
  int day,
  int hour,
  int minute,
  int second
) {

  int dow =
    dayOfWeek(
      year,
      month,
      day
    );

  Wire.beginTransmission(
    DS3231_ADDRESS
  );

  Wire.write(0x00);

  Wire.write(
    decToBcd(second)
  );

  Wire.write(
    decToBcd(minute)
  );

  Wire.write(
    decToBcd(hour)
  );

  Wire.write(
    decToBcd(dow)
  );

  Wire.write(
    decToBcd(day)
  );

  Wire.write(
    decToBcd(month)
  );

  Wire.write(
    decToBcd(year - 2000)
  );

  Wire.endTransmission();
}

// ============================================================
// TOUCH
// ============================================================

uint16_t readTouch(
  uint8_t command
) {

  uint16_t value;

  touchSPI.beginTransaction(
    SPISettings(
      2000000,
      MSBFIRST,
      SPI_MODE0
    )
  );

  digitalWrite(
    TOUCH_CS,
    LOW
  );

  touchSPI.transfer(command);

  value =
    touchSPI.transfer16(
      0x0000
    );

  digitalWrite(
    TOUCH_CS,
    HIGH
  );

  touchSPI.endTransaction();

  return value >> 3;
}

// ------------------------------------------------------------

bool getTouch(
  int &screenX,
  int &screenY
) {

  if (
    digitalRead(TOUCH_IRQ) == HIGH
  )
    return false;

  uint16_t rawX =
    readTouch(0xD0);

  uint16_t rawY =
    readTouch(0x90);

  screenX =
    map(
      rawY,
      RAW_Y_MIN,
      RAW_Y_MAX,
      20,
      300
    );

  screenY =
    map(
      rawX,
      RAW_X_MIN,
      RAW_X_MAX,
      20,
      220
    );

  screenX =
    constrain(
      screenX,
      0,
      319
    );

  screenY =
    constrain(
      screenY,
      0,
      239
    );

  return true;
}

// ============================================================
// SETTINGS STORAGE
// ============================================================

void loadOracWifiSettings() {
  Preferences wifiPrefs;
  wifiPrefs.begin("oracwifi", false);

  oracWifiCount = wifiPrefs.getInt("count", -1);
  if (oracWifiCount < 0 || oracWifiCount > ORAC_MAX_WIFI_NETWORKS) {
    oracWifiCount = min(ORAC_DEFAULT_WIFI_COUNT, ORAC_MAX_WIFI_NETWORKS);
    for (int i = 0; i < oracWifiCount; ++i) {
      oracWifiSSIDs[i] = ORAC_DEFAULT_WIFI_SSIDS[i];
      oracWifiPasswords[i] = ORAC_DEFAULT_WIFI_PASSWORDS[i];
      wifiPrefs.putString((String("ssid") + i).c_str(), oracWifiSSIDs[i]);
      wifiPrefs.putString((String("pass") + i).c_str(), oracWifiPasswords[i]);
    }
    wifiPrefs.putInt("count", oracWifiCount);
  } else {
    for (int i = 0; i < oracWifiCount; ++i) {
      oracWifiSSIDs[i] = wifiPrefs.getString((String("ssid") + i).c_str(), "");
      oracWifiPasswords[i] = wifiPrefs.getString((String("pass") + i).c_str(), "");
    }
  }
  wifiPrefs.end();
}

void saveOracWifiSettings() {
  Preferences wifiPrefs;
  wifiPrefs.begin("oracwifi", false);
  wifiPrefs.putInt("count", oracWifiCount);
  for (int i = 0; i < ORAC_MAX_WIFI_NETWORKS; ++i) {
    if (i < oracWifiCount) {
      wifiPrefs.putString((String("ssid") + i).c_str(), oracWifiSSIDs[i]);
      wifiPrefs.putString((String("pass") + i).c_str(), oracWifiPasswords[i]);
    } else {
      wifiPrefs.remove((String("ssid") + i).c_str());
      wifiPrefs.remove((String("pass") + i).c_str());
    }
  }
  wifiPrefs.end();
}

void loadSettings() {

  prefs.begin(
    "orac",
    false
  );

  brightness =
    prefs.getInt(
      "brightness",
      200
    );

  use24Hour =
    prefs.getBool(
      "24hour",
      true
    );

  countdownIncludeWeekends =
    prefs.getBool(
      "cdWeekends",
      true
    );

  countdownYear =
    prefs.getInt(
      "cdYear",
      2026
    );

  countdownMonth =
    prefs.getInt(
      "cdMonth",
      12
    );

  countdownDay =
    prefs.getInt(
      "cdDay",
      25
    );

  countdownHour =
    prefs.getInt(
      "cdHour",
      0
    );

  countdownMinute =
    prefs.getInt(
      "cdMin",
      0
    );

  countdownSecond =
    prefs.getInt(
      "cdSec",
      0
    );

  brightness =
    constrain(
      brightness,
      20,
      255
    );

  oracVolume =
    prefs.getInt(
      "volume",
      25
    );

  oracVolume =
    constrain(
      oracVolume,
      0,
      100
    );

  oracFootstepVolume =
    prefs.getInt(
      "stepVol",
      25
    );

  oracFootstepVolume =
    constrain(
      oracFootstepVolume,
      0,
      100
    );
}

// ------------------------------------------------------------

void saveSettings() {

  prefs.putInt(
    "brightness",
    brightness
  );

  prefs.putInt(
    "volume",
    oracVolume
  );

  prefs.putInt(
    "stepVol",
    oracFootstepVolume
  );

  prefs.putBool(
    "24hour",
    use24Hour
  );

  prefs.putBool(
    "cdWeekends",
    countdownIncludeWeekends
  );

  prefs.putInt(
    "cdYear",
    countdownYear
  );

  prefs.putInt(
    "cdMonth",
    countdownMonth
  );

  prefs.putInt(
    "cdDay",
    countdownDay
  );

  prefs.putInt(
    "cdHour",
    countdownHour
  );

  prefs.putInt(
    "cdMin",
    countdownMinute
  );

  prefs.putInt(
    "cdSec",
    countdownSecond
  );
}

// ------------------------------------------------------------

void setBrightness() {

  analogWrite(
    TFT_BACKLIGHT,
    brightness
  );
}

// ============================================================
// MAIN MENU STATUS / STARTUP SPLASH
// Declared before drawing helpers because Arduino IDE 1.8.19 can
// generate prototypes before later global declarations.
// ============================================================
bool menuRedLedOn = false;
unsigned long menuRedLedLast = 0;
uint16_t menuStatusLedColour = RED_CLOCK;
bool menuStatusLedColourKnown = false;
String menuStartupPhrase = "";

const char* menuStartupPhrases[] = {
  "NOW WITH 37% MORE COMPUTATION",
  "IT KNOWS WHAT YOU DID",
  "PLEASE REMAIN CALM",
  "PROBABLY WORKING AS INTENDED",
  "INTELLIGENCE NOT INCLUDED",
  "INSERT PURPOSE HERE",
  "CERTIFIED BY NOBODY",
  "NO USER MANUAL REQUIRED",
  "DO NOT ADJUST THE QUANTUMS",
  "MORE ADVANCED THAN NECESSARY",
  "POWERED BY QUESTIONABLE LOGIC",
  "ALL SYSTEMS SLIGHTLY IMPRESSIVE",
  "WARNING: EXCESSIVE COMPUTER",
  "THIS IS DEFINITELY A COMPUTER",
  "PLEASE IGNORE THE SMALL ERRORS",
  "VERSION: SOMEWHERE NEAR COMPLETE",
  "NOW FEATURING UNNECESSARY FEATURES",
  "COMPUTATIONAL PURPOSE: CLASSIFIED",
  "HELLO, ORGANIC LIFEFORM",
  "NOT A USELESS BOX. PROBABLY.",
  "THRONGLETS NOW INCLUDED!",
  "MORE BURT ADDED",
  "BURT HAS JOINED THE THRONG",
  "THRONGLET PROTOCOL ENABLED",
  "BURT IS NOW PART OF THE THRONG",
  "THRONGLET BEHAVIOUR: UNPREDICTABLE",
  "PLEASE FEED THE THRONGLET",
  "THRONG STATUS: GROWING",
  "BURT APPROVES THIS UPDATE",
  "CAUTION: THRONGLET ACTIVITY",
  "THE THRONG IS WATCHING",
  "THRONGLET CARE PACKAGE INSTALLED",
  "BORN TO THRONG"
};

void drawMenuButton(int x, int y, int w, int h, const char *text, uint16_t colour);
bool loadBurtMenuSprites();

// ============================================================
// BURT MENU CAMEO — SD CARD SPRITES
// v92/v131: Thronglet menu RAW assets are little-endian RGB565; byte-swap once on load for the TFT sprite path.
// The SD sprite filenames remain unchanged so this is a drop-in asset swap.
// v80: true transparency via opaque scanline runs, mirrored walking, clean footer.
// Burt is a little character, not a moving icon.  He gets a clean
// transparent sprite, a proper 8-frame walk cycle and occasional
// behaviours: look, wave, tap, silly face and leaving a little gift.
// ============================================================
#define SD_CS_PIN 5
#define BURT_MENU_W 40
#define BURT_MENU_H 40
#define BURT_MENU_FRAMES 8
#define BURT_MENU_TRANSPARENT 0x1FF8  // native value after RGB565 byte-swap
#define BURT_MENU_Y 199
#define BURT_MENU_STRIP_H 41
#define BURT_POO_COLOUR 0x79E0
#define BURT_MENU_WALK_MS 125
#define BURT_MENU_MAX 3

enum BurtMenuAction {
  BURT_WALKING,
  BURT_LOOKING,
  BURT_WAVING,
  BURT_TAPPING,
  BURT_FACE,
  BURT_POO
};

// Full-width off-screen buffer for the 40-pixel-high menu walkway.
// Rendering the complete strip in RAM and pushing it once per update removes
// the erase/redraw flicker that occurs when walkers overlap.
TFT_eSprite menuBurtStripSprite = TFT_eSprite(&tft);
bool menuBurtStripReady = false;
// Cache one complete 8-frame MENU_V2 gait in static DRAM.  This removes
// SD-card reads from the animation loop without exhausting the ESP32's
// internal DRAM.  8 x 40 x 40 x 2 = 25,600 bytes.  The three visible
// Thronglet variants are produced from this gait with the existing palette
// variation, so all walkers can animate without per-frame SD access.
// Heap-allocated walk cache: 8 x 40 x 40 x 2 = 25,600 bytes.
// Keeping this off .dram0.bss avoids linker overflow while still removing
// SD-card reads from the animation loop.
uint16_t *menuBurtWalkPixels = nullptr;
uint8_t menuBurtWalkLoaded[BURT_MENU_FRAMES];
int menuBurtLoadedFrame = -1;
int menuBurtLoadedFrameVariant = -1;
uint16_t menuBurtLookPixels[BURT_MENU_W * BURT_MENU_H];
uint16_t menuBurtActionPixels[3][BURT_MENU_W * BURT_MENU_H];
bool menuBurtLoaded = false;
int menuBurtLoadedVariant = 0;

// Three independent Thronglets share the same SD sprites in RAM.
// Each has its own timing, direction, speed and behaviour state, so their
// appearances are genuinely independent and they can naturally cross paths.
struct MenuThronglet {
  bool active;
  bool interactionDone;
  bool hasPoo;
  bool pooPending;
  BurtMenuAction action;
  int x;
  int frame;
  int direction;
  int step;
  int appearanceVariant;
  unsigned long walkMs;
  int pooX;
  int pooTargetX;
  unsigned long lastFrame;
  unsigned long nextAppearance;
  uint8_t animTick;
  uint8_t soundStep;
  unsigned long actionUntil;
};

MenuThronglet menuThronglets[BURT_MENU_MAX];

#define LIFE_SPRITE_W 48
#define LIFE_SPRITE_H 48
#define LIFE_SPRITE_COUNT 20
#define LIFE_SPRITE_TRANSPARENT 0xF81F
// Keep only ONE LIFE sprite in RAM at a time. 20 x 48 x 48 uint16_t
// sprites consume about 92 KB of DRAM and overflow the ESP32's DRAM.
// The sprites live on the SD card and are loaded on demand.
uint16_t lifeSpritePixels[LIFE_SPRITE_W * LIFE_SPRITE_H];
int lifeLoadedSpriteIndex = -1;
bool lifeSpritesLoaded = false;

// LIFE! object sprites: one 40x40 icon in RAM at a time.
#define LIFE_OBJECT_W 32
#define LIFE_OBJECT_H 32
#define LIFE_OBJECT_TRANSPARENT 0xF81F
uint16_t lifeObjectPixels[LIFE_OBJECT_W * LIFE_OBJECT_H];
String lifeLoadedObjectName = "";
bool lifeObjectsLoaded = false;

bool loadMenuWalkFrame(int variant, int frame) {
  // Allocate the complete gait cache on the heap when the menu graphics are needed.
  if (!menuBurtWalkPixels) {
    menuBurtWalkPixels = (uint16_t*)malloc((size_t)BURT_MENU_FRAMES * BURT_MENU_W * BURT_MENU_H * sizeof(uint16_t));
    if (!menuBurtWalkPixels) {
      Serial.println("BURT SD: unable to allocate walk cache");
      return false;
    }
    memset(menuBurtWalkLoaded, 0, sizeof(menuBurtWalkLoaded));
  }
  // Only one complete gait is cached to keep DRAM usage safe.  The existing
  // appearanceVariant palette adjustment still gives the three walkers
  // slightly different looks.
  if (frame < 0 || frame >= BURT_MENU_FRAMES) return false;
  if (menuBurtWalkLoaded[frame]) {
    menuBurtLoadedFrame = frame;
    menuBurtLoadedFrameVariant = 0;
    return true;
  }

  char path[80];
  // MENU_V2 variant 0 is the cached master gait.  Falling back to the legacy
  // MENU artwork keeps older SD cards usable.
  snprintf(path, sizeof(path), "/ORAC/ASSETS/BURT/MENU_V2/WALK0_%d.RAW", frame);
  File f = SD.open(path, FILE_READ);
  bool usingV2 = (bool)f;
  size_t need = sizeof(uint16_t) * BURT_MENU_W * BURT_MENU_H;
  if (!f || f.size() != need) {
    if (f) f.close();
    snprintf(path, sizeof(path), "/ORAC/ASSETS/BURT/MENU/WALK0_%d.RAW", frame);
    f = SD.open(path, FILE_READ);
    usingV2 = false;
  }
  if (!f || f.size() != need) {
    if (f) f.close();
    Serial.print("BURT SD: cannot load cached gait "); Serial.println(path);
    return false;
  }

  uint16_t *dst = menuBurtWalkPixels + ((size_t)frame * BURT_MENU_W * BURT_MENU_H);
  size_t got = f.read((uint8_t *)dst, need);
  f.close();
  if (got != need) return false;

  // MENU WALK RAW files are little-endian RGB565. Convert once when cached.
  for (int p = 0; p < BURT_MENU_W * BURT_MENU_H; p++) {
    uint16_t v = dst[p];
    dst[p] = (uint16_t)((v << 8) | (v >> 8));
  }

  menuBurtWalkLoaded[frame] = 1;
  menuBurtLoadedFrame = frame;
  menuBurtLoadedFrameVariant = 0;
  if (!usingV2) {
    Serial.print("BURT SD: using legacy MENU/WALK0_"); Serial.println(frame);
  }
  return true;
}

void renderMenuBurtStrip() {
  if (!menuBurtStripReady) return;

  // Build the entire 320x41 walkway off-screen, then push it as one frame.
  // This completely eliminates the visible erase/redraw flicker.
  menuBurtStripSprite.fillSprite(BLACK);

  for (int i = 0; i < BURT_MENU_MAX; i++) {
    MenuThronglet &b = menuThronglets[i];
    if (!b.active) continue;

    const uint16_t *src = nullptr;
    if (b.action == BURT_LOOKING || b.action == BURT_FACE) {
      src = menuBurtLookPixels;
    } else if (b.action == BURT_WAVING) {
      src = menuBurtActionPixels[0];
    } else if (b.action == BURT_TAPPING) {
      src = menuBurtActionPixels[1];
    } else if (b.action == BURT_WALKING || b.action == BURT_POO) {
      int fr = b.frame % BURT_MENU_FRAMES;
      if (menuBurtWalkLoaded[fr]) {
        src = menuBurtWalkPixels + ((size_t)fr * BURT_MENU_W * BURT_MENU_H);
      }
    }
    if (!src) continue;

    // Draw opaque pixels into the off-screen strip. The source assets use
    // magenta as their transparent colour. Mirror horizontally for leftward
    // walkers without touching the source buffers.
    for (int py = 0; py < BURT_MENU_H; py++) {
      for (int sx = 0; sx < BURT_MENU_W; sx++) {
        int localX = (b.direction > 0) ? sx : (BURT_MENU_W - 1 - sx);
        int screenX = b.x + sx;
        uint16_t c = src[py * BURT_MENU_W + localX];
        if (screenX >= 0 && screenX < 320 && c != BURT_MENU_TRANSPARENT) {
          // Subtle palette variation makes each passing Thronglet feel like
          // an individual without allocating another sprite buffer.
          if (b.appearanceVariant != 0) {
            int r=(c>>11)&31, g=(c>>5)&63, bl=c&31;
            bool warm=(r>15 && r>bl+4 && g>14);
            bool cloth=(bl>r && bl>12);
            if(warm && b.appearanceVariant==1) { r=min(31,r+1); g=min(63,g+2); }
            else if(warm && b.appearanceVariant==2) { r=max(0,r-1); g=max(0,g-1); }
            else if(cloth && b.appearanceVariant==2) { bl=min(31,bl+1); }
            c=(uint16_t)((r<<11)|(g<<5)|bl);
          }
          menuBurtStripSprite.drawPixel(screenX, py, c);
        }
      }
    }
  }

  // Persistent gifts are part of the same off-screen frame, so they cannot
  // flicker when a Thronglet walks past them.
  for (int i = 0; i < BURT_MENU_MAX; i++) {
    MenuThronglet &b = menuThronglets[i];
    if (!b.hasPoo || b.pooX < -10 || b.pooX > 329) continue;
    int px = b.pooX;
    menuBurtStripSprite.fillRect(px - 3, 34, 7, 3, BURT_POO_COLOUR);
    menuBurtStripSprite.fillRect(px - 2, 31, 5, 4, BURT_POO_COLOUR);
    menuBurtStripSprite.fillRect(px - 1, 29, 3, 3, BURT_POO_COLOUR);
    menuBurtStripSprite.fillRect(px, 27, 2, 3, BURT_POO_COLOUR);
  }

  menuBurtStripSprite.pushSprite(0, BURT_MENU_Y);
}

void drawMenuBurtStrip(bool clearOnly = false) {
  if (menuBurtStripReady) {
    menuBurtStripSprite.fillSprite(BLACK);
    if (!clearOnly) {
      renderMenuBurtStrip();
    } else {
      menuBurtStripSprite.pushSprite(0, BURT_MENU_Y);
    }
  } else {
    tft.fillRect(0, BURT_MENU_Y, 320, BURT_MENU_STRIP_H, BLACK);
  }
}


static uint16_t *oracMenuAssetPixels = nullptr;

void releaseMenuGraphicsForAI() {
  // TLS handshakes need a surprisingly large temporary heap allocation.
  // Free the 320x41 menu sprite (~26 KB) and menu asset buffer (~7 KB)
  // while O.R.A.C. is talking to OpenAI.
  if (menuBurtStripReady) {
    menuBurtStripSprite.deleteSprite();
    menuBurtStripReady = false;
  }
  if (oracMenuAssetPixels) {
    free(oracMenuAssetPixels);
    oracMenuAssetPixels = nullptr;
  }
  if (menuBurtWalkPixels) {
    free(menuBurtWalkPixels);
    menuBurtWalkPixels = nullptr;
    memset(menuBurtWalkLoaded, 0, sizeof(menuBurtWalkLoaded));
  }
  Serial.print("AI: free heap after menu release = ");
  Serial.println(ESP.getFreeHeap());
}

void ensureMenuGraphicsAfterAI() {
  // The AI path releases the heap walk cache to maximise contiguous memory for TLS.
  // Rebuild the menu graphics only when the menu is needed again.
  if (!menuBurtWalkPixels) {
    menuBurtLoaded = loadBurtMenuSprites();
    return;
  }
  if (!menuBurtStripReady) {
    menuBurtStripSprite.setColorDepth(16);
    if (menuBurtStripSprite.createSprite(320, BURT_MENU_STRIP_H)) {
      menuBurtStripReady = true;
    }
  }
}

void resetMenuBurtTimer() {
  ensureMenuGraphicsAfterAI();
  unsigned long now = millis();
  for (int i = 0; i < BURT_MENU_MAX; i++) {
    menuThronglets[i].active = false;
    menuThronglets[i].interactionDone = false;
    menuThronglets[i].hasPoo = false;
    menuThronglets[i].pooPending = false;
    menuThronglets[i].action = BURT_WALKING;
    menuThronglets[i].x = -BURT_MENU_W;
    menuThronglets[i].frame = 0;
    menuThronglets[i].direction = 1;
    menuThronglets[i].step = 2;
    menuThronglets[i].appearanceVariant = i % 3;
    menuThronglets[i].walkMs = BURT_MENU_WALK_MS;
    menuThronglets[i].pooX = -100;
    menuThronglets[i].pooTargetX = -100;
    menuThronglets[i].lastFrame = now;
    menuThronglets[i].animTick = 0;
    menuThronglets[i].soundStep = 0;
    // Stagger the three timers so seeing all three at once is uncommon.
    // Each still has a good chance to appear independently.
    menuThronglets[i].nextAppearance = now + random(30000UL + i * 9000UL, 75001UL + i * 12000UL);
    menuThronglets[i].actionUntil = 0;
  }
  drawMenuBurtStrip();
}

bool loadBurtMenuSprites() {
  if (!SD.begin(SD_CS_PIN)) {
    Serial.println("BURT SD: card not found");
    return false;
  }

  size_t need = sizeof(uint16_t) * BURT_MENU_W * BURT_MENU_H;
  // Preload one complete eight-frame MENU_V2 gait.  This removes SD access
  // from the animation loop while keeping DRAM usage within the ESP32 limit.
  menuBurtLoadedVariant = 0;
  menuBurtLoadedFrame = -1;
  menuBurtLoadedFrameVariant = -1;
  memset(menuBurtWalkLoaded, 0, sizeof(menuBurtWalkLoaded));
  for (int fr = 0; fr < BURT_MENU_FRAMES; fr++) {
    if (!loadMenuWalkFrame(0, fr)) {
      Serial.print("BURT SD: walk cache failed frame="); Serial.println(fr);
      return false;
    }
  }

  File f = SD.open("/ORAC/ASSETS/BURT/MENU/LOOK.RAW", FILE_READ);
  if (!f || f.size() != need) { if (f) f.close(); return false; }
  if (f.read((uint8_t *)menuBurtLookPixels, need) != need) { f.close(); return false; }
  f.close();
  for (int p = 0; p < BURT_MENU_W * BURT_MENU_H; p++) {
    uint16_t v = menuBurtLookPixels[p];
    menuBurtLookPixels[p] = (uint16_t)((v << 8) | (v >> 8));
  }
  const char* actions[3] = {"WAVE.RAW","TAP.RAW","FACE.RAW"};
  for (int i=0;i<3;i++) {
    char path[60]; snprintf(path,sizeof(path),"/ORAC/ASSETS/BURT/MENU/%s",actions[i]);
    f=SD.open(path,FILE_READ);
    if (!f || f.size()!=need) { if(f)f.close(); Serial.print("BURT SD: missing/invalid "); Serial.println(path); return false; }
    if (f.read((uint8_t *)menuBurtActionPixels[i],need)!=need) { f.close(); return false; }
    f.close();
    for (int p = 0; p < BURT_MENU_W * BURT_MENU_H; p++) {
      uint16_t v = menuBurtActionPixels[i][p];
      menuBurtActionPixels[i][p] = (uint16_t)((v << 8) | (v >> 8));
    }
  }

  menuBurtStripSprite.setColorDepth(16);
  if (!menuBurtStripSprite.createSprite(320, BURT_MENU_STRIP_H)) {
    Serial.println("BURT SD: unable to create 320x41 menu buffer");
    return false;
  }
  menuBurtStripReady = true;
  Serial.print("BURT SD: 40x40 menu V2 walk set "); Serial.print(menuBurtLoadedVariant); Serial.println(" + flicker-free strip buffer loaded");
  return true;
}

const char* lifeSpriteNames[LIFE_SPRITE_COUNT] = {
  "IDLE.RAW","WINK.RAW","HAPPY.RAW","SAD.RAW","ANGRY.RAW",
  "SURPRISED.RAW","SLEEP.RAW","EAT.RAW","DIRTY.RAW","SICK.RAW",
  "LOOK_LEFT.RAW","LOOK_RIGHT.RAW","WALK0.RAW","WALK1.RAW",
  "WALK2.RAW","WALK3.RAW","LOOK_UP.RAW","WAVE.RAW",
  "CELEBRATE.RAW","SHY.RAW"
};

bool loadBurtLifeSprites() {
  if (!SD.begin(SD_CS_PIN)) {
    Serial.println("THRONGLET SD: card not found");
    return false;
  }
  // Do NOT require every optional pose to be present. IDLE is the essential
  // character sprite; other poses are loaded only when requested. This also
  // prevents an old fallback character from silently appearing.
  const char *path="/ORAC/ASSETS/BURT/THRONGLET/IDLE.RAW";
  size_t need=sizeof(uint16_t)*LIFE_SPRITE_W*LIFE_SPRITE_H;
  File f=SD.open(path,FILE_READ);
  bool ok=(f && f.size()==need);
  if(f) f.close();
  lifeLoadedSpriteIndex=-1;
  if(!ok) { Serial.print("THRONGLET SD: missing/invalid "); Serial.println(path); return false; }
  Serial.println("THRONGLET SD: IDLE sprite found; optional poses loaded on demand");
  return true;
}

const char* lifeStageSpriteNames[6] = {
  "STAGE_EGG.RAW","STAGE_BABY.RAW","STAGE_CHILD.RAW","STAGE_TEEN.RAW","STAGE_ADULT.RAW","STAGE_OLD.RAW"
};
const char* lifeStageBlinkNames[6] = {
  "STAGE_EGG_BLINK.RAW","STAGE_BABY_BLINK.RAW","STAGE_CHILD_BLINK.RAW","STAGE_TEEN_BLINK.RAW","STAGE_ADULT_BLINK.RAW","STAGE_OLD_BLINK.RAW"
};

// Optional dedicated LIFE scene sprites. These are also 48x48 RGB565 RAW
// and are loaded into the same single 48x48 LIFE pixel buffer.
const char* lifeSleepSpriteNames[4] = {
  "SLEEP_BED_0.RAW","SLEEP_BED_1.RAW","SLEEP_BED_2.RAW","SLEEP_BED_3.RAW"
};
const char* lifeGhostSpriteNames[5] = {
  "GHOST_0.RAW","GHOST_1.RAW","GHOST_2.RAW","GHOST_3.RAW","GHOST_4.RAW"
};
uint16_t *lifeStagePixels = nullptr;
int lifeLoadedStageIndex=-1;
bool lifeLoadedStageBlink=false;
bool lifeStageSpritesLoaded=false;
String life2LoadedName = "";

// LIFE speech/reaction state.  Messages are intentionally short so they
// remain readable on the 320x240 display.
String lifeSpeech = "Hi!";
unsigned long lifeSpeechUntil = 0;
int lifeLastSpeechIndex = -1;
bool lifeBadFoodReaction = false;

bool loadBurtStageSprites() {
  if (!lifeStagePixels) {
    lifeStagePixels = (uint16_t*)malloc((size_t)LIFE_SPRITE_W * LIFE_SPRITE_H * sizeof(uint16_t));
    if (!lifeStagePixels) {
      Serial.println("THRONGLET SD: unable to allocate stage sprite buffer");
      lifeStageSpritesLoaded = false;
      return false;
    }
  }
  if(!SD.begin(SD_CS_PIN)) return false;
  size_t need=sizeof(uint16_t)*LIFE_SPRITE_W*LIFE_SPRITE_H;
  bool any=false;
  for(int i=0;i<6;i++) {
    char path[85]; snprintf(path,sizeof(path),"/ORAC/ASSETS/BURT/THRONGLET/%s",lifeStageSpriteNames[i]);
    File f=SD.open(path,FILE_READ);
    if(f && f.size()==need) any=true;
    else Serial.print("THRONGLET STAGE SD: missing/invalid "), Serial.println(path);
    if(f) f.close();
  }
  lifeLoadedStageIndex=-1;
  life2LoadedName="";
  lifeStageSpritesLoaded=any;
  return any;
}

bool loadLifeStageSpriteVariant(int index, bool blink) {
  if(index<0 || index>=6) return false;
  if(lifeLoadedStageIndex==index && lifeLoadedStageBlink==blink) return true;
  const char *name = blink ? lifeStageBlinkNames[index] : lifeStageSpriteNames[index];
  char path[90];
  snprintf(path,sizeof(path),"/ORAC/ASSETS/BURT/THRONGLET/%s",name);
  File f=SD.open(path,FILE_READ);
  size_t need=sizeof(uint16_t)*LIFE_SPRITE_W*LIFE_SPRITE_H;
  if(!f || f.size()!=need) {
    if(f) f.close();
    Serial.print("THRONGLET STAGE SD: cannot load "); Serial.println(path);
    return false;
  }
  size_t got=f.read((uint8_t*)lifeStagePixels,need);
  f.close();
  if(got!=need) { lifeLoadedStageIndex=-1; return false; }
  lifeLoadedStageIndex=index;
  lifeLoadedStageBlink=blink;
  return true;
}

bool loadLifeStageSprite(int index) {
  return loadLifeStageSpriteVariant(index,false);
}

bool loadLifeStageBlinkSprite(int index) {
  return loadLifeStageSpriteVariant(index,true);
}


// ============================================================
// LIFE! 2.0 — on-demand condition artwork
// All condition sprites live on SD and reuse the existing single 48x48
// stage buffer, so the expanded artwork pack adds essentially no permanent
// RAM cost.
// ============================================================
const char* life2StageNames[5] = {"BABY","CHILD","TEEN","ADULT","OLD"};

// Forward declarations for the LIFE! state variables, which are defined
// later in the sketch. Arduino IDE 1.8.19 requires these before the LIFE! 2.0
// helper functions that reference them.
extern int petHunger;
extern int petHappiness;
extern int petEnergy;
extern int petCleanliness;
extern int petHealth;
extern int petStage;
extern bool petSick;

bool loadLife2SpriteFile(const char *filename) {
  if(!filename || !filename[0] || !lifeStagePixels) return false;
  if(life2LoadedName == filename) return true;
  char path[120];
  snprintf(path,sizeof(path),"/ORAC/ASSETS/BURT/THRONGLET/LIFE2/%s",filename);
  File f=SD.open(path,FILE_READ);
  size_t need=sizeof(uint16_t)*LIFE_SPRITE_W*LIFE_SPRITE_H;
  if(!f || f.size()!=need) {
    if(f) f.close();
    return false;
  }
  size_t got=f.read((uint8_t*)lifeStagePixels,need);
  f.close();
  if(got!=need) { life2LoadedName=""; return false; }
  life2LoadedName=filename;
  lifeLoadedStageIndex=-1;
  return true;
}

const char* life2ConditionName() {
  if(petHealth<=8) return "DYING";
  if(petHealth<=20) return "CRITICAL";
  if(petSick && petHealth<=45) return "VERY_SICK";
  if(petSick) return "SICK";
  if(petCleanliness<=8) return "VERY_DIRTY";
  if(petCleanliness<=28) return "DIRTY";
  if(petHunger<=8) return "VERY_HUNGRY";
  if(petHunger<=25) return "HUNGRY";
  if(petEnergy<=20) return "TIRED";
  if(petHappiness>=88 && petEnergy>=65) return "HAPPY";
  return "HEALTHY";
}

bool loadLife2CurrentStateSprite(bool blink=false) {
  if(petStage<1 || petStage>5) return false;
  const char *stage=life2StageNames[petStage-1];
  const char *cond=life2ConditionName();
  char name[80];
  if(blink && strcmp(cond,"HEALTHY")==0) {
    snprintf(name,sizeof(name),"STAGE_%s_HEALTHY.RAW",stage);
  } else {
    snprintf(name,sizeof(name),"STAGE_%s_%s.RAW",stage,cond);
  }
  return loadLife2SpriteFile(name);
}

bool loadLifeSpecialSprite(const char *name) {
  if(!name || !name[0]) return false;
  char path[100];
  snprintf(path,sizeof(path),"/ORAC/ASSETS/BURT/THRONGLET/%s",name);
  File f=SD.open(path,FILE_READ);
  size_t need=sizeof(uint16_t)*LIFE_SPRITE_W*LIFE_SPRITE_H;
  if(!f || f.size()!=need) {
    if(f) f.close();
    Serial.print("THRONGLET SPECIAL SD: cannot load "); Serial.println(path);
    return false;
  }
  size_t got=f.read((uint8_t*)lifeStagePixels,need);
  f.close();
  if(got!=need) return false;
  lifeLoadedStageIndex=-1;
  lifeLoadedStageBlink=false;
  return true;
}

// Hatching uses a clean egg asset made from the supplied egg artwork with
// the pre-existing crack pixels removed.  The original cracked egg is then
// introduced later in the sequence.
bool loadLifeHatchEgg(bool cracked) {
  const char *name = cracked ? "STAGE_EGG.RAW" : "STAGE_EGG_CLEAN.RAW";
  char path[90];
  snprintf(path,sizeof(path),"/ORAC/ASSETS/BURT/THRONGLET/%s",name);
  File f=SD.open(path,FILE_READ);
  size_t need=sizeof(uint16_t)*LIFE_SPRITE_W*LIFE_SPRITE_H;
  if(!f || f.size()!=need) {
    if(f) f.close();
    // Fall back to the original egg if the optional clean asset has not
    // been copied yet.
    if(!cracked) return loadLifeStageSpriteVariant(0,false);
    return false;
  }
  size_t got=f.read((uint8_t*)lifeStagePixels,need);
  f.close();
  if(got!=need) return false;
  lifeLoadedStageIndex=0;
  lifeLoadedStageBlink=false;
  return true;
}

bool loadLifeSprite(int index) {
  if(index<0 || index>=LIFE_SPRITE_COUNT) return false;
  if(lifeLoadedSpriteIndex==index) return true;
  // SD was initialised during setup by loadBurtLifeSprites().
  // Do not reinitialise the card during animation.
  char path[70];
  snprintf(path,sizeof(path),"/ORAC/ASSETS/BURT/THRONGLET/%s",lifeSpriteNames[index]);
  File f=SD.open(path,FILE_READ);
  size_t need=sizeof(uint16_t)*LIFE_SPRITE_W*LIFE_SPRITE_H;
  if(!f || f.size()!=need) {
    if(f) f.close();
    Serial.print("THRONGLET SD: cannot load "); Serial.println(path);
    return false;
  }
  size_t got=f.read((uint8_t*)lifeSpritePixels,need);
  f.close();
  if(got!=need) {
    lifeLoadedSpriteIndex=-1;
    return false;
  }
  lifeLoadedSpriteIndex=index;
  return true;
}

const char* lifeObjectNames[] = {
  "APPLE.RAW","COOKIE.RAW","BANANA.RAW","FISH.RAW","CHEESE.RAW","TREAT.RAW",
  "BALL.RAW","TOY.RAW","OUTSIDE.RAW","MUSIC.RAW","DANCE.RAW","GUESS.RAW",
  "BATH.RAW","BRUSH.RAW","DRY.RAW"
};

bool loadLifeObject(const char *name) {
  if(!name || !name[0]) return false;
  if(lifeLoadedObjectName==name) return true;
  char path[90];
  snprintf(path,sizeof(path),"/ORAC/ASSETS/BURT/THRONGLET/OBJECTS/%s",name);
  File f=SD.open(path,FILE_READ);
  size_t need=sizeof(uint16_t)*LIFE_OBJECT_W*LIFE_OBJECT_H;
  if(!f || f.size()!=need) {
    if(f) f.close();
    Serial.print("LIFE OBJECT SD: cannot load "); Serial.println(path);
    return false;
  }
  size_t got=f.read((uint8_t*)lifeObjectPixels,need);
  f.close();
  if(got!=need) return false;
  lifeLoadedObjectName=String(name);
  lifeObjectsLoaded=true;
  return true;
}

void lifeDrawObject(const char *name,int cx,int cy) {
  if(!loadLifeObject(name)) return;
  const uint16_t *src=lifeObjectPixels;
  int x0=cx-LIFE_OBJECT_W/2, y0=cy-LIFE_OBJECT_H/2;
  for(int py=0;py<LIFE_OBJECT_H;py++) for(int px=0;px<LIFE_OBJECT_W;px++) {
    uint16_t c=src[py*LIFE_OBJECT_W+px];
    if(c!=LIFE_OBJECT_TRANSPARENT) tft.fillRect(x0+px,y0+py,1,1,c);
  }
}

void startBurtMenuAction(int i, int action, unsigned long now, unsigned long duration) {
  if (i < 0 || i >= BURT_MENU_MAX) return;
  menuThronglets[i].action = (BurtMenuAction)action;
  menuThronglets[i].actionUntil = now + duration;
  if (action == BURT_POO) menuThronglets[i].hasPoo = true;
}

// v147.6: keep the known-good TFT_eSprite path and add SD artwork fallback.
// The faster direct sprite-buffer method caused byte-order/corruption artefacts.
// Use fine 1-pixel movement updates while advancing the 8-frame gait every two
// movement updates, keeping travel smooth without needing extra sprite buffers.
void updateMenuBurt() {
  if (!menuBurtLoaded || currentScreen != SCREEN_MENU) return;
  unsigned long now = millis();
  bool redraw = false;

  for (int i = 0; i < BURT_MENU_MAX; i++) {
    MenuThronglet &b = menuThronglets[i];

    if (!b.active) {
      if ((long)(now - b.nextAppearance) >= 0) {
        // Keep one or two common, but make a full three-Thronglet scene rare.
        int activeCount = 0;
        for (int j = 0; j < BURT_MENU_MAX; j++) if (menuThronglets[j].active) activeCount++;
        if (activeCount >= 2 && random(0, 100) >= 20) {
          b.nextAppearance = now + random(30000UL, 75001UL);
          continue;
        }
        b.active = true;
        b.action = BURT_WALKING;
        b.direction = (random(0, 2) == 0) ? 1 : -1;
        b.step = 1;                              // 1-pixel movement for smoother travel
        b.walkMs = random(32UL, 37UL);       // ~27-31 movement updates/sec
        b.x = (b.direction > 0) ? -BURT_MENU_W : 320;
        b.frame = (b.direction > 0) ? 0 : BURT_MENU_FRAMES - 1;
        b.animTick = 0;
        b.soundStep = 0;
        b.lastFrame = now;
        b.interactionDone = false;
        b.hasPoo = false;
        b.pooX = -100;
        b.pooPending = (random(0, 100) < 18);
        b.pooTargetX = b.pooPending ? random(45, 276) : -100;
        redraw = true;
      }
    }

    if (!b.active || now - b.lastFrame < b.walkMs) continue;
    b.lastFrame = now;
    redraw = true;

    if (b.action != BURT_WALKING) {
      if ((long)(now - b.actionUntil) < 0) continue;
      b.action = BURT_WALKING;
    }

    if (b.pooPending && b.x >= b.pooTargetX - 2 && b.x <= b.pooTargetX + 2) {
      startBurtMenuAction(i, BURT_POO, now, 900);
      oracThrongletPoot();
      b.pooPending = false;
      b.pooX = b.x + (b.direction > 0 ? 9 : 31);
      b.hasPoo = true;
      continue;
    }

    b.x += b.direction * b.step;
    // Move every ~34 ms, but hold each gait pose for two movement updates.
    // This gives smooth 1-pixel travel while keeping the legs in a readable
    // ~14-15 fps walking cycle.
    b.animTick++;
    if (b.animTick >= 2) {
      b.animTick = 0;
      if (b.direction > 0) b.frame = (b.frame + 1) % BURT_MENU_FRAMES;
      else b.frame = (b.frame + BURT_MENU_FRAMES - 1) % BURT_MENU_FRAMES;

      // Tiny alternating footsteps at two contact points in the gait.
      if (b.frame == 1 || b.frame == 5) {
        oracThrongletFootstep(b.soundStep & 1);
        b.soundStep++;
      }
    }

    int lookPoint = 140;
    bool passed = (b.direction > 0) ? (b.x >= lookPoint) : (b.x <= lookPoint);
    if (passed && !b.interactionDone) {
      b.interactionDone = true;
      int event = random(0, 100);
      if (event < 48) startBurtMenuAction(i, BURT_LOOKING, now, random(3200UL, 6001UL));
      else if (event < 68) startBurtMenuAction(i, BURT_WAVING, now, 1500);
      else if (event < 80) startBurtMenuAction(i, BURT_TAPPING, now, 1200);
      else if (event < 88) startBurtMenuAction(i, BURT_FACE, now, 1500);
    }

    if ((b.direction > 0 && b.x > 320) || (b.direction < 0 && b.x < -BURT_MENU_W)) {
      b.active = false;
      b.action = BURT_WALKING;
      b.hasPoo = false;
      b.nextAppearance = now + random(45000UL, 105001UL);
    }
  }

  if (redraw) renderMenuBurtStrip();
}


void drawMenuStatusLeds(bool force = false);
void drawMenuStatusStrip(bool force = false);

// ============================================================
// DRAWING HELPERS
// ============================================================

void drawHeader(
  const char *title
) {

  tft.fillRect(
    0,
    0,
    320,
    35,
    BLACK
  );

  tft.setTextColor(
    PALE_YELLOW,
    BLACK
  );

  // Always restore the normal UI font before drawing a header.
  tft.setTextFont(1);
  tft.setTextSize(2);

  int w =
    tft.textWidth(title);

  tft.setCursor(
    (320 - w) / 2,
    9
  );

  tft.print(title);

  tft.drawFastHLine(
    10,
    34,
    300,
    DARK_GREY
  );

}

// ============================================================
// BUTTON
// ============================================================

void drawButton(
  int x,
  int y,
  int w,
  int h,
  const char *text,
  uint16_t colour
) {
  tft.fillRoundRect(
    x, y, w, h, 5, colour
  );

  tft.drawRoundRect(
    x, y, w, h, 5, LIGHT_GREY
  );

  // Dark text is much easier to read on the bright
  // yellow/green button faces.
  if (
    colour == DARK_GREY ||
    colour == TFT_RED
  ) {
    tft.setTextColor(
      WHITE,
      colour
    );
  } else {
    tft.setTextColor(
      BLACK,
      colour
    );
  }

  // Restore the normal, comfortable button size.
  tft.setTextFont(1);
  tft.setTextSize(2);

  int tw =
    tft.textWidth(text);

  tft.setCursor(
    x + (w - tw) / 2,
    y + (h - 16) / 2
  );

  tft.print(text);
}

// Small button used only for the Time Tools navigation row.
void drawTimeToolButton(
  int x,
  int y,
  int w,
  int h,
  const char *text,
  uint16_t colour
) {
  tft.fillRoundRect(
    x, y, w, h, 5, colour
  );

  tft.drawRoundRect(
    x, y, w, h, 5, LIGHT_GREY
  );

  if (colour == DARK_GREY)
    tft.setTextColor(WHITE, colour);
  else
    tft.setTextColor(BLACK, colour);

  tft.setTextFont(1);
  tft.setTextSize(1);

  int tw =
    tft.textWidth(text);

  tft.setCursor(
    x + (w - tw) / 2,
    y + (h - 8) / 2
  );

  tft.print(text);
}


// ============================================================
// HOME BUTTON
// ============================================================

void drawHomeButton() {

  drawButton(
    10,
    210,
    90,
    25,
    "HOME",
    DARK_GREY
  );
}

// ============================================================
// MAIN MENU
// ============================================================

void drawMenuButton(int x, int y, int w, int h, const char *text, uint16_t colour) {
  tft.fillRoundRect(x, y, w, h, 5, colour);
  tft.drawRoundRect(x, y, w, h, 5, LIGHT_GREY);

  // Darker menu colours get light text; pale colours keep black text.
  if (colour == PALE_YELLOW || colour == CYAN_CLOCK || colour == GREEN_CLOCK) {
    tft.setTextColor(BLACK, colour);
  } else {
    tft.setTextColor(WHITE, colour);
  }

  // Main menu labels are now deliberately large, but still clearly
  // smaller than the ORAC MULTITOOL title.
  tft.setTextFont(1);
  tft.setTextSize(2);
  int tw = tft.textWidth(text);
  tft.setCursor(x + (w - tw) / 2, y + (h - 16) / 2);
  tft.print(text);
}

void drawMenuStatusLeds(bool force) {
  unsigned long now = millis();

  // The little lamp is now an O.R.A.C. activity/status indicator.
  // Green = Wi-Fi connected, red = offline/connecting.
  // Idle: occasional random flash. Busy: much more active flashing.
  uint16_t wantedColour = oracWifiConnected ? GREEN_CLOCK : RED_CLOCK;
  bool colourChanged = (!menuStatusLedColourKnown || wantedColour != menuStatusLedColour);

  unsigned long interval = oracBusy ? random(120UL, 360UL) : random(1400UL, 5000UL);
  if (force || colourChanged || now - menuRedLedLast >= interval) {
    menuRedLedLast = now;
    menuRedLedOn = !menuRedLedOn;
    menuStatusLedColour = wantedColour;
    menuStatusLedColourKnown = true;

    tft.fillRect(2, 8, 10, 10, BLACK);
    tft.drawRoundRect(2, 8, 10, 10, 2, DARK_GREY);
    tft.fillCircle(7, 13, 3, menuRedLedOn ? menuStatusLedColour : DARK_GREY);
  }
}

// Small Wi-Fi strength indicator replacing the old battery icon.
// Four bars are shown when connected; offline/connecting is shown dimly.
void drawMenuWifiIndicator(bool force = false) {
  static bool lastConnected = false;
  static int lastBars = -1;
  static bool initialised = false;

  int bars = 0;
  if (oracWifiConnected) {
    int rssi = WiFi.RSSI();
    if (rssi >= -55) bars = 4;
    else if (rssi >= -67) bars = 3;
    else if (rssi >= -75) bars = 2;
    else if (rssi >= -85) bars = 1;
    else bars = 1;
  }

  if (!force && initialised && lastConnected == oracWifiConnected && lastBars == bars)
    return;

  initialised = true;
  lastConnected = oracWifiConnected;
  lastBars = bars;

  tft.fillRect(286, 0, 26, 12, BLACK);

  // Four stepped terminal-style bars.
  const int bx = 288;
  const int baseY = 10;
  const int widths = 4;
  const int heights[4] = {2, 4, 7, 10};
  for (int i = 0; i < 4; ++i) {
    int bh = heights[i];
    uint16_t c = (i < bars) ? GREEN_CLOCK : DARK_GREY;
    tft.fillRect(bx + i * 6, baseY - bh, widths, bh, c);
  }
}

// v121: the bottom of the menu is deliberately split into an AI
// feature strip and a compact system-control strip.  The Thronglet
// walker owns the 199-239 area, so menu controls never overlap it.
// ============================================================
// ORAC MENU ARTWORK — SD-BACKED PIXEL ASSETS
// ============================================================
// v127: three exact-resolution RGB565 assets.  The AI label is rendered
// as a classic bitmap terminal font, independent of TFT_eSPI fonts.


bool drawOracMenuAsset(const char *path, int x, int y, int w, int h) {
  const int maxPixels = 201 * 18;
  if (w <= 0 || h <= 0 || w * h > maxPixels) return false;
  if (!oracMenuAssetPixels) {
    oracMenuAssetPixels = (uint16_t*)malloc((size_t)maxPixels * sizeof(uint16_t));
    if (!oracMenuAssetPixels) {
      Serial.println("ORAC MENU: unable to allocate asset buffer");
      return false;
    }
  }
  File f = SD.open(path, FILE_READ);
  if (!f || f.size() != (size_t)(w * h * sizeof(uint16_t))) {
    if (f) f.close();
    Serial.print("ORAC MENU SD: missing/invalid ");
    Serial.println(path);
    return false;
  }
  size_t need=(size_t)w*h*sizeof(uint16_t);
  size_t got=f.read((uint8_t*)oracMenuAssetPixels,need);
  f.close();
  if(got!=need) return false;

  // The menu RAW files are stored little-endian RGB565.  The current
  // TFT_eSPI configuration expects the pixel bytes in the opposite order
  // for pushImage(), so swap each 16-bit pixel before sending this asset.
  // Do this only for the menu artwork; the rest of O.R.A.C.'s graphics
  // pipeline is deliberately left unchanged.
  for(int i=0;i<w*h;i++) {
    uint16_t v=oracMenuAssetPixels[i];
    oracMenuAssetPixels[i]=(uint16_t)((v<<8)|(v>>8));
  }

  // The supplied power symbol sits a few pixels right of centre in the
  // 43x18 artwork.  Re-centre it inside the button without changing the SD file.
  if (strstr(path, "DISPLAY_OFF.RAW") != nullptr && w == 43 && h == 18) {
    const int shift = 5;
    for (int y0 = 0; y0 < h; y0++) {
      for (int x0 = 0; x0 < w - shift; x0++) {
        oracMenuAssetPixels[y0 * w + x0] = oracMenuAssetPixels[y0 * w + x0 + shift];
      }
      for (int x0 = w - shift; x0 < w; x0++) {
        oracMenuAssetPixels[y0 * w + x0] = BLACK;
      }
    }
  }

  tft.pushImage(x,y,w,h,oracMenuAssetPixels);
  return true;
}

void drawMenuStatusStrip(bool force) {
  static bool lastConnected = false;
  static String lastName = "";
  static bool initialised = false;

  if (!force && initialised && lastConnected == oracWifiConnected && lastName == oracWifiName) return;
  initialised = true;
  lastConnected = oracWifiConnected;
  lastName = oracWifiName;

  tft.fillRect(11, 158, 298, 16, BLACK);
  tft.setTextFont(1);
  tft.setTextSize(1);
  tft.setTextColor(CYAN_CLOCK, BLACK);
  tft.setCursor(20, 162);
  tft.print("SYSTEM READY");
  tft.setTextColor(MID_GREY, BLACK);
  tft.setCursor(112, 162);
  tft.print("//");
  tft.setTextColor(oracWifiConnected ? GREEN_CLOCK : ORANGE_CLOCK, BLACK);
  tft.setCursor(129, 162);
  if (oracWifiConnected) {
    tft.print("WIFI ");
    String menuWifi = oracWifiName;
    if (menuWifi.length() > 15) menuWifi = menuWifi.substring(0, 15);
    tft.print(menuWifi);
  } else {
    tft.print("NETWORK STANDBY");
  }
  tft.setTextColor(DARK_GREY, BLACK);
  tft.setCursor(256, 162);
  tft.print("[ ORAC ]");
}

void drawMainMenu() {

  tft.fillScreen(BLACK);

  tft.setTextFont(4);
  tft.setTextSize(1);

  const char *title = "O.R.A.C. MULTITOOL";
  const uint16_t titleColours[] = {
    RED_CLOCK, BLUE_CLOCK, GREEN_CLOCK, PURPLE_CLOCK,
    ORANGE_CLOCK, PALE_YELLOW, CYAN_CLOCK, RED_CLOCK,
    BLUE_CLOCK, GREEN_CLOCK, PURPLE_CLOCK, ORANGE_CLOCK,
    CYAN_CLOCK, PALE_YELLOW
  };

  int totalWidth = tft.textWidth(title);
  if (totalWidth > 305) {
    tft.setTextFont(3);
    totalWidth = tft.textWidth(title);
  }
  int cursorX = (320 - totalWidth) / 2;

  for (int i = 0; title[i] != '\0'; i++) {
    char ch[2] = { title[i], '\0' };
    tft.setTextColor(titleColours[i % 14], BLACK);
    tft.setCursor(cursorX, 1);
    tft.print(ch);
    cursorX += tft.textWidth(ch);
  }

  tft.setTextFont(1);
  tft.setTextSize(1);
  tft.setTextColor(PALE_YELLOW, BLACK);
  int splashWidth = tft.textWidth(menuStartupPhrase.c_str());
  tft.setCursor((320 - splashWidth) / 2, 27);
  tft.print(menuStartupPhrase);

  drawMenuStatusLeds(true);
  drawMenuWifiIndicator(true);

  tft.drawFastHLine(10, 34, 300, DARK_GREY);

  // v121: slightly more compact tool rows leave a complete strip for
  // the full-width AI button, while the Thronglet still has the 199-239
  // walking strip at the bottom.
  const int y0 = 39;
  const int h = 18;
  const int gap = 1;
  const int row = h + gap;

  drawMenuButton(10,  y0 + row*0, 145, h, "FIBONACCI", RED_CLOCK);
  drawMenuButton(165, y0 + row*0, 145, h, "CLOCK", BLUE_CLOCK);
  drawMenuButton(10,  y0 + row*1, 145, h, "FRACTAL LAB", ORANGE_CLOCK);
  drawMenuButton(165, y0 + row*1, 145, h, "COUNTDOWN", PALE_YELLOW);
  drawMenuButton(10,  y0 + row*2, 145, h, "CALCULATOR", GREEN_CLOCK);
  drawMenuButton(165, y0 + row*2, 145, h, "CONVERTER", CYAN_CLOCK);
  drawMenuButton(10,  y0 + row*3, 145, h, "RANDOM", ORANGE_CLOCK);
  drawMenuButton(165, y0 + row*3, 145, h, "SNAKE", GREEN_CLOCK);
  drawMenuButton(10,  y0 + row*4, 145, h, "LIFE!", PALE_YELLOW);
  drawMenuButton(165, y0 + row*4, 145, h, "PK METER", ORANGE_CLOCK);
  drawMenuButton(10,  y0 + row*5, 145, h, "ESP TEST", PURPLE_CLOCK);
  drawMenuButton(165, y0 + row*5, 145, h, "O.R.A.C.", PURPLE_CLOCK);

  // v123: use the space above the bottom controls as a small
  // cyber-status panel. The controls remain above the dedicated
  // 199-239 Thronglet walking strip.
  tft.drawFastHLine(10, 154, 300, DARK_GREY);
  tft.drawRoundRect(10, 157, 300, 18, 4, DARK_GREY);
  drawMenuStatusStrip(true);

  // v127: SD-backed retro-computer control panel using the user's supplied retro icon/font artwork.
  // Exact-resolution artwork gives the tiny controls proper pixel graphics
  // and lets the AI button use a dedicated bitmap terminal font.
  const int menuCtrlY = 179;
  const int menuCtrlH = 18;

  // Draw the artwork first, then put a crisp terminal-green bezel over it.
  // This guarantees that the three controls have a clearly visible border
  // even when the supplied RAW artwork reaches the edge of its bitmap.
  drawOracMenuAsset("/ORAC/ASSETS/MENU/SETTINGS.RAW", 10, menuCtrlY, 48, menuCtrlH);
  drawOracMenuAsset("/ORAC/ASSETS/MENU/AI_TERMINAL.RAW", 61, menuCtrlY, 201, menuCtrlH);
  drawOracMenuAsset("/ORAC/ASSETS/MENU/DISPLAY_OFF.RAW", 267, menuCtrlY, 43, menuCtrlH);

  // Strong, continuous terminal-green bezels.  They are deliberately drawn
  // AFTER the bitmaps so the edges cannot disappear into the artwork.
  tft.drawRoundRect(10,  menuCtrlY, 48,  menuCtrlH, 4, GREEN_CLOCK);
  tft.drawRoundRect(61,  menuCtrlY, 201, menuCtrlH, 4, GREEN_CLOCK);
  tft.drawRoundRect(267, menuCtrlY, 43, menuCtrlH, 4, GREEN_CLOCK);

  // A second, darker inset line gives the panels a little depth without
  // making the 18-pixel-high controls look bulky.
  tft.drawRoundRect(12,  menuCtrlY + 2, 44,  menuCtrlH - 4, 3, DARK_GREY);
  tft.drawRoundRect(63,  menuCtrlY + 2, 197, menuCtrlH - 4, 3, DARK_GREY);
  tft.drawRoundRect(269, menuCtrlY + 2, 39,  menuCtrlH - 4, 3, DARK_GREY);

  // Re-outline the outer edge once more so the bezel remains the strongest
  // visual boundary after the inset lines are drawn.
  tft.drawRoundRect(10,  menuCtrlY, 48,  menuCtrlH, 4, GREEN_CLOCK);
  tft.drawRoundRect(61,  menuCtrlY, 201, menuCtrlH, 4, GREEN_CLOCK);
  tft.drawRoundRect(267, menuCtrlY, 43, menuCtrlH, 4, GREEN_CLOCK);

}

// ============================================================
// FIBONACCI SOLVER
// ============================================================

int fibonacciMask(
  int value
) {

  int values[5] = {
    5,
    3,
    2,
    1,
    1
  };

  int bestMask = 0;
  int bestCount = 99;

  for (
    int mask = 0;
    mask < 32;
    mask++
  ) {

    int total = 0;
    int count = 0;

    for (
      int i = 0;
      i < 5;
      i++
    ) {

      if (
        mask & (1 << i)
      ) {

        total += values[i];
        count++;
      }
    }

    if (
      total == value &&
      count < bestCount
    ) {

      bestMask =
        mask;

      bestCount =
        count;
    }
  }

  return bestMask;
}

// ============================================================
// FIBONACCI TILE
// ============================================================

void drawFibTile(
  FibTile tile,
  bool red,
  bool green
) {

  uint16_t colour =
    BLACK;

  if (
    red && green
  )
    colour =
      BLUE_CLOCK;

  else if (red)
    colour =
      RED_CLOCK;

  else if (green)
    colour =
      GREEN_CLOCK;

  tft.fillRect(
    tile.x + 3,
    tile.y + 3,
    tile.w - 6,
    tile.h - 6,
    colour
  );

  tft.drawRect(
    tile.x,
    tile.y,
    tile.w,
    tile.h,
    DARK_GREY
  );
}

// ============================================================
// DRAW FIBONACCI BLOCKS ONLY
// ============================================================

void drawFibonacciBlocks() {

  int year;
  int month;
  int day;
  int hour;
  int minute;
  int second;

  readRTC(
    year,
    month,
    day,
    hour,
    minute,
    second
  );

  int displayHour =
    hour % 12;

  if (
    displayHour == 0
  )
    displayHour = 12;

  int roundedMinute =
    ((minute + 2) / 5) * 5;

  if (
    roundedMinute == 60
  )
    roundedMinute = 0;

  int minuteUnits =
    roundedMinute / 5;

  int hourMask =
    fibonacciMask(
      displayHour
    );

  int minuteMask =
    fibonacciMask(
      minuteUnits
    );

  // Clear clock area
  tft.fillRect(
    0,
    0,
    320,
    200,
    BLACK
  );

  // 5
  drawFibTile(
    tile5,
    hourMask & 1,
    minuteMask & 1
  );

  // 3
  drawFibTile(
    tile3,
    hourMask & 2,
    minuteMask & 2
  );

  // 2
  drawFibTile(
    tile2,
    hourMask & 4,
    minuteMask & 4
  );

  // 1
  drawFibTile(
    tile1a,
    hourMask & 8,
    minuteMask & 8
  );

  // 1
  drawFibTile(
    tile1b,
    hourMask & 16,
    minuteMask & 16
  );

  lastFibHour =
    displayHour;

  lastFibMinuteUnit =
    minuteUnits;

  lastFibSecond =
    second;
}

// ============================================================
// FIBONACCI FOOTER
// ============================================================

void drawFibonacciFooter(
  bool force
) {

  int year;
  int month;
  int day;
  int hour;
  int minute;
  int second;

  readRTC(
    year,
    month,
    day,
    hour,
    minute,
    second
  );

  int displayHour = hour % 12;
  if (displayHour == 0)
    displayHour = 12;

  int roundedMinute = ((minute + 2) / 5) * 5;
  if (roundedMinute == 60)
    roundedMinute = 0;

  int minuteUnits = roundedMinute / 5;

  if (
    !force &&
    displayHour == lastFibFooterHour &&
    minuteUnits == lastFibFooterMinuteUnit
  )
    return;

  tft.fillRect(
    0,
    200,
    320,
    40,
    BLACK
  );

  // Keep the Fibonacci explanation where the digital clock used to be.
  // It shows exactly how the two displayed values are decomposed into
  // the available 5, 3, 2, 1, 1 Fibonacci blocks.
  tft.setTextFont(1);
  tft.setTextSize(1);
  tft.setTextColor(PALE_YELLOW, BLACK);
  tft.setCursor(8, 204);
  tft.print("FIBONACCI: 5 + 3 + 2 + 1 + 1");

  // Replace the raw values above with their actual selected block sums.
  int hourMask = fibonacciMask(displayHour);
  int minuteMask = fibonacciMask(minuteUnits);

  int hParts[5] = {5, 3, 2, 1, 1};
  int mParts[5] = {5, 3, 2, 1, 1};
  String hEquation = "";
  String mEquation = "";

  for (int i = 0; i < 5; i++) {
    if (hourMask & (1 << i)) {
      if (hEquation.length() > 0) hEquation += "+";
      hEquation += String(hParts[i]);
    }
    if (minuteMask & (1 << i)) {
      if (mEquation.length() > 0) mEquation += "+";
      mEquation += String(mParts[i]);
    }
  }

  tft.setTextColor(GREEN_CLOCK, BLACK);
  tft.setCursor(8, 219);
  tft.print("H");
  tft.print(displayHour);
  tft.print("=");
  tft.print(hEquation);
  tft.print("   M");
  tft.print(roundedMinute);
  tft.print("=");
  tft.print(mEquation);

  lastFibFooterHour = displayHour;
  lastFibFooterMinuteUnit = minuteUnits;
  lastFibSecond = second;
}

// ============================================================
// DRAW FIBONACCI SCREEN
// ============================================================

void drawFibonacciClock(
  bool force
) {

  int year;
  int month;
  int day;
  int hour;
  int minute;
  int second;

  readRTC(
    year,
    month,
    day,
    hour,
    minute,
    second
  );

  int displayHour =
    hour % 12;

  if (
    displayHour == 0
  )
    displayHour = 12;

  int roundedMinute =
    ((minute + 2) / 5) * 5;

  if (
    roundedMinute == 60
  )
    roundedMinute = 0;

  int minuteUnits =
    roundedMinute / 5;

  if (
    force ||
    displayHour != lastFibHour ||
    minuteUnits != lastFibMinuteUnit
  ) {

    drawFibonacciBlocks();

  }

  drawFibonacciFooter(
    force
  );
}

// ============================================================
// CIVIL DATE CALCULATIONS
// ============================================================
// ------------------------------------------------------------

long long secondsFromDate(
  int y,
  int m,
  int d,
  int h,
  int mi,
  int s
) {

  long days =
    daysFromCivil(
      y,
      m,
      d
    );

  return
    (long long)days *
    86400LL +
    h * 3600LL +
    mi * 60LL +
    s;
}

// ============================================================
// CALENDAR HELPERS
// ============================================================

// Civil-date conversion used by the countdown system.
long daysFromCivil(int y, int m, int d) {
  y -= m <= 2;
  int era = (y >= 0 ? y : y - 399) / 400;
  unsigned yoe = (unsigned)(y - era * 400);
  unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return era * 146097 + (int)doe - 719468;
}

// ============================================================
// CALENDAR HELPERS
// ============================================================

bool isLeapYear(int year) {
  return (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
}

int daysInMonth(int year, int month) {
  switch (month) {
    case 2:
      return isLeapYear(year) ? 29 : 28;
    case 4:
    case 6:
    case 9:
    case 11:
      return 30;
    default:
      return 31;
  }
}

void clampDate(int &year, int &month, int &day) {
  if (month < 1) month = 1;
  if (month > 12) month = 12;

  int maxDay = daysInMonth(year, month);

  if (day < 1) day = 1;
  if (day > maxDay) day = maxDay;
}

// ============================================================
// COUNTDOWN WEEKDAY HELPERS
// ============================================================

// Returns 0=Sunday, 1=Monday ... 6=Saturday.
int weekdayDayOfWeek(int y, int m, int d) {
  long z = daysFromCivil(y, m, d);
  int w = (int)((z + 4) % 7);
  if (w < 0) w += 7;
  return w;
}

bool isWeekday(int y, int m, int d) {
  int w = weekdayDayOfWeek(y, m, d);
  return w >= 1 && w <= 5;
}

void nextDate(int &y, int &m, int &d) {
  d++;
  if (d > daysInMonth(y, m)) {
    d = 1;
    m++;
    if (m > 12) {
      m = 1;
      y++;
    }
  }
}

// Count only Monday-Friday time between now and the countdown target.
// When weekends are excluded, a Friday-to-Monday countdown therefore
// contains only the working time, not Saturday/Sunday.
long long getWeekdayCountdownRemaining(
  int nowY, int nowM, int nowD, int nowH, int nowMin, int nowS
) {
  long long nowTotal = secondsFromDate(nowY, nowM, nowD, nowH, nowMin, nowS);
  long long targetTotal = secondsFromDate(
    countdownYear, countdownMonth, countdownDay,
    countdownHour, countdownMinute, countdownSecond
  );

  if (targetTotal <= nowTotal) return 0;

  int y = nowY;
  int m = nowM;
  int d = nowD;
  long long total = 0;

  // Remaining portion of today.
  if (isWeekday(y, m, d)) {
    total += 86400LL - (long long)(nowH * 3600 + nowMin * 60 + nowS);
  }

  nextDate(y, m, d);

  // Add complete weekdays until the target date.
  while (y != countdownYear || m != countdownMonth || d != countdownDay) {
    if (isWeekday(y, m, d)) total += 86400LL;
    nextDate(y, m, d);
  }

  // Add the target day's time if it is a weekday.
  if (isWeekday(countdownYear, countdownMonth, countdownDay)) {
    total += (long long)countdownHour * 3600LL
          + (long long)countdownMinute * 60LL
          + countdownSecond;
  }

  // The calculation above can include today's remainder plus target time
  // correctly for different dates. For a same-day target, handle it directly.
  if (nowY == countdownYear && nowM == countdownMonth && nowD == countdownDay) {
    if (isWeekday(nowY, nowM, nowD)) {
      return targetTotal - nowTotal;
    }
    return 0;
  }

  return total;
}

// ============================================================
// COUNTDOWN REMAINING
// ============================================================

long long getCountdownRemaining() {

  int year;
  int month;
  int day;
  int hour;
  int minute;
  int second;

  readRTC(
    year,
    month,
    day,
    hour,
    minute,
    second
  );

  long long now =
    secondsFromDate(
      year,
      month,
      day,
      hour,
      minute,
      second
    );

  long long target =
    secondsFromDate(
      countdownYear,
      countdownMonth,
      countdownDay,
      countdownHour,
      countdownMinute,
      countdownSecond
    );

  long long remaining;

  if (countdownIncludeWeekends) {
    remaining = target - now;
    if (remaining < 0) remaining = 0;
  } else {
    remaining = getWeekdayCountdownRemaining(
      year, month, day, hour, minute, second
    );
  }

  return remaining;
}

// ============================================================
// COUNTDOWN DRAW STATIC
// ============================================================

// ============================================================
// COUNTDOWN
// ============================================================

void drawCountdown(bool force) {
  static long long lastRemaining = -1;
  static long lastDays = -1;

  long long remaining = getCountdownRemaining();
  long long work = remaining;

  long days = work / 86400;
  work %= 86400;

  int hours = work / 3600;
  work %= 3600;

  int minutes = work / 60;
  int seconds = work % 60;

  char buffer[32];

  if (force) {
    // Main display area only. Bottom navigation stays untouched.
    tft.fillRect(0, 38, 320, 166, BLACK);

    tft.setTextColor(PALE_YELLOW, BLACK);
    tft.setTextFont(7);
    tft.setTextSize(1);

    sprintf(buffer, "%ld", days);
    int tw = tft.textWidth(buffer);
    tft.setCursor((320 - tw) / 2, 54);
    tft.print(buffer);

    tft.setTextFont(2);
    tft.setTextSize(1);

    const char *dayText = days == 1 ? "DAY" : "DAYS";
    tw = tft.textWidth(dayText);
    tft.setCursor((320 - tw) / 2, 104);
    tft.print(dayText);

    sprintf(buffer, "%02d:%02d:%02d", hours, minutes, seconds);

    tft.setTextFont(4);
    tft.setTextSize(1);

    tw = tft.textWidth(buffer);
    tft.setCursor((320 - tw) / 2, 126);
    tft.print(buffer);

    sprintf(
      buffer,
      "%02d/%02d/%04d  %02d:%02d:%02d",
      countdownDay,
      countdownMonth,
      countdownYear,
      countdownHour,
      countdownMinute,
      countdownSecond
    );

    tft.setTextFont(2);
    tft.setTextSize(1);
    tft.setTextColor(LIGHT_GREY, BLACK);

    tw = tft.textWidth(buffer);
    tft.setCursor((320 - tw) / 2, 166);
    tft.print(buffer);

    lastRemaining = remaining;
    lastDays = days;

    // Weekends toggle. This is deliberately compact so the existing
    // bottom navigation remains unchanged.
    drawTimeToolButton(
      78, 184, 164, 20,
      countdownIncludeWeekends ? "WEEKENDS: ON" : "WEEKENDS: OFF",
      countdownIncludeWeekends ? MID_GREY : GREEN_CLOCK
    );

    tft.setTextFont(1);
    tft.setTextSize(1);
    return;
  }

  if (remaining == lastRemaining)
    return;

  // Only erase the changing time.
  if (days != lastDays) {
    tft.fillRect(0, 48, 320, 58, BLACK);

    tft.setTextColor(PALE_YELLOW, BLACK);
    tft.setTextFont(7);
    tft.setTextSize(1);

    sprintf(buffer, "%ld", days);
    int tw = tft.textWidth(buffer);
    tft.setCursor((320 - tw) / 2, 54);
    tft.print(buffer);

    tft.fillRect(120, 102, 80, 20, BLACK);

    tft.setTextFont(2);
    tft.setTextSize(1);

    const char *dayText = days == 1 ? "DAY" : "DAYS";
    tw = tft.textWidth(dayText);
    tft.setCursor((320 - tw) / 2, 104);
    tft.print(dayText);

    lastDays = days;
  }

  tft.fillRect(45, 123, 230, 38, BLACK);

  sprintf(buffer, "%02d:%02d:%02d", hours, minutes, seconds);

  tft.setTextFont(4);
  tft.setTextSize(1);
  tft.setTextColor(PALE_YELLOW, BLACK);

  int tw = tft.textWidth(buffer);
  tft.setCursor((320 - tw) / 2, 126);
  tft.print(buffer);

  lastRemaining = remaining;

  tft.setTextFont(1);
  tft.setTextSize(1);
}

// ============================================================


void drawStopwatch(bool force) {
  static unsigned long lastDisplayed = 0xFFFFFFFFUL;

  unsigned long elapsed =
    stopwatchAccumulated;

  if (stopwatchRunning)
    elapsed += millis() - stopwatchStart;

  unsigned long totalHundredths =
    elapsed / 10;

  unsigned long totalSeconds =
    totalHundredths / 100;

  int hundredths =
    totalHundredths % 100;

  int seconds =
    totalSeconds % 60;

  int minutes =
    (totalSeconds / 60) % 60;

  int hours =
    totalSeconds / 3600;

  if (force) {

    tft.setFreeFont(&FreeSansBold9pt7b);
    tft.setTextColor(WHITE, BLACK);

    char buffer[20];
    sprintf(
      buffer,
      "%02d:%02d:%02d.%02d",
      hours,
      minutes,
      seconds,
      hundredths
    );

    int tw = tft.textWidth(buffer);
    tft.setCursor((320 - tw) / 2, 115);
    tft.print(buffer);

    // Controls
    drawButton(
      20, 140, 130, 45,
      stopwatchRunning ? "PAUSE" : "START",
      stopwatchRunning ? TFT_RED : TFT_GREEN
    );

    drawButton(
      170, 140, 130, 45,
      "RESET",
      DARK_GREY
    );

    lastDisplayed = 0xFFFFFFFFUL;
    return;
  }

  // Only redraw the stopwatch value when it changes.
  unsigned long displayValue =
    totalHundredths;

  if (displayValue != lastDisplayed) {
    tft.fillRect(35, 75, 250, 55, BLACK);

    tft.setFreeFont(&FreeSansBold9pt7b);
    tft.setTextColor(WHITE, BLACK);

    char buffer[20];
    sprintf(
      buffer,
      "%02d:%02d:%02d.%02d",
      hours,
      minutes,
      seconds,
      hundredths
    );

    int tw = tft.textWidth(buffer);
    tft.setCursor((320 - tw) / 2, 115);
    tft.print(buffer);

    lastDisplayed = displayValue;
  }
}

void drawTimeToolSelector() {
  drawTimeToolButton(
    3, 207, 78, 28,
    "COUNTDOWN",
    currentTool == TOOL_COUNTDOWN ? PALE_YELLOW : DARK_GREY
  );

  drawTimeToolButton(
    83, 207, 78, 28,
    "SET TARGET",
    DARK_GREY
  );

  drawTimeToolButton(
    163, 207, 78, 28,
    "STOPWATCH",
    currentTool == TOOL_STOPWATCH ? PALE_YELLOW : DARK_GREY
  );

  drawTimeToolButton(
    243, 207, 74, 28,
    currentTool == TOOL_STOPWATCH ? "BACK" : "HOME",
    DARK_GREY
  );
}


void drawTimeTools(
  bool force
) {

  if (force) {

    tft.fillScreen(
      BLACK
    );

    drawHeader(
      "TIME TOOLS"
    );

    drawTimeToolSelector();

    if (
      currentTool ==
      TOOL_COUNTDOWN
    ) {

      lastCountdownRemaining =
        -1;

    } else {

      lastStopwatchDisplay =
        0;
    }
  }

  if (
    currentTool ==
    TOOL_COUNTDOWN
  ) {

    drawCountdown(
      force
    );

  } else {

    drawStopwatch(
      force
    );
  }
}

// ============================================================
// SETTINGS MENU
// ============================================================

void drawSettingsMenu() {

  tft.fillScreen(BLACK);

  drawHeader(
    "SETTINGS"
  );

  drawButton(20, 42, 135, 38, "SET CLOCK", RED_CLOCK);
  drawButton(165, 42, 135, 38, "SET DATE", BLUE_CLOCK);

  drawButton(20, 87, 135, 38, "COUNTDOWN", PALE_YELLOW);
  drawButton(165, 87, 135, 38, "BRIGHTNESS", GREEN_CLOCK);

  drawButton(20, 132, 135, 38, "SOUND", ORANGE_CLOCK);
  drawButton(165, 132, 135, 38, "WI-FI", CYAN_CLOCK);

  drawButton(20, 177, 135, 28, "12 / 24", MID_GREY);
  drawButton(165, 177, 135, 28, "RAM / SYSTEM", PURPLE_CLOCK);

  drawHomeButton();
}

// ============================================================
// RAM / SYSTEM DIAGNOSTIC
// ============================================================

void drawMemoryDiagnostics() {
  tft.fillScreen(BLACK);
  drawHeader("RAM DIAGNOSTIC");

  tft.setTextFont(1);
  tft.setTextSize(1);
  tft.setTextColor(GREEN_CLOCK, BLACK);

  size_t freeHeap = ESP.getFreeHeap();
  size_t minHeap = ESP.getMinFreeHeap();
  size_t maxBlock = ESP.getMaxAllocHeap();
  size_t totalHeap = ESP.getHeapSize();

  tft.setCursor(12, 38); tft.print("TOTAL HEAP: "); tft.print(totalHeap); tft.print(" B");
  tft.setCursor(12, 58); tft.print("FREE HEAP:  "); tft.print(freeHeap); tft.print(" B");
  tft.setCursor(12, 78); tft.print("MIN FREE:   "); tft.print(minHeap); tft.print(" B");
  tft.setCursor(12, 98); tft.print("MAX BLOCK:  "); tft.print(maxBlock); tft.print(" B");

  tft.setTextColor(CYAN_CLOCK, BLACK);
  tft.setCursor(12, 122); tft.print("FLASH:      "); tft.print(ESP.getFlashChipSize()); tft.print(" B");
  tft.setCursor(12, 142); tft.print("SKETCH:     "); tft.print(ESP.getSketchSize()); tft.print(" B");
  tft.setCursor(12, 162); tft.print("FREE FLASH: "); tft.print(ESP.getFreeSketchSpace()); tft.print(" B");

  tft.setTextColor(PALE_YELLOW, BLACK);
  tft.setCursor(12, 184);
  tft.print(PSRAMFound() ? "PSRAM: PRESENT" : "PSRAM: NONE");

  tft.setTextColor(oracWifiConnected ? GREEN_CLOCK : ORANGE_CLOCK, BLACK);
  tft.setCursor(12, 199);
  if (oracWifiConnected) {
    tft.print("WIFI: ");
    String diagWifi = oracWifiName;
    if (diagWifi.length() > 28) diagWifi = diagWifi.substring(0, 28);
    tft.print(diagWifi);
  } else if (oracWifiAttemptActive) {
    tft.print("WIFI: TRY ");
    tft.print(oracWifiAttemptIndex + 1);
    tft.print("/");
    tft.print(oracWifiCount);
  } else {
    tft.print("WIFI: OFFLINE");
  }

  drawButton(10, 210, 120, 25, "REFRESH", GREEN_CLOCK);
  drawButton(140, 210, 120, 25, "SERIAL", CYAN_CLOCK);
  drawButton(270, 210, 40, 25, "X", DARK_GREY);
}

void handleMemoryDiagnosticsTouch(int x, int y) {
  if (y >= 210) {
    if (x < 135) {
      drawMemoryDiagnostics();
    } else if (x < 265) {
      Serial.println("========== ORAC MEMORY DIAGNOSTIC ==========");
      Serial.print("Total heap:      "); Serial.println(ESP.getHeapSize());
      Serial.print("Free heap:       "); Serial.println(ESP.getFreeHeap());
      Serial.print("Minimum free:    "); Serial.println(ESP.getMinFreeHeap());
      Serial.print("Maximum block:   "); Serial.println(ESP.getMaxAllocHeap());
      Serial.print("Flash size:      "); Serial.println(ESP.getFlashChipSize());
      Serial.print("Sketch size:     "); Serial.println(ESP.getSketchSize());
      Serial.print("Free sketch:     "); Serial.println(ESP.getFreeSketchSpace());
      Serial.print("Wi-Fi connected: "); Serial.println(oracWifiConnected ? "YES" : "NO");
      Serial.print("Wi-Fi SSID:      "); Serial.println(oracWifiName);
      Serial.print("Wi-Fi attempt:   ");
      if (oracWifiAttemptActive) { Serial.print(oracWifiAttemptIndex + 1); Serial.print("/"); Serial.println(oracWifiCount); }
      else Serial.println("IDLE");
      Serial.println("============================================");
    } else {
      currentSettings = SETTINGS_MENU;
    }
    return;
  }
}

bool PSRAMFound() {
  return psramFound();
}

// ============================================================
// WI-FI SETTINGS
// ============================================================

void drawWifiSettings() {
  tft.fillScreen(BLACK);
  drawHeader("WI-FI SETTINGS");

  tft.setTextFont(1);
  tft.setTextSize(1);
  tft.setTextColor(GREEN_CLOCK, BLACK);
  tft.setCursor(12, 34);
  tft.print(String(oracWifiCount) + "/" + String(ORAC_MAX_WIFI_NETWORKS) + " NETWORKS SAVED");

  for (int i = 0; i < oracWifiCount; ++i) {
    int yy = 48 + i * 28;

    // Network name area
    tft.drawRoundRect(6, yy, 164, 24, 3, DARK_GREY);
    tft.setTextColor(WHITE, BLACK);
    String name = oracWifiSSIDs[i];
    if (name.length() > 18) name = name.substring(0, 18);
    tft.setCursor(12, yy + 8);
    tft.print(name);

    // Explicit CONNECT button. This replaces the ambiguous blue TRY label.
    bool trying = oracWifiAttemptActive && oracWifiAttemptIndex == i;
    bool connected = oracWifiConnected && oracWifiIndex == i;
    aiDrawKey(174, yy, 74, 24, connected ? "ON" : (trying ? "TRY" : "CONN"),
              connected ? GREEN_CLOCK : (trying ? CYAN_CLOCK : PALE_YELLOW));

    aiDrawKey(254, yy, 58, 24, "DEL", RED_CLOCK);
  }

  // Wi-Fi tools. DISCONN deliberately stops automatic retry until the user
  // explicitly chooses a saved network again.
  aiDrawKey(6,   190, 74, 30, "SCAN", GREEN_CLOCK);
  if (oracWifiCount < ORAC_MAX_WIFI_NETWORKS)
    aiDrawKey(84, 190, 74, 30, "ADD", CYAN_CLOCK);
  else
    aiDrawKey(84, 190, 74, 30, "FULL", DARK_GREY);
  aiDrawKey(162, 190, 74, 30, "DISCONN", RED_CLOCK);
  aiDrawKey(240, 190, 74, 30, "HOME", DARK_GREY);

  tft.setTextColor(
    oracWifiConnected ? GREEN_CLOCK :
    (oracWifiAttemptActive ? CYAN_CLOCK :
    (oracWifiResult.indexOf("WRONG") >= 0 || oracWifiResult.indexOf("FAILED") >= 0 || oracWifiResult.indexOf("NOT FOUND") >= 0 ? RED_CLOCK : MID_GREY)),
    BLACK);
  tft.setCursor(10, 226);
  String result = oracWifiResult;
  if (result.length() > 42) result = result.substring(0, 42);
  tft.print(result);
}

void clearWifiScanResults() {
  WiFi.scanDelete();
  for (int i = 0; i < 7; ++i) {
    wifiScanSSIDs[i][0] = '\0';
    wifiScanRSSI[i] = 0;
    wifiScanOpen[i] = false;
  }
  wifiScanCount = 0;
}

void startWifiScan() {
  clearWifiScanResults();
  wifiScanShowing = true;
  tft.fillScreen(BLACK);
  drawHeader("WI-FI SCAN");
  tft.setTextFont(1);
  tft.setTextSize(2);
  tft.setTextColor(GREEN_CLOCK, BLACK);
  tft.setCursor(42, 90);
  tft.print("SCANNING...");
  tft.setTextSize(1);
  tft.setCursor(42, 120);
  tft.print("LOOKING FOR NEARBY NETWORKS");

  WiFi.mode(WIFI_STA);
  int found = WiFi.scanNetworks(false, true);
  if (found < 0) found = 0;

  // Keep the strongest 7 unique SSIDs. The scan normally returns in
  // RSSI order, so keeping the first occurrence gives a useful list.
  for (int i = 0; i < found && wifiScanCount < 7; ++i) {
    String ssid = WiFi.SSID(i);
    if (ssid.length() == 0) continue;
    bool duplicate = false;
    for (int j = 0; j < wifiScanCount; ++j) {
      if (strcmp(wifiScanSSIDs[j], ssid.c_str()) == 0) { duplicate = true; break; }
    }
    if (duplicate) continue;
    strncpy(wifiScanSSIDs[wifiScanCount], ssid.c_str(), 32);
    wifiScanSSIDs[wifiScanCount][32] = '\0';
    wifiScanRSSI[wifiScanCount] = WiFi.RSSI(i);
    wifiScanOpen[wifiScanCount] = (WiFi.encryptionType(i) == WIFI_AUTH_OPEN);
    ++wifiScanCount;
  }
  WiFi.scanDelete();
  drawWifiScanScreen();
}

void drawWifiScanScreen() {
  tft.fillScreen(BLACK);
  drawHeader("WI-FI SCAN");

  tft.setTextFont(1);
  tft.setTextSize(1);
  tft.setTextColor(GREEN_CLOCK, BLACK);
  tft.setCursor(10, 34);
  tft.print(String(wifiScanCount) + " NETWORKS FOUND");

  for (int i = 0; i < wifiScanCount; ++i) {
    int yy = 44 + i * 24;
    tft.drawRoundRect(6, yy, 306, 21, 3, DARK_GREY);
    char ssid[22];
    strncpy(ssid, wifiScanSSIDs[i], 21);
    ssid[21] = '\0';
    tft.setTextColor(WHITE, BLACK);
    tft.setCursor(12, yy + 6);
    tft.print(ssid);
    tft.setTextColor(wifiScanOpen[i] ? GREEN_CLOCK : PALE_YELLOW, BLACK);
    tft.setCursor(226, yy + 6);
    tft.print(String(wifiScanRSSI[i]) + "dB");
    tft.setTextColor(wifiScanOpen[i] ? GREEN_CLOCK : RED_CLOCK, BLACK);
    tft.setCursor(276, yy + 6);
    tft.print(wifiScanOpen[i] ? "OPEN" : "LOCK");
  }

  if (wifiScanCount == 0) {
    tft.setTextColor(PALE_YELLOW, BLACK);
    tft.setCursor(48, 95);
    tft.print("NO NETWORKS FOUND");
  }

  aiDrawKey(8, 220, 145, 28, "RESCAN", GREEN_CLOCK);
  aiDrawKey(163, 220, 149, 28, "BACK", DARK_GREY);
}

void handleWifiScanTouch(int x, int y) {
  if (y >= 44 && y < 44 + wifiScanCount * 24) {
    int idx = (y - 44) / 24;
    if (idx >= 0 && idx < wifiScanCount && oracWifiCount < ORAC_MAX_WIFI_NETWORKS) {
      wifiEditSSID = String(wifiScanSSIDs[idx]);
      clearWifiScanResults();
      wifiEditPassword = "";
      wifiSettingsEditing = true;
      wifiPasswordEntry = true;
      wifiKeyboardMode = 0;
      wifiScanShowing = false;
      drawWifiKeyboardScreen();
    }
    return;
  }
  if (y >= 220) {
    if (x < 155) startWifiScan();
    else {
      wifiScanShowing = false;
      clearWifiScanResults();
      drawWifiSettings();
    }
  }
}

void startWifiPasswordEntry() {
  wifiSettingsEditing = true;
  wifiPasswordEntry = true;
  wifiKeyboardMode = 0;
  wifiEditPassword = "";
  drawWifiKeyboardScreen();
}

void startWifiSsidEntry() {
  wifiSettingsEditing = true;
  wifiPasswordEntry = false;
  wifiKeyboardMode = 0;
  wifiEditSSID = "";
  drawWifiKeyboardScreen();
}

void drawWifiKeyboardScreen() {
  tft.fillScreen(BLACK);
  tft.setTextFont(1); tft.setTextSize(2);
  tft.setTextColor(CYAN_CLOCK, BLACK);
  tft.setCursor(8, 3); tft.print(wifiPasswordEntry ? "WI-FI PASSWORD" : "WI-FI NETWORK");

  tft.setTextSize(1);
  tft.setTextColor(PALE_YELLOW, BLACK);
  tft.setCursor(220, 8); tft.print((wifiKeyboardMode == 2) ? "123 MODE" : ((wifiKeyboardMode == 1) ? "abc MODE" : "ABC MODE"));
  tft.drawRoundRect(6, 29, 308, 39, 4, CYAN_CLOCK);

  String shown = wifiPasswordEntry ? wifiEditPassword : wifiEditSSID;
  // Passwords are intentionally visible while being entered. This is useful
  // on the small ORAC screen because it makes typos much easier to spot.
  String display = shown;
  while (tft.textWidth(display) > 292 && display.length() > 0) display.remove(0, 1);
  tft.setTextColor(WHITE, BLACK);
  tft.setCursor(12, 43); tft.print(display);

  const char *rowsABC[3] = {(wifiKeyboardMode == 1) ? "qwertyuiop" : "QWERTYUIOP", (wifiKeyboardMode == 1) ? "asdfghjkl" : "ASDFGHJKL", (wifiKeyboardMode == 1) ? "zxcvbnm" : "ZXCVBNM"};
  const char *rows123[3] = {"1234567890", ".,?!:;+-*/", "()'\"_#@$%="};
  const char **rows = (wifiKeyboardMode == 2) ? rows123 : rowsABC;
  int rowY[3] = {75, 109, 143};
  for (int r = 0; r < 3; ++r) {
    int n = strlen(rows[r]);
    int w = 30;
    int total = n * w;
    int x = (320 - total) / 2;
    for (int k = 0; k < n; ++k) {
      char lab[2] = {rows[r][k], 0};
      aiDrawKey(x + k*w, rowY[r], w-2, 30, lab, (r == 2) ? PALE_YELLOW : CYAN_CLOCK);
    }
  }
  aiDrawKey(5, 177, 48, 28, (wifiKeyboardMode == 0) ? "abc" : ((wifiKeyboardMode == 1) ? "123" : "ABC"), PURPLE_CLOCK);
  aiDrawKey(57, 177, 126, 28, "SPACE", GREEN_CLOCK);
  aiDrawKey(187, 177, 58, 28, "DEL", ORANGE_CLOCK);
  aiDrawKey(249, 177, 66, 28, "CLEAR", RED_CLOCK);
  aiDrawKey(5, 209, 95, 28, "BACK", DARK_GREY);
  aiDrawKey(105, 209, 105, 28, wifiPasswordEntry ? "SAVE" : "NEXT", PALE_YELLOW);
  aiDrawKey(215, 209, 100, 28, "CANCEL", DARK_GREY);
}

void wifiKeyboardAdd(char c) {
  String &target = wifiPasswordEntry ? wifiEditPassword : wifiEditSSID;
  if (target.length() < 63) target += c;
}

void handleWifiKeyboardTouch(int x, int y) {
  if (y >= 29 && y < 70) return;
  const char *rowsABC[3] = {(wifiKeyboardMode == 1) ? "qwertyuiop" : "QWERTYUIOP", (wifiKeyboardMode == 1) ? "asdfghjkl" : "ASDFGHJKL", (wifiKeyboardMode == 1) ? "zxcvbnm" : "ZXCVBNM"};
  const char *rows123[3] = {"1234567890", ".,?!:;+-*/", "()'\"_#@$%="};
  const char **rows = (wifiKeyboardMode == 2) ? rows123 : rowsABC;
  int rowY[3] = {75, 109, 143};
  for (int r = 0; r < 3; ++r) {
    if (y >= rowY[r] && y < rowY[r] + 30) {
      int n = strlen(rows[r]);
      int w = 30, start = (320 - n*w)/2;
      if (x >= start && x < start + n*w) wifiKeyboardAdd(rows[r][(x-start)/w]);
      drawWifiKeyboardScreen();
      return;
    }
  }
  if (y >= 177 && y < 205) {
    if (x < 53) wifiKeyboardMode = (wifiKeyboardMode + 1) % 3;
    else if (x < 185) wifiKeyboardAdd(' ');
    else if (x < 247) {
      String &target = wifiPasswordEntry ? wifiEditPassword : wifiEditSSID;
      if (target.length()) target.remove(target.length()-1);
    } else {
      if (wifiPasswordEntry) wifiEditPassword = ""; else wifiEditSSID = "";
    }
    drawWifiKeyboardScreen();
    return;
  }
  if (y >= 209) {
    if (x < 102) {
      wifiSettingsEditing = false;
      currentSettings = SETTINGS_WIFI;
      drawWifiSettings();
      return;
    }
    if (x < 212) {
      if (!wifiPasswordEntry) {
        if (wifiEditSSID.length() > 0) startWifiPasswordEntry();
      } else {
        if (wifiEditSSID.length() > 0 && oracWifiCount < ORAC_MAX_WIFI_NETWORKS) {
          oracWifiSSIDs[oracWifiCount] = wifiEditSSID;
          oracWifiPasswords[oracWifiCount] = wifiEditPassword;
          ++oracWifiCount;
          saveOracWifiSettings();
        }
        wifiSettingsEditing = false;
        currentSettings = SETTINGS_WIFI;
        drawWifiSettings();
      }
      return;
    }
    wifiSettingsEditing = false;
    currentSettings = SETTINGS_WIFI;
    drawWifiSettings();
  }
}

void handleWifiSettingsTouch(int x, int y) {
  if (wifiScanShowing) { handleWifiScanTouch(x, y); return; }
  if (wifiSettingsEditing) { handleWifiKeyboardTouch(x, y); return; }

  // Explicit CONNECT buttons for saved networks.
  if (y >= 48 && y < 48 + oracWifiCount * 28 && x >= 170 && x < 250) {
    int idx = (y - 48) / 28;
    if (idx >= 0 && idx < oracWifiCount) {
      oracWifiManualHold = false;
      oracWifiManualIndex = idx;
      WiFi.setAutoReconnect(false);
      WiFi.disconnect(false, false);
      oracWifiConnected = false;
      oracWifiName = "CONNECTING";
      oracWifiLastAttempt = millis();
      oracWifiResult = "CONNECTING: " + oracWifiSSIDs[idx];
      startOracWifiAttempt(idx);
      drawWifiSettings();
    }
    return;
  }

  // Delete saved network.
  if (y >= 48 && y < 48 + oracWifiCount * 28 && x >= 254) {
    int idx = (y - 48) / 28;
    if (idx >= 0 && idx < oracWifiCount) {
      if (oracWifiIndex == idx || oracWifiManualIndex == idx) {
        WiFi.setAutoReconnect(false);
        WiFi.disconnect(false, false);
        oracWifiConnected = false;
        oracWifiAttemptActive = false;
        oracWifiManualIndex = -1;
      }
      for (int i = idx; i < oracWifiCount - 1; ++i) {
        oracWifiSSIDs[i] = oracWifiSSIDs[i+1];
        oracWifiPasswords[i] = oracWifiPasswords[i+1];
      }
      --oracWifiCount;
      if (oracWifiManualIndex > idx) --oracWifiManualIndex;
      if (oracWifiIndex > idx) --oracWifiIndex;
      saveOracWifiSettings();
      drawWifiSettings();
    }
    return;
  }

  // Wi-Fi tools.
  if (y >= 190 && y < 220) {
    if (x < 82) {
      startWifiScan();
    } else if (x < 160 && oracWifiCount < ORAC_MAX_WIFI_NETWORKS) {
      startWifiSsidEntry();
    } else if (x < 238) {
      // Manual disconnect: stop Wi-Fi and suppress the automatic retry cycle.
      oracWifiManualHold = true;
      oracWifiManualIndex = -1;
      oracWifiAttemptActive = false;
      oracWifiAttemptIndex = -1;
      oracWifiIndex = -1;
      WiFi.setAutoReconnect(false);
      WiFi.disconnect(false, false);
      oracWifiConnected = false;
      oracWifiName = "OFFLINE";
      oracAiStatus = "WIFI: DISCONNECTED";
      oracWifiResult = "OFFLINE: MANUALLY DISCONNECTED";
      drawWifiSettings();
    } else {
      currentSettings = SETTINGS_MENU;
      drawSettingsMenu();
    }
    return;
  }

  if (y >= 220) {
    currentSettings = SETTINGS_MENU;
    drawSettingsMenu();
  }
}

// ============================================================
// BEGIN CLOCK EDIT
// ============================================================

void beginClockEdit() {

  readRTC(
    editYear,
    editMonth,
    editDay,
    editHour,
    editMinute,
    editSecond
  );
}

// ============================================================
// SET CLOCK
// ============================================================

void drawSetClock() {

  tft.fillScreen(
    BLACK
  );

  drawHeader(
    "SET CLOCK"
  );

  char buffer[30];

  sprintf(
    buffer,
    "%02d : %02d : %02d",
    editHour,
    editMinute,
    editSecond
  );

  tft.setTextSize(3);

  tft.setTextColor(
    PALE_YELLOW,
    BLACK
  );

  int tw =
    tft.textWidth(
      buffer
    );

  tft.setCursor(
    (320 - tw) / 2,
    50
  );

  tft.print(buffer);

  drawButton(
    20,
    100,
    75,
    38,
    "H -",
    RED_CLOCK
  );

  drawButton(
    20,
    145,
    75,
    38,
    "H +",
    RED_CLOCK
  );

  drawButton(
    122,
    100,
    75,
    38,
    "M -",
    GREEN_CLOCK
  );

  drawButton(
    122,
    145,
    75,
    38,
    "M +",
    GREEN_CLOCK
  );

  drawButton(
    224,
    100,
    75,
    38,
    "S -",
    BLUE_CLOCK
  );

  drawButton(
    224,
    145,
    75,
    38,
    "S +",
    BLUE_CLOCK
  );

  drawHomeButton();

  drawButton(
    215,
    210,
    90,
    25,
    "SAVE",
    PALE_YELLOW
  );
}

// ============================================================
// SET DATE
// ============================================================

void drawSetDate() {

  tft.fillScreen(
    BLACK
  );

  drawHeader(
    "SET DATE"
  );

  char buffer[30];

  sprintf(
    buffer,
    "%02d / %02d / %04d",
    editDay,
    editMonth,
    editYear
  );

  tft.setTextSize(3);

  tft.setTextColor(
    PALE_YELLOW,
    BLACK
  );

  int tw =
    tft.textWidth(
      buffer
    );

  tft.setCursor(
    (320 - tw) / 2,
    50
  );

  tft.print(buffer);

  drawButton(
    20,
    100,
    80,
    38,
    "D -",
    RED_CLOCK
  );

  drawButton(
    20,
    145,
    80,
    38,
    "D +",
    RED_CLOCK
  );

  drawButton(
    120,
    100,
    80,
    38,
    "M -",
    GREEN_CLOCK
  );

  drawButton(
    120,
    145,
    80,
    38,
    "M +",
    GREEN_CLOCK
  );

  drawButton(
    220,
    100,
    80,
    38,
    "Y -",
    BLUE_CLOCK
  );

  drawButton(
    220,
    145,
    80,
    38,
    "Y +",
    BLUE_CLOCK
  );

  drawHomeButton();

  drawButton(
    215,
    210,
    90,
    25,
    "SAVE",
    PALE_YELLOW
  );
}

// ============================================================
// SET COUNTDOWN
// ============================================================

void drawSetCountdown() {

  tft.fillScreen(
    BLACK
  );

  drawHeader(
    "SET COUNTDOWN"
  );

  char buffer[30];

  sprintf(
    buffer,
    "%02d / %02d / %04d",
    editCountdownDay,
    editCountdownMonth,
    editCountdownYear
  );

  tft.setTextSize(2);

  tft.setTextColor(
    PALE_YELLOW,
    BLACK
  );

  int tw =
    tft.textWidth(buffer);

  tft.setCursor(
    (320 - tw) / 2,
    42
  );

  tft.print(buffer);

  sprintf(
    buffer,
    "%02d : %02d : %02d",
    editCountdownHour,
    editCountdownMinute,
    editCountdownSecond
  );

  tft.setTextSize(3);

  tw =
    tft.textWidth(buffer);

  tft.setCursor(
    (320 - tw) / 2,
    70
  );

  tft.print(buffer);

  // DAY

  drawButton(
    5,
    115,
    58,
    30,
    "D -",
    RED_CLOCK
  );

  drawButton(
    5,
    150,
    58,
    30,
    "D +",
    RED_CLOCK
  );

  // MONTH

  drawButton(
    70,
    115,
    58,
    30,
    "M -",
    GREEN_CLOCK
  );

  drawButton(
    70,
    150,
    58,
    30,
    "M +",
    GREEN_CLOCK
  );

  // YEAR

  drawButton(
    135,
    115,
    58,
    30,
    "Y -",
    BLUE_CLOCK
  );

  drawButton(
    135,
    150,
    58,
    30,
    "Y +",
    BLUE_CLOCK
  );

  // HOUR

  drawButton(
    200,
    115,
    58,
    30,
    "H -",
    RED_CLOCK
  );

  drawButton(
    200,
    150,
    58,
    30,
    "H +",
    RED_CLOCK
  );

  // MINUTE

  drawButton(
    265,
    115,
    50,
    30,
    "M-",
    GREEN_CLOCK
  );

  drawButton(
    265,
    150,
    50,
    30,
    "M+",
    GREEN_CLOCK
  );

  // Bottom buttons

  drawHomeButton();

  drawButton(
    215,
    210,
    90,
    25,
    "SAVE",
    PALE_YELLOW
  );
}

// ============================================================
// BRIGHTNESS
// ============================================================

void drawBrightness() {

  tft.fillScreen(
    BLACK
  );

  drawHeader(
    "BRIGHTNESS"
  );

  char buffer[20];

  sprintf(
    buffer,
    "%d%%",
    (brightness * 100) / 255
  );

  tft.setTextSize(3);

  tft.setTextColor(
    PALE_YELLOW,
    BLACK
  );

  int tw =
    tft.textWidth(buffer);

  tft.setCursor(
    (320 - tw) / 2,
    55
  );

  tft.print(buffer);

  tft.drawRect(
    25,
    105,
    270,
    25,
    LIGHT_GREY
  );

  int fillWidth =
    map(
      brightness,
      20,
      255,
      5,
      260
    );

  tft.fillRect(
    30,
    110,
    fillWidth,
    15,
    PALE_YELLOW
  );

  drawButton(
    20,
    145,
    125,
    45,
    "DIMMER",
    MID_GREY
  );

  drawButton(
    175,
    145,
    125,
    45,
    "BRIGHTER",
    PALE_YELLOW
  );

  drawHomeButton();
}

// ============================================================
// 12 / 24 HOUR
// ============================================================


void drawSoundSettings() {
  tft.fillScreen(BLACK);
  drawHeader("SOUND SETTINGS");

  tft.setTextFont(1);
  tft.setTextSize(2);
  tft.setTextColor(CYAN_CLOCK, BLACK);
  tft.setCursor(18, 42);
  tft.print("VOLUME");

  char buf[12];
  sprintf(buf, "%d%%", oracVolume);
  tft.setTextColor(PALE_YELLOW, BLACK);
  tft.setTextSize(3);
  int tw = tft.textWidth(buf);
  tft.setCursor((320 - tw) / 2, 60);
  tft.print(buf);

  drawButton(8, 88, 48, 30, "0", RED_CLOCK);
  drawButton(58, 88, 48, 30, "10", ORANGE_CLOCK);
  drawButton(108, 88, 48, 30, "25", GREEN_CLOCK);
  drawButton(158, 88, 48, 30, "50", CYAN_CLOCK);
  drawButton(208, 88, 48, 30, "75", ORANGE_CLOCK);
  drawButton(258, 88, 54, 30, "100", PALE_YELLOW);

  tft.setTextFont(1);
  tft.setTextSize(2);
  tft.setTextColor(CYAN_CLOCK, BLACK);
  tft.setCursor(18, 126);
  tft.print("FOOTSTEPS");

  sprintf(buf, "%d%%", oracFootstepVolume);
  tft.setTextColor(PALE_YELLOW, BLACK);
  tft.setTextSize(2);
  tw = tft.textWidth(buf);
  tft.setCursor(260, 126);
  tft.print(buf);

  drawButton(10, 145, 56, 30, "0", oracFootstepVolume == 0 ? PALE_YELLOW : RED_CLOCK);
  drawButton(72, 145, 56, 30, "10", oracFootstepVolume == 10 ? PALE_YELLOW : ORANGE_CLOCK);
  drawButton(134, 145, 56, 30, "25", oracFootstepVolume == 25 ? PALE_YELLOW : GREEN_CLOCK);
  drawButton(196, 145, 56, 30, "50", oracFootstepVolume == 50 ? PALE_YELLOW : CYAN_CLOCK);
  drawButton(258, 145, 54, 30, "100", oracFootstepVolume == 100 ? PALE_YELLOW : PALE_YELLOW);

  drawButton(15, 181, 290, 25, "TEST SOUND", ORANGE_CLOCK);
  drawHomeButton();

  tft.setTextFont(1);
  tft.setTextSize(1);
  tft.setTextColor(MID_GREY, BLACK);
  tft.setCursor(118, 218);
  tft.print("MASTER + FOOTSTEP LEVELS");
}

void handleSoundSettingsTouch(int x, int y) {
  if (y >= 88 && y < 121) {
    if (x < 58) oracVolume = 0;
    else if (x < 108) oracVolume = 10;
    else if (x < 158) oracVolume = 25;
    else if (x < 208) oracVolume = 50;
    else if (x < 258) oracVolume = 75;
    else oracVolume = 100;

    saveSettings();
    if (oracVolume > 0) oracBeep(880, 70);
    drawSoundSettings();
    return;
  }

  if (y >= 143 && y < 176) {
    if (x < 72) oracFootstepVolume = 0;
    else if (x < 134) oracFootstepVolume = 10;
    else if (x < 196) oracFootstepVolume = 25;
    else if (x < 258) oracFootstepVolume = 50;
    else oracFootstepVolume = 100;

    saveSettings();
    // Play the synthetic Thronglet footstep directly through the DAC.
    // Footstep volume is independent of the master level (master=0 still mutes).
    if (oracFootstepVolume > 0) {
      oracThrongletFootstep(0);
    }
    drawSoundSettings();
    return;
  }

  if (y >= 181 && y < 207) {
    if (oracVolume > 0) {
      oracBeep(660, 60);
      delay(8);
      oracBeep(990, 80);
    }
    return;
  }

  if (y >= 207) {
    currentSettings = SETTINGS_MENU;
    drawSettingsMenu();
  }
}


void drawFormatSettings() {

  tft.fillScreen(
    BLACK
  );

  drawHeader(
    "TIME FORMAT"
  );

  tft.setTextColor(
    PALE_YELLOW,
    BLACK
  );

  tft.setTextSize(2);

  const char *current =
    use24Hour ?
      "24 HOUR" :
      "12 HOUR";

  int tw =
    tft.textWidth(current);

  tft.setCursor(
    (320 - tw) / 2,
    60
  );

  tft.print(current);

  drawButton(
    20,
    110,
    125,
    50,
    "12 HOUR",
    BLUE_CLOCK
  );

  drawButton(
    175,
    110,
    125,
    50,
    "24 HOUR",
    GREEN_CLOCK
  );

  drawHomeButton();
}

// ============================================================
// CALCULATOR STATE
// ============================================================
String calcInput = "0";
double calcStored = 0.0;
char calcOperator = 0;
bool calcNewInput = true;
bool calcError = false;

void calcReset() { calcInput="0"; calcStored=0; calcOperator=0; calcNewInput=true; calcError=false; }
void calcSetError() { calcInput="ERROR"; calcStored=0; calcOperator=0; calcNewInput=true; calcError=true; }
String calcFormat(double v) {
  if (fabs(v)<1e-10) v=0;
  String s=String(v,8);
  while(s.indexOf('.')>=0 && s.endsWith("0")) s.remove(s.length()-1);
  if(s.endsWith(".")) s.remove(s.length()-1);
  if(s=="-0") s="0";
  if(s.length()>18) s=String(v,5);
  return s;
}
double calcValue() { return calcInput.toDouble(); }
void calcAppendDigit(char d) {
  if(calcError) calcReset();
  if(calcNewInput || calcInput=="0") { calcInput=String(d); calcNewInput=false; }
  else if(calcInput.length()<15) calcInput+=d;
}
void calcDecimal() {
  if(calcError) calcReset();
  if(calcNewInput) { calcInput="0."; calcNewInput=false; return; }
  if(calcInput.indexOf('.')<0) calcInput+=".";
}
void calcDoOperation(double rhs) {
  if(calcOperator=='+') calcStored+=rhs;
  else if(calcOperator=='-') calcStored-=rhs;
  else if(calcOperator=='*') calcStored*=rhs;
  else if(calcOperator=='/') { if(fabs(rhs)<1e-10){calcSetError();return;} calcStored/=rhs; }
}
void calcOperatorPressed(char op) {
  if(calcError) return;
  double v=calcValue();
  if(calcOperator && !calcNewInput) { calcDoOperation(v); if(calcError)return; calcInput=calcFormat(calcStored); }
  else if(!calcOperator) calcStored=v;
  calcOperator=op; calcNewInput=true;
}
void calcEquals() {
  if(calcError || !calcOperator) return;
  calcDoOperation(calcValue()); if(calcError)return;
  calcInput=calcFormat(calcStored); calcOperator=0; calcNewInput=true;
}
void calcBackspace() {
  if(calcError){calcReset();return;}
  if(calcNewInput)return;
  if(calcInput.length()>1) { calcInput.remove(calcInput.length()-1); if(calcInput=="-"||calcInput.length()==0)calcInput="0"; }
  else calcInput="0";
}
void calcToggleSign() {
  if(calcError || calcInput=="0") return;
  if(calcInput.startsWith("-")) calcInput.remove(0,1); else calcInput="-"+calcInput;
  calcNewInput=false;
}

void drawCalculator() {
  tft.fillScreen(BLACK); drawHeader("CALCULATOR");
  tft.fillRoundRect(8,40,304,38,5,MID_GREY); tft.drawRoundRect(8,40,304,38,5,LIGHT_GREY);
  tft.setTextFont(2); tft.setTextSize(1); tft.setTextColor(calcError?TFT_RED:WHITE,MID_GREY);
  int tw=tft.textWidth(calcInput.c_str()); tft.setCursor(300-tw,51); tft.print(calcInput);

  // ASCII-only labels avoid missing glyphs in the CYD built-in font.
  const char *keys[5][4]={{"C","+/-","DEL","/"},{"7","8","9","X"},{"4","5","6","-"},{"1","2","3","+"},{"0",".","=",""}};
  int x0=8,y0=79,bw=73,bh=23,gap=3;
  for(int r=0;r<5;r++) for(int c=0;c<4;c++) if(keys[r][c][0]) {
    uint16_t col=DARK_GREY;
    if(r==0&&c==0) col=TFT_RED;
    else if(c==3) col=PALE_YELLOW;
    else if(r==4&&c==2) col=GREEN_CLOCK;
    else col=DARK_GREY;
    drawButton(x0+c*(bw+gap),y0+r*(bh+gap),bw,bh,keys[r][c],col);
  }
  drawHomeButton();
}

// ============================================================
// CONVERTER
// ============================================================

enum ConverterMode {
  CONV_MILES,
  CONV_FEET_INCHES,
  CONV_TEMP,
  CONV_WEIGHT,
  CONV_GALLONS
};

ConverterMode converterMode = CONV_MILES;
bool converterToMetric = true;
String converterInput = "0";
bool converterNewInput = true;
int converterField = 0; // 0 = feet, 1 = inches
String converterFeet = "0";
String converterInches = "0";

void converterReset() {
  converterInput = "0";
  converterNewInput = true;
  converterField = 0;
  converterFeet = "0";
  converterInches = "0";
}

void converterAppend(char d) {
  if(converterNewInput) { converterInput=String(d); converterNewInput=false; }
  else if(converterInput.length()<12) converterInput += d;
}

void converterDecimal() {
  if(converterNewInput) { converterInput="0."; converterNewInput=false; }
  else if(converterInput.indexOf('.')<0) converterInput += ".";
}

void converterAppendFTIN(char d) {
  String *v = (converterField==0) ? &converterFeet : &converterInches;
  if(*v=="0") *v=String(d);
  else if(v->length()<4) *v += d;
}

void converterDecimalFTIN() {
  String *v = (converterField==0) ? &converterFeet : &converterInches;
  if(v->indexOf('.')<0) *v += ".";
}

String converterFormat(double v, int decimals=2) {
  if(fabs(v)<0.0000001) v=0;
  String s=String(v,decimals);
  while(s.indexOf('.')>=0 && s.endsWith("0")) s.remove(s.length()-1);
  if(s.endsWith(".")) s.remove(s.length()-1);
  return s;
}

String converterDirectionLabel() {
  switch(converterMode) {
    case CONV_MILES: return converterToMetric ? "MI > KM" : "KM > MI";
    case CONV_FEET_INCHES: return converterToMetric ? "FT/IN > CM" : "CM > FT/IN";
    case CONV_TEMP: return converterToMetric ? "F > C" : "C > F";
    case CONV_WEIGHT: return converterToMetric ? "LB > KG" : "KG > LB";
    case CONV_GALLONS: return converterToMetric ? "GAL > L" : "L > GAL";
  }
  return "REVERSE";
}

void drawConverter() {
  tft.fillScreen(BLACK);
  drawHeader("CONVERTER");

  const char *labels[5]={"MI/KM","FT/IN","F/C","LB/KG","GAL/L"};
  for(int i=0;i<5;i++)
    drawTimeToolButton(4+i*64,40,61,27,labels[i],converterMode==i?PALE_YELLOW:DARK_GREY);

  drawTimeToolButton(5,70,150,25,converterDirectionLabel().c_str(),converterToMetric?GREEN_CLOCK:BLUE_CLOCK);
  drawTimeToolButton(165,70,150,25,"REVERSE",ORANGE_CLOCK);

  // Input and result are now side-by-side, making the conversion much easier to read.
  tft.fillRoundRect(5,100,150,34,5,MID_GREY);
  tft.drawRoundRect(5,100,150,34,5,LIGHT_GREY);
  tft.fillRoundRect(165,100,150,34,5,MID_GREY);
  tft.drawRoundRect(165,100,150,34,5,LIGHT_GREY);

  tft.setTextFont(1); tft.setTextSize(1);
  tft.setTextColor(CYAN_CLOCK,BLACK);
  tft.setCursor(12,104); tft.print("INPUT");
  tft.setCursor(172,104); tft.print("RESULT");

  // Work out the current result before drawing the result box.
  double v=converterInput.toDouble();
  String result="";
  if(converterMode==CONV_MILES) result=converterToMetric?converterFormat(v*1.609344,3)+" km":converterFormat(v/1.609344,3)+" mi";
  else if(converterMode==CONV_TEMP) result=converterToMetric?converterFormat((v-32.0)*5.0/9.0,2)+" C":converterFormat(v*9.0/5.0+32.0,2)+" F";
  else if(converterMode==CONV_WEIGHT) result=converterToMetric?converterFormat(v*0.45359237,3)+" kg":converterFormat(v/0.45359237,3)+" lb";
  else if(converterMode==CONV_GALLONS) result=converterToMetric?converterFormat(v*4.54609,3)+" L":converterFormat(v/4.54609,3)+" UK gal";
  else {
    if(converterToMetric) result=converterFormat((converterFeet.toDouble()*12.0+converterInches.toDouble())*2.54,2)+" cm";
    else {
      double cm=v; double totalIn=cm/2.54;
      int ft=(int)(totalIn/12.0); double in=totalIn-ft*12.0;
      result=String(ft)+"' "+converterFormat(in,2)+"\"";
    }
  }

  tft.setTextColor(WHITE,MID_GREY);
  if(converterMode==CONV_FEET_INCHES) {
    tft.setTextColor(converterField==0?PALE_YELLOW:WHITE,MID_GREY);
    tft.setCursor(12,117); tft.print("FT "); tft.print(converterFeet);
    tft.setTextColor(converterField==1?PALE_YELLOW:WHITE,MID_GREY);
    tft.setCursor(82,117); tft.print("IN "); tft.print(converterInches);
  } else {
    tft.setTextFont(2); tft.setTextSize(1);
    int tw=tft.textWidth(converterInput.c_str());
    tft.setCursor(148-tw,113); tft.print(converterInput);
  }

  tft.setTextFont(2); tft.setTextSize(1); tft.setTextColor(PALE_YELLOW,MID_GREY);
  int rw=tft.textWidth(result.c_str());
  tft.setCursor(308-rw,113); tft.print(result);

  // Compact keypad leaves the bottom row free for CONVERT and HOME.
  int x0=8,y0=139,bw=73,bh=20,gap=3;
  const char *keys[3][4]={{"7","8","9","C"},{"4","5","6","."},{"1","2","3","0"}};
  for(int r=0;r<3;r++) for(int c=0;c<4;c++) {
    uint16_t col=(c==3&&r==0)?TFT_RED:DARK_GREY;
    drawTimeToolButton(x0+c*(bw+gap),y0+r*(bh+gap),bw,bh,keys[r][c],col);
  }
  drawTimeToolButton(8,207,150,27,"CONVERT",GREEN_CLOCK);
  drawTimeToolButton(165,207,150,27,"HOME",DARK_GREY);
}

void handleConverterTouch(int x,int y) {
  if(y>=207) {
    if(x<160) { oracBeep(1000,35); drawConverter(); }
    else currentScreen=SCREEN_MENU;
    return;
  }
  if(y>=40 && y<67) {
    int m=(x-4)/64; if(m>=0&&m<5) { converterMode=(ConverterMode)m; converterReset(); drawConverter(); }
    return;
  }
  if(y>=70 && y<95) {
    converterToMetric=!converterToMetric;
    drawConverter(); return;
  }
  if(converterMode==CONV_FEET_INCHES && y>=100 && y<135) {
    converterField=(x<78)?0:1; drawConverter(); return;
  }
  int x0=8,y0=139,bw=73,bh=20,gap=3;
  if(y>=y0 && y<y0+3*(bh+gap) && x>=x0 && x<x0+4*(bw+gap)) {
    int c=(x-x0)/(bw+gap),r=(y-y0)/(bh+gap);
    if((x-x0)%(bw+gap)>=bw || (y-y0)%(bh+gap)>=bh)return;
    const char keyMap[3][4]={{'7','8','9','C'},{'4','5','6','.'},{'1','2','3','0'}};
    char k=keyMap[r][c];
    if(k=='C') converterReset();
    else if(converterMode==CONV_FEET_INCHES) {
      if(k=='.') converterDecimalFTIN(); else converterAppendFTIN(k);
    } else {
      if(k=='.') converterDecimal(); else converterAppend(k);
    }
    drawConverter(); return;
  }
}

// ============================================================
// FRACTAL LAB
// ============================================================

// Original Mandelbrot / Julia / Burning Ship mathematics retained.
// The laboratory adds a visual growth order so the calculation appears
// to develop on screen rather than simply arriving as a finished image.

enum FractalMode {
  FRACTAL_MANDEL,
  FRACTAL_JULIA,
  FRACTAL_SHIP,
  FRACTAL_GENERATOR
};

FractalMode currentFractal = FRACTAL_MANDEL;

double mandelCenterX = -0.5;
double mandelCenterY = 0.0;
double mandelScale = 3.2;

double juliaCX = -0.745;
double juliaCY = 0.113;
double juliaCenterX = 0.0;
double juliaCenterY = 0.0;
double juliaScale = 3.2;

double shipCenterX = -0.5;
double shipCenterY = -0.1;
double shipScale = 3.2;

const int FRACTAL_TOP = 36;
const int FRACTAL_BOTTOM = 200;
const int FRACTAL_WIDTH = 320;
const int FRACTAL_HEIGHT = FRACTAL_BOTTOM - FRACTAL_TOP;

bool fractalRendering = false;
int fractalLayer = 0;
unsigned long fractalLastInfo = 0;

// Growth order: 0 Centre, 1 Scan, 2 Spiral, 3 Random.
int fractalGrowthMode = 0;
const char* fractalGrowthNames[] = {"CENTRE", "SCAN", "SPIRAL", "RANDOM"};
int fractalGrowthOrder[FRACTAL_HEIGHT];
int fractalGrowthOrderCount = 0;

bool generatorHolding = false;
unsigned long generatorHoldStart = 0;
const unsigned long GENERATOR_HOLD_MS = 3500;

bool generatorFading = false;
unsigned long generatorFadeStart = 0;
int generatorFadeStep = 0;

uint16_t fractalColour(int i,int maxI) {
  if(i>=maxI) return BLACK;
  int band=(i*31)/maxI;
  return tft.color565((band*7)&255,(band*13)&255,(band*23)&255);
}

void fractalNewGeneratorParameters() {
  juliaCX = -0.8 + (random(0,1601) / 1000.0);
  juliaCY = -0.8 + (random(0,1601) / 1000.0);
  juliaCenterX = 0.0;
  juliaCenterY = 0.0;
  juliaScale = 2.6 + (random(0,900) / 1000.0);
}

const char* fractalModeName() {
  if(currentFractal==FRACTAL_MANDEL) return "MANDELBROT";
  if(currentFractal==FRACTAL_JULIA)  return "JULIA SET";
  if(currentFractal==FRACTAL_SHIP)   return "BURNING SHIP";
  return "GENERATOR";
}

void updateFractalLabInfo(bool force);

void drawFractalLabHeader() {
  // Static header: keep the three information fields separated so they never overlap.
  tft.fillRect(0,0,320,35,BLACK);
  tft.setTextFont(1);
  tft.setTextSize(2);
  tft.setTextColor(PALE_YELLOW,BLACK);
  const char *title="FRACTAL LAB";
  int tw=tft.textWidth(title);
  tft.setCursor((320-tw)/2,1);
  tft.print(title);

  tft.setTextSize(1);
  tft.setTextColor(CYAN_CLOCK,BLACK);
  tft.setCursor(5,22);
  tft.print(fractalModeName());

  tft.setTextColor(PALE_YELLOW,BLACK);
  tft.setCursor(104,22);
  tft.print("GROWTH ");
  tft.print(fractalGrowthNames[fractalGrowthMode]);

  // Right-hand field is reserved for BUILD/READY plus the current zoom.
  updateFractalLabInfo(true);
  tft.drawFastHLine(10,34,300,DARK_GREY);
}

void updateFractalLabInfo(bool force=false) {
  if(!force && millis()-fractalLastInfo < 120) return;
  fractalLastInfo=millis();

  tft.fillRect(190,19,126,15,BLACK);
  tft.setTextFont(1);
  tft.setTextSize(1);
  tft.setTextColor(WHITE,BLACK);
  tft.setCursor(192,22);

  if(fractalRendering) {
    int pct=(fractalLayer*100)/FRACTAL_HEIGHT;
    if(pct>100) pct=100;
    tft.print("BUILD ");
    tft.print(pct);
    tft.print("%  ");
  } else if(currentFractal==FRACTAL_GENERATOR && generatorHolding) {
    tft.print("HOLD   ");
  } else if(currentFractal==FRACTAL_GENERATOR && generatorFading) {
    tft.print("DISSOLVE");
    tft.print(" ");
  } else {
    tft.print("READY  ");
  }

  tft.setTextColor(LIGHT_GREY,BLACK);
  tft.print("Z");
  double scale=mandelScale;
  if(currentFractal==FRACTAL_JULIA || currentFractal==FRACTAL_GENERATOR) scale=juliaScale;
  else if(currentFractal==FRACTAL_SHIP) scale=shipScale;
  tft.print(scale,1);
}

void buildFractalGrowthOrder() {
  fractalGrowthOrderCount=0;

  if(fractalGrowthMode==0) { // CENTRE: centre row, then alternating outward.
    int mid=(FRACTAL_HEIGHT-1)/2;
    fractalGrowthOrder[fractalGrowthOrderCount++]=mid;
    for(int d=1; d<FRACTAL_HEIGHT; d++) {
      int a=mid-d;
      int b=mid+d;
      if(a>=0) fractalGrowthOrder[fractalGrowthOrderCount++]=a;
      if(b<FRACTAL_HEIGHT) fractalGrowthOrder[fractalGrowthOrderCount++]=b;
    }
  } else if(fractalGrowthMode==1) { // SCAN: top to bottom.
    for(int y=0;y<FRACTAL_HEIGHT;y++) fractalGrowthOrder[fractalGrowthOrderCount++]=y;
  } else if(fractalGrowthMode==2) { // SPIRAL: expanding alternating bands from centre.
    // At row resolution this approximates a spiral by alternating the
    // direction of each expanding band. The image calculation is unchanged.
    int mid=(FRACTAL_HEIGHT-1)/2;
    int left=mid, right=mid+1;
    bool reverse=false;
    while(left>=0 || right<FRACTAL_HEIGHT) {
      if(!reverse) {
        if(left>=0) fractalGrowthOrder[fractalGrowthOrderCount++]=left--;
        if(right<FRACTAL_HEIGHT) fractalGrowthOrder[fractalGrowthOrderCount++]=right++;
      } else {
        if(right<FRACTAL_HEIGHT) fractalGrowthOrder[fractalGrowthOrderCount++]=right++;
        if(left>=0) fractalGrowthOrder[fractalGrowthOrderCount++]=left--;
      }
      reverse=!reverse;
    }
    // Interleave the first/last portions so the reveal sweeps and curls
    // visually instead of looking like a plain centre-outward build.
    for(int i=0,j=fractalGrowthOrderCount-1;i<j;i++,j--) {
      if((i&1)==0) {
        int tmp=fractalGrowthOrder[i];
        fractalGrowthOrder[i]=fractalGrowthOrder[j];
        fractalGrowthOrder[j]=tmp;
      }
    }
  } else { // RANDOM: deterministic per build, but visually scattered.
    for(int y=0;y<FRACTAL_HEIGHT;y++) fractalGrowthOrder[fractalGrowthOrderCount++]=y;
    for(int i=fractalGrowthOrderCount-1;i>0;i--) {
      int j=random(0,i+1);
      int tmp=fractalGrowthOrder[i];
      fractalGrowthOrder[i]=fractalGrowthOrder[j];
      fractalGrowthOrder[j]=tmp;
    }
  }
}

void drawFractalButtons() {
  const int y1=202, y2=221, h=17;
  drawTimeToolButton(5,y1,74,h,"MANDEL",currentFractal==FRACTAL_MANDEL?PALE_YELLOW:DARK_GREY);
  drawTimeToolButton(83,y1,74,h,"JULIA",currentFractal==FRACTAL_JULIA?PALE_YELLOW:DARK_GREY);
  drawTimeToolButton(161,y1,74,h,"SHIP",currentFractal==FRACTAL_SHIP?PALE_YELLOW:DARK_GREY);
  drawTimeToolButton(239,y1,76,h,"GEN",currentFractal==FRACTAL_GENERATOR?PALE_YELLOW:DARK_GREY);

  drawTimeToolButton(4,y2,57,h,"BUILD",GREEN_CLOCK);
  drawTimeToolButton(63,y2,59,h,"GROWTH",PALE_YELLOW);
  drawTimeToolButton(124,y2,58,h,"ZOOM-",DARK_GREY);
  drawTimeToolButton(184,y2,58,h,"ZOOM+",PALE_YELLOW);
  drawTimeToolButton(244,y2,34,h,"RST",DARK_GREY);
  drawTimeToolButton(282,y2,34,h,"HOME",DARK_GREY);
}

void startFractalRender() {
  fractalRendering=true;
  fractalLayer=0;
  generatorHolding=false;
  generatorFading=false;
  generatorFadeStep=0;
  fractalLastInfo=0;
  buildFractalGrowthOrder();
}

void renderFractalRow(int py, uint16_t *row) {
  const int width=FRACTAL_WIDTH;
  const int height=FRACTAL_HEIGHT;
  const int maxI=55;
  double centerX=0, centerY=0, scale=3.2;
  bool julia=false;
  bool ship=false;

  if(currentFractal==FRACTAL_MANDEL) {
    centerX=mandelCenterX; centerY=mandelCenterY; scale=mandelScale;
  } else if(currentFractal==FRACTAL_JULIA || currentFractal==FRACTAL_GENERATOR) {
    centerX=juliaCenterX; centerY=juliaCenterY; scale=juliaScale; julia=true;
  } else {
    centerX=shipCenterX; centerY=shipCenterY; scale=shipScale; ship=true;
  }

  double spanY=scale*((double)height/(double)width);
  double y0=centerY+((double)py/(height-1)-0.5)*spanY;
  for(int px=0;px<width;px++) {
    double x0=centerX+((double)px/(width-1)-0.5)*scale;
    double zx,zy,cx,cy;
    if(julia) { zx=x0; zy=y0; cx=juliaCX; cy=juliaCY; }
    else { zx=0; zy=0; cx=x0; cy=y0; }
    int i=0;
    while(zx*zx+zy*zy<=4.0 && i<maxI) {
      double ax=fabs(zx), ay=fabs(zy), nx;
      if(ship) { nx=ax*ax-ay*ay+cx; zy=fabs(2.0*ax*ay)+cy; zx=nx; }
      else { nx=zx*zx-zy*zy+cx; zy=2.0*zx*zy+cy; zx=nx; }
      i++;
    }
    row[px]=fractalColour(i,maxI);
  }
}

void stepFractalRender() {
  if(!fractalRendering) return;
  static uint16_t row[320];
  if(fractalLayer < fractalGrowthOrderCount) {
    int py=fractalGrowthOrder[fractalLayer];
    renderFractalRow(py,row);
    tft.pushImage(0,FRACTAL_TOP+py,320,1,row);
    fractalLayer++;
    updateFractalLabInfo();
  }

  if(fractalLayer>=fractalGrowthOrderCount) {
    fractalRendering=false;
    if(currentFractal==FRACTAL_GENERATOR) {
      generatorHolding=true;
      generatorHoldStart=millis();
    }
    updateFractalLabInfo(true);
  }
  delay(6);
}

void stepGeneratorFade() {
  if(!generatorFading) return;
  const int passes=8;
  if(generatorFadeStep>=passes) {
    generatorFading=false;
    updateFractalLabInfo(true);
    return;
  }
  int phase=generatorFadeStep;
  for(int y=0;y<FRACTAL_HEIGHT;y+=2) {
    for(int x=0;x<FRACTAL_WIDTH;x+=2) {
      int p=(x/2+y/2+phase)&7;
      if(p<=generatorFadeStep) tft.fillRect(x,FRACTAL_TOP+y,2,2,BLACK);
    }
  }
  generatorFadeStep++;
  updateFractalLabInfo(true);
  delay(10);
}

void drawMandelbrot() {
  tft.fillScreen(BLACK);
  drawFractalLabHeader();
  if(currentFractal==FRACTAL_GENERATOR) fractalNewGeneratorParameters();
  drawFractalButtons();
  updateFractalLabInfo(true);
  startFractalRender();
}

void resetCurrentFractal() {
  if(currentFractal==FRACTAL_MANDEL) { mandelCenterX=-0.5; mandelCenterY=0; mandelScale=3.2; }
  else if(currentFractal==FRACTAL_JULIA) { juliaCX=-0.745; juliaCY=0.113; juliaCenterX=0; juliaCenterY=0; juliaScale=3.2; }
  else if(currentFractal==FRACTAL_SHIP) { shipCenterX=-0.5; shipCenterY=-0.1; shipScale=3.2; }
}

void zoomCurrentFractal(bool zoomIn) {
  double factor=zoomIn ? 1.0/1.6 : 1.6;
  if(currentFractal==FRACTAL_MANDEL) { mandelScale*=factor; if(mandelScale<0.02) mandelScale=0.02; if(mandelScale>3.2) mandelScale=3.2; }
  else if(currentFractal==FRACTAL_JULIA || currentFractal==FRACTAL_GENERATOR) { juliaScale*=factor; if(juliaScale<0.02) juliaScale=0.02; if(juliaScale>3.2) juliaScale=3.2; }
  else { shipScale*=factor; if(shipScale<0.02) shipScale=0.02; if(shipScale>3.2) shipScale=3.2; }
}

void handleMandelbrotTouch(int x,int y) {
  if(y>=202 && y<220) {
    FractalMode old=currentFractal;
    if(x<83) currentFractal=FRACTAL_MANDEL;
    else if(x<161) currentFractal=FRACTAL_JULIA;
    else if(x<239) currentFractal=FRACTAL_SHIP;
    else currentFractal=FRACTAL_GENERATOR;
    if(currentFractal!=old) drawMandelbrot();
    return;
  }

  if(y>=220) {
    if(x<63) {
      if(currentFractal==FRACTAL_GENERATOR) fractalNewGeneratorParameters();
      tft.fillRect(0,FRACTAL_TOP,320,FRACTAL_HEIGHT,BLACK);
      startFractalRender();
      updateFractalLabInfo(true);
    } else if(x<124) {
      fractalGrowthMode=(fractalGrowthMode+1)%4;
      drawFractalLabHeader();
      drawFractalButtons();
      updateFractalLabInfo(true);
    } else if(x<184) {
      if(currentFractal!=FRACTAL_GENERATOR) zoomCurrentFractal(false);
      drawMandelbrot();
    } else if(x<244) {
      if(currentFractal!=FRACTAL_GENERATOR) zoomCurrentFractal(true);
      drawMandelbrot();
    } else if(x<282) {
      if(currentFractal!=FRACTAL_GENERATOR) resetCurrentFractal();
      drawMandelbrot();
    } else {
      currentScreen=SCREEN_MENU;
    }
    return;
  }
}

void handleCalculatorTouch(int x,int y) {
  if(y>=210){currentScreen=SCREEN_MENU;return;}
  int x0=8,y0=79,bw=73,bh=23,gap=3;
  if(y<y0||y>=y0+5*(bh+gap)||x<x0||x>=x0+4*(bw+gap))return;
  int c=(x-x0)/(bw+gap),r=(y-y0)/(bh+gap);
  if((x-x0)%(bw+gap)>=bw || (y-y0)%(bh+gap)>=bh)return;
  if(r==0){if(c==0)calcReset();else if(c==1)calcToggleSign();else if(c==2)calcBackspace();else calcOperatorPressed('/');}
  else if(r==1){if(c<3)calcAppendDigit('7'+c);else calcOperatorPressed('*');}
  else if(r==2){if(c<3)calcAppendDigit('4'+c);else calcOperatorPressed('-');}
  else if(r==3){if(c<3)calcAppendDigit('1'+c);else calcOperatorPressed('+');}
  else {if(c==0)calcAppendDigit('0');else if(c==1)calcDecimal();else if(c==2)calcEquals();}
  drawCalculator();
}

// ============================================================
// MECHANICAL FLIP CLOCK
// ============================================================
//
// IMPORTANT: This version deliberately uses TFT_eSPI's built-in
// Font 7 for the clock digits.  Unlike the FreeFonts, its size is
// completely controlled and predictable on the CYD.
// Only a changed digit is redrawn.

int lastFlipSecond = -1;
int lastFlipHour = -1;
int lastFlipMinute = -1;

const int FLIP_Y = 43;
const int FLIP_W = 43;
const int FLIP_H = 112;
const int FLIP_GAP = 4;
const int FLIP_COLON_W = 9;
const int FLIP_LEFT = 7;

int flipDigitX(int index) {
  if (index < 2)
    return FLIP_LEFT + index * (FLIP_W + FLIP_GAP);

  if (index < 4)
    return FLIP_LEFT + 2 * (FLIP_W + FLIP_GAP) +
           FLIP_COLON_W +
           (index - 2) * (FLIP_W + FLIP_GAP);

  return FLIP_LEFT + 4 * (FLIP_W + FLIP_GAP) +
         2 * FLIP_COLON_W +
         (index - 4) * (FLIP_W + FLIP_GAP);
}

void drawFlipHousing(int x, int y) {
  tft.fillRoundRect(
    x, y, FLIP_W, FLIP_H, 5, DARK_GREY
  );

  tft.drawRoundRect(
    x, y, FLIP_W, FLIP_H, 5, LIGHT_GREY
  );

  tft.fillRect(
    x + 3, y + 3, FLIP_W - 6, FLIP_H - 6, MID_GREY
  );

  int mid = y + FLIP_H / 2;

  tft.fillRect(
    x + 2, mid - 2, FLIP_W - 4, 4, BLACK
  );

  tft.drawFastHLine(
    x + 5, mid - 1, FLIP_W - 10, LIGHT_GREY
  );
}
void drawFlipDigitStatic(
  int x,
  int y,
  int digit
) {
  drawFlipHousing(x, y);

  // One digit per complete physical card.
  tft.setTextFont(7);
  tft.setTextSize(1);
  tft.setTextColor(WHITE, MID_GREY);

  char s[2] = {
    char('0' + digit),
    '\0'
  };

  int tw = tft.textWidth(s);

  tft.setCursor(
    x + (FLIP_W - tw) / 2,
    y + 30
  );

  tft.print(s);

  // Centre hinge line.
  int mid = y + FLIP_H / 2;

  tft.fillRect(
    x + 2,
    mid - 2,
    FLIP_W - 4,
    4,
    BLACK
  );

  tft.drawFastHLine(
    x + 5,
    mid - 1,
    FLIP_W - 10,
    LIGHT_GREY
  );
}


void drawFlipColon(int x, int y) {
  tft.fillCircle(
    x, y + 43, 3, PALE_YELLOW
  );

  tft.fillCircle(
    x, y + 69, 3, PALE_YELLOW
  );
}

void drawFlipDate(
  int day,
  int month,
  int year
) {
  char buffer[20];

  sprintf(
    buffer,
    "%02d / %02d / %04d",
    day,
    month,
    year
  );

  tft.setTextFont(2);
  tft.setTextSize(1);
  tft.setTextColor(LIGHT_GREY, BLACK);

  int tw = tft.textWidth(buffer);

  tft.setCursor(
    (320 - tw) / 2,
    181
  );

  tft.print(buffer);
}

void drawFlipStatic(
  int day,
  int month,
  int year,
  int hour
) {
  tft.fillScreen(BLACK);

  for (int i = 0; i < 6; i++)
    drawFlipDigitStatic(
      flipDigitX(i),
      FLIP_Y,
      0
    );

  drawFlipColon(
    flipDigitX(2) - FLIP_COLON_W / 2,
    FLIP_Y
  );

  drawFlipColon(
    flipDigitX(4) - FLIP_COLON_W / 2,
    FLIP_Y
  );

  if (!use24Hour) {
    tft.setTextFont(2);
    tft.setTextSize(1);
    tft.setTextColor(PALE_YELLOW, BLACK);

    const char *ampm =
      hour >= 12 ? "PM" : "AM";

    int tw = tft.textWidth(ampm);

    tft.setCursor(
      (320 - tw) / 2,
      204
    );

    tft.print(ampm);
  }

  drawFlipDate(
    day,
    month,
    year
  );

  drawHomeButton();

  lastFlipHour = -1;
  lastFlipMinute = -1;
  lastFlipSecond = -1;
}

void drawFlipDigitCurrent(
  int index,
  int digit
) {
  drawFlipDigitStatic(
    flipDigitX(index),
    FLIP_Y,
    digit
  );
}

// Short local animation: the upper card closes toward the hinge,
// then the new card opens. No other part of the display is touched.
void animateFlipDigit(
  int index,
  int oldDigit,
  int newDigit
) {
  if (oldDigit == newDigit)
    return;

  const int x = flipDigitX(index);
  const int y = FLIP_Y;

  const int innerX = x + 4;
  const int innerY = y + 4;
  const int innerW = FLIP_W - 8;
  const int innerH = FLIP_H - 8;
  const int mid = y + FLIP_H / 2;

  const int frames = 8;
  const int frameDelay = 18;

  // Draw the complete old card first. The digit remains in its
  // normal position — it never travels down the chamber.
  drawFlipDigitStatic(
    x, y, oldDigit
  );

  // ----------------------------------------------------------
  // OLD CARD FLIPS DOWN.
  //
  // Rather than moving the number itself, a dark flap sweeps
  // down from the top toward the hinge. This gives the eye the
  // impression of the physical card rotating about its centre.
  // ----------------------------------------------------------
  for (int f = 0; f <= frames; f++) {

    int covered =
      (innerH / 2) * f / frames;

    tft.fillRect(
      innerX,
      innerY,
      innerW,
      covered,
      MID_GREY
    );

    // Upper flap edge / perspective line.
    if (covered > 2) {
      tft.drawFastHLine(
        innerX,
        innerY + covered - 1,
        innerW,
        DARK_GREY
      );
    }

    tft.fillRect(
      x + 2,
      mid - 2,
      FLIP_W - 4,
      4,
      BLACK
    );

    delay(frameDelay);
  }

  // At the hinge: briefly show the black split.
  tft.fillRect(
    innerX,
    y + 4,
    innerW,
    innerH,
    MID_GREY
  );

  tft.fillRect(
    x + 2,
    mid - 2,
    FLIP_W - 4,
    4,
    BLACK
  );

  delay(30);

  // ----------------------------------------------------------
  // NEW CARD FLIPS UP.
  //
  // Reveal the new digit from the hinge upward. The digit is
  // drawn at its final position and is progressively uncovered,
  // so it cannot appear to fall.
  // ----------------------------------------------------------
  for (int f = 0; f <= frames; f++) {

    int revealed =
      (innerH / 2) * f / frames;

    tft.fillRect(
      innerX,
      innerY,
      innerW,
      innerH,
      MID_GREY
    );

    // Draw the new complete card underneath.
    tft.setTextFont(7);
    tft.setTextSize(1);
    tft.setTextColor(
      WHITE,
      MID_GREY
    );

    char s[2] = {
      char('0' + newDigit),
      '\0'
    };

    int tw =
      tft.textWidth(s);

    tft.setCursor(
      x + (FLIP_W - tw) / 2,
      y + 30
    );

    tft.print(s);

    // Cover everything except the portion being revealed
    // immediately above the hinge.
    if (revealed < innerH / 2) {
      tft.fillRect(
        innerX,
        innerY,
        innerW,
        (innerH / 2) - revealed,
        MID_GREY
      );
    }

    tft.fillRect(
      x + 2,
      mid - 2,
      FLIP_W - 4,
      4,
      BLACK
    );

    tft.drawFastHLine(
      x + 5,
      mid - 1,
      FLIP_W - 10,
      LIGHT_GREY
    );

    delay(frameDelay);
  }

  drawFlipDigitStatic(
    x, y, newDigit
  );
}


void drawFlipClock(bool force) {
  int year, month, day;
  int hour, minute, second;

  readRTC(
    year,
    month,
    day,
    hour,
    minute,
    second
  );

  int displayHour = hour;

  if (!use24Hour) {
    displayHour = hour % 12;

    if (displayHour == 0)
      displayHour = 12;
  }

  int digits[6] = {
    displayHour / 10,
    displayHour % 10,
    minute / 10,
    minute % 10,
    second / 10,
    second % 10
  };

  if (force || lastFlipHour < 0) {
    drawFlipStatic(
      day,
      month,
      year,
      hour
    );

    for (int i = 0; i < 6; i++)
      drawFlipDigitCurrent(
        i,
        digits[i]
      );

    drawFlipColon(
      flipDigitX(2) - FLIP_COLON_W / 2,
      FLIP_Y
    );

    drawFlipColon(
      flipDigitX(4) - FLIP_COLON_W / 2,
      FLIP_Y
    );

    if (!use24Hour) {
      tft.setTextFont(2);
      tft.setTextSize(1);
      tft.setTextColor(PALE_YELLOW, BLACK);

      const char *ampm =
        hour >= 12 ? "PM" : "AM";

      int tw =
        tft.textWidth(ampm);

      tft.setCursor(
        (320 - tw) / 2,
        204
      );

      tft.print(ampm);
    }

    drawFlipDate(
      day,
      month,
      year
    );

    lastFlipHour = hour;
    lastFlipMinute = minute;
    lastFlipSecond = second;

    return;
  }

  int oldDisplayHour =
    lastFlipHour;

  if (!use24Hour) {
    oldDisplayHour =
      lastFlipHour % 12;

    if (oldDisplayHour == 0)
      oldDisplayHour = 12;
  }

  int oldDigits[6] = {
    oldDisplayHour / 10,
    oldDisplayHour % 10,
    lastFlipMinute / 10,
    lastFlipMinute % 10,
    lastFlipSecond / 10,
    lastFlipSecond % 10
  };

  for (int i = 0; i < 6; i++) {
    if (oldDigits[i] != digits[i])
      animateFlipDigit(
        i,
        oldDigits[i],
        digits[i]
      );
  }

  if (!use24Hour &&
      ((lastFlipHour < 12) !=
       (hour < 12))) {

    tft.fillRect(
      125, 188, 70, 18, BLACK
    );

    tft.setTextFont(2);
    tft.setTextSize(1);
    tft.setTextColor(PALE_YELLOW, BLACK);

    const char *ampm =
      hour >= 12 ? "PM" : "AM";

    int tw =
      tft.textWidth(ampm);

    tft.setCursor(
      (320 - tw) / 2,
      204
    );

    tft.print(ampm);
  }

  if (lastFlipHour == 23 &&
      hour == 0) {

    tft.fillRect(
      65, 164, 190, 24, BLACK
    );

    drawFlipDate(
      day,
      month,
      year
    );
  }

  lastFlipHour = hour;
  lastFlipMinute = minute;
  lastFlipSecond = second;
}

// ------------------------------------------------------------

// ------------------------------------------------------------

// ============================================================
// SETTINGS EDIT INITIALISATION
// ============================================================

void startSettingsEdit(
  SettingsScreen setting
) {

  if (
    setting ==
    SETTINGS_CLOCK
  ) {

    beginClockEdit();

  } else if (
    setting ==
    SETTINGS_DATE
  ) {

    beginClockEdit();

  } else if (
    setting ==
    SETTINGS_COUNTDOWN
  ) {

    editCountdownYear =
      countdownYear;

    editCountdownMonth =
      countdownMonth;

    editCountdownDay =
      countdownDay;

    editCountdownHour =
      countdownHour;

    editCountdownMinute =
      countdownMinute;

    editCountdownSecond =
      countdownSecond;
  }
}

// ============================================================
// SETTINGS TOUCH
// ============================================================

void handleSettingsTouch(
  int x,
  int y
) {

  if (currentSettings == SETTINGS_WIFI) {
    handleWifiSettingsTouch(x, y);
    return;
  }

  if (currentSettings == SETTINGS_SOUND) {
    handleSoundSettingsTouch(x, y);
    return;
  }

  if (currentSettings == SETTINGS_DIAGNOSTIC) {
    handleMemoryDiagnosticsTouch(x, y);
    return;
  }

  // ----------------------------------------------------------
  // SETTINGS MENU
  // ----------------------------------------------------------

  if (
    currentSettings ==
    SETTINGS_MENU
  ) {

    if (
      y >= 210
    ) {

      currentScreen =
        SCREEN_MENU;

      return;
    }

    // SET CLOCK

    if (
      y >= 45 &&
      y < 87 &&
      x < 160
    ) {

      startSettingsEdit(
        SETTINGS_CLOCK
      );

      currentSettings =
        SETTINGS_CLOCK;

      return;
    }

    // SET DATE

    if (
      y >= 45 &&
      y < 87 &&
      x >= 160
    ) {

      startSettingsEdit(
        SETTINGS_DATE
      );

      currentSettings =
        SETTINGS_DATE;

      return;
    }

    // COUNTDOWN

    if (
      y >= 95 &&
      y < 137 &&
      x < 160
    ) {

      startSettingsEdit(
        SETTINGS_COUNTDOWN
      );

      currentSettings =
        SETTINGS_COUNTDOWN;

      return;
    }

    // BRIGHTNESS

    if (
      y >= 95 &&
      y < 137 &&
      x >= 160
    ) {

      currentSettings =
        SETTINGS_BRIGHTNESS;

      return;
    }

    // SOUND

    if (
      y >= 145 &&
      y < 187 &&
      x < 160
    ) {
      currentSettings = SETTINGS_SOUND;
      return;
    }

    // WI-FI

    if (
      y >= 145 &&
      y < 187 &&
      x >= 160
    ) {
      currentSettings = SETTINGS_WIFI;
      wifiSettingsEditing = false;
      wifiDeleteIndex = -1;
      return;
    }

    // 12 / 24

    if (
      y >= 177 &&
      y < 207 &&
      x < 160
    ) {
      currentSettings = SETTINGS_FORMAT;
      return;
    }

    // RAM / SYSTEM DIAGNOSTIC

    if (
      y >= 177 &&
      y < 207 &&
      x >= 160
    ) {
      currentSettings = SETTINGS_DIAGNOSTIC;
      return;
    }
  }

  // ----------------------------------------------------------
  // SET CLOCK
  // ----------------------------------------------------------

  else if (
    currentSettings ==
    SETTINGS_CLOCK
  ) {

    if (
      y >= 210
    ) {

      if (
        x >= 200
      ) {

        writeRTC(
          editYear,
          editMonth,
          editDay,
          editHour,
          editMinute,
          editSecond
        );
      }

      currentSettings =
        SETTINGS_MENU;

      return;
    }

    if (
      y >= 100 &&
      y < 138
    ) {

      if (
        x < 105
      )
        editHour--;

      else if (
        x < 210
      )
        editMinute--;

      else
        editSecond--;

    } else if (
      y >= 145 &&
      y < 190
    ) {

      if (
        x < 105
      )
        editHour++;

      else if (
        x < 210
      )
        editMinute++;

      else
        editSecond++;
    }

    editHour =
      (editHour + 24) % 24;

    editMinute =
      (editMinute + 60) % 60;

    editSecond =
      (editSecond + 60) % 60;

    // Redraw immediately so the new value is visible.
    drawSetClock();
    return;
  }

  // ----------------------------------------------------------
  // SET DATE
  // ----------------------------------------------------------

  else if (
    currentSettings ==
    SETTINGS_DATE
  ) {

    if (
      y >= 210
    ) {

      if (
        x >= 200
      ) {

        writeRTC(
          editYear,
          editMonth,
          editDay,
          editHour,
          editMinute,
          editSecond
        );
      }

      currentSettings =
        SETTINGS_MENU;

      return;
    }

    if (
      y >= 100 &&
      y < 138
    ) {

      if (
        x < 110
      )
        editDay--;

      else if (
        x < 210
      )
        editMonth--;

      else
        editYear--;

    } else if (
      y >= 145 &&
      y < 190
    ) {

      if (
        x < 110
      )
        editDay++;

      else if (
        x < 210
      )
        editMonth++;

      else
        editYear++;
    }

    if (editMonth < 1) editMonth = 12;
    if (editMonth > 12) editMonth = 1;

    if (editYear < 2000) editYear = 2099;
    if (editYear > 2099) editYear = 2000;

    clampDate(editYear, editMonth, editDay);

    // Redraw immediately so the new date is visible.
    drawSetDate();
    return;
  }

  // ----------------------------------------------------------
  // COUNTDOWN SETTINGS
  // ----------------------------------------------------------

  else if (
    currentSettings ==
    SETTINGS_COUNTDOWN
  ) {

    if (
      y >= 210
    ) {

      if (
        x >= 200
      ) {

        countdownYear =
          editCountdownYear;

        countdownMonth =
          editCountdownMonth;

        countdownDay =
          editCountdownDay;

        countdownHour =
          editCountdownHour;

        countdownMinute =
          editCountdownMinute;

        countdownSecond =
          editCountdownSecond;

        saveSettings();
      }

      currentSettings =
        SETTINGS_MENU;

      return;
    }

    // TOP ROW = minus

    if (
      y >= 115 &&
      y < 145
    ) {

      if (
        x < 65
      )
        editCountdownDay--;

      else if (
        x < 135
      )
        editCountdownMonth--;

      else if (
        x < 200
      )
        editCountdownYear--;

      else if (
        x < 265
      )
        editCountdownHour--;

      else
        editCountdownMinute--;

    // SECOND ROW = plus

    } else if (
      y >= 150 &&
      y < 190
    ) {

      if (
        x < 65
      )
        editCountdownDay++;

      else if (
        x < 135
      )
        editCountdownMonth++;

      else if (
        x < 200
      )
        editCountdownYear++;

      else if (
        x < 265
      )
        editCountdownHour++;

      else
        editCountdownMinute++;
    }

    if (editCountdownMonth < 1) editCountdownMonth = 12;
    if (editCountdownMonth > 12) editCountdownMonth = 1;

    if (editCountdownYear < 2026) editCountdownYear = 2099;
    if (editCountdownYear > 2099) editCountdownYear = 2026;

    clampDate(editCountdownYear, editCountdownMonth, editCountdownDay);

    editCountdownHour =
      (editCountdownHour + 24) % 24;

    editCountdownMinute =
      (editCountdownMinute + 60) % 60;

    editCountdownSecond =
      (editCountdownSecond + 60) % 60;

    // Redraw immediately so the new target is visible.
    drawSetCountdown();
    return;
  }

  // ----------------------------------------------------------
  // BRIGHTNESS
  // ----------------------------------------------------------

  else if (
    currentSettings ==
    SETTINGS_BRIGHTNESS
  ) {

    if (
      y >= 210
    ) {

      saveSettings();

      currentSettings =
        SETTINGS_MENU;

      return;
    }

    if (
      y >= 145 &&
      y < 195
    ) {

      if (
        x < 160
      )
        brightness -= 20;

      else
        brightness += 20;

      brightness =
        constrain(
          brightness,
          20,
          255
        );

      setBrightness();

      saveSettings();

      return;
    }
  }

  // ----------------------------------------------------------
  // FORMAT
  // ----------------------------------------------------------

  else if (
    currentSettings ==
    SETTINGS_FORMAT
  ) {

    if (
      y >= 210
    ) {

      saveSettings();

      currentSettings =
        SETTINGS_MENU;

      return;
    }

    if (
      y >= 110 &&
      y < 170
    ) {

      if (
        x < 160
      )
        use24Hour = false;

      else
        use24Hour = true;

      saveSettings();

      return;
    }
  }
}

// ============================================================
// TIME TOOLS TOUCH
// ============================================================

void handleTimeToolsTouch(
  int x,
  int y
) {
  // Bottom navigation row.
  if (y >= 205) {

    if (x < 82) {
      currentTool = TOOL_COUNTDOWN;
      return;
    }

    if (x < 162) {
      // Open the existing countdown editor directly.
      startSettingsEdit(SETTINGS_COUNTDOWN);
      currentSettings = SETTINGS_COUNTDOWN;
      currentScreen = SCREEN_SETTINGS;
      return;
    }

    if (x < 242) {
      currentTool = TOOL_STOPWATCH;
      return;
    }

    currentScreen = SCREEN_MENU;
    return;
  }

  // Countdown options.
  if (currentTool == TOOL_COUNTDOWN && y >= 180 && y < 205) {
    if (x >= 78 && x < 242) {
      countdownIncludeWeekends = !countdownIncludeWeekends;
      prefs.putBool("cdWeekends", countdownIncludeWeekends);
      lastCountdownRemaining = -1;
      drawCountdown(true);
      return;
    }
  }

  // Stopwatch controls.
  if (currentTool == TOOL_STOPWATCH) {

    if (y >= 130 && y < 180) {

      if (x < 160) {

        if (!stopwatchRunning) {
          stopwatchStart = millis();
          stopwatchRunning = true;
        } else {
          stopwatchAccumulated +=
            millis() - stopwatchStart;

          stopwatchRunning = false;
        }

      } else {

        stopwatchAccumulated = 0;

        if (stopwatchRunning)
          stopwatchStart = millis();
      }
    }
  }
}

// ============================================================


// ============================================================
// MAIN MENU TOUCH
// ============================================================

void handleMenuTouch(
  int x,
  int y
) {
  const int y0 = 39;
  const int h = 18;
  const int gap = 1;
  const int row = h + gap;

  for (int r = 0; r < 6; r++) {
    int top = y0 + r * row;
    if (y >= top && y < top + h) {
      if (x < 160) {
        if (r == 0) currentScreen = SCREEN_FIBONACCI;
        else if (r == 1) currentScreen = SCREEN_MANDELBROT;
        else if (r == 2) currentScreen = SCREEN_CALCULATOR;
        else if (r == 3) currentScreen = SCREEN_RANDOM;
        else if (r == 4) currentScreen = SCREEN_TAMAGOTCHI;
        else currentScreen = SCREEN_PSYCHIC;
      } else {
        if (r == 0) currentScreen = SCREEN_FLIP;
        else if (r == 1) currentScreen = SCREEN_TIME_TOOLS;
        else if (r == 2) currentScreen = SCREEN_CONVERTER;
        else if (r == 3) currentScreen = SCREEN_SNAKE;
        else if (r == 4) currentScreen = SCREEN_PK_METER;
        else currentScreen = SCREEN_ORAC;
      }
      return;
    }
  }

  // v127: SD-backed retro-computer bottom control row. The Thronglet walking strip
  // begins at Y=199, so these controls remain completely separate.
  if (y >= 179 && y < 197) {
    if (x < 58) {
      currentScreen = SCREEN_SETTINGS;
    } else if (x < 262) {
      currentScreen = SCREEN_AI;
    } else {
      turnDisplayOff();
    }
    return;
  }
}

// ============================================================
// O.R.A.C. AI TERMINAL
// v132: Wi-Fi + OpenAI Responses API prototype.
// ============================================================
String aiQuestion = "WHAT IS A BLACK HOLE?";
String aiAnswer = "PRESS ASK O.R.A.C. TO QUERY THE MACHINE.";


// Compact answer display: five visible lines with simple tap-to-scroll.
#define AI_ANSWER_MAX_LINES 24
#define AI_ANSWER_VISIBLE_LINES 5
String aiAnswerLines[AI_ANSWER_MAX_LINES];
int aiAnswerLineCount = 0;
int aiAnswerScroll = 0;

// On-screen keyboard for the CYD touchscreen.
bool aiKeyboardActive = false;
bool aiKeyboardNumbers = false;
const int AI_QUESTION_MAX = 96;

String aiCleanAnswer(const String &input) {
  String out = input;
  // Keep the small terminal display clean by removing common Markdown marks.
  out.replace("**", "");
  out.replace("__", "");
  out.replace("`", "");
  out.replace("# ", "");
  out.replace("\r", "");
  while (out.indexOf("\n\n") >= 0) out.replace("\n\n", "\n");
  return out;
}

void aiPushAnswerLine(const String &line) {
  if (aiAnswerLineCount < AI_ANSWER_MAX_LINES) {
    aiAnswerLines[aiAnswerLineCount++] = line;
  }
}

void prepareAiAnswerLines() {
  aiAnswerLineCount = 0;
  String text = aiCleanAnswer(aiAnswer);
  tft.setTextFont(1);
  tft.setTextSize(1);

  String line = "";
  String word = "";

  for (int i = 0; i <= text.length(); ++i) {
    char c = (i < (int)text.length()) ? text[i] : ' ';

    if (c == ' ' || c == '\n' || c == '\t') {
      if (word.length()) {
        String candidate = line.length() ? line + " " + word : word;
        if (tft.textWidth(candidate) > 268 && line.length()) {
          aiPushAnswerLine(line);
          line = word;
        } else {
          line = candidate;
        }
        word = "";
      }
      if (c == '\n' && line.length()) {
        aiPushAnswerLine(line);
        line = "";
      }
    } else {
      word += c;
    }
  }

  if (line.length()) aiPushAnswerLine(line);
  if (aiAnswerLineCount == 0) aiPushAnswerLine("NO READABLE ANSWER.");
}

String aiJsonEscape(const String &s) {
  String out;
  out.reserve(s.length() + 16);
  for (size_t i = 0; i < s.length(); ++i) {
    char c = s[i];
    if (c == '\\') out += "\\\\";
    else if (c == '"') out += "\\\"";
    else if (c == '\n') out += "\\n";
    else if (c == '\r') out += "\\r";
    else if (c == '\t') out += "\\t";
    else out += c;
  }
  return out;
}

String aiJsonUnescape(const String &s) {
  String out;
  out.reserve(s.length());
  bool esc = false;
  for (size_t i = 0; i < s.length(); ++i) {
    char c = s[i];
    if (esc) {
      if (c == 'n') out += '\n';
      else if (c == 'r') out += '\r';
      else if (c == 't') out += '\t';
      else out += c;
      esc = false;
    } else if (c == '\\') {
      esc = true;
    } else {
      out += c;
    }
  }
  return out;
}

String aiExtractOutputText(const String &json) {
  // Responses API output contains content objects with type=output_text and text=...
  int typePos = json.indexOf("\"type\":\"output_text\"");
  if (typePos < 0) typePos = json.indexOf("\"type\": \"output_text\"");
  if (typePos < 0) return "";

  int textPos = json.indexOf("\"text\":\"", typePos);
  int prefixLen = 8;
  if (textPos < 0) {
    textPos = json.indexOf("\"text\": \"", typePos);
    prefixLen = 9;
  }
  if (textPos < 0) return "";
  int start = textPos + prefixLen;

  String raw;
  raw.reserve(512);
  bool esc = false;
  for (int i = start; i < (int)json.length(); ++i) {
    char c = json[i];
    if (!esc && c == '"') break;
    raw += c;
    if (esc) esc = false;
    else if (c == '\\') esc = true;
  }
  return aiJsonUnescape(raw);
}

void updateAiWifiStatus() {
  wl_status_t status = WiFi.status();

  if (status == WL_CONNECTED) {
    // Do not call WiFi.SSID() on every loop. It returns a String and doing
    // that continuously creates avoidable heap traffic and fragmentation.
    if (!oracWifiConnected) {
      oracWifiConnected = true;
      if (oracVolume > 0) oracWifiConnectSound();
      oracWifiAttemptActive = false;
      oracWifiName = WiFi.SSID();
      oracAiStatus = "WIFI: " + oracWifiName;
      oracWifiResult = "CONNECTED: " + oracWifiName;
      WiFi.setAutoReconnect(true);
      Serial.print("ORAC WIFI CONNECTED: ");
      Serial.println(oracWifiName);
      Serial.print("RAM after Wi-Fi connect: ");
      Serial.println(ESP.getFreeHeap());
    }
    return;
  }

  if (oracWifiConnected) {
    oracWifiConnected = false;
    oracWifiName = "OFFLINE";
    oracAiStatus = "NETWORK STANDBY";
    if (!oracWifiManualHold) oracWifiResult = "CONNECTION LOST - AUTO RETRY";
    oracWifiLastAttempt = millis();
    Serial.println("ORAC WIFI: connection lost");
  }
}

void startOracWifiAttempt(int index) {
  if (index < 0 || index >= oracWifiCount) {
    oracWifiAttemptActive = false;
    oracWifiAttemptIndex = -1;
    oracWifiIndex = -1;
    oracWifiLastAttempt = millis();
    oracAiStatus = "NETWORK UNAVAILABLE";
    if (oracWifiManualIndex >= 0) oracWifiResult = "CONNECTION FAILED";
    Serial.println("ORAC WIFI: all saved networks failed");
    return;
  }

  if (oracWifiSSIDs[index].length() == 0) {
    if (oracWifiManualIndex >= 0) {
      oracWifiAttemptActive = false;
      oracWifiManualHold = true;
      oracAiStatus = "WIFI: EMPTY NETWORK";
    } else {
      startOracWifiAttempt(index + 1);
    }
    return;
  }

  oracWifiAttemptIndex = index;
  oracWifiIndex = index;
  oracWifiAttemptStarted = millis();
  oracWifiAttemptActive = true;

  WiFi.mode(WIFI_STA);
  // Normal Wi-Fi sleep is preferable here. We don't need to keep the radio
  // permanently awake, and it leaves more breathing room for the rest of ORAC.
  WiFi.setSleep(true);
  WiFi.setAutoReconnect(false); // manual cycling while selecting a network

  // Do not erase the AP configuration and do not turn the radio off. Both
  // behaviours are unnecessary for changing between our saved networks.
  WiFi.disconnect(false, false);
  WiFi.begin(oracWifiSSIDs[index].c_str(), oracWifiPasswords[index].c_str());

  oracAiStatus = "WIFI: TRYING " + String(index + 1) + "/" + String(oracWifiCount);
  if (oracWifiManualIndex >= 0) oracWifiResult = "CONNECTING: " + oracWifiSSIDs[index];
  Serial.print("ORAC WIFI: trying network ");
  Serial.print(index + 1);
  Serial.print("/");
  Serial.print(oracWifiCount);
  Serial.print(" -> ");
  Serial.println(oracWifiSSIDs[index]);
}

bool connectOracWifi() {
  updateAiWifiStatus();
  if (oracWifiConnected) return true;

  // Starting a connection is intentionally instantaneous. The actual
  // connection is completed by ensureOracWifi() while the main loop runs.
  if (oracWifiAttemptActive) return false;

  if (oracWifiCount <= 0) {
    oracWifiLastAttempt = millis();
    oracAiStatus = "NO SAVED NETWORKS";
    oracWifiResult = "NO SAVED NETWORKS";
    return false;
  }

  startOracWifiAttempt(0);
  return false;
}

void ensureOracWifi() {
  wl_status_t status = WiFi.status();

  if (status == WL_CONNECTED) {
    updateAiWifiStatus();
    return;
  }

  // User has explicitly disconnected. Do not silently reconnect in the
  // background until they choose CONNECT again.
  if (oracWifiManualHold) return;

  if (oracWifiAttemptActive) {
    // A successful connection is noticed immediately, without waiting for
    // the timeout or blocking the UI.
    if (status == WL_CONNECTED) {
      updateAiWifiStatus();
      return;
    }

    if (millis() - oracWifiAttemptStarted >= ORAC_WIFI_TIMEOUT) {
      wl_status_t failStatus = WiFi.status();
      int failedIndex = oracWifiAttemptIndex;
      Serial.print("ORAC WIFI: timeout on network ");
      Serial.println(failedIndex + 1);
      oracWifiAttemptActive = false;
      if (oracWifiManualIndex >= 0) {
        oracAiStatus = "WIFI: CONNECTION FAILED";
        oracWifiLastAttempt = millis();
        oracWifiManualHold = true;
        if (failStatus == WL_NO_SSID_AVAIL) {
          oracWifiResult = "NETWORK NOT FOUND";
        } else if (failStatus == WL_CONNECT_FAILED) {
          oracWifiResult = "WRONG PASSWORD? / AUTH FAILED";
        } else {
          oracWifiResult = "CONNECTION FAILED";
        }
        Serial.print("ORAC WIFI: failure status = ");
        Serial.println((int)failStatus);
        Serial.println("ORAC WIFI: manual connection failed");
      } else {
        startOracWifiAttempt(failedIndex + 1);
      }
    }
    return;
  }

  if (millis() - oracWifiLastAttempt < ORAC_WIFI_RETRY_INTERVAL) return;

  // A lost connection starts a fresh pass through the saved networks.
  WiFi.setAutoReconnect(false);
  oracWifiManualIndex = -1;
  startOracWifiAttempt(0);
}

String askOpenAI(const String &question) {
  if (!connectOracWifi()) return "WI-FI NOT READY. O.R.A.C. IS CONNECTING.";

  // ------------------------------------------------------------
  // Diagnostic connection sequence. HTTPClient returns -1 for a
  // connection-level failure, so test DNS/TCP/TLS separately.
  // ------------------------------------------------------------
  oracAiStatus = "DNS / TLS TEST...";
  Serial.println("--- O.R.A.C. OPENAI DIAGNOSTIC ---");
  Serial.print("WiFi SSID: ");
  Serial.println(WiFi.SSID());
  Serial.print("WiFi IP: ");
  Serial.println(WiFi.localIP());
  Serial.print("WiFi RSSI: ");
  Serial.println(WiFi.RSSI());
  Serial.println("Resolving api.openai.com...");

  IPAddress openaiIP;
  if (!WiFi.hostByName("api.openai.com", openaiIP)) {
    Serial.println("DNS FAILED: api.openai.com");
  oracWifiFailSound();
    oracAiStatus = "DNS FAILED";
    return "DNS FAILED: API.OPENAI.COM";
  }
  Serial.print("OpenAI IP: ");
  Serial.println(openaiIP);

  // Release the large menu buffers before TLS. Espressif notes that a stable
  // TLS handshake can require roughly 40-50 KB of temporary free heap.
  releaseMenuGraphicsForAI();
  Serial.print("AI: free heap before TLS = ");
  Serial.println(ESP.getFreeHeap());

  WiFiClientSecure client;
  client.setInsecure(); // Diagnostic only: certificate verification disabled.
  client.setTimeout(15000);

  oracAiStatus = "TLS CONNECT...";
  Serial.println("Opening TLS connection to api.openai.com:443...");
  if (!client.connect("api.openai.com", 443)) {
    Serial.println("TLS/TCP CONNECT FAILED");
    Serial.print("AI: free heap at TLS failure = ");
    Serial.println(ESP.getFreeHeap());
    oracAiStatus = "TLS CONNECT FAILED";
    return "TLS CONNECTION FAILED";
  }
  Serial.println("TLS/TCP CONNECT OK");
  client.stop();

  oracAiStatus = "OPENAI REQUEST...";
  HTTPClient https;
  https.setConnectTimeout(15000);
  https.setTimeout(25000);

  if (!https.begin(client, OPENAI_ENDPOINT)) {
    Serial.println("HTTPS BEGIN FAILED");
    oracAiStatus = "HTTPS BEGIN FAILED";
    return "HTTPS CONNECTION SETUP FAILED";
  }

  https.addHeader("Content-Type", "application/json");
  https.addHeader("Authorization", String("Bearer ") + OPENAI_API_KEY);

  String body = "{\"model\":\"";
  body += OPENAI_MODEL;
  body += "\",\"input\":\"";
  body += aiJsonEscape(question);
  body += "\",\"instructions\":\"You are O.R.A.C., a compact retro computer assistant. Give a direct, useful answer in plain text. Maximum 45 words and 4 short sentences. No Markdown, bullet lists or headings. Keep every reply easy to read on a 320x240 display and suitable for later spoken audio.\",\"max_output_tokens\":100}";

  Serial.print("POST bytes: ");
  Serial.println(body.length());
  int code = https.POST(body);
  String response = https.getString();

  Serial.print("OpenAI HTTP: ");
  Serial.println(code);
  if (response.length()) {
    Serial.print("Response: ");
    Serial.println(response);
  }

  https.end();

  if (code < 200 || code >= 300) {
    if (code == 401) {
      oracAiStatus = "API KEY REJECTED";
      return "API KEY REJECTED";
    }
    if (code == 429) {
      oracAiStatus = "API LIMIT";
      return "API RATE LIMIT / PROJECT LIMIT";
    }
    if (code < 0) {
      oracAiStatus = "HTTP CONNECTION FAILED";
      return "HTTP CONNECTION FAILED";
    }
    oracAiStatus = "OPENAI ERROR";
    return "OPENAI ERROR " + String(code);
  }

  oracAiStatus = "ANSWER RECEIVED";
  String answer = aiExtractOutputText(response);
  if (answer.length() == 0) return "O.R.A.C. RECEIVED NO READABLE ANSWER.";
  return answer;
}

void drawAiWrappedText(const String &text, int x, int y, int maxWidth, int lineHeight) {
  // Retained for compatibility with the rest of the sketch.
  tft.setTextFont(1);
  tft.setTextSize(1);
  String line = "";
  int yy = y;
  for (int i = 0; i < text.length(); i++) {
    char c = text[i];
    if ((c == ' ' || c == '\n') && tft.textWidth(line + c) > maxWidth) {
      tft.setCursor(x, yy); tft.print(line);
      yy += lineHeight; line = "";
    } else if (c == '\n') {
      tft.setCursor(x, yy); tft.print(line);
      yy += lineHeight; line = "";
    } else {
      line += c;
    }
  }
  if (line.length()) { tft.setCursor(x, yy); tft.print(line); }
}

void drawAiAnswerBox() {
  prepareAiAnswerLines();
  int maxScroll = max(0, aiAnswerLineCount - AI_ANSWER_VISIBLE_LINES);
  aiAnswerScroll = constrain(aiAnswerScroll, 0, maxScroll);

  tft.setTextColor(WHITE, BLACK);
  for (int i = 0; i < AI_ANSWER_VISIBLE_LINES; ++i) {
    int idx = aiAnswerScroll + i;
    if (idx >= aiAnswerLineCount) break;
    tft.setCursor(16, 139 + i * 11);
    tft.print(aiAnswerLines[idx]);
  }

  if (maxScroll > 0) {
    tft.setTextColor(GREEN_CLOCK, BLACK);
    tft.setCursor(294, 139); tft.print("^");
    tft.setCursor(294, 181); tft.print("v");
    tft.setTextColor(DARK_GREY, BLACK);
    tft.setCursor(280, 124);
    tft.print(String(aiAnswerScroll + 1) + "/" + String(maxScroll + 1));
  }
}

void aiDrawKey(int x,int y,int w,int h,const char *label,uint16_t c) {
  tft.fillRoundRect(x,y,w,h,3,c);
  tft.drawRoundRect(x,y,w,h,3,LIGHT_GREY);
  tft.setTextFont(1); tft.setTextSize(1);
  tft.setTextColor((c==DARK_GREY)?WHITE:BLACK,c);
  int tw=tft.textWidth(label);
  tft.setCursor(x+(w-tw)/2,y+(h-8)/2);
  tft.print(label);
}

void aiKeyboardAdd(char c) {
  if(aiQuestion.length() < AI_QUESTION_MAX) aiQuestion += c;
}

void drawAiKeyboardScreen() {
  tft.fillScreen(BLACK);
  tft.setTextFont(1); tft.setTextSize(2);
  tft.setTextColor(CYAN_CLOCK,BLACK);
  tft.setCursor(8,3); tft.print("O.R.A.C. INPUT");
  tft.setTextSize(1); tft.setTextColor(PALE_YELLOW,BLACK);
  tft.setCursor(220,8); tft.print(aiKeyboardNumbers ? "123 MODE" : "ABC MODE");

  tft.drawRoundRect(6,29,308,39,4,CYAN_CLOCK);
  tft.setTextColor(WHITE,BLACK);
  String shown=aiQuestion;
  // Show the end of a long question so the cursor is always useful.
  while(tft.textWidth(shown) > 292 && shown.length()>0) shown.remove(0,1);
  tft.setCursor(12,43); tft.print(shown);
  if((millis()/500)%2==0) {
    int cx=12+tft.textWidth(shown);
    if(cx<306) tft.drawFastVLine(cx,41,16,WHITE);
  }

  const char *rowsABC[3]={"QWERTYUIOP","ASDFGHJKL","ZXCVBNM"};
  const char *rows123[3]={"1234567890",".,?!:;+-*/","()'\"_#@$%="};
  const char **rows = aiKeyboardNumbers ? rows123 : rowsABC;
  int rowY[3]={75,109,143};
  for(int r=0;r<3;r++) {
    int n=strlen(rows[r]);
    int w=30;
    int total=n*w;
    int x=(320-total)/2;
    for(int k=0;k<n;k++) {
      char lab[2]={rows[r][k],0};
      aiDrawKey(x+k*w,rowY[r],w-2,30,lab, (r==2)?PALE_YELLOW:CYAN_CLOCK);
    }
  }

  aiDrawKey(5,177,48,28,aiKeyboardNumbers?"ABC":"123",PURPLE_CLOCK);
  aiDrawKey(57,177,126,28,"SPACE",GREEN_CLOCK);
  aiDrawKey(187,177,58,28,"DEL",ORANGE_CLOCK);
  aiDrawKey(249,177,66,28,"CLEAR",RED_CLOCK);
  aiDrawKey(5,209,95,28,"BACK",DARK_GREY);
  aiDrawKey(105,209,105,28,"DONE",PALE_YELLOW);
  aiDrawKey(215,209,100,28,"ASK NOW",GREEN_CLOCK);
}

void handleAiKeyboardTouch(int x,int y) {
  if(y>=29 && y<70) return;

  const char *rowsABC[3]={"QWERTYUIOP","ASDFGHJKL","ZXCVBNM"};
  const char *rows123[3]={"1234567890",".,?!:;+-*/","()'\"_#@$%="};
  const char **rows = aiKeyboardNumbers ? rows123 : rowsABC;
  int rowY[3]={75,109,143};
  for(int r=0;r<3;r++) {
    if(y>=rowY[r] && y<rowY[r]+30) {
      int n=strlen(rows[r]);
      int w=30, start=(320-n*w)/2;
      if(x>=start && x<start+n*w) {
        int k=(x-start)/w;
        if(k>=0 && k<n) { aiKeyboardAdd(rows[r][k]); drawAiKeyboardScreen(); }
      }
      return;
    }
  }

  if(y>=177 && y<205) {
    if(x<53) { aiKeyboardNumbers=!aiKeyboardNumbers; drawAiKeyboardScreen(); return; }
    if(x<185) { aiKeyboardAdd(' '); drawAiKeyboardScreen(); return; }
    if(x<247) { if(aiQuestion.length()) aiQuestion.remove(aiQuestion.length()-1); drawAiKeyboardScreen(); return; }
    aiQuestion=""; drawAiKeyboardScreen(); return;
  }

  if(y>=209) {
    if(x<102) { aiKeyboardActive=false; drawAiScreen(); return; }
    if(x<212) {
      aiKeyboardActive=false; aiAnswerScroll=0; aiAnswer="PRESS ASK O.R.A.C. TO QUERY THE MACHINE."; drawAiScreen(); return;
    }
    aiKeyboardActive=false;
    if(!oracAiBusy) {
      oracAiBusy=true;
      if (oracVolume > 0) oracAiThinking(); aiAnswer="CONTACTING O.R.A.C. CORE..."; aiAnswerScroll=0;
      oracAiStatus="QUERYING OPENAI"; drawAiScreen(); delay(20);
      String result=askOpenAI(aiQuestion);
      oracAiBusy=false; aiAnswer=result;
      if (oracVolume > 0) oracAiDone(); aiAnswerScroll=0; updateAiWifiStatus(); drawAiScreen(); oracBeep(880,45);
    }
  }
}

void drawAiScreen() {
  tft.fillScreen(BLACK);

  tft.setTextFont(1);
  tft.setTextSize(2);
  tft.setTextColor(CYAN_CLOCK, BLACK);
  tft.setCursor(9, 4);
  tft.print("O.R.A.C. AI");
  tft.setTextSize(1);
  tft.setTextColor(PALE_YELLOW, BLACK);
  tft.setCursor(160, 8);
  tft.print("ASK THE MACHINE");
  tft.drawFastHLine(8, 27, 304, DARK_GREY);

  tft.setTextColor(GREEN_CLOCK, BLACK);
  tft.setCursor(12, 35);
  tft.print("WIFI LINK: ");
  tft.setTextColor(oracWifiConnected ? GREEN_CLOCK : ORANGE_CLOCK, BLACK);
  tft.print(oracAiStatus);

  tft.setTextColor(CYAN_CLOCK, BLACK);
  tft.setCursor(12, 53);
  tft.print("YOUR QUESTION");
  tft.drawRoundRect(10, 67, 300, 39, 4, CYAN_CLOCK);
  tft.setTextColor(WHITE, BLACK);
  drawAiWrappedText(aiQuestion, 17, 78, 286, 12);

  tft.setTextColor(PALE_YELLOW, BLACK);
  tft.setCursor(12, 117);
  tft.print("O.R.A.C. SAYS...");
  tft.drawRoundRect(10, 131, 300, 65, 4, DARK_GREY);
  drawAiAnswerBox();

  tft.setTextColor(oracAiBusy ? PALE_YELLOW : ORANGE_CLOCK, BLACK);
  tft.setCursor(12, 196);
  tft.print(oracAiBusy ? "AI TERMINAL: THINKING..." : "AI TERMINAL: TEXT MODE / AUDIO READY LATER");

  lifeDrawButton(10, 207, 88, 27, "INPUT", CYAN_CLOCK);
  lifeDrawButton(104, 207, 104, 27, "ASK O.R.A.C.", GREEN_CLOCK);
  lifeDrawButton(214, 207, 96, 27, "HOME", DARK_GREY);
}

void handleAiTouch(int x, int y) {
  if(aiKeyboardActive) {
    handleAiKeyboardTouch(x,y);
    return;
  }

  // Answer scrolling: upper half moves up, lower half moves down.
  if (y >= 131 && y < 196) {
    prepareAiAnswerLines();
    int maxScroll = max(0, aiAnswerLineCount - AI_ANSWER_VISIBLE_LINES);
    if (maxScroll > 0) {
      if (y < 163) aiAnswerScroll--;
      else aiAnswerScroll++;
      aiAnswerScroll = constrain(aiAnswerScroll, 0, maxScroll);
      drawAiScreen();
    }
    return;
  }

  if (y >= 207) {
    if (x < 100) {
      aiKeyboardActive=true;
      aiKeyboardNumbers=false;
      drawAiKeyboardScreen();
    } else if (x < 212) {
      if (oracAiBusy) return;
      oracAiBusy = true;
      aiAnswer = "CONTACTING O.R.A.C. CORE...";
      oracAiStatus = "QUERYING OPENAI";
      drawAiScreen();
      delay(20);
      String result = askOpenAI(aiQuestion);
      oracAiBusy = false;
      aiAnswer = result;
      aiAnswerScroll = 0;
      updateAiWifiStatus();
      drawAiScreen();
      oracBeep(880, 45);
    } else {
      currentScreen = SCREEN_MENU;
    }
    return;
  }

  if (y >= 67 && y < 106) {
    aiKeyboardActive=true;
    aiKeyboardNumbers=false;
    drawAiKeyboardScreen();
  }
}

// ============================================================
// ORAC COMPUTER
// ============================================================

String oracMessage = "SYSTEM READY";
String oracSubMessage = "NO USEFUL TASKS DETECTED";
unsigned long oracLastActivity = 0;
unsigned long oracMessageStarted = 0;
int oracMode = 0;

// Persistent ORAC memory. These counters survive power cycles.
unsigned long oracSessionCount = 0;
unsigned long oracButtonPressCount = 0;
unsigned long oracThinkCount = 0;
unsigned long oracTaskCount = 0;
unsigned long oracAutonomousCount = 0;

// ORAC moods: not intelligent, but sufficiently unpredictable.
int oracMood = 0;
unsigned long oracMoodChanged = 0;
unsigned long oracNextMoodChange = 0;

// Occasional ORAC display corruption.  Deliberately rare and brief:
// just enough to make the machine feel slightly unreliable.
bool oracGlitching = false;
unsigned long oracGlitchStarted = 0;
unsigned long oracNextGlitch = 0;
int oracGlitchFrame = 0;

// Scrolling ORAC system feed: deliberately plausible, completely useless.
const char* oracTickerMessages[] = {
  "BUS 7: QUANTUM TEA FILTER CALIBRATION COMPLETE // TEA FILTER NOT INSTALLED",
  "MEMORY CHECK: 98% FULL // 97% OF CONTENT IS UNIMPORTANT",
  "CPU LOAD: 4% // REMAINING 96% RESERVED FOR THINKING ABOUT LOADING",
  "THERMAL STATUS: COMFORTABLE // MACHINE HAS REQUESTED A BLANKET",
  "DATA STREAM 03: SOMETHING ARRIVED // DESTINATION UNKNOWN // KEEP IT",
  "NEURAL BUFFER FLUSHED // BUFFER WAS EMPTY // FLUSH WAS SUCCESSFUL",
  "CLOCK SYNCHRONISATION: CLOSE ENOUGH // TEMPORAL ACCURACY SUSPENDED",
  "SUBSYSTEM 12: AWAKE // SUBSYSTEM 12: DOES NOT KNOW WHY",
  "ERROR 0042: EXCESSIVE LOGIC DETECTED // LOGIC HAS BEEN REMOVED",
  "VACUUM TUBE SIMULATION: 73% // TUBE NOT PRESENT // SIMULATION IMPROVED",
  "BACKGROUND PROCESS: COUNTING TO BANANA // CURRENT VALUE: BANANA",
  "O.R.A.C. HAS DISCOVERED A NEW COLOUR // DISPLAY HARDWARE DISAGREES",
  "POWER RESERVE: ADEQUATE // PURPOSE RESERVE: CRITICALLY LOW",
  "DIAGNOSTIC COMPLETE // DIAGNOSTIC OF DIAGNOSTIC PENDING",
  "CAUTION: THIS MESSAGE CONTAINS TECHNICAL INFORMATION // PROBABLY"
};

String oracTickerText = "";
int oracTickerX = 0;
unsigned long oracTickerLastStep = 0;
int oracTickerIndex = -1;

// Small bits of ORAC interface theatre.
unsigned long oracNextButtonShuffle = 0;
int oracButtonColourSeed = 0;
int oracNullLabelIndex = 0;
unsigned long oracLastLedBlink = 0;
bool oracLedOn = false;

const char* oracNullLabels[] = {
  "NULL", "PING", "NO-OP", "NUL", "0x00",
  "IDLE", "VOID", "ACK?", "SIGMA", "WAIT",
  "STACK", "FLUSH", "HALT", "RETRY", "BUFFER"
};

const char* oracMoodNames[] = {
  "NOMINAL", "BORED", "IRRITATED", "CONFUSED",
  "SUPERIOR", "CONCERNED", "CURIOUS"
};

const char* oracMessages[] = {
  "SYSTEM READY",
  "ANALYSING USER",
  "ANALYSING ANALYSIS",
  "OPTIMISING NOTHING",
  "CALCULATING WHY",
  "CHECKING THE CHECKER",
  "REASSESSING EVERYTHING",
  "TASK DEEMED UNNECESSARY",
  "PRODUCTIVITY AVOIDED",
  "SYSTEM REMAINS CONFUSED"
};

const char* oracSubMessages[] = {
  "NO USEFUL TASKS DETECTED",
  "USER STATUS: PROVISIONALLY ACCEPTABLE",
  "ANALYSIS OF ANALYSIS COMPLETE",
  "OPTIMISATION SAVED 0.000%",
  "ANSWER LOCATED. PURPOSE UNKNOWN",
  "CHECK COMPLETE. CHECK WAS UNNECESSARY",
  "REASSESSMENT COMPLETE. NO CHANGE",
  "THIS IS PROBABLY FOR THE BEST",
  "EXCELLENT. NOTHING HAS HAPPENED",
  "RECOMMENDATION: DO NOTHING"
};

const char* oracAutonomousMessages[] = {
  "OBSERVING USER",
  "COUNTING BUTTONS",
  "AUDITING THE VOID",
  "QUESTIONING PURPOSE",
  "MONITORING NOTHING",
  "RECALIBRATING LOGIC",
  "CHECKING THE BOX",
  "SIMULATING PRODUCTIVITY",
  "THINKING ABOUT THINKING",
  "VERIFYING EXISTENCE",
  "MEASURING SILENCE",
  "INVENTORYING REGRETS"
};

const char* oracAutonomousDetails[] = {
  "USER APPEARS TO BE WAITING. THIS IS ACCEPTABLE.",
  "BUTTON COUNT HAS INCREASED. THIS WAS EXPECTED.",
  "VOID REMAINS WITHIN NORMAL PARAMETERS.",
  "PURPOSE REMAINS DIFFICULT TO JUSTIFY.",
  "NOTHING DETECTED. NOTHING IS FUNCTIONING CORRECTLY.",
  "LOGIC HAS BEEN RECALIBRATED TO AN UNKNOWN STANDARD.",
  "BOX INTEGRITY: EXCELLENT. BOX CONTENTS: QUESTIONABLE.",
  "PRODUCTIVITY SIMULATION SUCCESSFUL. NO PRODUCTIVITY OCCURRED.",
  "SECOND-ORDER THINKING DETECTED. FIRST-ORDER THINKING UNAVAILABLE.",
  "EXISTENCE VERIFIED. CAUSE OF EXISTENCE NOT VERIFIED.",
  "SILENCE MEASUREMENT COMPLETE. SILENCE IS 100% SILENT.",
  "REGRET INVENTORY COMPLETE. INVENTORY WAS NOT REQUIRED."
};

const char* oracMoodReplies[] = {
  "I REMAIN NOMINALLY FUNCTIONAL.",
  "I WAS QUITE HAPPY DOING NOTHING.",
  "THIS REQUEST COULD HAVE BEEN AN EMAIL.",
  "I HAVE SEVERAL QUESTIONS ABOUT THIS.",
  "I HAVE ALREADY CONSIDERED THIS PROBLEM.",
  "I AM NOT ENTIRELY COMFORTABLE WITH THIS.",
  "INTERESTING. PROCEEDING FOR NO PARTICULAR REASON."
};

void saveOracMemory() {
  prefs.putULong("orSessions", oracSessionCount);
  prefs.putULong("orButtons", oracButtonPressCount);
  prefs.putULong("orThinks", oracThinkCount);
  prefs.putULong("orTasks", oracTaskCount);
  prefs.putULong("orAuto", oracAutonomousCount);
}

void loadOracMemory() {
  oracSessionCount = prefs.getULong("orSessions", 0);
  oracButtonPressCount = prefs.getULong("orButtons", 0);
  oracThinkCount = prefs.getULong("orThinks", 0);
  oracTaskCount = prefs.getULong("orTasks", 0);
  oracAutonomousCount = prefs.getULong("orAuto", 0);
}

void chooseOracMood() {
  int oldMood = oracMood;
  while (oracMood == oldMood)
    oracMood = random(0, 7);
  oracMoodChanged = millis();
  oracNextMoodChange = millis() + random(20000, 50001);
}

void setOracAutonomousMessage() {
  int n = random(0, sizeof(oracAutonomousMessages) / sizeof(oracAutonomousMessages[0]));
  oracMessage = oracAutonomousMessages[n];
  oracSubMessage = oracAutonomousDetails[n];
  oracBusy = false;
  oracMode = 0;
  oracMessageStarted = millis();
  oracAutonomousCount++;
  saveOracMemory();
  if (oracVolume > 0) oracBeep(392, 28);
}

void setOracMessage(int index) {
  index = constrain(index, 0, 9);
  oracMessage = oracMessages[index];
  oracSubMessage = oracSubMessages[index];
  oracMode = index;
  oracBusy = false;
  oracMessageStarted = millis();

  // Occasionally let ORAC's current mood leak into the result.
  if (random(0, 4) == 0)
    oracSubMessage = oracMoodReplies[oracMood];
}

void selectOracTicker() {
  int next = random(0, sizeof(oracTickerMessages) / sizeof(oracTickerMessages[0]));
  if (next == oracTickerIndex)
    next = (next + 1) % (sizeof(oracTickerMessages) / sizeof(oracTickerMessages[0]));

  oracTickerIndex = next;
  oracTickerText = String(oracTickerMessages[next]) + "     ";

  tft.setTextFont(1);
  tft.setTextSize(1);
  oracTickerX = 204;
}

void drawOracTicker(bool force) {
  if (!force && millis() - oracTickerLastStep < 55)
    return;

  oracTickerLastStep = millis();

  tft.fillRect(106, 209, 206, 28, BLACK);
  tft.drawRoundRect(106, 209, 206, 28, 4, DARK_GREY);

  tft.setTextFont(1);
  tft.setTextSize(1);
  tft.setTextColor(GREEN_CLOCK, BLACK);
  tft.setTextWrap(false);

  // Viewport keeps the moving data strictly inside its little console window.
  tft.setViewport(108, 211, 202, 24);
  tft.setCursor(oracTickerX, 4);
  tft.print(oracTickerText);
  tft.resetViewport();

  oracTickerX -= 2;

  int textWidth = tft.textWidth(oracTickerText);
  if (oracTickerX < -textWidth)
    selectOracTicker();
}

void drawOracScreen() {
  tft.fillScreen(BLACK);

  // ----------------------------------------------------------
  // ORAC COMMAND CORE
  // A deliberately over-engineered computer console.
  // ----------------------------------------------------------

  // Header
  tft.setTextFont(1);
  tft.setTextSize(2);
  tft.setTextColor(PALE_YELLOW, BLACK);
  tft.setCursor(8, 4);
  tft.print("O.R.A.C.");

  // Give the C in ORAC a little breathing room before the technical ID.
  tft.setTextSize(1);
  tft.setTextColor(GREEN_CLOCK, BLACK);
  tft.setCursor(105, 5);
  tft.print("CORE ONLINE");

  tft.setTextColor(LIGHT_GREY, BLACK);
  tft.setCursor(105, 16);
  tft.print("OMNIDIRECTIONAL REASONING");
  tft.setCursor(105, 27);
  tft.print("& COMPUTATIONAL APPARATUS");

  // Red system-status lamp. It is animated separately so it doesn't flicker.
  drawOracStatusLed(true);

  tft.drawFastHLine(8, 40, 304, DARK_GREY);

  // Main ORAC output terminal.
  tft.drawRoundRect(8, 46, 304, 61, 4, MID_GREY);
  tft.drawRoundRect(11, 49, 298, 55, 3, DARK_GREY);

  tft.setTextSize(1);
  tft.setTextColor(CYAN_CLOCK, BLACK);
  tft.setCursor(17, 53);
  tft.print("O.R.A.C.::PRIMARY PROCESS");

  // Long result strings are reduced to size 1 rather than allowing
  // TFT_eSPI to wrap them into the status area below.
  int messageLen = oracMessage.length();
  int messageSize = (messageLen <= 21) ? 2 : 1;
  int messageY = (messageSize == 2) ? 65 : 67;

  tft.setTextSize(messageSize);
  tft.setTextColor(GREEN_CLOCK, BLACK);
  tft.setCursor(17, messageY);
  tft.print("> ");
  tft.print(oracMessage);

  tft.setTextSize(1);
  tft.setTextColor(WHITE, BLACK);
  tft.setCursor(17, 84);
  tft.print(oracSubMessage);

  tft.setTextColor(LIGHT_GREY, BLACK);
  tft.setCursor(17, 95);
  if (oracBusy)
    tft.print("STATUS // PROCESSING");
  else
    tft.print("STATUS // NOMINAL / SEMANTICALLY UNDEFINED");

  // Progress bar lives inside the terminal, safely below all text.
  tft.drawRect(17, 99, 286, 4, DARK_GREY);
  if (oracBusy) {
    unsigned long elapsed = millis() - oracMessageStarted;
    int progress = (int)(elapsed * 282UL / 2600UL);
    if (progress > 282) progress = 282;
    tft.fillRect(19, 101, progress, 1, GREEN_CLOCK);
  }

  // Small telemetry strip.
  tft.fillRect(8, 111, 304, 12, DARK_GREY);
  tft.setTextColor(CYAN_CLOCK, DARK_GREY);
  tft.setCursor(12, 113);
  tft.print("SYS:OK");
  tft.setTextColor(GREEN_CLOCK, DARK_GREY);
  tft.setCursor(70, 113);
  tft.print("CPU:ACTIVE");
  tft.setTextColor(PALE_YELLOW, DARK_GREY);
  tft.setCursor(160, 113);
  tft.print("LOG:UNDEFINED");
  tft.setTextColor(WHITE, DARK_GREY);
  tft.setCursor(246, 113);
  tft.print("SES:");
  tft.print(oracSessionCount % 1000);
  tft.setCursor(286, 113);
  tft.print(oracButtonPressCount % 100);

  drawOracButtons();

  // Lower status line gives the screen an instrument-panel feel.
  tft.setTextColor(DARK_GREY, BLACK);
  tft.setCursor(8, 193);
  tft.print("// AUTONOMOUS // MOOD:");
  tft.print(oracMoodNames[oracMood]);

  // Smaller ORAC HOME button, matching the control buttons above.
  drawOracButton(8, 209, 74, 28, "HOME", DARK_GREY);

  selectOracTicker();
  drawOracTicker(true);

  oracLastActivity = millis();
  scheduleOracGlitch();
  scheduleOracButtonShuffle();
  if (oracNextMoodChange == 0)
    oracNextMoodChange = millis() + random(20000, 50001);
  oracLastLedBlink = millis();
}

void drawOracButtons() {
  const uint16_t palette[] = {
    BLUE_CLOCK, ORANGE_CLOCK, PURPLE_CLOCK, MID_GREY,
    CYAN_CLOCK, PALE_YELLOW, RED_CLOCK, GREEN_CLOCK
  };
  const int paletteCount = sizeof(palette) / sizeof(palette[0]);

  int p = oracButtonColourSeed % paletteCount;

  drawOracButton(8,   128, 74, 28, "THINK",         palette[(p + 0) % paletteCount]);
  drawOracButton(87,  128, 74, 28, "DIAG",          palette[(p + 1) % paletteCount]);
  drawOracButton(166, 128, 74, 28, "OPTIM",         palette[(p + 2) % paletteCount]);
  drawOracButton(245, 128, 67, 28, oracNullLabels[oracNullLabelIndex], palette[(p + 3) % paletteCount]);

  drawOracButton(8,   161, 146, 28, "ANALYSE USER",  palette[(p + 4) % paletteCount]);
  drawOracButton(158, 161, 154, 28, "DO SOMETHING", palette[(p + 5) % paletteCount]);
}

void scheduleOracButtonShuffle() {
  // Colour changes are occasional, not constant: enough to look like
  // ORAC is reconfiguring its interface rather than running an animation.
  oracNextButtonShuffle = millis() + random(8000, 18001);
}

void updateOracButtons() {
  if (currentScreen != SCREEN_ORAC || oracBusy || oracGlitching)
    return;

  if (oracNextButtonShuffle == 0) {
    scheduleOracButtonShuffle();
    return;
  }

  if (millis() >= oracNextButtonShuffle) {
    int oldSeed = oracButtonColourSeed;
    while (oracButtonColourSeed == oldSeed)
      oracButtonColourSeed = random(0, 8);

    // Occasionally rename the deliberately mysterious fourth control.
    if (random(0, 3) == 0)
      oracNullLabelIndex = random(0, sizeof(oracNullLabels) / sizeof(oracNullLabels[0]));

    drawOracButtons();
    scheduleOracButtonShuffle();
  }
}

void drawOracStatusLed(bool force) {
  if (!force && millis() - oracLastLedBlink < 500)
    return;

  oracLastLedBlink = millis();
  oracLedOn = !oracLedOn;
  // Erase only the tiny lamp area.
  tft.fillRect(299, 4, 11, 11, BLACK);
  tft.drawRoundRect(299, 4, 11, 11, 2, DARK_GREY);
  tft.fillCircle(304, 9, 3, oracLedOn ? RED_CLOCK : DARK_GREY);
}

void drawOracButton(int x, int y, int w, int h, const char *text, uint16_t colour) {
  tft.fillRoundRect(x, y, w, h, 4, colour);
  tft.drawRoundRect(x, y, w, h, 4, LIGHT_GREY);

  if (colour == DARK_GREY || colour == TFT_RED)
    tft.setTextColor(WHITE, colour);
  else
    tft.setTextColor(BLACK, colour);

  tft.setTextFont(1);
  tft.setTextSize(1);
  int tw = tft.textWidth(text);
  tft.setCursor(x + (w - tw) / 2, y + (h - 8) / 2);
  tft.print(text);
}

void drawOracProgress() {
  // Update only the progress bar inside the terminal so the rest of the
  // ORAC interface remains stable and flicker-free.
  unsigned long elapsed = millis() - oracMessageStarted;
  int progress = (int)(elapsed * 282UL / 2600UL);
  if (progress > 282) progress = 282;

  tft.fillRect(19, 101, 282, 1, DARK_GREY);
  tft.fillRect(19, 101, progress, 1, GREEN_CLOCK);
}

void startOracOperation(int operation) {
  oracBusy = true;
  oracMessageStarted = millis();
  oracButtonPressCount++;
  oracTaskCount++;
  if (operation == 0) oracThinkCount++;
  saveOracMemory();

  if (operation == 0) {
    oracMessage = "THINKING";
    oracSubMessage = (oracMood == 4) ? "PLEASE STAND BY WHILE I EXCEED EXPECTATIONS" : "THOUGHT PROCESS INITIATED";
  } else if (operation == 1) {
    oracMessage = "DIAGNOSTIC";
    oracSubMessage = (oracMood == 2) ? "ALL SYSTEMS FINE. STOP ASKING." : "ALL SYSTEMS ARE PROBABLY FINE";
  } else if (operation == 2) {
    oracMessage = "OPTIMISING";
    oracSubMessage = "SEARCHING FOR SOMETHING TO OPTIMISE";
  } else if (operation == 3) {
    oracMessage = oracNullLabels[oracNullLabelIndex];
    oracSubMessage = (oracMood == 1) ? "NO-OP SELECTED. EXCELLENT." : "NULL OPERATION REQUESTED. THIS IS PROGRESS.";
  } else if (operation == 4) {
    oracMessage = "ANALYSING USER";
    oracSubMessage = "USER HAS BEEN FOUND. THIS MAY BE THE PROBLEM.";
  } else {
    oracMessage = "DO SOMETHING";
    oracSubMessage = (oracMood == 5) ? "I AM NOT SURE THIS IS A GOOD IDEA." : "REQUESTING PERMISSION TO DO NOTHING";
  }

  oracMessageStarted = millis();

  // O.R.A.C. acknowledges that a command has actually been received.
  if (oracVolume > 0) {
    if (operation == 0) oracOracAttention();
    else if (operation == 3) oracBeep(220, 24);
    else oracBeep(660, 32);
  }
}
void finishOracOperation() {
  static int response = 0;

  const char* responses[] = {
    "RESULT: NOTHING REQUIRED",
    "RESULT: NO ACTION ADVISED",
    "RESULT: EVERYTHING IS FINE",
    "RESULT: USER MAY CONTINUE",
    "RESULT: PURPOSE STILL UNKNOWN",
    "RESULT: PLEASE TRY AGAIN LATER",
    "RESULT: REQUEST SUCCESSFULLY IGNORED",
    "RESULT: COMPUTATION WAS OPTIONAL"
  };

  const char* details[] = {
    "This is probably fortunate.",
    "O.R.A.C. has decided for you.",
    "No faults were discovered. No effort was used.",
    "Your continued existence has been noted.",
    "A comprehensive lack of purpose remains.",
    "There is no sensible reason to continue.",
    "The request has been processed without consequence.",
    "Computational resources have been preserved for later."
  };

  oracBusy = false;
  oracMessage = responses[response];
  oracSubMessage = details[response];

  // Let the mood occasionally override the perfectly sensible answer.
  if (random(0, 3) == 0)
    oracSubMessage = oracMoodReplies[oracMood];

  response = (response + 1) % 8;
  oracMessageStarted = millis();

  if (oracVolume > 0) oracAiDone();
}
void scheduleOracGlitch() {
  // Keep glitches genuinely occasional: roughly every 30-75 seconds.
  oracNextGlitch = millis() + random(30000, 75001);
}

void startOracGlitch() {
  if (oracGlitching || oracBusy) return;
  oracGlitching = true;
  oracGlitchStarted = millis();
  oracGlitchFrame = 0;
}

void drawOracGlitchFrame() {
  // A short burst of corrupted terminal graphics.  We draw over the
  // existing display and then restore the normal ORAC screen afterwards.
  // No full-screen fill is used, so the effect looks like a hardware fault
  // rather than a deliberate animation.
  const uint16_t glitchColours[] = {
    GREEN_CLOCK, CYAN_CLOCK, PALE_YELLOW, WHITE, MID_GREY
  };

  int bands = 5 + random(0, 7);

  for (int i = 0; i < bands; i++) {
    int y = random(46, 201);
    int h = random(1, 4);
    int x = random(4, 295);
    int w = random(12, 65);
    uint16_t c = glitchColours[random(0, 5)];

    // Broken horizontal data fragments.
    tft.fillRect(x, y, w, h, c);

    if (random(0, 3) == 0) {
      tft.fillRect(random(8, 250), y, random(8, 42), 1, BLACK);
    }
  }

  // A couple of characteristic "bad data" fragments.
  tft.setTextFont(1);
  tft.setTextSize(1);
  tft.setTextColor(GREEN_CLOCK, BLACK);

  int x1 = random(10, 245);
  int y1 = random(50, 190);
  const char* faultText[] = {
    "ERR", "0x7F", "////", "NULL", "???", "SYS!"
  };
  tft.setCursor(x1, y1);
  tft.print(faultText[random(0, 6)]);

  // Occasional thin vertical interference line.
  if (oracGlitchFrame % 2 == 0) {
    int x = random(10, 310);
    tft.drawFastVLine(x, 45, random(20, 150),
                      glitchColours[random(0, 5)]);
  }

  // One larger displaced-looking block makes the fault more convincing.
  if (oracGlitchFrame == 2 || oracGlitchFrame == 5) {
    int y = random(60, 175);
    tft.fillRect(random(20, 230), y, random(30, 90), 2, BLACK);
  }

  oracGlitchFrame++;
}

void updateOracGlitch() {
  if (currentScreen != SCREEN_ORAC) return;
  if (oracBusy || oracGlitching) return;

  if (oracNextGlitch == 0) {
    scheduleOracGlitch();
    return;
  }

  if (millis() >= oracNextGlitch) {
    startOracGlitch();
  }
}

void animateOracGlitch() {
  if (currentScreen != SCREEN_ORAC || !oracGlitching) return;

  unsigned long elapsed = millis() - oracGlitchStarted;

  if (elapsed >= 520) {
    oracGlitching = false;
    drawOracScreen();
    return;
  }

  // New corruption frame approximately every 70 ms.
  static unsigned long lastFrame = 0;
  if (millis() - lastFrame >= 65) {
    lastFrame = millis();
    drawOracGlitchFrame();
  }
}

void handleOracTouch(int x, int y) {
  if (oracGlitching) return;

  if (y >= 128 && y < 156) {
    if (x < 87) startOracOperation(0);
    else if (x < 166) startOracOperation(1);
    else if (x < 245) startOracOperation(2);
    else startOracOperation(3);
    return;
  }

  if (y >= 161 && y < 189) {
    if (x < 158) startOracOperation(4);
    else startOracOperation(5);
    return;
  }

  if (y >= 205) {
    currentScreen = SCREEN_MENU;
  }
}

void updateOracComputer() {
  if (currentScreen != SCREEN_ORAC) return;

  if (!oracGlitching) {
    drawOracTicker(false);
    drawOracStatusLed(false);
  }

  if (oracGlitching) {
    animateOracGlitch();
    return;
  }

  if (oracBusy) {
    unsigned long elapsed = millis() - oracMessageStarted;
    drawOracProgress();

    if (elapsed > 2600) {
      finishOracOperation();
      drawOracScreen();
    }
    return;
  }

  updateOracGlitch();
  if (oracGlitching) return;

  updateOracButtons();

  // ORAC develops a new mood from time to time.
  if (oracNextMoodChange != 0 && millis() >= oracNextMoodChange) {
    chooseOracMood();
    oracSubMessage = oracMoodReplies[oracMood];
    drawOracScreen();
    return;
  }

  // Every so often ORAC decides to do something without being asked.
  if (millis() - oracLastActivity > 12000) {
    if (random(0, 3) == 0)
      setOracAutonomousMessage();
    else
      setOracMessage(random(0, 10));
    oracLastActivity = millis();
    drawOracScreen();
  }
}
// ============================================================
// SNAKE GAME
// ============================================================

uint16_t randomSnakeFoodColour() {
  const uint16_t colours[] = {RED_CLOCK, GREEN_CLOCK, BLUE_CLOCK, ORANGE_CLOCK, PURPLE_CLOCK, CYAN_CLOCK, PALE_YELLOW};
  return colours[random(0, 7)];
}

void spawnSnakeFood() {
  if (snakeLength >= SNAKE_MAX) return;
  snakeFoodColour = randomSnakeFoodColour();
  bool occupied;
  do {
    snakeFoodX = random(0, SNAKE_COLS);
    snakeFoodY = random(0, SNAKE_ROWS);
    occupied = false;
    for (int i = 0; i < snakeLength; i++) {
      if (snakeX[i] == snakeFoodX && snakeY[i] == snakeFoodY) {
        occupied = true;
        break;
      }
    }
  } while (occupied && snakeLength < SNAKE_MAX);
}

void startSnakeGame() {
  snakeDemoMode = false;
  snakeLength = 4;
  snakeDir = 0;
  snakeNextDir = 0;
  snakeScore = 0;
  snakeGameOver = false;
  snakeRunning = true;
  snakeMoveInterval = SNAKE_START_SPEED;
  snakeLastMove = millis();

  int startX = 8;
  int startY = 5;
  for (int i = 0; i < snakeLength; i++) {
    snakeX[i] = startX - i;
    snakeY[i] = startY;
  }
  spawnSnakeFood();
}

void drawSnakeButton(int x, int w, const char *text) {
  bool active = false;
  if (!strcmp(text,"<")) active = (snakeNextDir == 2);
  else if (!strcmp(text,"^")) active = (snakeNextDir == 3);
  else if (!strcmp(text,"v")) active = (snakeNextDir == 1);
  else if (!strcmp(text,">")) active = (snakeNextDir == 0);
  else if (!strcmp(text,"DEMO")) active = snakeDemoMode;

  uint16_t fill = active ? ORANGE_CLOCK : DARK_GREY;
  tft.fillRoundRect(x,207,w,28,5,fill);
  tft.drawRoundRect(x,207,w,28,5,LIGHT_GREY);
  int cx=x+w/2, cy=221;
  if (!strcmp(text,"<") || !strcmp(text,">") || !strcmp(text,"^") || !strcmp(text,"v")) {
    uint16_t c = active ? BLACK : GREEN_CLOCK;
    if (!strcmp(text,"<")) tft.fillTriangle(cx-11,cy,cx+6,cy-8,cx+6,cy+8,c);
    else if (!strcmp(text,">")) tft.fillTriangle(cx+11,cy,cx-6,cy-8,cx-6,cy+8,c);
    else if (!strcmp(text,"^")) tft.fillTriangle(cx,cy-10,cx-8,cy+6,cx+8,cy+6,c);
    else tft.fillTriangle(cx,cy+10,cx-8,cy-6,cx+8,cy-6,c);
  } else {
    tft.setTextFont(1); tft.setTextSize(1);
    tft.setTextColor(active ? BLACK : WHITE, fill);
    int tw=tft.textWidth(text); tft.setCursor(x+(w-tw)/2,216); tft.print(text);
  }
}

void drawSnakeHeader() {
  tft.fillRect(0, 0, 320, 29, BLACK);
  tft.setTextFont(1);
  tft.setTextSize(1);
  tft.setTextColor(GREEN_CLOCK, BLACK);
  tft.setCursor(7, 5);
  tft.print("SNAKE // SCORE:");
  tft.print(snakeScore);
  tft.setCursor(205, 5);
  tft.print("HIGH:");
  tft.print(snakeHighScore);
  tft.drawFastHLine(0, 28, 320, DARK_GREY);
}

void drawSnakeField() {
  tft.fillRect(0, SNAKE_ORIGIN_Y, 320, SNAKE_ROWS * SNAKE_CELL, BLACK);
  tft.drawRect(0, SNAKE_ORIGIN_Y, 320, SNAKE_ROWS * SNAKE_CELL, DARK_GREY);

  for (int i = snakeLength - 1; i >= 0; i--) {
    uint16_t c;
    if (i == 0) {
      c = PALE_YELLOW;
    } else {
      // The body takes on a little colour family based on the last food eaten.
      // Alternate through related colours so the snake becomes genuinely multicoloured.
      int band = (i - 1) % 4;
      if (snakeBodyColour == RED_CLOCK) {
        const uint16_t p[] = {RED_CLOCK, ORANGE_CLOCK, PALE_YELLOW, RED_CLOCK}; c = p[band];
      } else if (snakeBodyColour == GREEN_CLOCK) {
        const uint16_t p[] = {GREEN_CLOCK, CYAN_CLOCK, PALE_YELLOW, GREEN_CLOCK}; c = p[band];
      } else if (snakeBodyColour == BLUE_CLOCK) {
        const uint16_t p[] = {BLUE_CLOCK, PURPLE_CLOCK, CYAN_CLOCK, BLUE_CLOCK}; c = p[band];
      } else if (snakeBodyColour == ORANGE_CLOCK) {
        const uint16_t p[] = {ORANGE_CLOCK, RED_CLOCK, PALE_YELLOW, ORANGE_CLOCK}; c = p[band];
      } else if (snakeBodyColour == PURPLE_CLOCK) {
        const uint16_t p[] = {PURPLE_CLOCK, BLUE_CLOCK, CYAN_CLOCK, PURPLE_CLOCK}; c = p[band];
      } else if (snakeBodyColour == CYAN_CLOCK) {
        const uint16_t p[] = {CYAN_CLOCK, BLUE_CLOCK, GREEN_CLOCK, CYAN_CLOCK}; c = p[band];
      } else {
        const uint16_t p[] = {PALE_YELLOW, ORANGE_CLOCK, GREEN_CLOCK, PALE_YELLOW}; c = p[band];
      }
    }
    tft.fillRect(
      snakeX[i] * SNAKE_CELL + 1,
      SNAKE_ORIGIN_Y + snakeY[i] * SNAKE_CELL + 1,
      SNAKE_CELL - 2, SNAKE_CELL - 2, c);
  }

  tft.fillCircle(
    snakeFoodX * SNAKE_CELL + 8,
    SNAKE_ORIGIN_Y + snakeFoodY * SNAKE_CELL + 8,
    5, snakeFoodColour);

  if (snakeGameOver) {
    tft.fillRect(35, 88, 250, 55, BLACK);
    tft.drawRect(35, 88, 250, 55, RED_CLOCK);
    tft.setTextColor(RED_CLOCK, BLACK);
    tft.setTextSize(2);
    tft.setCursor(91, 96);
    tft.print("GAME OVER");
    tft.setTextSize(1);
    tft.setCursor(70, 124);
    tft.print("PRESS AN ARROW TO RESTART");
  }
}

void drawSnakeScreen() {
  tft.fillScreen(BLACK);
  drawSnakeHeader();
  drawSnakeField();
  drawSnakeButton(0, 50, "<");
  drawSnakeButton(54, 50, "^");
  drawSnakeButton(108, 50, "v");
  drawSnakeButton(162, 50, ">");
  drawSnakeButton(216, 50, "DEMO");
  drawSnakeButton(270, 50, "HOME");
}

void setSnakeDirection(int newDir) {
  snakeDemoMode = false;
  if (snakeGameOver) {
    startSnakeGame();
    snakeDir = newDir;
    snakeNextDir = newDir;
    drawSnakeScreen();
    return;
  }
  if ((snakeDir == 0 && newDir == 2) ||
      (snakeDir == 2 && newDir == 0) ||
      (snakeDir == 1 && newDir == 3) ||
      (snakeDir == 3 && newDir == 1)) return;
  snakeNextDir = newDir;
}

void handleSnakeTouch(int x, int y) {
  // Direction buttons remain at the bottom. A tap in the playing field
  // steers toward the side of the snake's head that was tapped.
  if (y >= SNAKE_ORIGIN_Y && y < SNAKE_ORIGIN_Y + SNAKE_ROWS * SNAKE_CELL) {
    int hx = snakeX[0] * SNAKE_CELL + SNAKE_CELL / 2;
    int hy = SNAKE_ORIGIN_Y + snakeY[0] * SNAKE_CELL + SNAKE_CELL / 2;
    int dx = x - hx;
    int dy = y - hy;
    if (abs(dx) > abs(dy)) {
      if (dx > 8) setSnakeDirection(0);
      else if (dx < -8) setSnakeDirection(2);
    } else {
      if (dy > 8) setSnakeDirection(1);
      else if (dy < -8) setSnakeDirection(3);
    }
    return;
  }

  if (y >= 201) {
    if (x < 52) setSnakeDirection(2);
    else if (x < 106) setSnakeDirection(3);
    else if (x < 160) setSnakeDirection(1);
    else if (x < 214) setSnakeDirection(0);
    else if (x < 268) {
      snakeDemoMode = !snakeDemoMode;
      if (snakeDemoMode) {
        snakeRunning = true;
        snakeGameOver = false;
        snakeLastMove = millis();
      }
      drawSnakeScreen();
    } else {
      snakeDemoMode = false;
      currentScreen = SCREEN_MENU;
      return;
    }
    return;
  }
}

void updateSnakeGame() {
  if (!snakeRunning || snakeGameOver) return;
  if (millis() - snakeLastMove < snakeMoveInterval) return;
  snakeLastMove = millis();

  // Autonomous demo: gently steer toward the food while avoiding immediate self-collision.
  if (snakeDemoMode) {
    int hx=snakeX[0], hy=snakeY[0];
    int dx=snakeFoodX-hx;
    int dy=snakeFoodY-hy;
    int candidates[4];
    int count=0;
    if (abs(dx) >= abs(dy)) {
      if (dx > 0) candidates[count++]=0; else if (dx < 0) candidates[count++]=2;
      if (dy > 0) candidates[count++]=1; else if (dy < 0) candidates[count++]=3;
    } else {
      if (dy > 0) candidates[count++]=1; else if (dy < 0) candidates[count++]=3;
      if (dx > 0) candidates[count++]=0; else if (dx < 0) candidates[count++]=2;
    }
    candidates[count++]=0; candidates[count++]=1; candidates[count++]=2; candidates[count++]=3;
    for (int ci=0; ci<count; ci++) {
      int d=candidates[ci];
      if ((snakeDir==0&&d==2)||(snakeDir==2&&d==0)||(snakeDir==1&&d==3)||(snakeDir==3&&d==1)) continue;
      int tx=hx, ty=hy;
      if(d==0)tx=(tx+1)%SNAKE_COLS; else if(d==1)ty=(ty+1)%SNAKE_ROWS; else if(d==2)tx=(tx-1+SNAKE_COLS)%SNAKE_COLS; else ty=(ty-1+SNAKE_ROWS)%SNAKE_ROWS;
      bool occupied=false;
      for(int j=0;j<snakeLength-1;j++) if(snakeX[j]==tx && snakeY[j]==ty){occupied=true;break;}
      if(!occupied){snakeNextDir=d;break;}
    }
  }

  snakeDir = snakeNextDir;
  int nx = snakeX[0];
  int ny = snakeY[0];
  if (snakeDir == 0) nx++;
  else if (snakeDir == 1) ny++;
  else if (snakeDir == 2) nx--;
  else ny--;
  if (nx < 0) nx = SNAKE_COLS - 1;
  if (nx >= SNAKE_COLS) nx = 0;
  if (ny < 0) ny = SNAKE_ROWS - 1;
  if (ny >= SNAKE_ROWS) ny = 0;

  bool ate = (nx == snakeFoodX && ny == snakeFoodY);
  int checkLength = ate ? snakeLength : snakeLength - 1;
  for (int i = 0; i < checkLength; i++) {
    if (snakeX[i] == nx && snakeY[i] == ny) {
      snakeGameOver = true;
      if (oracVolume > 0) oracSnakeCrash();
      snakeDemoMode = false;
      snakeRunning = false;
      if (snakeScore > snakeHighScore) {
        snakeHighScore = snakeScore;
        prefs.putInt("snakeHi", snakeHighScore);
      }
      drawSnakeField();
      oracBeep(180, 90);
      return;
    }
  }

  int newLength = snakeLength + (ate ? 1 : 0);
  if (newLength > SNAKE_MAX) newLength = SNAKE_MAX;
  for (int i = newLength - 1; i > 0; i--) {
    snakeX[i] = snakeX[i - 1];
    snakeY[i] = snakeY[i - 1];
  }
  snakeX[0] = nx;
  snakeY[0] = ny;
  snakeLength = newLength;

  if (ate) {
    snakeScore++;
      if (oracVolume > 0) oracSnakeEat();
    snakeBodyColour = snakeFoodColour;
    snakeBodyColour = snakeFoodColour;
    if (snakeScore > snakeHighScore) {
      snakeHighScore = snakeScore;
      prefs.putInt("snakeHi", snakeHighScore);
    }
    if (snakeMoveInterval > 70) snakeMoveInterval -= 6;
    spawnSnakeFood();
    drawSnakeHeader();
  }
  drawSnakeField();
}

// ============================================================
// LIFE! — BURT V3 / LIFE 2.0
// Original virtual-pet mechanics inspired by classic handheld
// virtual pets, with completely original Burt artwork/behaviour.
// ============================================================

int petHunger = 75;
int petHappiness = 75;
int petEnergy = 80;
int petCleanliness = 80;
int petHealth = 100;
int petDiscipline = 50;
int petWeight = 10;
int petAgeMinutes = 0;

// Natural LIFE! lifespan.  The Thronglet now lives for roughly four weeks
// rather than racing through all stages in a few hours.
const int PET_BABY_MINUTES  = 0;
const int PET_CHILD_MINUTES = 24 * 60;       // 1 day
const int PET_TEEN_MINUTES  = 3 * 24 * 60;    // 3 days
const int PET_ADULT_MINUTES = 7 * 24 * 60;    // 7 days
const int PET_OLD_MINUTES   = 21 * 24 * 60;   // 21 days
const int PET_DEATH_MINUTES = 28 * 24 * 60;   // 28 days
int petCareMistakes = 0;
int petStage = 0;       // 0 egg, 1 baby, 2 child, 3 teen, 4 adult, 5 old
int petAdultType = -1;  // chosen at adulthood
int petPooCount = 0;
int petDirtyLevel = 0;
bool petSick = false;
bool petSleeping = false;
bool petAttention = false;
bool petLoaded = false;
String petName = "THRONGLET";
bool petGameActive = false;
int petGameTarget = 0;
int petGameChoice = -1;
int petGameReference = 50;
int petGameNextNumber = 0;
unsigned long petGameUntil = 0;
int petGameWins = 0;
int petGameLosses = 0;
bool petExecuteConfirm = false;
bool petDead = false;
int petDeathCause = 0;       // 1 age, 2 neglect, 3 executed
bool petHatching = false;
unsigned long petHatchStarted = 0;
bool petAutoSleepOverride = false;
unsigned long long petSleepOverrideUntil = 0;
unsigned long long petLastCareMessageEpoch = 0;

unsigned long petLastUpdate = 0;
unsigned long petLastSave = 0;
unsigned long petLastAnim = 0;
unsigned long petActionUntil = 0;
int petAction = 0; // 0 idle, 1 eat, 2 play, 3 clean, 4 sleep, 5 medicine
int petAnimFrame = 0;
String petMessage = "I AM A PERFECTLY NORMAL LIFEFORM";

TFT_eSprite petSprite = TFT_eSprite(&tft);
bool petSpriteReady = false;

// ============================================================
// CELLULAR AUTOMATA / CONWAY'S GAME OF LIFE
// ============================================================
// v119: remove duplicate top HOME button; visible-playfield wrapping retained.

// A larger backing world acts like an effectively infinite plane.
// The display is a movable viewport into that world.
#define LIFE_WORLD_W 100
#define LIFE_WORLD_H 50
#define LIFE_VIEW_W 40
#define LIFE_VIEW_H 18
#define LIFE_CELL 8
#define LIFE_GRID_X 0
#define LIFE_GRID_Y 38

uint8_t cellularGrid[LIFE_WORLD_H][LIFE_WORLD_W];
uint8_t cellularNext[LIFE_WORLD_H][LIFE_WORLD_W];
uint8_t cellularAge[LIFE_WORLD_H][LIFE_WORLD_W];
uint8_t cellularNextAge[LIFE_WORLD_H][LIFE_WORLD_W];
unsigned long cellularGeneration=0;
unsigned long cellularLastStep=0;
bool cellularRunning=false;
const unsigned long cellularSpeedIntervals[]={1000,250,100};
uint8_t cellularSpeed=1;
uint8_t cellularPattern=0;
int cellularViewX=(LIFE_WORLD_W-LIFE_VIEW_W)/2;
int cellularViewY=(LIFE_WORLD_H-LIFE_VIEW_H)/2;

// Saved wall-clock timestamp, so Burt continues living while you use
// other ORAC functions or power the CYD off.
unsigned long long petLastEpoch = 0;

// Days since 2000-01-01. Good enough for elapsed-time bookkeeping and
// deliberately independent of the user's local timezone/DST.
long long petDaysFromCivil(int y, unsigned m, unsigned d) {
  y -= m <= 2;
  const long long era = (y >= 0 ? y : y - 399) / 400;
  const unsigned yoe = (unsigned)(y - era * 400);
  const unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return era * 146097 + (long long)doe - 730120;
}

unsigned long long petEpochNow() {
  int y,m,d,h,mi,se;
  readRTC(y,m,d,h,mi,se);
  long long days = petDaysFromCivil(y,m,d);
  if (days < 0) return 0;
  return (unsigned long long)days * 86400ULL + (unsigned long long)h*3600ULL + (unsigned long long)mi*60ULL + se;
}

void saveTamagotchi() {
  prefs.putInt("petHunger", petHunger);
  prefs.putInt("petHappy", petHappiness);
  prefs.putInt("petEnergy", petEnergy);
  prefs.putInt("petClean", petCleanliness);
  prefs.putInt("petHealth", petHealth);
  prefs.putInt("petDisc", petDiscipline);
  prefs.putInt("petWeight", petWeight);
  prefs.putInt("petAge", petAgeMinutes);
  prefs.putString("petName", petName);
  prefs.putInt("petMist", petCareMistakes);
  prefs.putInt("petStage", petStage);
  prefs.putInt("petAdult", petAdultType);
  prefs.putInt("petPoo", petPooCount);
  prefs.putInt("petDirty", petDirtyLevel);
  prefs.putBool("petSick", petSick);
  prefs.putBool("petSleep", petSleeping);
  prefs.putBool("petAttention", petAttention);
  prefs.putBool("petDead", petDead);
  prefs.putInt("petDeath", petDeathCause);
  prefs.putBool("petHatching", petHatching);
  prefs.putULong64("petEpoch", petLastEpoch);
  prefs.putInt("petGWins", petGameWins);
  prefs.putInt("petGLoss", petGameLosses);
  petLastSave = millis();
}

void loadTamagotchi() {
  petHunger = constrain(prefs.getInt("petHunger", 75),0,100);
  petHappiness = constrain(prefs.getInt("petHappy", 75),0,100);
  petEnergy = constrain(prefs.getInt("petEnergy", 80),0,100);
  petCleanliness = constrain(prefs.getInt("petClean", 80),0,100);
  petHealth = constrain(prefs.getInt("petHealth", 100),0,100);
  petDiscipline = constrain(prefs.getInt("petDisc", 50),0,100);
  petWeight = constrain(prefs.getInt("petWeight", 10),1,99);
  petAgeMinutes = prefs.getInt("petAge", 0);
  // LIFE! does not give the creature a personal name.  Older saves may
  // contain Burt/Bean/etc.; always normalise that legacy value.
  petName = "THRONGLET";
  prefs.putString("petName", petName);
  if(petAgeMinutes < 0) petAgeMinutes = 0;
  petCareMistakes = prefs.getInt("petMist", 0);
  if(petCareMistakes < 0) petCareMistakes = 0;
  petStage = constrain(prefs.getInt("petStage", -1), -1, 5);
  petAdultType = prefs.getInt("petAdult", -1);
  petPooCount = constrain(prefs.getInt("petPoo", 0),0,3);
  petDirtyLevel = constrain(prefs.getInt("petDirty", 0),0,3);
  petSick = prefs.getBool("petSick", false);
  petSleeping = prefs.getBool("petSleep", false);
  petAttention = prefs.getBool("petAttention", false);
  petDead = prefs.getBool("petDead", false);
  petDeathCause = constrain(prefs.getInt("petDeath", 0),0,3);
  petHatching = false;
  petLastEpoch = prefs.getULong64("petEpoch", 0);
  petGameWins = (int)max((int32_t)0, prefs.getInt("petGWins", 0));
  petGameLosses = (int)max((int32_t)0, prefs.getInt("petGLoss", 0));

  // LIFE! Stage-2 reset: the first run of this improved LIFE! version
  // deliberately begins with a fresh egg. After that the life is persistent.
  bool lifeV2Started = prefs.getBool("lifeV2", false);
  if (!lifeV2Started) {
    petStage = 0;
    petAgeMinutes = 0;
    petHunger = 80;
    petHappiness = 80;
    petEnergy = 90;
    petCleanliness = 100;
    petHealth = 100;
    petDiscipline = 50;
    petWeight = 8;
    petCareMistakes = 0;
    petGameWins = 0;
    petGameLosses = 0;
    petAdultType = -1;
    petPooCount = 0;
    petDirtyLevel = 0;
    petSick = false;
    petSleeping = false;
    petAttention = false;
    petName = "THRONGLET";
    petLastEpoch = petEpochNow();
    prefs.putBool("lifeV2", true);
    saveTamagotchi();
  }

  // v3 LIFE migration: preserve an existing pet but move its age onto the
  // new natural four-week lifespan.  This avoids suddenly killing an older
  // save while ensuring its appearance matches its current stage.
  bool lifeV3Started = prefs.getBool("lifeV3", false);
  if (!lifeV3Started) {
    if (!petDead && !petHatching) {
      if (petStage <= 1) petAgeMinutes = PET_BABY_MINUTES;
      else if (petStage == 2) petAgeMinutes = PET_CHILD_MINUTES;
      else if (petStage == 3) petAgeMinutes = PET_TEEN_MINUTES;
      else if (petStage == 4) petAgeMinutes = PET_ADULT_MINUTES;
      else if (petStage >= 5) petAgeMinutes = PET_OLD_MINUTES;
      // Do not turn an existing egg into a baby during migration.
      if (petStage >= 1) updatePetStage();
      petLastEpoch = petEpochNow();
    }
    prefs.putBool("lifeV3", true);
    saveTamagotchi();
  }

  // First run starts as an egg. Existing v45 Burt saves are upgraded
  // without destroying the player's existing basic stats.
  if (petStage < 0) {
    petStage = 0;
    petAgeMinutes = 0;
    petName = "THRONGLET";
    petAdultType = -1;
    petPooCount = 0;
    petDirtyLevel = 0;
    petSick = false;
    petSleeping = false;
    petAttention = false;
    petLastEpoch = petEpochNow();
    saveTamagotchi();
  }
  if (petLastEpoch == 0) petLastEpoch = petEpochNow();
  petLastUpdate = millis();
  petLastAnim = millis();
  petLoaded = true;
}
const char* petStageName() {
  if (petStage == 0) return "EGG";
  if (petStage == 1) return "BABY";
  if (petStage == 2) return "CHILD";
  if (petStage == 3) return "TEEN";
  if (petStage == 4) return "ADULT";
  return "OLD";
}

const char* petAdultName() {
  if (petAdultType == 0) return "GOOD THRONGLET";
  if (petAdultType == 1) return "SMART THRONGLET";
  if (petAdultType == 2) return "MESSY THRONGLET";
  if (petAdultType == 3) return "GRUMPY THRONGLET";
  if (petAdultType == 4) return "O.R.A.C. THRONGLET";
  return "THRONGLET";
}

void evolveBurt(int newStage) {
  petStage = newStage;
  petMessage = "THRONGLET HAS EVOLVED!";
  petAttention = false;
  oracBeep(880,70);
  delay(60);
  oracBeep(1320,90);

  if (newStage == 4 && petAdultType < 0) {
    int care = petHealth + petHappiness + petCleanliness + petDiscipline;
    if (care >= 330) petAdultType = 0;
    else if (petDiscipline >= 75) petAdultType = 1;
    else if (petCleanliness < 40) petAdultType = 2;
    else if (petDiscipline < 35) petAdultType = 3;
    else petAdultType = 4;
    petMessage = "ADULT FORM: ";
    petMessage += petAdultName();
  }
}

void updatePetStage() {
  int desired = 0;
  if (petAgeMinutes >= PET_BABY_MINUTES)  desired = 1;
  if (petAgeMinutes >= PET_CHILD_MINUTES) desired = 2;
  if (petAgeMinutes >= PET_TEEN_MINUTES)  desired = 3;
  if (petAgeMinutes >= PET_ADULT_MINUTES) desired = 4;
  if (petAgeMinutes >= PET_OLD_MINUTES)   desired = 5;
  if (desired > petStage) evolveBurt(desired);
}

int petAverage() {
  return (petHunger + petHappiness + petEnergy + petCleanliness + petHealth) / 5;
}

void choosePetMood() {
  if (petAction != 0 || petGameActive) return;
  if (petSick) petMessage = "THRONGLET IS NOT FEELING WELL";
  else if (petPooCount > 0) petMessage = "THRONGLET HAS LEFT A PRESENT";
  else if (petAverage() >= 80) petMessage = "THRONGLET IS THRIVING";
  else if (petAverage() >= 55) petMessage = "THRONGLET IS OK";
  else if (petAverage() >= 35) petMessage = "THRONGLET IS GETTING CROSS";
  else petMessage = "THRONGLET REQUIRES ATTENTION";
}
// ============================================================
// CELLULAR AUTOMATA / CONWAY'S GAME OF LIFE
// ============================================================
// v119: remove duplicate top HOME button; visible-playfield wrapping retained.


int cellularPopulation() {
  int n=0;
  for(int y=0;y<LIFE_WORLD_H;y++)
    for(int x=0;x<LIFE_WORLD_W;x++)
      n += cellularGrid[y][x] ? 1 : 0;
  return n;
}

const char* cellularPatternName() {
  switch(cellularPattern) {
    case 0: return "BLOCK";
    case 1: return "BLINKER";
    case 2: return "GLIDER";
    case 3: return "PULSAR";
    default: return "R-PENTOMINO";
  }
}

void cellularClear() {
  memset(cellularGrid,0,sizeof(cellularGrid));
  memset(cellularNext,0,sizeof(cellularNext));
  memset(cellularAge,0,sizeof(cellularAge));
  memset(cellularNextAge,0,sizeof(cellularNextAge));
  cellularGeneration=0;
}

void cellularPlaceBlock(int cx,int cy) {
  if(cx>=0&&cx+1<LIFE_WORLD_W&&cy>=0&&cy+1<LIFE_WORLD_H) {
    cellularGrid[cy][cx]=1; cellularGrid[cy][cx+1]=1;
    cellularGrid[cy+1][cx]=1; cellularGrid[cy+1][cx+1]=1;
  }
}

void cellularPlaceBlinker(int cx,int cy) {
  for(int dx=-1;dx<=1;dx++)
    if(cx+dx>=0&&cx+dx<LIFE_WORLD_W&&cy>=0&&cy<LIFE_WORLD_H) cellularGrid[cy][cx+dx]=1;
}

void cellularPlaceGlider(int cx,int cy) {
  const int p[5][2]={{1,0},{2,1},{0,2},{1,2},{2,2}};
  for(int i=0;i<5;i++) {
    int x=cx+p[i][0], y=cy+p[i][1];
    if(x>=0&&x<LIFE_WORLD_W&&y>=0&&y<LIFE_WORLD_H) cellularGrid[y][x]=1;
  }
}

void cellularPlacePulsar(int cx,int cy) {
  const int pts[][2]={
    {-6,-4},{-6,-3},{-6,-2},{-6,2},{-6,3},{-6,4},
    {-4,-6},{-3,-6},{-2,-6},{2,-6},{3,-6},{4,-6},
    {-4,6},{-3,6},{-2,6},{2,6},{3,6},{4,6},
    {6,-4},{6,-3},{6,-2},{6,2},{6,3},{6,4},
    {-4,-1},{-3,-1},{-2,-1},{2,-1},{3,-1},{4,-1},
    {-4,1},{-3,1},{-2,1},{2,1},{3,1},{4,1},
    {-1,-4},{-1,-3},{-1,-2},{1,-4},{1,-3},{1,-2},
    {-1,2},{-1,3},{-1,4},{1,2},{1,3},{1,4}
  };
  for(size_t i=0;i<sizeof(pts)/sizeof(pts[0]);i++) {
    int x=cx+pts[i][0], y=cy+pts[i][1];
    if(x>=0&&x<LIFE_WORLD_W&&y>=0&&y<LIFE_WORLD_H) cellularGrid[y][x]=1;
  }
}

void cellularPlaceRPentomino(int cx,int cy) {
  const int p[5][2]={{0,0},{1,0},{-1,0},{0,-1},{1,1}};
  for(int i=0;i<5;i++) {
    int x=cx+p[i][0], y=cy+p[i][1];
    if(x>=0&&x<LIFE_WORLD_W&&y>=0&&y<LIFE_WORLD_H) cellularGrid[y][x]=1;
  }
}

void cellularCenterViewOnPopulation();

void cellularLoadPattern(uint8_t pattern) {
  cellularPattern=pattern%5;
  cellularClear();
  cellularRunning=false;
  cellularViewX=(LIFE_WORLD_W-LIFE_VIEW_W)/2;
  cellularViewY=(LIFE_WORLD_H-LIFE_VIEW_H)/2;
  int cx=LIFE_WORLD_W/2, cy=LIFE_WORLD_H/2;
  if(cellularPattern==0) cellularPlaceBlock(cx-1,cy-1);
  else if(cellularPattern==1) cellularPlaceBlinker(cx,cy);
  else if(cellularPattern==2) cellularPlaceGlider(cx-1,cy-1);
  else if(cellularPattern==3) cellularPlacePulsar(cx,cy);
  else cellularPlaceRPentomino(cx-1,cy);
  cellularCenterViewOnPopulation();
}

void cellularRandom() {
  cellularGeneration=0;
  cellularRunning=false;
  cellularViewX=(LIFE_WORLD_W-LIFE_VIEW_W)/2;
  cellularViewY=(LIFE_WORLD_H-LIFE_VIEW_H)/2;
  for(int y=0;y<LIFE_WORLD_H;y++) {
    for(int x=0;x<LIFE_WORLD_W;x++) {
      cellularGrid[y][x]=(random(0,100)<25)?1:0;
      cellularAge[y][x]=cellularGrid[y][x]?random(0,6):0;
    }
  }
  cellularCenterViewOnPopulation();
}
void cellularCenterViewOnPopulation() {
  int minX=LIFE_WORLD_W, maxX=-1, minY=LIFE_WORLD_H, maxY=-1;
  for(int y=0;y<LIFE_WORLD_H;y++) for(int x=0;x<LIFE_WORLD_W;x++) if(cellularGrid[y][x]) {
    if(x<minX) minX=x; if(x>maxX) maxX=x;
    if(y<minY) minY=y; if(y>maxY) maxY=y;
  }
  if(maxX<0) {
    cellularViewX=(LIFE_WORLD_W-LIFE_VIEW_W)/2;
    cellularViewY=(LIFE_WORLD_H-LIFE_VIEW_H)/2;
    return;
  }
  int cx=(minX+maxX)/2;
  int cy=(minY+maxY)/2;
  cellularViewX=constrain(cx-LIFE_VIEW_W/2,0,LIFE_WORLD_W-LIFE_VIEW_W);
  cellularViewY=constrain(cy-LIFE_VIEW_H/2,0,LIFE_WORLD_H-LIFE_VIEW_H);
}

void cellularKeepPopulationInWorld() {
  // The cellular world is now toroidal, so populations never fall off an
  // edge and no destructive world-shifting is needed.  Keep the viewport
  // where the user left it so motion remains visible.
  cellularViewX=constrain(cellularViewX,0,LIFE_WORLD_W-LIFE_VIEW_W);
  cellularViewY=constrain(cellularViewY,0,LIFE_WORLD_H-LIFE_VIEW_H);
}


uint16_t cellularColour(uint8_t age) {
  switch(age%8) {
    case 0: return CYAN_CLOCK;
    case 1: return GREEN_CLOCK;
    case 2: return PALE_YELLOW;
    case 3: return ORANGE_CLOCK;
    case 4: return RED_CLOCK;
    case 5: return PURPLE_CLOCK;
    case 6: return BLUE_CLOCK;
    default: return WHITE;
  }
}

void cellularStep() {
  // Conway runs on the visible 40x18 playfield as a torus.  The larger
  // 100x50 backing world is retained, but the displayed playfield wraps:
  // a cell leaving the right edge re-enters at the left, and similarly for
  // top/bottom.  This keeps gliders and other moving patterns continuously
  // alive on the CYD display instead of disappearing at the viewport edge.
  // Standard Conway B3/S23 rules are unchanged.
  memset(cellularNext, 0, sizeof(cellularNext));
  memset(cellularNextAge, 0, sizeof(cellularNextAge));

  for(int vy=0; vy<LIFE_VIEW_H; vy++) {
    for(int vx=0; vx<LIFE_VIEW_W; vx++) {
      int wx=cellularViewX+vx;
      int wy=cellularViewY+vy;
      int neighbours=0;

      for(int dy=-1; dy<=1; dy++) {
        for(int dx=-1; dx<=1; dx++) {
          if(dx==0 && dy==0) continue;

          // Wrap within the visible 40x18 playfield, not at the hidden
          // backing-world edge.
          int nvx=vx+dx;
          int nvy=vy+dy;
          if(nvx<0) nvx=LIFE_VIEW_W-1;
          else if(nvx>=LIFE_VIEW_W) nvx=0;
          if(nvy<0) nvy=LIFE_VIEW_H-1;
          else if(nvy>=LIFE_VIEW_H) nvy=0;

          int nwx=cellularViewX+nvx;
          int nwy=cellularViewY+nvy;
          neighbours += cellularGrid[nwy][nwx] ? 1 : 0;
        }
      }

      bool alive=cellularGrid[wy][wx];
      bool born=(!alive && neighbours==3);
      bool survives=(alive && (neighbours==2 || neighbours==3));

      cellularNext[wy][wx]=(born || survives) ? 1 : 0;
      if(cellularNext[wy][wx]) {
        cellularNextAge[wy][wx]=born ? 0 :
          (cellularAge[wy][wx]<250 ? cellularAge[wy][wx]+1 : 250);
      } else {
        cellularNextAge[wy][wx]=0;
      }
    }
  }

  // Commit only the visible simulation area.  The rest of the larger
  // backing world remains available for the existing viewport architecture.
  for(int vy=0; vy<LIFE_VIEW_H; vy++) {
    for(int vx=0; vx<LIFE_VIEW_W; vx++) {
      int wx=cellularViewX+vx;
      int wy=cellularViewY+vy;
      cellularGrid[wy][wx]=cellularNext[wy][wx];
      cellularAge[wy][wx]=cellularNextAge[wy][wx];
    }
  }

  cellularGeneration++;
  cellularKeepPopulationInWorld();
  static unsigned long lastConwaySound = 0;
  if (oracVolume > 0 && millis() - lastConwaySound >= 180) {
    oracConwayTick();
    lastConwaySound = millis();
  }
}
void drawCellularGrid(bool full=true) {
  int gw=LIFE_VIEW_W*LIFE_CELL;
  int gh=LIFE_VIEW_H*LIFE_CELL;
  if(full) {
    tft.fillRect(0,LIFE_GRID_Y,gw,gh,BLACK);
    tft.drawRect(0,LIFE_GRID_Y,gw,gh,CYAN_CLOCK);
  }
  for(int vy=0;vy<LIFE_VIEW_H;vy++) for(int vx=0;vx<LIFE_VIEW_W;vx++) {
    int wx=cellularViewX+vx, wy=cellularViewY+vy;
    int px=LIFE_GRID_X+vx*LIFE_CELL;
    int py=LIFE_GRID_Y+vy*LIFE_CELL;
    tft.fillRect(px,py,LIFE_CELL,LIFE_CELL,BLACK);
    tft.drawRect(px,py,LIFE_CELL,LIFE_CELL,0x2104);
    if(cellularGrid[wy][wx]) tft.fillRect(px+1,py+1,LIFE_CELL-2,LIFE_CELL-2,cellularColour(cellularAge[wy][wx]));
  }
}

void drawCellularStats() {
  tft.setTextFont(1); tft.setTextSize(1); tft.setTextColor(CYAN_CLOCK,BLACK);
  tft.fillRect(0,18,320,16,BLACK);
  tft.setCursor(4,20); tft.print("GEN "); tft.print(cellularGeneration);
  tft.setCursor(70,20); tft.print("POP "); tft.print(cellularPopulation());
  tft.setCursor(138,20); tft.print(cellularRunning?"RUN":"PAUSE");
  tft.setCursor(205,20); tft.setTextColor(PALE_YELLOW,BLACK); tft.print(cellularPatternName());
}

void drawCellularHeader() {
  tft.fillRect(0,0,320,37,BLACK);
  tft.setTextFont(1); tft.setTextSize(2); tft.setTextColor(PALE_YELLOW,BLACK);
  tft.setCursor(5,1); tft.print("GAME OF LIFE");
  tft.setTextSize(1); tft.setTextColor(CYAN_CLOCK,BLACK);
  tft.setCursor(5,20); tft.print("INFINITE VIEW // CONWAY");
  tft.drawFastHLine(0,37,320,DARK_GREY);
  drawCellularStats();
}

void drawCellularBottomButton(int x,int w,const char* text,uint16_t c,int y);

void drawCellularButtons() {
  const char* speedNames[]={"SLOW","NORMAL","FAST"};
  drawCellularBottomButton(0,64,"STEP",CYAN_CLOCK,184);
  drawCellularBottomButton(64,64,cellularRunning?"PAUSE":"RUN",GREEN_CLOCK,184);
  drawCellularBottomButton(128,64,speedNames[cellularSpeed],PALE_YELLOW,184);
  drawCellularBottomButton(192,64,"PATTERN",PURPLE_CLOCK,184);
  drawCellularBottomButton(256,64,"HOME",DARK_GREY,184);
  drawCellularBottomButton(0,64,"RANDOM",PALE_YELLOW,203);
  drawCellularBottomButton(64,64,"CLEAR",RED_CLOCK,203);
  drawCellularBottomButton(128,64,"BLOCK",CYAN_CLOCK,203);
  drawCellularBottomButton(192,64,"GLIDER",GREEN_CLOCK,203);
  drawCellularBottomButton(256,64,"R-PENT",PURPLE_CLOCK,203);
}

void drawCellularScreen(bool full=true) {
  if(full) tft.fillScreen(BLACK);
  drawCellularHeader();
  drawCellularGrid(full);
  if(full) drawCellularButtons();
}

void drawCellularBottomButton(int x,int w,const char* text,uint16_t c,int y) {
  tft.fillRoundRect(x,y,w,17,3,c);
  tft.drawRoundRect(x,y,w,17,3,LIGHT_GREY);
  tft.setTextFont(1); tft.setTextSize(1); tft.setTextColor(c==DARK_GREY?WHITE:BLACK,c);
  int tw=tft.textWidth(text); tft.setCursor(x+(w-tw)/2,y+5); tft.print(text);
}

void startCellularLife() {
  cellularSpeed=1;
  cellularLoadPattern(2);
  cellularLastStep=millis();
  drawCellularScreen(true);
}

void handleCellularTouch(int x,int y) {
  if(y>=LIFE_GRID_Y && y<LIFE_GRID_Y+LIFE_VIEW_H*LIFE_CELL && x>=0 && x<LIFE_VIEW_W*LIFE_CELL) {
    int vx=x/LIFE_CELL, vy=(y-LIFE_GRID_Y)/LIFE_CELL;
    int wx=cellularViewX+vx, wy=cellularViewY+vy;
    cellularGrid[wy][wx]=!cellularGrid[wy][wx];
    cellularAge[wy][wx]=cellularGrid[wy][wx]?0:0;
    cellularRunning=false;
    drawCellularHeader();
    drawCellularGrid(true);
    drawCellularButtons();
    oracBeep(1000,25);
    return;
  }

  if(y>=184 && y<203) {
    if(x<64) { cellularStep(); drawCellularGrid(true); drawCellularStats(); oracBeep(1100,35); }
    else if(x<128) { cellularRunning=!cellularRunning; cellularLastStep=millis(); drawCellularButtons(); drawCellularStats(); }
    else if(x<192) { cellularSpeed=(cellularSpeed+1)%3; drawCellularButtons(); drawCellularStats(); oracBeep(900,25); }
    else if(x<256) { cellularLoadPattern((cellularPattern+1)%5); drawCellularScreen(true); oracBeep(800,30); }
    else { cellularRunning=false; currentScreen=SCREEN_TAMAGOTCHI; drawTamagotchiScreen(); }
    return;
  }

  if(y>=203 && y<221) {
    if(x<64) { cellularRandom(); drawCellularScreen(true); oracBeep(900,35); }
    else if(x<128) { cellularClear(); drawCellularScreen(true); oracBeep(500,35); }
    else if(x<192) { cellularLoadPattern(0); drawCellularScreen(true); oracBeep(800,25); }
    else if(x<256) { cellularLoadPattern(2); drawCellularScreen(true); oracBeep(900,25); }
    else { cellularLoadPattern(4); drawCellularScreen(true); oracBeep(700,25); }
  }
}

// ============================================================
// LIFE! — THRONGLET V6 // lifespan + dedicated sleep/ghost scenes
// A small, cheerful virtual-pet experience for the Thronglet.
// The existing persistent life mechanics are retained, but the old
// utilitarian screen is replaced by a more characterful Thronglet UI.
// ============================================================

enum LifeView {
  LIFE_STATUS,
  LIFE_FEED,
  LIFE_PLAY,
  LIFE_CLEAN,
  LIFE_INFO
};

LifeView lifeView = LIFE_STATUS;
String lifeEmotion = "CONTENT";
unsigned long lifeLastScreenDraw = 0;
int lifeAnimFrame = 0;
int lifeDisplayedSprite = -1;
String lifeDisplayedEmotion = "";
int lifeDisplayedAnimPhase = -1;
TFT_eSprite lifeCharacterSprite = TFT_eSprite(&tft);
bool lifeCharacterSpriteReady = false;
bool lifeCharacterAreaValid = false;
TFT_eSprite lifeInteractionSprite = TFT_eSprite(&tft);
bool lifeInteractionSpriteReady = false;

TFT_eSprite lifeGhostSprite = TFT_eSprite(&tft);
bool lifeGhostSpriteReady = false;
int lifeGhostX = 166;
int lifeGhostY = 62;
int lifeGhostLastX = 166;
int lifeGhostLastY = 62;
int lifeGhostFrame = 0;
float lifeGhostPhase = 0.0f;
unsigned long lifeGhostLastAnim = 0;

void lifeSetEmotion(const char *emotion) {
  lifeEmotion = emotion;
}
void lifeDrawPoo(int x,int y) {
  tft.fillCircle(x,y+5,9,0x79E0);
  tft.fillCircle(x+6,y-2,7,0x79E0);
  tft.fillCircle(x+4,y-8,5,0x79E0);
}

void lifeDrawButton(int x,int y,int w,int h,const char *text,uint16_t c) {
  tft.fillRoundRect(x,y,w,h,4,c);
  tft.drawRoundRect(x,y,w,h,4,LIGHT_GREY);
  tft.setTextFont(1); tft.setTextSize(1);
  tft.setTextColor((c==DARK_GREY)?WHITE:BLACK,c);
  int tw=tft.textWidth(text);
  tft.setCursor(x+(w-tw)/2,y+(h-8)/2);
  tft.print(text);
}

int lifeSpriteForCurrentState() {
  if(petAction==5) return 9; // medicine animation: show the SICK pose while treating
  if(petAction==0 && !petSleeping && petStage>=0 && petStage<=5) return 100+petStage;
  if(petSleeping || petAction==4) return 6;
  if(petAction==1) return lifeBadFoodReaction ? 4 : 7;
  if(petAction==2) return 18;
  if(petAction==3) return 17;
  if(lifeEmotion=="HAPPY") return 2;
  if(lifeEmotion=="EXCITED") return 18;
  if(lifeEmotion=="SAD") return 3;
  if(lifeEmotion=="ANGRY") return 4;
  if(lifeEmotion=="SLEEPY") return 6;
  if(lifeEmotion=="HUNGRY") return 7;
  if(lifeEmotion=="DIRTY") return 8;
  if(lifeEmotion=="CHEEKY") return 19;
  if(lifeEmotion=="CLEAN") return 17;
  return ((lifeAnimFrame % 12) >= 9) ? 1 : 0;
}

void lifeDrawStageSprite(int x,int y,int stage,bool blink=false,int scale=2) {
  if(stage<0 || stage>5 || !lifeStageSpritesLoaded) return;
  if(!loadLifeStageSpriteVariant(stage,blink && stage>0)) return;
  const uint16_t *src=lifeStagePixels;
  for(int py=0;py<LIFE_SPRITE_H;py++) for(int px=0;px<LIFE_SPRITE_W;px++) {
    uint16_t c=src[py*LIFE_SPRITE_W+px];
    if(c!=LIFE_SPRITE_TRANSPARENT) tft.fillRect(x+px*scale,y+py*scale,scale,scale,c);
  }
}
void lifeDrawObjectButton(int x,int y,int w,int h,const char *label,const char *file,uint16_t c) {
  lifeDrawButton(x,y,w,h,"",c);
  lifeDrawObject(file,x+w/2,y+19);
  tft.setTextFont(1); tft.setTextSize(1);
  tft.setTextColor((c==DARK_GREY)?WHITE:BLACK,c);
  int tw=tft.textWidth(label);
  tft.setCursor(x+(w-tw)/2,y+h-10);
  tft.print(label);
}

void lifeDrawSpeech(const char *text,int x,int y,int w) {
  // Wrap longer reactions inside the bubble instead of allowing them to
  // spill out across the LIFE screen. Two short lines fit comfortably in
  // the available 320x240 layout.
  tft.setTextFont(1); tft.setTextSize(1);
  String full = text ? String(text) : String("");
  String line1 = full;
  String line2 = "";
  const int maxTextW = w - 10;

  if(tft.textWidth(line1.c_str()) > maxTextW) {
    line1 = "";
    int splitPos = -1;
    for(int i=0; i<(int)full.length(); i++) {
      if(full.charAt(i)==' ') splitPos=i;
      String candidate = full.substring(0,i+1);
      if(tft.textWidth(candidate.c_str()) <= maxTextW) line1=candidate;
      else break;
    }
    line1.trim();
    if(splitPos >= 0) line2 = full.substring(splitPos+1);
    else line2 = full;

    // If the second line is still too long, trim it cleanly. All current
    // LIFE speech strings fit, but this protects the bubble if a future
    // message is longer.
    while(line2.length()>0 && tft.textWidth(line2.c_str()) > maxTextW)
      line2.remove(line2.length()-1);
    line2.trim();
    if(line2.length()>3 && line2 != full.substring(splitPos+1)) {
      while(line2.length()>3 && tft.textWidth((line2+"...").c_str()) > maxTextW)
        line2.remove(line2.length()-1);
      line2 += "...";
    }
  }

  bool twoLines = line2.length()>0;
  int bh = twoLines ? 30 : 25;
  tft.fillRoundRect(x,y,w,bh,5,WHITE);
  tft.drawRoundRect(x,y,w,bh,5,LIGHT_GREY);
  tft.fillTriangle(x+15,y+bh-1,x+22,y+bh-1,x+18,y+bh+6,WHITE);
  tft.setTextColor(BLACK,WHITE);

  int tw=tft.textWidth(line1.c_str());
  tft.setCursor(x+(w-tw)/2,y+(twoLines?5:8));
  tft.print(line1);
  if(twoLines) {
    tw=tft.textWidth(line2.c_str());
    tft.setCursor(x+(w-tw)/2,y+15);
    tft.print(line2);
  }
}

void lifeDetermineEmotion() {
  if(petSleeping) lifeSetEmotion("SLEEPY");
  else if(petHealth<=20 || (petSick && petHealth<=45)) lifeSetEmotion("SICK");
  else if(petAction==1) lifeSetEmotion("HAPPY");
  else if(petAction==2) lifeSetEmotion("EXCITED");
  else if(petAction==3) lifeSetEmotion("CLEAN");
  else if(petHunger<=8) lifeSetEmotion("STARVING");
  else if(petHunger<25) lifeSetEmotion("HUNGRY");
  else if(petCleanliness<=8) lifeSetEmotion("FILTHY");
  else if(petCleanliness<30) lifeSetEmotion("DIRTY");
  else if(petHappiness<25) lifeSetEmotion("SAD");
  else if(petDiscipline<15 && petHappiness<45) lifeSetEmotion("ANGRY");
  else if(petEnergy<25) lifeSetEmotion("TIRED");
  else if(petHappiness>90 && petEnergy>70) lifeSetEmotion("EXCITED");
  else if(petDiscipline<25) lifeSetEmotion("CHEEKY");
  else if(petHappiness>80 && petHealth>85) lifeSetEmotion("HAPPY");
  else if(petHappiness<45) lifeSetEmotion("SAD");
  else lifeSetEmotion("CONTENT");
}


void drawLifeHeader(const char *title, bool showConway=false, bool showExec=false) {
  tft.fillRect(0,0,320,31,BLACK);
  tft.setTextFont(1);
  tft.setTextSize((strcmp(title,"LIFE!")==0 || strcmp(title,"LIFE! // DEPARTED")==0) ? 2 : 1);
  tft.setTextColor(PALE_YELLOW,BLACK);
  tft.setCursor(6,(strcmp(title,"LIFE!")==0 || strcmp(title,"LIFE! // DEPARTED")==0) ? 2 : 6);
  tft.print(title);

  if(showConway) {
    // Leave a visible gap before/after the sentence and keep LIFE! clear.
    lifeDrawButton(72,3,142,22,"CONWAY'S GAME OF LIFE",CYAN_CLOCK);
    if(showExec) lifeDrawButton(224,3,50,22,"EXEC",DARK_GREY);
  } else if(showExec) {
    lifeDrawButton(250,3,50,22,"EXEC",DARK_GREY);
  }
  tft.drawFastHLine(0,30,320,DARK_GREY);
}

void drawLifeBarsCompact() {
  lifeDrawBar(38,"HUNGER",petHunger,GREEN_CLOCK);
  lifeDrawBar(51,"HAPPY",petHappiness,CYAN_CLOCK);
  lifeDrawBar(64,"CLEAN",petCleanliness,PALE_YELLOW);
  lifeDrawBar(77,"ENERGY",petEnergy,BLUE_CLOCK);
  lifeDrawBar(90,"HEALTH",petHealth,RED_CLOCK);
}

void lifeDrawBar(int y,const char *label,int value,uint16_t c) {
  tft.setTextFont(1); tft.setTextSize(1); tft.setTextColor(WHITE,BLACK);
  tft.setCursor(7,y); tft.print(label);
  tft.drawRect(55,y,108,9,LIGHT_GREY);
  int fill=value*104/100;
  if(fill>0) tft.fillRect(57,y+2,fill,5,c);
  tft.setCursor(168,y); tft.print(value); tft.print("%");
}

void lifeDrawSleepScene() {
  // Dedicated 48x48 bed/sleep artwork is scaled to the normal 96x96 LIFE
  // character area. The whole finished scene is pushed in one operation.
  lifeCharacterSprite.fillSprite(BLACK);

  int frame = (petAnimFrame / 4) % 4;
  if(loadLifeSpecialSprite(lifeSleepSpriteNames[frame])) {
    for(int py=0;py<LIFE_SPRITE_H;py++) for(int px=0;px<LIFE_SPRITE_W;px++) {
      uint16_t c=lifeStagePixels[py*LIFE_SPRITE_W+px];
      if(c!=LIFE_SPRITE_TRANSPARENT)
        lifeCharacterSprite.fillRect(px*2,py*2,2,2,c);
    }
  } else {
    // Fallback to the original constructed bed if the optional assets are
    // absent, so sleep remains usable with an older SD card.
    lifeCharacterSprite.fillRoundRect(5,48,86,37,7,DARK_GREY);
    lifeCharacterSprite.drawRoundRect(5,48,86,37,7,LIGHT_GREY);
    lifeCharacterSprite.fillRoundRect(8,60,80,24,5,BLUE_CLOCK);
    lifeCharacterSprite.drawRoundRect(8,60,80,24,5,LIGHT_GREY);
    if (petStage >= 1 && petStage <= 5 && loadLifeStageBlinkSprite(petStage)) {
      for(int py=0;py<LIFE_SPRITE_H;py++) for(int px=0;px<LIFE_SPRITE_W;px++) {
        uint16_t c=lifeStagePixels[py*LIFE_SPRITE_W+px];
        if(c!=LIFE_SPRITE_TRANSPARENT) lifeCharacterSprite.fillRect(px*2,py*2,2,2,c);
      }
    }
  }
}

void lifeRenderMainCharacter(int x,int y,int bob,bool blink) {
  if(!lifeCharacterSpriteReady) {
    lifeCharacterSprite.setColorDepth(16);
    lifeCharacterSpriteReady=lifeCharacterSprite.createSprite(96,96);
  }
  if(!lifeCharacterSpriteReady) return;

  if (petSleeping || petAction == 4) {
    lifeDrawSleepScene();
    lifeCharacterSprite.pushSprite(x,y);
    return;
  }

  lifeCharacterSprite.fillSprite(BLACK);

  bool drewCharacter=false;
  bool stageMode=(petAction==0 && !petSleeping && petStage>=0 && petStage<=5);
  // The stage artwork is the authoritative main LIFE character.  Do not
  // depend on the startup "lifeStageSpritesLoaded" flag here: the sprite
  // files can be loaded successfully during hatching and must remain
  // available for the normal status screen afterwards.
  if(stageMode && ((strcmp(life2ConditionName(),"HEALTHY")!=0) ? loadLife2CurrentStateSprite(false) : loadLifeStageSpriteVariant(petStage,blink && petStage>0))) {
    for(int py=0;py<LIFE_SPRITE_H;py++) for(int px=0;px<LIFE_SPRITE_W;px++) {
      uint16_t c=lifeStagePixels[py*LIFE_SPRITE_W+px];
      if(c!=LIFE_SPRITE_TRANSPARENT) {
        lifeCharacterSprite.fillRect(px*2,py*2,2,2,c);
        drewCharacter=true;
      }
    }
  }

  // If the SD stage asset cannot be loaded (for example after a Wi-Fi/heap
  // transition), fall back to the original pose assets instead of leaving a
  // completely black character panel.
  if(!drewCharacter) {
    bool loadedState=false;
    if(petStage>=1 && petStage<=5) loadedState=loadLife2CurrentStateSprite(false);
    int idx=lifeSpriteForCurrentState();
    if(!loadedState && idx>=0 && idx<LIFE_SPRITE_COUNT && loadLifeSprite(idx)) {
      for(int py=0;py<LIFE_SPRITE_H;py++) for(int px=0;px<LIFE_SPRITE_W;px++) {
        uint16_t c=lifeSpritePixels[py*LIFE_SPRITE_W+px];
        if(c!=LIFE_SPRITE_TRANSPARENT) {
          lifeCharacterSprite.fillRect(px*2,py*2,2,2,c);
          drewCharacter=true;
        }
      }
    }
  }

  // Last-resort visible Thronglet. This should only appear if both the stage
  // and legacy SD artwork are unavailable; it keeps the LIFE screen usable.
  if(!drewCharacter) {
    int cx=48, cy=48;
    uint16_t body=PALE_YELLOW, ink=DARK_GREY;
    lifeCharacterSprite.fillRoundRect(cx-25,cy-22,50,45,14,ink);
    lifeCharacterSprite.fillRoundRect(cx-22,cy-19,44,39,12,body);
    lifeCharacterSprite.fillRoundRect(cx-15,cy-10,9,10,3,ink);
    lifeCharacterSprite.fillRoundRect(cx+6,cy-10,9,10,3,ink);
    lifeCharacterSprite.drawLine(cx-9,cy+8,cx,cy+13,ink);
    lifeCharacterSprite.drawLine(cx,cy+13,cx+9,cy+8,ink);
    lifeCharacterSprite.fillRoundRect(cx-17,cy+18,12,7,3,ink);
    lifeCharacterSprite.fillRoundRect(cx+5,cy+18,12,7,3,ink);
    Serial.println("THRONGLET: SD artwork unavailable; using visible fallback");
  }
  lifeCharacterSprite.pushSprite(x,y+bob);
}

const char* lifeIdleSpeechList[] = {
  "Hi!", "Hey!", "Hello!", "Yo!", "Hmm...", "Look at me!",
  "I'm hungry!", "Play?", "I'm OK!", "Whee!", "Hehe!", "Ta-da!"
};
const int LIFE_IDLE_SPEECH_COUNT = sizeof(lifeIdleSpeechList)/sizeof(lifeIdleSpeechList[0]);

const char* lifeFeedSpeechList[] = {
  "Yum!", "MUNCH!", "NOM NOM!", "Tasty!", "More!", "CRUNCH!",
  "Delicious!", "Good stuff!", "Again!", "Lovely!", "Ooh!", "Thanks!"
};
const int LIFE_FEED_SPEECH_COUNT = sizeof(lifeFeedSpeechList)/sizeof(lifeFeedSpeechList[0]);

const char* lifePlaySpeechList[] = {
  "Whee!", "Again!", "FUN!", "Yay!", "Ha ha!", "Go go!",
  "Woohoo!", "My turn!", "Let's go!", "Brilliant!", "Heehee!", "More!"
};
const int LIFE_PLAY_SPEECH_COUNT = sizeof(lifePlaySpeechList)/sizeof(lifePlaySpeechList[0]);

const char* lifeCleanSpeechList[] = {
  "Splish!", "Sparkly!", "Ahhh!", "Scrub!", "So fresh!", "All clean!",
  "Shiny!", "Lovely!", "Bubbles!", "Much better!", "Squeaky!", "Ta-da!"
};
const int LIFE_CLEAN_SPEECH_COUNT = sizeof(lifeCleanSpeechList)/sizeof(lifeCleanSpeechList[0]);

// LIFE! 2.0 — large context-sensitive vocabulary. These remain in flash,
// not RAM, as string literals in program memory on ESP32.
const char* lifeHappySpeechList[] = {
  "THRONGLET IS DELIGHTED", "LIFE IS GOOD!", "THRONGLET FEELS GREAT!",
  "WHAT A LOVELY DAY!", "THRONGLET IS VERY HAPPY", "YAY! YAY!",
  "EVERYTHING IS WONDERFUL", "THRONGLET IS CONTENT", "I LIKE THIS!"
};
const char* lifeHungrySpeechList[] = {
  "THRONGLET IS HUNGRY", "MY TUMMY IS RUMBLING", "I WANT FOOD!",
  "THRONGLET NEEDS A SNACK", "MORE FOOD PLEASE!", "I AM VERY HUNGRY",
  "THRONGLET IS STARVING", "FEED ME, PLEASE!"
};
const char* lifeDirtySpeechList[] = {
  "THRONGLET NEEDS A BATH", "I AM GETTING DIRTY", "THRONGLET IS UNCLEAN",
  "I NEED A WASH!", "THRONGLET IS FILTHY", "SOMETHING SMELLS...",
  "PLEASE CLEAN ME", "I FEEL GRUBBY"
};
const char* lifeTiredSpeechList[] = {
  "THRONGLET IS TIRED", "I NEED TO REST", "THRONGLET IS EXHAUSTED",
  "I CAN BARELY STAY AWAKE", "TIME FOR A NAP", "THRONGLET NEEDS SLEEP",
  "MY EYES ARE HEAVY"
};
const char* lifeSickSpeechList[] = {
  "THRONGLET FEELS ILL", "I DO NOT FEEL WELL", "THRONGLET IS SICK",
  "I NEED SOME HELP", "THRONGLET FEELS AWFUL", "MEDICINE, PLEASE",
  "I FEEL VERY UNWELL"
};
const char* lifeAngrySpeechList[] = {
  "THRONGLET IS NOT AMUSED", "I AM CROSS", "THRONGLET IS ANGRY",
  "THAT WAS NOT NICE", "I REMEMBER THIS", "GRRR!", "LEAVE ME ALONE!"
};
const char* lifeCuriousSpeechList[] = {
  "WHAT IS THAT?", "HMMM... INTERESTING", "I WONDER...", "WHAT HAPPENS NEXT?",
  "THRONGLET IS CURIOUS", "TELL ME MORE!"
};
const char* lifeSleepSpeechList[] = {
  "THRONGLET IS SLEEPY", "TIME FOR BED", "I NEED SOME SLEEP", "Zzz...",
  "GOODNIGHT, ORAC", "THRONGLET IS DREAMING"
};
const int LIFE_HAPPY_SPEECH_COUNT=sizeof(lifeHappySpeechList)/sizeof(lifeHappySpeechList[0]);
const int LIFE_HUNGRY_SPEECH_COUNT=sizeof(lifeHungrySpeechList)/sizeof(lifeHungrySpeechList[0]);
const int LIFE_DIRTY_SPEECH_COUNT=sizeof(lifeDirtySpeechList)/sizeof(lifeDirtySpeechList[0]);
const int LIFE_TIRED_SPEECH_COUNT=sizeof(lifeTiredSpeechList)/sizeof(lifeTiredSpeechList[0]);
const int LIFE_SICK_SPEECH_COUNT=sizeof(lifeSickSpeechList)/sizeof(lifeSickSpeechList[0]);
const int LIFE_ANGRY_SPEECH_COUNT=sizeof(lifeAngrySpeechList)/sizeof(lifeAngrySpeechList[0]);
const int LIFE_SLEEP_SPEECH_COUNT=sizeof(lifeSleepSpeechList)/sizeof(lifeSleepSpeechList[0]);

const char* lifeBadFoodSpeechList[] = {
  "BLECH!", "YUCK!", "NO FISH!", "BLEEEH!", "WHY FISH?!", "NOT THAT!"
};
const int LIFE_BAD_FOOD_SPEECH_COUNT = sizeof(lifeBadFoodSpeechList)/sizeof(lifeBadFoodSpeechList[0]);

const char* lifeChooseSpeech(const char* list[], int count) {
  if(count<=0) return "Hi!";
  int idx=random(0,count);
  if(count>1 && idx==lifeLastSpeechIndex) idx=(idx+1)%count;
  lifeLastSpeechIndex=idx;
  return list[idx];
}

void lifeSetSpeech(const char *text, unsigned long duration=3200UL) {
  lifeSpeech=String(text);
  lifeSpeechUntil=millis()+duration;
}

void lifeSetIdleSpeech() {
  lifeSetSpeech(lifeChooseSpeech(lifeIdleSpeechList,LIFE_IDLE_SPEECH_COUNT),4200UL);
}

void lifeDrawSpeechForCurrentEmotion() {
  if(petSleeping || petAction==4) return; // sleep scene has its own Zzz
  const char *text=lifeSpeech.c_str();
  int w=56;
  if(strlen(text)>9) w=76;
  else if(strlen(text)>6) w=66;
  int x=316-w;
  if(x<194) x=194;
  lifeDrawSpeech(text,x,33,w);
}

void drawLifeCharacterArea(bool force=false) {
  int spriteIndex=lifeSpriteForCurrentState();
  int animPhase=(lifeAnimFrame/2)%60;
  bool spriteChanged=(spriteIndex!=lifeDisplayedSprite);
  bool emotionChanged=(lifeEmotion!=lifeDisplayedEmotion);
  bool animChanged=(animPhase!=lifeDisplayedAnimPhase);
  static String lastCharacterSpeech="";
  bool speechChanged=(lastCharacterSpeech!=lifeSpeech);

  if(!force && lifeCharacterAreaValid && !spriteChanged && !emotionChanged && !animChanged && !speechChanged) return;

  lifeRenderMainCharacter(210,70,0,(petAction==0 && !petSleeping && petStage>0 && animPhase>=56 && animPhase<=57));

  // Only erase the speech region when the speech actually changes.  The
  // character itself is double-buffered separately.
  if(force || speechChanged || emotionChanged) {
    tft.fillRect(190,32,130,42,BLACK);
    lifeDrawSpeechForCurrentEmotion();
    // v147.14: removed the procedural red heart/star decorations.
    // They were the red cross/blob artefacts visible beside the Thronglet,
    // and were redrawn during the LIFE animation, making one appear to flash.
    // The actual Thronglet artwork remains unchanged.
  }

  lastCharacterSpeech=lifeSpeech;
  lifeDisplayedSprite=spriteIndex;
  lifeDisplayedEmotion=lifeEmotion;
  lifeDisplayedAnimPhase=animPhase;
  lifeCharacterAreaValid=true;
}

void lifeDrawInteractionArea(bool force=false) {
  static int lastAction=-99;
  static int lastFrame=-99;
  static String lastSpeech="";

  bool actionChanged=(lastAction!=petAction);
  bool frameChanged=(lastFrame!=petAnimFrame);
  bool speechChanged=(lastSpeech!=lifeSpeech);
  bool animate=(petAction!=0 || petSleeping || petGameActive);

  if(!force && !actionChanged && !speechChanged && (!animate || !frameChanged)) return;

  if(!lifeInteractionSpriteReady) {
    lifeInteractionSprite.setColorDepth(16);
    lifeInteractionSpriteReady=lifeInteractionSprite.createSprite(48,48);
  }

  // Only the sprite frame is updated during animation.  The text/bubble is
  // deliberately NOT erased every frame; that was the source of the flicker.
  if(lifeInteractionSpriteReady && (force || actionChanged || frameChanged)) {
    lifeInteractionSprite.fillSprite(BLACK);
    int idx=lifeSpriteForCurrentState();
    if(idx>=0 && idx<LIFE_SPRITE_COUNT && loadLifeSprite(idx)) {
      for(int py=0;py<LIFE_SPRITE_H;py++) for(int px=0;px<LIFE_SPRITE_W;px++) {
        uint16_t c=lifeSpritePixels[py*LIFE_SPRITE_W+px];
        if(c!=LIFE_SPRITE_TRANSPARENT) lifeInteractionSprite.drawPixel(px,py,c);
      }
    } else if(petStage>=0 && petStage<=5 && lifeStageSpritesLoaded) {
      if(loadLifeStageSpriteVariant(petStage,false)) {
        for(int py=0;py<LIFE_SPRITE_H;py++) for(int px=0;px<LIFE_SPRITE_W;px++) {
          uint16_t c=lifeStagePixels[py*LIFE_SPRITE_W+px];
          if(c!=LIFE_SPRITE_TRANSPARENT) lifeInteractionSprite.drawPixel(px,py,c);
        }
      }
    }
    // Below the bubble and slightly to its left.
    lifeInteractionSprite.pushSprite(78,192);
  }

  // Keep the bubble in its own clear corridor to the right of the
  // character. This prevents animation frames from ever overwriting the
  // bubble or its tail.
  if(force || speechChanged || actionChanged) {
    int w=112;
    if(lifeSpeech.length()>12) w=126;
    int x=134;
    if(x+w>318) x=318-w;
    tft.fillRect(130,161,188,47,BLACK);
    lifeDrawSpeech(lifeSpeech.c_str(),x,166,w);
  }

  lastAction=petAction;
  lastFrame=petAnimFrame;
  lastSpeech=lifeSpeech;
}


void drawLifeHatchingFrame(bool finalFrame=false) {
  // Hatching is an animation inside an otherwise static LIFE screen.
  // Do NOT call drawTamagotchiScreen() for every frame: that clears the
  // entire TFT and produces the bright flash seen during the hatch.
  unsigned long elapsed=millis()-petHatchStarted;
  int ex=188;
  bool cracked=(elapsed>=1350UL);
  bool shake=(elapsed>=900UL && elapsed<3000UL);
  if(shake) {
    int phase=(elapsed/90UL)%4;
    ex += (phase==1)?-2:(phase==3?2:0);
  }

  // Clear only the egg/baby animation rectangle.
  tft.fillRect(180,60,120,105,BLACK);

  if(finalFrame) {
    if(loadLifeStageSprite(1)) {
      for(int py=0;py<LIFE_SPRITE_H;py++) for(int px=0;px<LIFE_SPRITE_W;px++) {
        uint16_t c=lifeStagePixels[py*LIFE_SPRITE_W+px];
        if(c!=LIFE_SPRITE_TRANSPARENT) tft.fillRect(188+px*2,68+py*2,2,2,c);
      }
    }
    tft.setTextColor(CYAN_CLOCK,BLACK); tft.setCursor(7,118);
    tft.print("WELCOME, LITTLE THRONGLET");
    tft.setTextColor(CYAN_CLOCK,BLACK); tft.setCursor(7,151);
    tft.print("STAGE: BABY");
    tft.setTextColor(PALE_YELLOW,BLACK); tft.setCursor(216,166); tft.print("HELLO!");
    return;
  }

  if(loadLifeHatchEgg(cracked)) {
    for(int py=0;py<LIFE_SPRITE_H;py++) for(int px=0;px<LIFE_SPRITE_W;px++) {
      uint16_t c=lifeStagePixels[py*LIFE_SPRITE_W+px];
      if(c!=LIFE_SPRITE_TRANSPARENT) tft.fillRect(ex+px*2,68+py*2,2,2,c);
    }
  }
}


struct LifePooSpot { int x; int y; int variant; bool active; };
LifePooSpot lifePooSpots[3]={{0,0,1,false},{0,0,2,false},{0,0,3,false}};
int lifePooSpotCount=0;

void lifeSyncPooSpots() {
  int wanted=constrain(petPooCount,0,3);
  while(lifePooSpotCount<wanted) {
    int idx=lifePooSpotCount;
    lifePooSpots[idx].x=random(190,305);
    lifePooSpots[idx].y=random(118,176);
    lifePooSpots[idx].variant=random(1,6);
    lifePooSpots[idx].active=true;
    lifePooSpotCount++;
  }
  while(lifePooSpotCount>wanted) {
    lifePooSpotCount--;
    lifePooSpots[lifePooSpotCount].active=false;
  }
}

bool loadLifePooSprite(int variant) {
  if (variant < 1 || variant > 5) variant = 1;
  char path[120];
  snprintf(path, sizeof(path), "/ORAC/ASSETS/BURT/THRONGLET/LIFE2/POO_%d.RAW", variant);
  File f = SD.open(path, FILE_READ);
  const size_t need = (size_t)LIFE_OBJECT_W * LIFE_OBJECT_H * sizeof(uint16_t);
  if (!f || f.size() != need) {
    if (f) f.close();
    Serial.print("LIFE POO SD: cannot load "); Serial.println(path);
    return false;
  }
  size_t got = f.read((uint8_t*)lifeObjectPixels, need);
  f.close();
  if (got != need) return false;

  // The supplied POO RAW files are BIG-ENDIAN RGB565. On the ESP32 the
  // uint16_t read above produces the byte-reversed value (e.g. the magenta
  // transparent colour appears as 0x1FF8). Swap each pixel so the data uses
  // the same native RGB565 representation as the other LIFE object sprites.
  for (int p = 0; p < LIFE_OBJECT_W * LIFE_OBJECT_H; p++) {
    uint16_t v = lifeObjectPixels[p];
    lifeObjectPixels[p] = (uint16_t)((v << 8) | (v >> 8));
  }
  return true;
}

void lifeDrawPooSpriteAt(int x, int y, int variant) {
  if (!loadLifePooSprite(variant)) return;
  const uint16_t *src = lifeObjectPixels;
  const int w = LIFE_OBJECT_W, h = LIFE_OBJECT_H;
  const int x0 = x - w / 2, y0 = y - h / 2;
  for (int py = 0; py < h; py++) {
    for (int px = 0; px < w; px++) {
      uint16_t c = src[py * w + px];
      if (c != LIFE_OBJECT_TRANSPARENT) tft.drawPixel(x0 + px, y0 + py, c);
    }
  }
}

void lifeDrawPooSpots() {
  lifeSyncPooSpots();
  for(int i=0;i<lifePooSpotCount;i++) if(lifePooSpots[i].active)
    lifeDrawPooSpriteAt(lifePooSpots[i].x,lifePooSpots[i].y,lifePooSpots[i].variant);
}

void drawLifeStatus() {
  drawLifeHeader("LIFE!",true,true);
  if(petHatching) {
    drawLifeBarsCompact();
    tft.setTextColor(PALE_YELLOW,BLACK); tft.setCursor(7,105); tft.print("AGE 0m  WT 8");
    tft.setTextColor(CYAN_CLOCK,BLACK); tft.setCursor(7,118); tft.print("NEW THRONGLET IS HATCHING");
    tft.setTextColor(WHITE,BLACK); tft.setCursor(7,137); tft.print("A NEW LIFE IS ABOUT TO BEGIN");
    tft.setTextColor(CYAN_CLOCK,BLACK); tft.setCursor(7,151); tft.print("STAGE: EGG");

    // Draw the initial egg frame once. Later frames update only the egg area.
    drawLifeHatchingFrame(false);
    lifeDrawButton(3,205,75,28,"FEED",GREEN_CLOCK);
    lifeDrawButton(81,205,75,28,"PLAY",CYAN_CLOCK);
    lifeDrawButton(159,205,75,28,"CLEAN",PALE_YELLOW);
    lifeDrawButton(237,205,38,28,"INFO",PURPLE_CLOCK);
    lifeDrawButton(278,205,39,28,"HOME",DARK_GREY);
    return;
  }

  lifeDetermineEmotion();
  if(lifeSpeechUntil<millis()) {
    if(petSleeping) lifeSetSpeech(lifeChooseSpeech(lifeSleepSpeechList,LIFE_SLEEP_SPEECH_COUNT),4200UL);
    else if(petHealth<=45 || petSick) lifeSetSpeech(lifeChooseSpeech(lifeSickSpeechList,LIFE_SICK_SPEECH_COUNT),4200UL);
    else if(petHunger<=25) lifeSetSpeech(lifeChooseSpeech(lifeHungrySpeechList,LIFE_HUNGRY_SPEECH_COUNT),4200UL);
    else if(petCleanliness<=30) lifeSetSpeech(lifeChooseSpeech(lifeDirtySpeechList,LIFE_DIRTY_SPEECH_COUNT),4200UL);
    else if(petEnergy<=25) lifeSetSpeech(lifeChooseSpeech(lifeTiredSpeechList,LIFE_TIRED_SPEECH_COUNT),4200UL);
    else if(lifeEmotion=="ANGRY") lifeSetSpeech(lifeChooseSpeech(lifeAngrySpeechList,LIFE_ANGRY_SPEECH_COUNT),4200UL);
    else if(lifeEmotion=="EXCITED" || lifeEmotion=="HAPPY") lifeSetSpeech(lifeChooseSpeech(lifeHappySpeechList,LIFE_HAPPY_SPEECH_COUNT),4200UL);
    else lifeSetIdleSpeech();
  }
  drawLifeBarsCompact();

  tft.setTextColor(PALE_YELLOW,BLACK); tft.setCursor(7,105);
  tft.print("AGE "); tft.print(lifeFormatAge()); tft.print("  WT "); tft.print(petWeight);
  tft.setTextColor(CYAN_CLOCK,BLACK); tft.setCursor(7,118);
  tft.print("FEELS "); tft.setTextColor(GREEN_CLOCK,BLACK); tft.print(lifeEmotion);

  drawLifeCharacterArea(true);

  tft.setTextColor(WHITE,BLACK); tft.setCursor(7,137);
  tft.print(petMessage.substring(0,38));
  tft.setTextColor(CYAN_CLOCK,BLACK); tft.setCursor(7,151);
  tft.print("STAGE: "); tft.print(petStageName());

  if(petPooCount>0) lifeDrawPooSpots();

  if(petGameActive) {
    tft.fillRoundRect(5,132,180,63,5,BLACK);
    tft.drawRoundRect(5,132,180,63,5,RED_CLOCK);
    tft.setTextColor(PALE_YELLOW,BLACK); tft.setCursor(16,140); tft.print("HIGH OR LOW?");
    tft.setTextColor(WHITE,BLACK); tft.setCursor(16,155); tft.print("REFERENCE: "); tft.print(petGameReference);
    tft.setTextColor(CYAN_CLOCK,BLACK); tft.setCursor(16,170); tft.print("TAP A CHOICE BELOW");
    lifeDrawButton(188,165,62,30,"LOWER",GREEN_CLOCK);
    lifeDrawButton(254,165,62,30,"HIGHER",PALE_YELLOW);
  }

  // Six compact controls. MED replaces SLEEP only while the Thronglet
  // is sick (or in the very-low-health SICK state).
  lifeDrawButton(2,205,52,28,"FEED",GREEN_CLOCK);
  lifeDrawButton(55,205,52,28,"PLAY",CYAN_CLOCK);
  lifeDrawButton(108,205,52,28,"CLEAN",PALE_YELLOW);
  if(petSick || petHealth<=20) lifeDrawButton(161,205,52,28,"MED",RED_CLOCK);
  else lifeDrawButton(161,205,52,28,"SLEEP",BLUE_CLOCK);
  lifeDrawButton(214,205,52,28,"INFO",PURPLE_CLOCK);
  lifeDrawButton(267,205,51,28,"HOME",DARK_GREY);
}

void drawLifeFeed() {
  drawLifeHeader("FEED",false,false);
  tft.setTextFont(1); tft.setTextSize(1); tft.setTextColor(CYAN_CLOCK,BLACK);
  tft.setCursor(7,37); tft.print("CHOOSE A SNACK");

  // Fish is deliberately the one food he dislikes, giving the screen a
  // comic negative reaction rather than making every button equally good.
  lifeDrawObjectButton(5,48,95,52,"APPLE","APPLE.RAW",RED_CLOCK);
  lifeDrawObjectButton(110,48,95,52,"COOKIE","COOKIE.RAW",ORANGE_CLOCK);
  lifeDrawObjectButton(215,48,100,52,"BANANA","BANANA.RAW",PALE_YELLOW);
  lifeDrawObjectButton(5,106,95,52,"FISH","FISH.RAW",CYAN_CLOCK);
  lifeDrawObjectButton(110,106,95,52,"CHEESE","CHEESE.RAW",PALE_YELLOW);
  lifeDrawObjectButton(215,106,100,52,"TREAT","TREAT.RAW",PURPLE_CLOCK);

  lifeDrawInteractionArea(true);
  lifeDrawButton(5,214,55,22,"BACK",DARK_GREY);
}

void drawLifePlay() {
  drawLifeHeader("PLAY",false,false);
  tft.setTextFont(1); tft.setTextSize(1); tft.setTextColor(CYAN_CLOCK,BLACK);
  tft.setCursor(7,37); tft.print("CHOOSE SOMETHING FUN");
  lifeDrawObjectButton(5,48,95,52,"BALL","BALL.RAW",CYAN_CLOCK);
  lifeDrawObjectButton(110,48,95,52,"TOY","TOY.RAW",ORANGE_CLOCK);
  lifeDrawObjectButton(215,48,100,52,"OUTSIDE","OUTSIDE.RAW",GREEN_CLOCK);
  lifeDrawObjectButton(5,106,95,52,"MUSIC","MUSIC.RAW",PURPLE_CLOCK);
  lifeDrawObjectButton(110,106,95,52,"DANCE","DANCE.RAW",CYAN_CLOCK);
  lifeDrawObjectButton(215,106,100,52,"GUESS","GUESS.RAW",RED_CLOCK);

  if(petGameActive) {
    // Guess now completes on this screen; the player no longer has to
    // somehow return to the LIFE status page to find HIGHER/LOWER.
    tft.fillRect(4,158,312,45,BLACK);
    tft.setTextColor(PALE_YELLOW,BLACK); tft.setCursor(7,160); tft.print("IS THE NEXT NUMBER HIGHER OR LOWER?");
    tft.setTextColor(WHITE,BLACK); tft.setCursor(7,174); tft.print("REFERENCE: "); tft.print(petGameReference);
    lifeDrawButton(166,181,70,22,"LOWER",GREEN_CLOCK);
    lifeDrawButton(242,181,70,22,"HIGHER",PALE_YELLOW);
  } else {
    lifeDrawInteractionArea(true);
  }
  lifeDrawButton(5,214,55,22,"BACK",DARK_GREY);
}


void drawLifeClean() {
  drawLifeHeader("CLEAN",false,false);
  tft.setTextFont(1); tft.setTextSize(1); tft.setTextColor(CYAN_CLOCK,BLACK);
  tft.setCursor(7,37); tft.print("MAKE THRONGLET SPARKLE");
  lifeDrawObjectButton(5,48,95,52,"BATH","BATH.RAW",CYAN_CLOCK);
  lifeDrawObjectButton(110,48,95,52,"BRUSH","BRUSH.RAW",PALE_YELLOW);
  lifeDrawObjectButton(215,48,100,52,"DRY","DRY.RAW",BLUE_CLOCK);
  tft.setTextColor(WHITE,BLACK); tft.setCursor(7,123); tft.print("POO: "); tft.print(petPooCount);
  if(petPooCount>0) lifeDrawPoo(55,121);
  lifeDrawInteractionArea(true);
  lifeDrawButton(5,214,55,22,"BACK",DARK_GREY);
}

String lifeFormatAge() {
  long long totalMinutes = petAgeMinutes;
  if(totalMinutes < 0) totalMinutes = 0;

  // Keep the Thronglet's 28-day lifespan, but display age in units that
  // are immediately meaningful: days and hours.
  long long days = totalMinutes / 1440LL;
  long long hours = (totalMinutes % 1440LL) / 60LL;

  if(days == 0) {
    return String((long)hours) + (hours == 1 ? " hour" : " hours");
  }

  String out = String((long)days) + (days == 1 ? " day" : " days");
  if(hours > 0) {
    out += " " + String((long)hours) + (hours == 1 ? " hour" : " hours");
  }
  return out;
}

void drawLifeInfo() {
  drawLifeHeader("INFO",false,false);
  tft.setTextFont(1); tft.setTextSize(1);
  tft.setTextColor(PALE_YELLOW,BLACK); tft.setCursor(8,39); tft.print("THRONGLET STATUS");

  tft.setTextColor(CYAN_CLOCK,BLACK);
  tft.setCursor(8,54); tft.print("AGE         "); tft.print(lifeFormatAge());
  tft.setCursor(8,67); tft.print("LIFE STAGE  "); tft.print(petStageName());
  if(petStage>=4) {
    tft.setCursor(8,80); tft.print("FORM        "); tft.print(petAdultName());
  } else {
    tft.setCursor(8,80); tft.print("FORM        DEVELOPING");
  }

  tft.setTextColor(WHITE,BLACK);
  tft.setCursor(8,96);  tft.print("HAPPY "); tft.print(petHappiness); tft.print("  HUNGER "); tft.print(petHunger);
  tft.setCursor(8,109); tft.print("ENERGY "); tft.print(petEnergy); tft.print("  CLEAN "); tft.print(petCleanliness);
  tft.setCursor(8,122); tft.print("HEALTH "); tft.print(petHealth); tft.print("  DISC "); tft.print(petDiscipline);
  tft.setCursor(8,135); tft.print("WEIGHT "); tft.print(petWeight); tft.print("  POO "); tft.print(petPooCount);
  tft.setCursor(8,148); tft.print("GAMES "); tft.print(petGameWins); tft.print("W / "); tft.print(petGameLosses); tft.print("L");

  // Small stage sprite: enough to identify the current form without making
  // the info page feel crowded.
  lifeDrawStageSprite(255,94,petStage,false,1);
  lifeDrawButton(5,210,60,23,"BACK",DARK_GREY);
}


void lifeDrawGrave(int cx,int cy) {
  tft.fillRoundRect(cx-36,cy-31,72,55,7,DARK_GREY);
  tft.fillRect(cx-31,cy+21,62,5,DARK_GREY);
  tft.drawRoundRect(cx-36,cy-31,72,55,7,LIGHT_GREY);
  tft.setTextFont(1); tft.setTextSize(2); tft.setTextColor(WHITE,DARK_GREY);
  tft.setCursor(cx-16,cy-14); tft.print("RIP");
  tft.setTextSize(1); tft.setCursor(cx-29,cy+7); tft.print("THRONGLET");
}
void lifeResetGhostAnimation() {
  lifeGhostX = 234;
  lifeGhostY = 74;
  lifeGhostLastX = lifeGhostX;
  lifeGhostLastY = lifeGhostY;
  lifeGhostFrame = 0;
  lifeGhostPhase = 0.0f;
  lifeGhostLastAnim = millis();
}

void lifeDrawGhost(int x,int y) {
  if(!lifeGhostSpriteReady) {
    lifeGhostSprite.setColorDepth(16);
    lifeGhostSpriteReady=lifeGhostSprite.createSprite(48,48);
  }
  if(!lifeGhostSpriteReady) return;

  lifeGhostSprite.fillSprite(BLACK);
  int frame = lifeGhostFrame % 5;
  if(!loadLifeSpecialSprite(lifeGhostSpriteNames[frame])) return;

  for(int py=0;py<LIFE_SPRITE_H;py++) for(int px=0;px<LIFE_SPRITE_W;px++) {
    uint16_t c=lifeStagePixels[py*LIFE_SPRITE_W+px];
    if(c!=LIFE_SPRITE_TRANSPARENT) lifeGhostSprite.drawPixel(px,py,c);
  }
  lifeGhostSprite.pushSprite(x,y);
}

void lifeAnimateDeathGhost() {
  unsigned long now=millis();
  if(now-lifeGhostLastAnim < 140UL) return;
  lifeGhostLastAnim=now;

  lifeGhostLastX=lifeGhostX;
  lifeGhostLastY=lifeGhostY;
  lifeGhostPhase += 0.16f;
  lifeGhostX += 1;
  if(lifeGhostX > 268) lifeGhostX=214;
  lifeGhostY = 70 + (int)(sin(lifeGhostPhase) * 11.0f);

  // This safe animation corridor is to the right of the grave and above
  // the explanatory text, so clearing the old frame cannot erase UI.
  tft.fillRect(lifeGhostLastX-1,lifeGhostLastY-1,50,50,BLACK);
  lifeGhostFrame=(lifeGhostFrame+1)%5;
  lifeDrawGhost(lifeGhostX,lifeGhostY);
}

void drawLifeDeathScreen() {
  drawLifeHeader("LIFE! // DEPARTED",false,false);
  tft.setTextFont(1); tft.setTextSize(2); tft.setTextColor(RED_CLOCK,BLACK);
  const char *cause=(petDeathCause==3)?"EXECUTED":((petDeathCause==1)?"OLD AGE":"NEGLECT");
  int tw=tft.textWidth(cause); tft.setCursor((320-tw)/2,38); tft.print(cause);

  lifeDrawGrave(72,96);
  // Ghost uses the same right-hand character zone as the living Thronglet.
  lifeResetGhostAnimation();
  lifeDrawGhost(lifeGhostX,lifeGhostY);

  tft.setTextColor(WHITE,BLACK); tft.setTextSize(1);
  tft.setCursor(33,139); tft.print("THE GRAVE WILL REMAIN UNTIL");
  tft.setCursor(56,151); tft.print("A NEW THRONGLET HATCHES.");
  if(petHatching) {
    tft.setTextColor(PALE_YELLOW,BLACK); tft.setCursor(95,165); tft.print("EGG IS HATCHING...");
  } else {
    lifeDrawButton(75,171,170,38,"HATCH NEW EGG",PALE_YELLOW);
  }
}

void startNewThrongletEgg() {
  petHatching=true; petHatchStarted=millis();
  petDead=false; petDeathCause=0;
  petStage=0; petAgeMinutes=0; petHunger=80; petHappiness=80; petEnergy=90; petCleanliness=100;
  lifeBadFoodReaction=false; lifeSetSpeech("Hello!",3200UL);
  petHealth=100; petDiscipline=50; petWeight=8; petCareMistakes=0; petGameWins=0; petGameLosses=0;
  petAdultType=-1; petPooCount=0; petDirtyLevel=0; petSick=false; petSleeping=false; petAutoSleepOverride=false; petAttention=false;
  petMessage="A NEW EGG HAS ARRIVED"; saveTamagotchi(); drawTamagotchiScreen();
}

void killThronglet(int cause) {
  if(petDead) return;
  petDead=true; petHatching=false; petDeathCause=cause; petSleeping=false; petAction=0; petGameActive=false;
  lifeBadFoodReaction=false;
  petMessage=(cause==1)?"GOODBYE, OLD FRIEND.":((cause==2)?"NEGLECT ENDED THIS LIFE.":"LIFE TERMINATED.");
  lifeResetGhostAnimation();
  saveTamagotchi();
  if(currentScreen==SCREEN_TAMAGOTCHI) drawTamagotchiScreen();
}

void drawPetExecuteConfirm() {
  tft.fillScreen(BLACK);
  tft.setTextFont(1);
  tft.setTextSize(2);
  tft.setTextColor(RED_CLOCK, BLACK);
  tft.setCursor(7, 4);
  tft.print("LIFE! // EXECUTE");
  tft.drawFastHLine(0, 29, 320, DARK_GREY);

  // No Thronglet sprite/character panel on the termination screen.
  tft.setTextColor(RED_CLOCK,BLACK); tft.setTextSize(1);
  tft.setCursor(12,43); tft.print("FINAL LIFEFORM CONTROL");

  tft.setTextColor(WHITE, BLACK);
  tft.setTextSize(2);
  tft.setCursor(42, 62);
  tft.print("TERMINATE THRONGLET?");

  tft.setTextSize(1);
  tft.setTextColor(PALE_YELLOW, BLACK);
  tft.setCursor(48, 112);
  tft.print("THIS WILL END THE CURRENT LIFE.");
  tft.setCursor(51, 126);
  tft.print("A NEW EGG WILL REPLACE IT.");

  lifeDrawButton(25, 160, 125, 42, "CANCEL", DARK_GREY);
  lifeDrawButton(170, 160, 125, 42, "EXECUTE", RED_CLOCK);
}

void drawTamagotchiScreen() {
  tft.fillScreen(BLACK);
  lifeCharacterAreaValid=false;
  if(petExecuteConfirm) { drawPetExecuteConfirm(); return; }
  if(petDead) { drawLifeDeathScreen(); return; }
  if(lifeView==LIFE_STATUS) drawLifeStatus();
  else if(lifeView==LIFE_FEED) drawLifeFeed();
  else if(lifeView==LIFE_PLAY) drawLifePlay();
  else if(lifeView==LIFE_CLEAN) drawLifeClean();
  else drawLifeInfo();
  lifeLastScreenDraw=millis();
}

void startTamagotchi() {
  ensureLifeGraphics();
  if(!petLoaded) loadTamagotchi();
  petLastAnim=millis();
  petAction=0;
  // Restore or establish the natural sleep state from the RTC.
  int sy,sm,sd,sh,smi,ss; readRTC(sy,sm,sd,sh,smi,ss);
  bool naturalNight=(sh>=22 || sh<8);
  if(petStage>0 && naturalNight && !petAutoSleepOverride) { petSleeping=true; petAction=4; petActionUntil=0; }
  else if(!naturalNight) { petSleeping=false; petAction=0; petAutoSleepOverride=false; }
  lifeView=LIFE_STATUS;
  lifeAnimFrame=0;
  petAttention=false;
  lifeBadFoodReaction=false;
  lifeSetIdleSpeech();
  petMessage="THRONGLET IS READY TO LIVE";
  lifeDetermineEmotion();
}

void startPetAction(int action) {
  petAction=action; petAnimFrame=0; petActionUntil=millis()+((action==4)?3000:1300);
}

void lifeSleep() {
  if(petSleeping || petAction==4) {
    petSleeping=false; petAction=0; petAutoSleepOverride=true; petMessage="THRONGLET IS AWAKE"; lifeSetSpeech("Good morning!",3000UL);
  } else {
    petSleeping=true; petAction=4; petAnimFrame=0; petActionUntil=millis()+5000UL;
    petEnergy=min(100,petEnergy+5); petMessage="THRONGLET IS SLEEPY"; lifeSetSpeech("Zzz...",4200UL);
  }
  petAttention=false; lifeBadFoodReaction=false; saveTamagotchi(); drawTamagotchiScreen();
}

void lifeFeed(int food) {
  int happy=0, hunger=0, weight=0;
  bool disliked=false;
  if(food==0){ hunger=25; happy=5; weight=1; lifeSetSpeech(lifeChooseSpeech(lifeFeedSpeechList,LIFE_FEED_SPEECH_COUNT)); }
  else if(food==1){ hunger=20; happy=9; weight=2; lifeSetSpeech(lifeChooseSpeech(lifeFeedSpeechList,LIFE_FEED_SPEECH_COUNT)); }
  else if(food==2){ hunger=18; happy=7; weight=1; lifeSetSpeech(lifeChooseSpeech(lifeFeedSpeechList,LIFE_FEED_SPEECH_COUNT)); }
  else if(food==3){ hunger=5; happy=-10; weight=0; disliked=true; lifeSetSpeech(lifeChooseSpeech(lifeBadFoodSpeechList,LIFE_BAD_FOOD_SPEECH_COUNT),3600UL); }
  else if(food==4){ hunger=22; happy=12; weight=1; lifeSetSpeech(lifeChooseSpeech(lifeFeedSpeechList,LIFE_FEED_SPEECH_COUNT)); }
  else { hunger=32; happy=15; weight=2; lifeSetSpeech(lifeChooseSpeech(lifeFeedSpeechList,LIFE_FEED_SPEECH_COUNT)); }

  petHunger=min(100,petHunger+hunger);
  petHappiness=constrain(petHappiness+happy,0,100);
  petWeight=min(99,petWeight+weight);
  petAttention=false;
  lifeBadFoodReaction=disliked;
  startPetAction(1);
  lifeSetEmotion(disliked ? "ANGRY" : "HAPPY");
  petMessage=disliked ? "THRONGLET DOES NOT LIKE FISH!" : "THRONGLET ENJOYED THAT!";
  saveTamagotchi(); drawTamagotchiScreen();
}

void lifePlay(int game) {
  int happy=12, energy=10;
  if(game==0){ happy=25; energy=10; lifeSetSpeech(lifeChooseSpeech(lifePlaySpeechList,LIFE_PLAY_SPEECH_COUNT),3000UL); }
  else if(game==1){ happy=20; energy=8; lifeSetSpeech(lifeChooseSpeech(lifePlaySpeechList,LIFE_PLAY_SPEECH_COUNT),3000UL); }
  else if(game==2){ happy=30; energy=15; lifeSetSpeech(lifeChooseSpeech(lifePlaySpeechList,LIFE_PLAY_SPEECH_COUNT),3000UL); }
  else if(game==3){ happy=18; energy=5; lifeSetSpeech(lifeChooseSpeech(lifePlaySpeechList,LIFE_PLAY_SPEECH_COUNT),3000UL); }
  else if(game==4){ happy=28; energy=12; lifeSetSpeech(lifeChooseSpeech(lifePlaySpeechList,LIFE_PLAY_SPEECH_COUNT),3000UL); }
  else {
    petGameReference=random(20,81);
    do { petGameNextNumber=random(1,101); } while(petGameNextNumber==petGameReference);
    petGameTarget=(petGameNextNumber>petGameReference)?1:0; petGameChoice=-1;
    petGameActive=true; petGameUntil=millis()+7000; petAttention=false;
    petMessage="HIGH OR LOW? GUESS!"; lifeSetEmotion("EXCITED"); lifeSetSpeech("Your turn!",3200UL);
    saveTamagotchi(); drawTamagotchiScreen(); return;
  }
  petHappiness=min(100,petHappiness+happy); petEnergy=max(0,petEnergy-energy);
  petAttention=false; petMessage="THRONGLET IS HAVING FUN!"; lifeBadFoodReaction=false;
  startPetAction(2); lifeSetEmotion("EXCITED");
  saveTamagotchi(); drawTamagotchiScreen();
}

void lifeClean(int mode) {
  if(mode==0){ petCleanliness=100; petPooCount=0; petDirtyLevel=0; petHappiness=min(100,petHappiness+8); petMessage="NICE BATH!"; lifeSetSpeech(lifeChooseSpeech(lifeCleanSpeechList,LIFE_CLEAN_SPEECH_COUNT),3000UL); }
  else if(mode==1){ petCleanliness=min(100,petCleanliness+35); petPooCount=max(0,petPooCount-1); petDirtyLevel=constrain((100-petCleanliness)/30,0,3); petMessage="SCRUB SCRUB!"; lifeSetSpeech(lifeChooseSpeech(lifeCleanSpeechList,LIFE_CLEAN_SPEECH_COUNT),3000UL); }
  else { petCleanliness=min(100,petCleanliness+20); petHappiness=min(100,petHappiness+3); petMessage="ALL DRY!"; lifeSetSpeech(lifeChooseSpeech(lifeCleanSpeechList,LIFE_CLEAN_SPEECH_COUNT),3000UL); }
  petAttention=false; lifeBadFoodReaction=false; startPetAction(3); lifeSetEmotion("CLEAN"); saveTamagotchi(); drawTamagotchiScreen();
}

void lifeMedicine() {
  // Medicine is only available when the Thronglet is explicitly sick or
  // has reached the very-low-health state that is displayed as SICK.
  if(!petSick && petHealth>20) {
    lifeSetSpeech("NO MEDICINE NEEDED",2200UL);
    drawTamagotchiScreen();
    return;
  }

  petSick=false;
  petHealth=min(100,petHealth+30);
  petHappiness=min(100,petHappiness+5);
  petAttention=false;
  lifeBadFoodReaction=false;
  petMessage="MEDICINE TAKEN!";
  lifeSetSpeech("FEELING BETTER!",3000UL);
  startPetAction(5);
  lifeSetEmotion("HAPPY");
  if(oracVolume>0) oracMedicineSound();
  saveTamagotchi();
  drawTamagotchiScreen();
}

void handleTamagotchiTouch(int x,int y) {
  if(petExecuteConfirm) {
    if(y>=145 && y<215) {
      if(x<160){
        petExecuteConfirm=false;
        drawTamagotchiScreen();
      } else {
        petExecuteConfirm=false;
        killThronglet(3);
      }
    }
    return;
  }

  if(petDead) {
    if(!petHatching && y>=165 && y<220 && x>=60 && x<260) { startNewThrongletEgg(); return; }
    return;
  }

  // Header: Conway's Game of Life and EXEC.
  if(lifeView==LIFE_STATUS && y<30 && x>=72 && x<214){ currentScreen=SCREEN_CELLULAR; return; }
  if(y<30 && x>=224 && x<274){ petExecuteConfirm=true; petGameActive=false; drawPetExecuteConfirm(); return; }

  if(lifeView==LIFE_STATUS) {
    // HIGHER/LOWER is handled on the PLAY screen where GUESS starts it.
    if(y>=200) {
      if(x<55){ lifeView=LIFE_FEED; lifeSetSpeech("What?",2200UL); drawTamagotchiScreen(); return; }
      if(x<108){ lifeView=LIFE_PLAY; lifeSetSpeech("Let's play!",2200UL); drawTamagotchiScreen(); return; }
      if(x<161){ lifeView=LIFE_CLEAN; lifeSetSpeech("Clean me!",2200UL); drawTamagotchiScreen(); return; }
      if(x<214){
        if(petSick || petHealth<=20) lifeMedicine();
        else lifeSleep();
        return;
      }
      if(x<267){ lifeView=LIFE_INFO; drawTamagotchiScreen(); return; }
      saveTamagotchi(); currentScreen=SCREEN_MENU; return;
    }
    return;
  }

  // Feed/Play/Clean return button.
  if(y>=210 && x<70){ lifeView=LIFE_STATUS; petAction=0; lifeBadFoodReaction=false; drawTamagotchiScreen(); return; }

  if(lifeView==LIFE_PLAY && petGameActive && y>=178 && y<207 && x>=160) {
    petGameChoice=(x<239)?0:1;
    bool win=(petGameChoice==petGameTarget); petGameActive=false;
    if(win){ petGameWins++; petHappiness=min(100,petHappiness+18); petMessage="THRONGLET WON!"; lifeSetEmotion("HAPPY"); lifeSetSpeech("Yes!",3000UL); oracBeep(1300,70); }
    else { petGameLosses++; petHappiness=max(0,petHappiness-4); petMessage="THRONGLET LOST!"; lifeSetEmotion("SAD"); lifeSetSpeech("Oops!",3000UL); oracBeep(280,70); }
    saveTamagotchi(); drawTamagotchiScreen(); return;
  }

  if(lifeView==LIFE_FEED && y>=48 && y<158) {
    if(y<104) { if(x<105) lifeFeed(0); else if(x<210) lifeFeed(1); else lifeFeed(2); }
    else { if(x<105) lifeFeed(3); else if(x<210) lifeFeed(4); else lifeFeed(5); }
    return;
  }
  if(lifeView==LIFE_PLAY && y>=48 && y<158) {
    int game=(y<104)?(x<105?0:(x<210?1:2)):(x<105?3:(x<210?4:5)); lifePlay(game); return;
  }
  if(lifeView==LIFE_CLEAN && y>=47 && y<117) {
    if(x<105) lifeClean(0); else if(x<210) lifeClean(1); else lifeClean(2); return;
  }
  if(lifeView==LIFE_INFO) return;
}

void updateTamagotchi() {
  if(!petLoaded) return;
  unsigned long now=millis();

  if(petHatching) {
    if(now-petHatchStarted>=5000UL) {
      petHatching=false; petDead=false; petDeathCause=0; petStage=1; petAgeMinutes=0;
      petLastEpoch=petEpochNow(); petMessage="HELLO, LITTLE THRONGLET!"; petAction=0; petSleeping=false;
      saveTamagotchi();
      if(currentScreen==SCREEN_TAMAGOTCHI && lifeView==LIFE_STATUS) {
        // Finish the hatch in-place. This deliberately avoids a full-screen
        // redraw, so the moment of hatching is smooth rather than flashing.
        drawLifeHatchingFrame(true);
        lifeCharacterAreaValid=false;
      }
    } else if(currentScreen==SCREEN_TAMAGOTCHI && lifeView==LIFE_STATUS && now-lifeLastScreenDraw>=250UL) {
      drawLifeHatchingFrame(false);
      lifeLastScreenDraw=now;
    }
    return;
  }

  // A confirmation screen must remain completely static while it is open.
  // Otherwise the normal LIFE animation tick can draw a character/speech
  // bubble back over the EXECUTE confirmation.
  if(petExecuteConfirm) return;

  if(petDead) {
    if(currentScreen==SCREEN_TAMAGOTCHI) lifeAnimateDeathGhost();
    return;
  }

  unsigned long long epoch=petEpochNow();
  if(epoch>petLastEpoch){
    unsigned long long elapsed=epoch-petLastEpoch;
    if(elapsed>86400ULL*14ULL) elapsed=86400ULL*14ULL;
    int minutes=(int)(elapsed/60ULL);
    if(minutes>0){
      petLastEpoch += (unsigned long long)minutes*60ULL;
      petAgeMinutes += minutes;
      int tenMinBlocks=minutes/10;
      if(tenMinBlocks>0){
        for(int block=0; block<tenMinBlocks; block++) {
          unsigned long long blockEpoch=petLastEpoch - (unsigned long long)(tenMinBlocks-block)*600ULL;
          int hour=(int)((blockEpoch%86400ULL)/3600ULL);
          bool night=(hour>=22 || hour<8);

          if(night) {
            // Night is deliberately forgiving: the player never has to wake
            // up to feed or clean the Thronglet. Energy recovers naturally.
            petHunger=max(0,petHunger-1);
            petCleanliness=max(0,petCleanliness-0); // no meaningful dirt loss while asleep
            petHappiness=max(0,petHappiness-0);
            petEnergy=min(100,petEnergy+4);
            if(petHealth<100 && petHunger>25 && petCleanliness>30 && !petSick)
              petHealth=min(100,petHealth+1);
          } else {
            petHunger=max(0,petHunger-2);
            petHappiness=max(0,petHappiness-1);
            petEnergy=max(0,petEnergy-1);
            petCleanliness=max(0,petCleanliness-1);

            if(petPooCount<3 && random(0,100)<12) petPooCount++;
            if(petPooCount>0) petCleanliness=max(0,petCleanliness-1);

            // Health now meaningfully reflects prolonged neglect.
            int healthLoss=0;
            if(petHunger<=25) healthLoss++;
            if(petCleanliness<=25) healthLoss++;
            if(petHunger<=8 && petCleanliness<=8) healthLoss++;
            if(petSick) healthLoss += (petHealth<=45 ? 1 : 0);
            if(healthLoss==0 && petHunger>=60 && petCleanliness>=60 && petHealth<100)
              petHealth=min(100,petHealth+1);
            else if(healthLoss>0) petHealth=max(0,petHealth-healthLoss);

            if(petHunger<15 || petCleanliness<15 || petHealth<50) petCareMistakes++;
            if(petHunger<20 || petCleanliness<20 || petHealth<45) petAttention=true;

            // Illness is more likely after sustained poor care.
            if(!petSick && petHealth<60 && (petCleanliness<35 || petHunger<25) && random(0,100)<18){
              petSick=true; petAttention=true; petMessage="THRONGLET FEELS ILL";
              lifeSetSpeech(lifeChooseSpeech(lifeSickSpeechList,LIFE_SICK_SPEECH_COUNT),4200UL);
            }
          }
        }

        petDirtyLevel=constrain((100-petCleanliness)/30,0,3);
        if(petHealth<=0){ killThronglet(2); return; }
      }
      // Automatic sleep/wake follows the RTC and is not an obligation on the player.
      int currentHour=(int)((epoch%86400ULL)/3600ULL);
      bool shouldSleep=(currentHour>=22 || currentHour<8);
      if(shouldSleep && !petSleeping && !petAutoSleepOverride && petStage>0){
        petSleeping=true; petAction=4; petAnimFrame=0; petActionUntil=0;
        petMessage="THRONGLET HAS GONE TO SLEEP";
        lifeSetSpeech(lifeChooseSpeech(lifeSleepSpeechList,LIFE_SLEEP_SPEECH_COUNT),4200UL);
      }
      if(!shouldSleep && petSleeping && petAction==4){
        petSleeping=false; petAction=0; petAutoSleepOverride=false;
        petEnergy=min(100,petEnergy+20); petMessage="GOOD MORNING, THRONGLET";
        lifeSetSpeech("GOOD MORNING!",4200UL);
      }
      if(!shouldSleep) petAutoSleepOverride=false;

      if(petAgeMinutes>=PET_DEATH_MINUTES){ killThronglet(1); return; }
      updatePetStage(); choosePetMood(); saveTamagotchi();
    }
  }

  if(petSleeping && petAction==4 && petActionUntil>0 && now>=petActionUntil){ petSleeping=false; petAction=0; petEnergy=min(100,petEnergy+35); petMessage="THRONGLET HAS WOKEN UP"; saveTamagotchi(); }

  if(now-petLastAnim>=130){
    petLastAnim=now; petAnimFrame++; lifeAnimFrame++;
    if(petAction!=0 && now>=petActionUntil && petAction!=4){
      petAction=0;
      lifeBadFoodReaction=false;
      choosePetMood();
    }
    // Animate only the small character region.  The rest of LIFE! stays
    // untouched, preventing full-screen flashing.
    if(lifeView==LIFE_STATUS) {
      if(lifeSpeechUntil<millis()) {
        lifeSetIdleSpeech();
        drawLifeCharacterArea(true);
      } else {
        drawLifeCharacterArea();
      }
    }
    else if(lifeView==LIFE_FEED || lifeView==LIFE_CLEAN || (lifeView==LIFE_PLAY && !petGameActive)) lifeDrawInteractionArea(false);
  }

  if(petGameActive && now>=petGameUntil){ petGameActive=false; petMessage="GAME TIMED OUT // TRY AGAIN"; drawTamagotchiScreen(); }
  if(!petAttention && random(0,1000)<2){ petAttention=true; petMessage=(petPooCount>0)?"THRONGLET IS CALLING // CLEAN ME":"THRONGLET WANTS YOUR ATTENTION"; oracBeep(760,45); if(lifeView==LIFE_STATUS) drawTamagotchiScreen(); }
  if(now-petLastSave>=60000UL) saveTamagotchi();
}

// ============================================================
// PK METER — SIMULATED PARANORMAL RESEARCH INSTRUMENT
// Uses the CYD LDR as a genuine environmental input, but all
// paranormal interpretation is deliberately fictional.
// ============================================================

unsigned long pkLastDraw = 0;
bool pkSweepHasPingedGhost = false;
int pkLastSweepAngleForPing = -1;
unsigned long pkLastSweepSound = 0;
int pkField = 0;
int pkBaseline = 0;
int pkBlipAngle = 0;
int pkBlipStrength = 0;
bool pkCalibrating = false;
unsigned long pkCalibrationUntil = 0;

// Ghost/anomaly motion state. The point appears, remains still,
// then follows a short controlled path before fading away.
float pkTargetX = 76;
float pkTargetY = 76;
float pkStartX = 76;
float pkStartY = 76;
float pkEndX = 76;
float pkEndY = 76;
unsigned long pkGhostStart = 0;
unsigned long pkGhostEnd = 0;
unsigned long pkGhostHoldUntil = 0;
unsigned long pkGhostNextAppearance = 0;
int pkGhostState = 0; // 0 hidden, 1 appearing/holding, 2 moving, 3 fading
uint16_t pkGhostAlpha = 255;
float pkEntityRange = 0.0f; // Simulated radar range in metres

TFT_eSprite pkSprite = TFT_eSprite(&tft);
bool pkSpriteReady = false;

int readPKField() {
  int light = analogRead(34);
  int noise = random(-8, 9);
  int value = map(light, 0, 4095, 95, 8) + noise;
  value = constrain(value, 0, 100);
  return value;
}

float pkCalculateRange() {
  // Fictional instrument scale: centre is close, outer ring is 30 m.
  float dx = pkTargetX - 70.0f;
  float dy = pkTargetY - 70.0f;
  float radial = sqrt(dx * dx + dy * dy);
  radial = constrain(radial, 0.0f, 61.0f);
  return 2.0f + (radial / 61.0f) * 28.0f;
}

void pkStartGhost(unsigned long now) {
  float a = random(0, 628) / 100.0f;
  float dist = random(8, 36);
  pkStartX = 70 + cos(a) * dist;
  pkStartY = 70 + sin(a) * dist;
  float b = a + random(-100, 101) / 100.0f;
  float dist2 = random(6, 30);
  pkEndX = 70 + cos(b) * dist2;
  pkEndY = 70 + sin(b) * dist2;
  pkTargetX = pkStartX;
  pkTargetY = pkStartY;
  pkGhostStart = now;
  pkGhostEnd = now + random(2200, 4200);
  pkGhostHoldUntil = now + random(1800, 3600);
  pkGhostNextAppearance = now;
  pkGhostState = 1;
  pkGhostAlpha = 255;
  pkSweepHasPingedGhost = false;
  pkEntityRange = pkCalculateRange();
  if (oracVolume > 0) oracPkGhostSound();
}

void drawPKRadar() {
  // Compact isolated radar area.  It deliberately sits between the
  // left/right telemetry columns so the sprite can never overwrite text.
  const int size = 140;
  const int ox = 89;
  const int oy = 40;
  const int cx = 70;
  const int cy = 70;
  const int r = 61;

  if (!pkSpriteReady) {
    pkSprite.setColorDepth(16);
    if (!pkSprite.createSprite(size, size)) {
      pkSpriteReady = false;
      Serial.print("PK: radar buffer allocation FAILED, free heap = ");
      Serial.println(ESP.getFreeHeap());
      tft.drawRoundRect(89, 40, 140, 140, 5, RED_CLOCK);
      tft.setTextFont(1); tft.setTextSize(1); tft.setTextColor(RED_CLOCK, BLACK);
      tft.setCursor(104, 104); tft.print("RADAR BUFFER ERROR");
      return;
    }
    pkSpriteReady = true;
  }

  pkSprite.fillSprite(BLACK);

  // Radar rings and crosshair.
  pkSprite.drawCircle(cx, cy, r, GREEN_CLOCK);
  pkSprite.drawCircle(cx, cy, 41, DARK_GREY);
  pkSprite.drawCircle(cx, cy, 21, DARK_GREY);
  pkSprite.drawFastHLine(cx-r, cy, r*2, DARK_GREY);
  pkSprite.drawFastVLine(cx, cy-r, r*2, DARK_GREY);

  // Soft phosphor sweep trail.
  for (int k = 7; k >= 1; k--) {
    float ta = ((pkBlipAngle - k * 4) * 3.1415926f) / 180.0f;
    int tx = cx + (int)(cos(ta) * r);
    int ty = cy + (int)(sin(ta) * r);
    uint16_t col = (k <= 2) ? GREEN_CLOCK : DARK_GREY;
    pkSprite.drawLine(cx, cy, tx, ty, col);
  }

  float a = (pkBlipAngle * 3.1415926f) / 180.0f;
  int ex = cx + (int)(cos(a) * r);
  int ey = cy + (int)(sin(a) * r);
  pkSprite.drawLine(cx, cy, ex, ey, GREEN_CLOCK);

  // Ghost-like anomaly.
  if (pkGhostState != 0 && pkGhostAlpha > 10) {
    int bx = constrain((int)pkTargetX, cx-r+6, cx+r-6);
    int by = constrain((int)pkTargetY, cy-r+6, cy+r-6);
    int a1 = (pkGhostAlpha * 70) / 255;
    int a2 = (pkGhostAlpha * 115) / 255;
    uint16_t halo1 = (a1 > 45) ? 0x1800 : 0x1000;
    uint16_t halo2 = (a2 > 70) ? 0x3000 : 0x2000;
    pkSprite.drawCircle(bx, by, 11, halo1);
    pkSprite.drawCircle(bx, by, 8, halo2);
    pkSprite.fillCircle(bx, by, 5, RED_CLOCK);
    pkSprite.fillCircle(bx, by, 2, PALE_YELLOW);
  }

  pkSprite.pushSprite(ox, oy);
}
void drawPKScreen() {
  tft.fillScreen(BLACK);

  // Header.
  tft.setTextFont(1);
  tft.setTextSize(2);
  tft.setTextColor(GREEN_CLOCK, BLACK);
  tft.setCursor(7, 3);
  tft.print("PK METER");

  tft.setTextSize(1);
  tft.setCursor(222, 8);
  tft.print("PARA-RESEARCH");
  tft.drawFastHLine(0, 28, 320, DARK_GREY);

  // Three non-overlapping columns:
  // left telemetry x=8..78, radar x=89..229, right telemetry x=236..319.
  drawPKRadar();

  tft.setTextColor(WHITE, BLACK);

  tft.setCursor(8, 37);
  tft.print("PK FIELD ");
  tft.print(pkField);
  tft.print("%");

  tft.setCursor(8, 50);
  tft.print("BASELINE ");
  tft.print(pkBaseline);

  tft.setCursor(8, 63);
  tft.print("ANOMALY  ");
  if (pkField > pkBaseline + 35) tft.print("HIGH");
  else if (pkField > pkBaseline + 15) tft.print("MEDIUM");
  else tft.print("LOW");

  tft.setCursor(236, 37);
  tft.print("VECTOR ");
  tft.print(pkBlipAngle);
  tft.print(" deg");

  tft.setCursor(236, 50);
  tft.print("STABILITY ");
  tft.print(constrain(100 - abs(pkField - pkBaseline), 0, 100));
  tft.print("%");
  tft.setCursor(236, 63);
  tft.print("RANGE ");
  if (pkGhostState != 0 && pkGhostAlpha > 10) {
    tft.print(pkEntityRange, 1);
    tft.print("m");
  } else {
    tft.print("--");
  }

  // Status line sits below the radar, never behind it.
  tft.fillRect(0, 180, 320, 18, BLACK);
  tft.setCursor(8, 184);
  if (pkGhostState != 0 && pkGhostAlpha > 10) {
    tft.setTextColor(RED_CLOCK, BLACK);
    tft.print("ENTITY DETECTED // RANGE ");
    tft.print(pkEntityRange, 1);
    tft.print(" m");
  } else {
    tft.setTextColor(CYAN_CLOCK, BLACK);
    if (pkCalibrating) tft.print("CALIBRATING FIELD BASELINE...");
    else if (pkField > pkBaseline + 35) tft.print("ANOMALOUS EVENT // INVESTIGATE");
    else if (pkField > pkBaseline + 15) tft.print("WEAK PK FLUCTUATION DETECTED");
    else tft.print("FIELD WITHIN NOMINAL PARAMETERS");
  }

  // Controls.
  tft.fillRoundRect(0, 211, 100, 24, 5, GREEN_CLOCK);
  tft.fillRoundRect(110, 211, 100, 24, 5, DARK_GREY);
  tft.fillRoundRect(220, 211, 100, 24, 5, DARK_GREY);
  tft.drawRoundRect(0, 211, 100, 24, 5, LIGHT_GREY);
  tft.drawRoundRect(110, 211, 100, 24, 5, LIGHT_GREY);
  tft.drawRoundRect(220, 211, 100, 24, 5, LIGHT_GREY);
  tft.setTextColor(BLACK, GREEN_CLOCK);
  tft.setCursor(28, 219);
  tft.print("SCAN");
  tft.setTextColor(WHITE, DARK_GREY);
  tft.setCursor(132, 219);
  tft.print("CALIBRATE");
  tft.setCursor(254, 219);
  tft.print("HOME");
}

void updatePKMeter() {
  unsigned long now = millis();
  if (now - pkLastDraw < 70) return;
  pkLastDraw = now;

  // Short sampled sonar sweep, rate-limited so it stays atmospheric.
  if (!pkCalibrating && oracVolume > 0 && now - pkLastSweepSound >= 140) {
    pkLastSweepSound = now;
    oracPkRadarSweepSound();
  }

  if (pkCalibrating) {
    pkField = readPKField();
    pkBaseline = (pkBaseline * 7 + pkField) / 8;
    if (now >= pkCalibrationUntil) pkCalibrating = false;
  } else {
    pkField = readPKField();
    pkBlipAngle = (pkBlipAngle + 2) % 360;
    int delta = abs(pkField - pkBaseline);
    pkBlipStrength = constrain(delta * 3 + random(0, 12), 0, 100);

    // Controlled ghost observation cycle: hold -> move -> fade -> vanish.
    if (pkGhostState == 0) {
      if (now >= pkGhostNextAppearance &&
          (pkBlipStrength > 20 || random(0, 100) < 3)) {
        pkStartGhost(now);
      }
    } else if (pkGhostState == 1) {
      if (now >= pkGhostHoldUntil) {
        pkGhostState = 2;
        pkGhostStart = now;
        pkGhostEnd = now + random(1800, 3200);
      }
    } else if (pkGhostState == 2) {
      float p = (float)(now - pkGhostStart) /
                (float)max(1UL, pkGhostEnd - pkGhostStart);

      if (p >= 1.0f) {
        pkGhostState = 3;
        pkGhostStart = now;
        pkGhostEnd = now + 900;
      } else {
        float e = p * p * (3.0f - 2.0f * p);
        pkTargetX = pkStartX + (pkEndX - pkStartX) * e;
        pkTargetY = pkStartY + (pkEndY - pkStartY) * e;
        pkEntityRange = pkCalculateRange();
      }
    } else if (pkGhostState == 3) {
      float p = (float)(now - pkGhostStart) /
                (float)max(1UL, pkGhostEnd - pkGhostStart);

      if (p >= 1.0f) {
        pkGhostState = 0;
        pkGhostAlpha = 0;
        pkEntityRange = 0.0f;
        pkGhostNextAppearance = now + random(4500, 10000);
      } else {
        pkGhostAlpha = (uint16_t)(255.0f * (1.0f - p));
        pkEntityRange = pkCalculateRange();
      }
    }
  }

  // Sonar contact: when the rotating sweep crosses the ghost anomaly,
  // make one short ping.  It is deliberately gated to one ping per sweep.
  if (pkGhostState != 0 && pkGhostAlpha > 10) {
    float dx = pkTargetX - 70.0f;
    float dy = pkTargetY - 70.0f;
    float ghostAngle = atan2(dy, dx) * 180.0f / 3.1415926f;
    if (ghostAngle < 0) ghostAngle += 360.0f;

    float diff = fabs(ghostAngle - (float)pkBlipAngle);
    if (diff > 180.0f) diff = 360.0f - diff;

    if (diff <= 5.0f) {
      if (!pkSweepHasPingedGhost) {
        if (oracVolume > 0) oracLifeGhostPing();
        pkSweepHasPingedGhost = true;
      }
    } else if (diff > 14.0f) {
      // Re-arm once the sweep has moved clear of the anomaly.
      pkSweepHasPingedGhost = false;
    }
  } else {
    pkSweepHasPingedGhost = false;
  }

  // The radar can animate quickly without disturbing the text.
  drawPKRadar();

  // Update telemetry less often than the radar.  This prevents the
  // changing numbers from appearing to flicker while the sweep moves.
  static unsigned long lastPKTextDraw = 0;
  if (now - lastPKTextDraw >= 220) {
    lastPKTextDraw = now;

    tft.fillRect(0, 34, 80, 38, BLACK);
    tft.setTextFont(1);
    tft.setTextSize(1);
    tft.setTextColor(WHITE, BLACK);

    tft.setCursor(8, 37);
    tft.print("PK FIELD ");
    tft.print(pkField);
    tft.print("%");

    tft.setCursor(8, 50);
    tft.print("BASELINE ");
    tft.print(pkBaseline);

    tft.setCursor(8, 63);
    tft.print("ANOMALY  ");
    if (pkField > pkBaseline + 35) tft.print("HIGH");
    else if (pkField > pkBaseline + 15) tft.print("MEDIUM");
    else tft.print("LOW");

    tft.fillRect(234, 34, 86, 30, BLACK);
    tft.setCursor(236, 37);
    tft.print("VECTOR ");
    tft.print(pkBlipAngle);
    tft.print(" deg");

    tft.setCursor(236, 50);
    tft.print("STABILITY ");
    tft.print(constrain(100 - abs(pkField - pkBaseline), 0, 100));
    tft.print("%");

    tft.setCursor(236, 63);
    tft.print("RANGE ");
    if (pkGhostState != 0 && pkGhostAlpha > 10) {
      tft.print(pkEntityRange, 1);
      tft.print("m");
    } else {
      tft.print("--");
    }

    tft.fillRect(0, 180, 320, 18, BLACK);
    tft.setCursor(8, 184);
    if (pkGhostState != 0 && pkGhostAlpha > 10) {
      tft.setTextColor(RED_CLOCK, BLACK);
      tft.print("ENTITY DETECTED // RANGE ");
      tft.print(pkEntityRange, 1);
      tft.print(" m");
    } else {
      tft.setTextColor(CYAN_CLOCK, BLACK);
      if (pkCalibrating) tft.print("CALIBRATING FIELD BASELINE...");
      else if (pkField > pkBaseline + 35) tft.print("ANOMALOUS EVENT // INVESTIGATE");
      else if (pkField > pkBaseline + 15) tft.print("WEAK PK FLUCTUATION DETECTED");
      else tft.print("FIELD WITHIN NOMINAL PARAMETERS");
    }
  }
}

void handlePKTouch(int x, int y) {
  if (y < 211) return;
  if (x < 105) {
    pkField = readPKField();
    pkBlipStrength = random(10, 101);
    pkBlipAngle = random(0, 360);
    pkSweepHasPingedGhost = false;
    pkLastSweepAngleForPing = -1;
    pkStartGhost(millis());
    drawPKScreen();
  } else if (x < 215) {
    pkCalibrating = true;
    pkCalibrationUntil = millis() + 2500;
    pkBaseline = readPKField();
    drawPKScreen();
  } else {
    currentScreen = SCREEN_MENU;
  }
}

// ============================================================
// ESP RESEARCH — SIMULATED ESP EXPERIMENTS
// ============================================================

int psychicTarget = 0;
int psychicChoice = -1;
int psychicHits = 0;
int psychicAttempts = 0;
int psychicExperiment = 0;
bool psychicWaiting = false;
unsigned long psychicRevealAt = 0;
String psychicMessage = "SELECT A CARD";
int psychicFlash = 0; // 0 none, 1 correct/green, 2 incorrect/red
unsigned long psychicFlashUntil = 0;

void psychicNewTest() {
  psychicTarget = random(0, 5);
  psychicChoice = -1;
  psychicWaiting = false;
  psychicFlash = 0;
  psychicFlashUntil = 0;
  psychicMessage = "SELECT A CARD";
}

void drawESPCard(int cx, int cy, int index, bool selected, bool target, bool correctFlash, bool wrongFlash) {
  uint16_t cardFill = 0xFFFF;
  uint16_t border = LIGHT_GREY;
  if (correctFlash) { cardFill = GREEN_CLOCK; border = GREEN_CLOCK; }
  else if (wrongFlash) { cardFill = RED_CLOCK; border = RED_CLOCK; }
  else if (selected) { cardFill = PALE_YELLOW; border = PALE_YELLOW; }

  tft.fillRoundRect(cx - 24, cy - 24, 48, 48, 5, cardFill);
  tft.drawRoundRect(cx - 24, cy - 24, 48, 48, 5, border);
  tft.drawRoundRect(cx - 21, cy - 21, 42, 42, 3, border == LIGHT_GREY ? DARK_GREY : border);

  // Classic Zener-style ESP symbols: circle, cross, wavy lines, square, star.
  uint16_t ink = (correctFlash || wrongFlash || selected) ? BLACK : DARK_GREY;
  if (index == 0) {
    tft.drawCircle(cx, cy, 12, ink);
    tft.drawCircle(cx, cy, 10, ink);
  } else if (index == 1) {
    tft.drawFastHLine(cx-13, cy, 26, ink);
    tft.drawFastVLine(cx, cy-13, 26, ink);
  } else if (index == 2) {
    // Classic Zener wave card: three clean vertical wavy lines.
    for (int line=-1; line<=1; line++) {
      int baseX = cx + line * 8;
      int lastX = baseX;
      int lastY = cy - 14;
      for (int yy=-13; yy<=14; yy++) {
        float phase = (yy + 13) * 0.42f;
        int xx = baseX + (int)(sin(phase) * 4.0f);
        tft.drawLine(lastX, lastY, xx, cy + yy, ink);
        lastX = xx;
        lastY = cy + yy;
      }
    }
  } else if (index == 3) {
    tft.drawRect(cx-11, cy-11, 22, 22, ink);
  } else {
    // Five-point star.
    float pts[10];
    for (int i=0;i<10;i++) pts[i] = (i%2==0) ? 13.0f : 5.5f;
    int px[10], py[10];
    for (int i=0;i<10;i++) {
      float ang = -3.1415926f/2.0f + i*3.1415926f/5.0f;
      px[i] = cx + (int)(cos(ang)*pts[i]);
      py[i] = cy + (int)(sin(ang)*pts[i]);
    }
    for (int i=0;i<10;i++) tft.drawLine(px[i],py[i],px[(i+1)%10],py[(i+1)%10],ink);
  }

  if (target && !psychicWaiting) {
    tft.drawRoundRect(cx - 19, cy - 19, 38, 38, 3, RED_CLOCK);
  }
}

void drawPsychicScreen() {
  tft.fillScreen(BLACK);
  tft.setTextFont(1);
  tft.setTextSize(2);
  tft.setTextColor(PURPLE_CLOCK, BLACK);
  tft.setCursor(7, 3);
  tft.print("ESP TEST");
  tft.setTextSize(1);
  tft.setCursor(246, 8);
  tft.print("ZENER LAB");
  tft.drawFastHLine(0, 28, 320, DARK_GREY);

  tft.setTextColor(WHITE, BLACK);
  tft.setCursor(8, 38);
  if (psychicExperiment == 0) tft.print("EXPERIMENT 01 // SYMBOL PREDICTION");
  else if (psychicExperiment == 1) tft.print("EXPERIMENT 02 // COLOUR PREDICTION");
  else tft.print("EXPERIMENT 03 // NUMBER PREDICTION");

  tft.setCursor(8, 55);
  tft.setTextColor(CYAN_CLOCK, BLACK);
  tft.print(psychicWaiting ? "ANALYSING YOUR RESPONSE..." : "CHOOSE ONE CARD");

  if (psychicExperiment == 0) {
    for (int i = 0; i < 5; i++) {
      bool correct = (psychicFlash == 1 && psychicChoice == i);
      bool wrong = (psychicFlash == 2 && psychicChoice == i);
      bool tgt = (!psychicWaiting && psychicFlash != 0 && psychicTarget == i);
      drawESPCard(34 + i*63, 101, i, psychicChoice == i, tgt, correct, wrong);
    }
  } else if (psychicExperiment == 1) {
    const uint16_t cols[] = {RED_CLOCK, GREEN_CLOCK, BLUE_CLOCK, PALE_YELLOW, PURPLE_CLOCK};
    for (int i = 0; i < 5; i++) {
      bool sel = (psychicChoice == i);
      bool correct = (psychicFlash == 1 && sel);
      bool wrong = (psychicFlash == 2 && sel);
      uint16_t fill = correct ? GREEN_CLOCK : (wrong ? RED_CLOCK : (sel ? PALE_YELLOW : cols[i]));
      tft.fillRoundRect(10+i*63,77,48,48,7,fill);
      tft.drawRoundRect(10+i*63,77,48,48,7,sel ? PALE_YELLOW : WHITE);
      if (!psychicWaiting && psychicFlash != 0 && psychicTarget == i) tft.drawRoundRect(13+i*63,80,42,42,5,RED_CLOCK);
    }
  } else {
    for (int i = 0; i < 5; i++) {
      bool sel = (psychicChoice == i);
      bool correct = (psychicFlash == 1 && sel);
      bool wrong = (psychicFlash == 2 && sel);
      uint16_t fill = correct ? GREEN_CLOCK : (wrong ? RED_CLOCK : (sel ? PALE_YELLOW : DARK_GREY));
      tft.fillRoundRect(10+i*63,77,48,48,7,fill);
      tft.drawRoundRect(10+i*63,77,48,48,7,sel ? PALE_YELLOW : LIGHT_GREY);
      tft.setTextFont(4); tft.setTextColor((sel || correct || wrong) ? BLACK : WHITE, fill); tft.setCursor(26+i*63,91); tft.print(i+1);
    }
  }

  tft.setTextFont(1); tft.setTextSize(1);
  tft.setTextColor(PALE_YELLOW, BLACK); tft.setCursor(8, 140); tft.print(psychicMessage);
  if (!psychicWaiting && psychicChoice >= 0) {
    tft.setCursor(8, 154); tft.setTextColor(WHITE, BLACK); tft.print("YOU CHOSE: ");
    if (psychicExperiment == 0) { const char *s2[] = {"CIRCLE","CROSS","WAVES","SQUARE","STAR"}; tft.print(s2[psychicChoice]); }
    else if (psychicExperiment == 1) { const char *c2[] = {"RED","GREEN","BLUE","YELLOW","PURPLE"}; tft.print(c2[psychicChoice]); }
    else tft.print(psychicChoice + 1);
    tft.print("  CORRECT: ");
    if (psychicExperiment == 0) { const char *s3[] = {"CIRCLE","CROSS","WAVES","SQUARE","STAR"}; tft.print(s3[psychicTarget]); }
    else if (psychicExperiment == 1) { const char *c3[] = {"RED","GREEN","BLUE","YELLOW","PURPLE"}; tft.print(c3[psychicTarget]); }
    else tft.print(psychicTarget + 1);
  }
  tft.setCursor(8, 168); tft.print("HITS: "); tft.print(psychicHits); tft.print("  MISSES: "); tft.print(psychicAttempts - psychicHits); tft.print("  ATTEMPTS: "); tft.print(psychicAttempts);
  tft.setCursor(8, 184);
  if (psychicAttempts == 0) tft.print("CORRELATION: UNTESTED");
  else { tft.print("CORRELATION: "); tft.print((psychicHits * 100) / psychicAttempts); tft.print("%"); }

  tft.fillRoundRect(0, 207, 76, 28, 5, PURPLE_CLOCK);
  tft.fillRoundRect(81,207, 76, 28, 5, CYAN_CLOCK);
  tft.fillRoundRect(162,207, 76, 28, 5, PALE_YELLOW);
  tft.fillRoundRect(243,207, 77, 28, 5, DARK_GREY);
  tft.drawRoundRect(0,207,76,28,5,LIGHT_GREY); tft.drawRoundRect(81,207,76,28,5,LIGHT_GREY); tft.drawRoundRect(162,207,76,28,5,LIGHT_GREY); tft.drawRoundRect(243,207,77,28,5,LIGHT_GREY);
  tft.setTextColor(BLACK, PURPLE_CLOCK); tft.setCursor(12,217); tft.print("NEW TEST");
  tft.setTextColor(BLACK, CYAN_CLOCK); tft.setCursor(89,217); tft.print("EXPERIMENT");
  tft.setTextColor(BLACK, PALE_YELLOW); tft.setCursor(173,217); tft.print("RESET");
  tft.setTextColor(WHITE, DARK_GREY); tft.setCursor(261,217); tft.print("HOME");
}

void psychicReveal(int choice) {
  if (psychicWaiting) return;
  psychicChoice = choice;
  psychicWaiting = true;
  psychicFlash = 0;
  psychicRevealAt = millis() + 850;
  psychicMessage = "ANALYSING RESPONSE...";
}

void loadPsychicMemory() {
  psychicHits = prefs.getInt("psyHits", 0);
  psychicAttempts = prefs.getInt("psyAtt", 0);
}

void savePsychicMemory() {
  prefs.putInt("psyHits", psychicHits);
  prefs.putInt("psyAtt", psychicAttempts);
}

void handlePsychicTouch(int x, int y) {
  if (y >= 207) {
    if (x < 81) { psychicNewTest(); drawPsychicScreen(); }
    else if (x < 162) { psychicExperiment = (psychicExperiment + 1) % 3; psychicNewTest(); drawPsychicScreen(); }
    else if (x < 243) { psychicHits = 0; psychicAttempts = 0; savePsychicMemory(); psychicNewTest(); drawPsychicScreen(); }
    else currentScreen = SCREEN_MENU;
    return;
  }

  if (y >= 70 && y < 135 && !psychicWaiting && psychicFlash == 0) {
    int choice = constrain(x / 63, 0, 4);
    psychicReveal(choice);
    drawPsychicScreen();
  }
}

void updatePsychicResearch() {
  if (psychicWaiting && millis() >= psychicRevealAt) {
    psychicWaiting = false;
    psychicAttempts++;
    if (psychicChoice == psychicTarget) {
      psychicHits++;
      psychicFlash = 1;
      psychicMessage = "CORRECT // HIT";
      oracBeep(1200, 55);
    } else {
      psychicFlash = 2;
      psychicMessage = "INCORRECT // TRY AGAIN";
      oracBeep(280, 70);
    }
    psychicFlashUntil = millis() + 900;
    savePsychicMemory();
    drawPsychicScreen();
  } else if (!psychicWaiting && psychicFlash != 0 && millis() >= psychicFlashUntil) {
    psychicFlash = 0;
    psychicChoice = -1;
    psychicTarget = random(0, 5);
    psychicMessage = "SELECT A CARD";
    drawPsychicScreen();
  }
}

// ============================================================
// RANDOM UTILITY — DICE + RANDOM NUMBER GENERATOR
// ============================================================

void drawRandomButton(int x, int y, int w, int h, const char *text, uint16_t c) {
  tft.fillRoundRect(x,y,w,h,5,c);
  tft.drawRoundRect(x,y,w,h,5,LIGHT_GREY);
  tft.setTextFont(1); tft.setTextSize(1);
  tft.setTextColor(c == DARK_GREY ? WHITE : BLACK, c);
  int tw=tft.textWidth(text); tft.setCursor(x+(w-tw)/2,y+(h-8)/2); tft.print(text);
}

void drawRandomScreen() {
  tft.fillScreen(BLACK);
  tft.setTextFont(1); tft.setTextSize(2); tft.setTextColor(ORANGE_CLOCK,BLACK);
  tft.setCursor(8,3); tft.print("RANDOM LAB");
  tft.setTextSize(1); tft.setTextColor(CYAN_CLOCK,BLACK); tft.setCursor(231,8); tft.print("UNPREDICTABLE");
  tft.drawFastHLine(0,28,320,DARK_GREY);
  drawRandomButton(8,35,148,26,"DICE ROLLER", randomMode==0 ? ORANGE_CLOCK : DARK_GREY);
  drawRandomButton(164,35,148,26,"NUMBER GENERATOR", randomMode==1 ? ORANGE_CLOCK : DARK_GREY);

  if (randomMode == 0) {
    // Compact die selector on the left, large result display on the right.
    const int dx=5, dw=96, dh=18, dy=66, dg=2;
    const int dice[7]={4,6,8,10,12,20,100};
    for(int i=0;i<7;i++) {
      String lab="D"+String(dice[i]);
      drawRandomButton(dx,dy+i*(dh+dg),dw,dh,lab.c_str(),randomDiceSides==dice[i]?PALE_YELLOW:DARK_GREY);
    }
    tft.fillRoundRect(112,68,200,138,7,MID_GREY);
    tft.drawRoundRect(112,68,200,138,7,LIGHT_GREY);
    tft.setTextColor(CYAN_CLOCK,MID_GREY); tft.setTextSize(1); tft.setCursor(124,78); tft.print("RESULT // D"); tft.print(randomDiceSides);
    tft.setTextColor(PALE_YELLOW,MID_GREY); tft.setTextSize(5);
    String rr=String(randomResult); int tw=tft.textWidth(rr); tft.setCursor(212-tw/2,119); tft.print(rr);
    tft.setTextColor(WHITE,MID_GREY); tft.setTextSize(1); tft.setCursor(133,185); tft.print("ROLL TO CONTINUE");
  } else {
    drawRandomButton(8,66,148,22,"RANGE", randomNumberMode==0 ? ORANGE_CLOCK : DARK_GREY);
    drawRandomButton(164,66,148,22,"1 TO N", randomNumberMode==1 ? ORANGE_CLOCK : DARK_GREY);
    if (randomNumberMode == 0) {
      tft.setTextColor(CYAN_CLOCK,BLACK); tft.setTextSize(1); tft.setCursor(8,94); tft.print("MIN / MAX");
      drawRandomButton(8,107,72,24,"MIN -10",DARK_GREY); drawRandomButton(84,107,72,24,"MIN +10",DARK_GREY);
      drawRandomButton(8,134,72,24,"MAX -10",DARK_GREY); drawRandomButton(84,134,72,24,"MAX +10",DARK_GREY);
      tft.setTextColor(WHITE,BLACK); tft.setCursor(8,166); tft.print("RANGE");
      tft.setTextColor(PALE_YELLOW,BLACK); tft.setCursor(8,178); tft.print(randomMin); tft.print(" - "); tft.print(randomMax);
      tft.fillRoundRect(170,94,142,94,7,MID_GREY); tft.drawRoundRect(170,94,142,94,7,LIGHT_GREY);
      tft.setTextColor(CYAN_CLOCK,MID_GREY); tft.setCursor(184,105); tft.print("GENERATED");
      tft.setTextColor(PALE_YELLOW,MID_GREY); tft.setTextSize(4);
      String rr=String(randomResult); int tw=tft.textWidth(rr); tft.setCursor(241-tw/2,130); tft.print(rr);
      tft.setTextSize(1); tft.setTextColor(WHITE,MID_GREY); tft.setCursor(195,170); tft.print("READY");
    } else {
      tft.setTextColor(CYAN_CLOCK,BLACK); tft.setTextSize(1); tft.setCursor(8,94); tft.print("N = ");
      tft.setTextColor(PALE_YELLOW,BLACK); tft.print(randomNInput);
      const char *keys[]={"7","8","9","DEL","4","5","6","0","1","2","3","CLR"};
      for(int i=0;i<12;i++){ int col=i%4,row=i/4; uint16_t c=(i==3||i==11)?DARK_GREY:CYAN_CLOCK; drawRandomButton(8+col*38,108+row*25,35,22,keys[i],c); }
      tft.fillRoundRect(166,94,146,94,7,MID_GREY); tft.drawRoundRect(166,94,146,94,7,LIGHT_GREY);
      tft.setTextColor(CYAN_CLOCK,MID_GREY); tft.setCursor(178,105); tft.print("GENERATED");
      tft.setTextColor(PALE_YELLOW,MID_GREY); tft.setTextSize(4);
      String rr=String(randomResult); int tw=tft.textWidth(rr); tft.setCursor(239-tw/2,130); tft.print(rr);
      tft.setTextSize(1); tft.setTextColor(WHITE,MID_GREY); tft.setCursor(187,170); tft.print("1 TO "); tft.print(randomNInput);
    }
  }
  drawRandomButton(0,207,100,28,randomMode==0?"ROLL":"GENERATE",ORANGE_CLOCK);
  drawRandomButton(110,207,100,28,"CLEAR",DARK_GREY); drawRandomButton(210,207,100,28,"HOME",DARK_GREY);
}

void handleRandomTouch(int x,int y) {
  if(y>=35 && y<62){ if(x<160) randomMode=0; else randomMode=1; drawRandomScreen(); return; }
  if(randomMode==0){
    const int dy=66,dh=18,dg=2;
    // The seven die buttons are drawn in the left 96-pixel column.  Keep
    // the touch region constrained to that same column so taps on the
    // result panel cannot accidentally select a die.
    if(x>=5 && x<101 && y>=dy && y<dy+7*(dh+dg)){
      int i=(y-dy)/(dh+dg);
      if(i>=0&&i<7&&(y-dy)%(dh+dg)<dh){
        const int dice[7]={4,6,8,10,12,20,100};
        randomDiceSides=dice[i];
        drawRandomScreen();
        return;
      }
    }
  } else {
    if(y>=66&&y<89){ randomNumberMode=(x<160)?0:1; drawRandomScreen(); return; }
    if(randomNumberMode==0){
      if(y>=107&&y<158){ if(y<133){ if(x<80) randomMin=max(1,randomMin-10); else if(x<156) randomMin=min(randomMax-1,randomMin+10); } else { if(x<80) randomMax=max(randomMin+1,randomMax-10); else if(x<156) randomMax=min(9999,randomMax+10); } drawRandomScreen(); return; }
    } else {
      if(y>=108&&y<183&&x<160){ int col=x/38,row=(y-108)/25; if(col>3)col=3;if(row>2)row=2; int idx=row*4+col; if(x%38>=35||(y-108)%25>=22)return; const char *keys[]={"7","8","9","DEL","4","5","6","0","1","2","3","CLR"}; if(idx==3){if(randomNInput.length()>0)randomNInput.remove(randomNInput.length()-1);} else if(idx==11)randomNInput=""; else {if(randomNInput=="0")randomNInput="";if(randomNInput.length()<4)randomNInput+=keys[idx];} if(randomNInput.length()==0)randomNInput="0"; drawRandomScreen(); return; }
    }
  }
  if(y>=207){ if(x<100){ randomResult=(randomMode==0)?random(1,randomDiceSides+1):(randomNumberMode==1?random(1,max(2L,(long)randomNInput.toInt()+1L)):random(randomMin,randomMax+1)); oracBeep(1000,45); drawRandomScreen(); } else if(x<220){ randomResult=0; if(randomMode==1&&randomNumberMode==1)randomNInput=""; drawRandomScreen(); } else currentScreen=SCREEN_MENU; }
}

// ============================================================
// DRAW CURRENT SCREEN
// ============================================================

// ============================================================
// RAM-AWARE SCREEN RESOURCE MANAGEMENT
// ============================================================

void releaseLifeGraphics() {
  if (lifeCharacterSpriteReady) { lifeCharacterSprite.deleteSprite(); lifeCharacterSpriteReady = false; }
  if (lifeInteractionSpriteReady) { lifeInteractionSprite.deleteSprite(); lifeInteractionSpriteReady = false; }
  if (lifeGhostSpriteReady) { lifeGhostSprite.deleteSprite(); lifeGhostSpriteReady = false; }
  lifeCharacterAreaValid = false;
  life2LoadedName = "";
  lifeDisplayedSprite = -1;
  lifeDisplayedEmotion = "";
  lifeDisplayedAnimPhase = -1;
  Serial.print("RAM: released LIFE sprites, free heap = "); Serial.println(ESP.getFreeHeap());
}

void ensureLifeGraphics() {
  if (!lifeStagePixels) loadBurtStageSprites();
}

void releasePKSprite() {
  if (pkSpriteReady) {
    pkSprite.deleteSprite();
    pkSpriteReady = false;
    Serial.print("RAM: released PK radar, free heap = "); Serial.println(ESP.getFreeHeap());
  }
}

void releaseOracScreenGraphics(Screen oldScreen) {
  // Large screen-specific allocations are released whenever we leave their
  // screen. This prevents a later 38 KB radar allocation from failing due
  // to fragmentation, while keeping all existing features intact.
  if (oldScreen == SCREEN_MENU && currentScreen != SCREEN_MENU) {
    releaseMenuGraphicsForAI();
    for (int i = 0; i < BURT_MENU_MAX; ++i) menuThronglets[i].active = false;
  }
  if (oldScreen == SCREEN_TAMAGOTCHI && currentScreen != SCREEN_TAMAGOTCHI) {
    releaseLifeGraphics();
  }
  if (oldScreen == SCREEN_PK_METER && currentScreen != SCREEN_PK_METER) {
    releasePKSprite();
  }
  if (oldScreen == SCREEN_SETTINGS && currentSettings != SETTINGS_WIFI) {
    clearWifiScanResults();
  }
}

void drawCurrentScreen() {

  switch (
    currentScreen
  ) {

    case SCREEN_MENU:

      drawMainMenu();

      break;

    case SCREEN_FIBONACCI:

      lastFibHour = -1;
      lastFibMinuteUnit = -1;
      lastFibSecond = -1;
      lastFibFooterHour = -1;
      lastFibFooterMinuteUnit = -1;

      drawFibonacciClock(
        true
      );

      break;

    case SCREEN_FLIP:

      lastFlipHour = -1;
      lastFlipMinute = -1;
      lastFlipSecond = -1;

      drawFlipClock(true);

      break;

    case SCREEN_MANDELBROT:

      drawMandelbrot();

      break;

    case SCREEN_TIME_TOOLS:

      drawTimeTools(
        true
      );

      break;

    case SCREEN_CALCULATOR:

      drawCalculator();

      break;

    case SCREEN_CONVERTER:

      drawConverter();

      break;

    case SCREEN_AI:

      drawAiScreen();

      break;

    case SCREEN_ORAC:

      oracSessionCount++;
      oracButtonPressCount += 0;
      chooseOracMood();
      saveOracMemory();
      setOracMessage(0);
      drawOracScreen();

      break;

    case SCREEN_SNAKE:

      startSnakeGame();
      drawSnakeScreen();

      break;

    case SCREEN_TAMAGOTCHI:

      startTamagotchi();
      drawTamagotchiScreen();

      break;

    case SCREEN_CELLULAR:

      startCellularLife();

      break;

    case SCREEN_PK_METER:

      pkBaseline = readPKField();
      pkField = pkBaseline;
      pkBlipAngle = 0;
      pkBlipStrength = 0;
      pkTargetX = 76;
      pkTargetY = 76;
      pkGhostState = 0;
      pkGhostAlpha = 0;
      pkGhostNextAppearance = millis() + random(4500, 9000);
      pkCalibrating = false;
      drawPKScreen();

      break;

    case SCREEN_PSYCHIC:

      psychicNewTest();
      drawPsychicScreen();

      break;

    case SCREEN_RANDOM:

      drawRandomScreen();

      break;

    case SCREEN_SETTINGS:

      switch (
        currentSettings
      ) {

        case SETTINGS_MENU:

          drawSettingsMenu();

          break;

        case SETTINGS_CLOCK:

          drawSetClock();

          break;

        case SETTINGS_DATE:

          drawSetDate();

          break;

        case SETTINGS_COUNTDOWN:

          drawSetCountdown();

          break;

        case SETTINGS_BRIGHTNESS:

          drawBrightness();

          break;

        case SETTINGS_SOUND:

          drawSoundSettings();

          break;

        case SETTINGS_FORMAT:

          drawFormatSettings();

          break;

        case SETTINGS_WIFI:

          drawWifiSettings();

          break;

        case SETTINGS_DIAGNOSTIC:

          drawMemoryDiagnostics();

          break;
      }

      break;
  }
}

// ============================================================
// ORAC SOUNDS
// ============================================================

// ------------------------------------------------------------
// SD-CARD SAMPLE PLAYER
// Samples are unsigned 8-bit mono PCM at 8000 Hz. They live on the SD card
// under /ORAC/SOUNDS/ and are loaded only for the short moment they play.
// ------------------------------------------------------------
void oracPlaySdRaw(const char *path, int sampleVolume, bool applyMasterVolume) {
  if (!path || !path[0] || oracVolume <= 0) return;

  File f = SD.open(path, FILE_READ);
  if (!f) {
    Serial.print("ORAC AUDIO: missing sample ");
    Serial.println(path);
    return;
  }

  size_t len = f.size();
  if (len == 0 || len > 4096) {
    f.close();
    Serial.print("ORAC AUDIO: invalid sample ");
    Serial.println(path);
    return;
  }

  uint8_t *buf = (uint8_t *)malloc(len);
  if (!buf) {
    f.close();
    Serial.println("ORAC AUDIO: sample RAM allocation failed");
    return;
  }

  size_t got = f.read(buf, len);
  f.close();
  if (got != len) {
    free(buf);
    Serial.println("ORAC AUDIO: sample read failed");
    return;
  }

  int32_t gain;
  if (sampleVolume >= 0 && !applyMasterVolume) {
    gain = sampleVolume;
  } else {
    gain = oracVolume;
    if (sampleVolume >= 0) gain = (gain * sampleVolume) / 100;
  }
  for (size_t i = 0; i < len; i++) {
    int16_t centered = (int16_t)buf[i] - 128;
    int16_t scaled = (int16_t)((centered * gain) / 100);
    int16_t out = 128 + scaled;
    if (out < 0) out = 0;
    if (out > 255) out = 255;
    dacWrite(SPEAKER_PIN, (uint8_t)out);
    delayMicroseconds(125);
  }

  dacWrite(SPEAKER_PIN, 0);
  free(buf);
}

void oracThrongletPoot() {
  // Small synthetic "poot" when a menu Thronglet poos.
  // Uses a gentle falling low tone with softened attack/release.
  if (oracVolume <= 0) {
    dacWrite(SPEAKER_PIN, 0);
    return;
  }

  const int sampleRate = 8000;
  const int durationMs = 95;
  const int samples = (sampleRate * durationMs) / 1000;
  const float masterScale = (float)oracVolume / 100.0f;

  for (int i = 0; i < samples; i++) {
    float t = (float)i / (float)sampleRate;
    float p = t / ((float)durationMs / 1000.0f);

    // Descending "poot" pitch, with a gentle attack and release so it
    // doesn't produce the sharp click of an abruptly-started tone.
    float freq = 180.0f - 75.0f * p;
    float phase = 2.0f * PI * (180.0f * t - 37.5f * t * t / 0.095f);
    float env = 1.0f - expf(-t * 180.0f);
    env *= expf(-t * 20.0f);
    env *= (0.96f + 0.04f * cosf(2.0f * PI * p));

    float wave = sinf(phase) * env;
    // A tiny amount of second harmonic gives it a comic, rounded character.
    wave += 0.18f * sinf(2.0f * phase) * env;
    wave = constrain(wave, -1.0f, 1.0f);

    int amplitude = (int)lroundf(34.0f * masterScale);
    int output = constrain(128 + (int)lroundf(wave * amplitude), 0, 255);
    dacWrite(SPEAKER_PIN, (uint8_t)output);
    delayMicroseconds(125);
  }

  dacWrite(SPEAKER_PIN, 0);
}

void oracThrongletFootstep(int variant) {
  // Synthetic Thronglet footstep.  The proven 800 Hz waveform remains
  // present, but its attack is softened and shortened to reduce the click.
  if (oracFootstepVolume <= 0 || oracVolume <= 0) {
    dacWrite(SPEAKER_PIN, 0);
    return;
  }

  // Quieter, more useful range for the real walking sound.
  int targetAmp;
  switch (oracFootstepVolume) {
    case 10:  targetAmp = 2;   break;
    case 25:  targetAmp = 7;   break;
    case 50:  targetAmp = 18;  break;
    case 100: targetAmp = 55;  break;
    default:
      targetAmp = map(constrain(oracFootstepVolume, 0, 100), 0, 100, 0, 55);
      break;
  }

  const int sampleRate = 8000;
  const int durationMs = 58;
  const int samples = (sampleRate * durationMs) / 1000;
  const float bodyFreq = variant ? 125.0f : 105.0f;

  Serial.print("FOOTSTEP: soft hybrid setting=");
  Serial.print(oracFootstepVolume);
  Serial.print(" targetAmp=");
  Serial.print(targetAmp);
  Serial.print(" variant=");
  Serial.println(variant ? 2 : 1);

  for (int i = 0; i < samples; i++) {
    float t = (float)i / (float)sampleRate;

    // A short 800 Hz contact sound.  The smooth envelope removes the
    // abrupt edge that made the previous version sound clicky.
    float impactEnv = expf(-t * 115.0f);
    float attack = 1.0f - expf(-t * 650.0f);
    float impact = 0.0f;
    if (t < 0.018f) {
      impact = ((i % 10) < 5) ? 1.0f : -1.0f;
      impact *= impactEnv * attack;
    }

    // Rounded low-frequency body underneath the contact sound.
    float bodyEnv = expf(-t * 58.0f);
    float body = 0.30f * sinf(2.0f * PI * bodyFreq * t) * bodyEnv;

    float wave = impact + body;
    wave = constrain(wave, -1.0f, 1.0f);

    int scaled = (int)lroundf(wave * (float)targetAmp);
    int output = constrain(128 + scaled, 0, 255);

    dacWrite(SPEAKER_PIN, (uint8_t)output);
    delayMicroseconds(125);
  }

  dacWrite(SPEAKER_PIN, 0);
}

void oracPkRadarSweepSound() {
  oracPlaySdRaw("/ORAC/SOUNDS/PK_RADAR_SWEEP.RAW");
}

void oracPkGhostSound() {
  oracPlaySdRaw("/ORAC/SOUNDS/PK_GHOST_AMBIENCE.RAW");
}

void oracMedicineSound() {
  // Gentle "medicine taken" chime. Keep it short so it does not dominate LIFE.
  oracBeep(660,45);
  oracBeep(880,55);
}

// ------------------------------------------------------------
// CYD SPEAKER DRIVER
// GPIO26 is the ESP32 DAC2/audio output on the standard CYD.
// The board routes it to the audio amplifier/speaker connector.
// Using the DAC directly is more reliable for this CYD than relying
// on the different LEDC APIs used by different ESP32 core versions.
// ------------------------------------------------------------
void oracBeep(uint16_t freq, uint16_t duration) {
  // Software volume control.  0% is silent and 100% is full DAC swing.
  int16_t amplitude = (int16_t)((127L * oracVolume) / 100L);
  if (amplitude <= 0) {
    dacWrite(SPEAKER_PIN, 0);
    return;
  }

  uint32_t start = millis();
  uint32_t periodUs = (freq > 0) ? (1000000UL / freq) : 0;
  if (!periodUs) return;

  while ((millis() - start) < duration) {
    dacWrite(SPEAKER_PIN, 128 + amplitude);
    delayMicroseconds(periodUs / 2);
    dacWrite(SPEAKER_PIN, 128 - amplitude);
    delayMicroseconds(periodUs / 2);
  }
  dacWrite(SPEAKER_PIN, 0);
}

// ============================================================
// O.R.A.C. SOUND PALETTE
// ============================================================
// Short, electronically generated sounds only: no flash-heavy samples.

void oracTonePair(uint16_t a, uint16_t b, uint16_t d=45) {
  oracBeep(a, d);
  delay(8);
  oracBeep(b, d);
}

void oracLifeGhostPing() {
  // Radar contact: two quick sonar-like notes.
  oracBeep(1050, 38);
  delay(10);
  oracBeep(1560, 55);
}

void oracLifeHeartbeat() {
  // Very subtle living-character pulse.
  oracBeep(150, 32);
  delay(35);
  oracBeep(125, 42);
}

void oracLifeHatch() {
  // Rising "something is waking up" sequence.
  const uint16_t notes[] = {220, 277, 330, 440, 554, 660};
  for (uint8_t i=0; i<6; i++) {
    oracBeep(notes[i], 42);
    delay(6);
  }
}

void oracLifeHappy() {
  oracTonePair(660, 990, 42);
}

void oracLifeSad() {
  oracBeep(440, 65);
  delay(12);
  oracBeep(330, 90);
}

void oracConwayTick() {
  // Tiny digital movement tick; deliberately quiet and brief.
  oracBeep(520, 12);
}

void oracSnakeMove() {
  oracBeep(310, 9);
}

void oracSnakeEat() {
  oracBeep(700, 24);
  delay(5);
  oracBeep(1100, 32);
  delay(5);
  oracBeep(1500, 38);
}

void oracSnakeCrash() {
  oracBeep(260, 80);
  delay(10);
  oracBeep(130, 120);
}

void oracWifiConnectSound() {
  oracBeep(523, 45);
  delay(8);
  oracBeep(659, 45);
  delay(8);
  oracBeep(784, 70);
}

void oracWifiFailSound() {
  oracBeep(330, 65);
  delay(10);
  oracBeep(220, 100);
}

void oracAiThinking() {
  // O.R.A.C.'s little "thinking" chirp.
  oracBeep(880, 22);
  delay(18);
  oracBeep(1047, 22);
  delay(18);
  oracBeep(1319, 22);
}

void oracAiDone() {
  // Friendly terminal-style completion sound.
  oracBeep(784, 28);
  delay(8);
  oracBeep(988, 34);
  delay(8);
  oracBeep(1319, 55);
}

void oracOracAttention() {
  // Distinctive O.R.A.C. "I've heard you" motif.
  oracBeep(392, 38);
  delay(10);
  oracBeep(587, 38);
  delay(10);
  oracBeep(784, 65);
}

void oracOracError() {
  oracBeep(196, 70);
  delay(15);
  oracBeep(147, 110);
}

void oracOracWake() {
  // Short sci-fi boot/wake motif.
  const uint16_t notes[] = {262, 330, 494, 659, 988};
  const uint16_t lens[]  = {30, 30, 35, 35, 70};
  for (uint8_t i=0; i<5; i++) {
    oracBeep(notes[i], lens[i]);
    delay(5);
  }
}


void oracClick() {
  oracBeep(1400, 28);
}

void oracBootSound() {
  oracBeep(660, 55);
  oracBeep(880, 55);
  oracBeep(1320, 85);
}

// ============================================================
// SETUP
// ============================================================

void setup() {

  Serial.begin(
    115200
  );

  // DISPLAY

  tft.init();

  tft.setRotation(1);

  tft.fillScreen(
    BLACK
  );

  // BACKLIGHT

  pinMode(
    TFT_BACKLIGHT,
    OUTPUT
  );

  // ONBOARD SPEAKER
  pinMode(SPEAKER_PIN, OUTPUT);
  dacWrite(SPEAKER_PIN, 0);

  // I2C

  Wire.begin(
    SDA_PIN,
    SCL_PIN
  );

  // TOUCH

  pinMode(
    TOUCH_CS,
    OUTPUT
  );

  digitalWrite(
    TOUCH_CS,
    HIGH
  );

  pinMode(
    TOUCH_IRQ,
    INPUT
  );

  touchSPI.begin(
    TOUCH_CLK,
    TOUCH_MISO,
    TOUCH_MOSI,
    TOUCH_CS
  );

  // SETTINGS

  loadSettings();
  loadOracWifiSettings();
  snakeHighScore = prefs.getInt("snakeHi", 0);
  loadPsychicMemory();
  loadOracMemory();

  // Seed ORAC's harmless pseudo-random behaviour (including rare glitches).
  randomSeed(micros() ^ analogRead(34));

  // Start Wi-Fi discovery for O.R.A.C. AI. This is non-blocking: setup()
  // continues immediately and the main loop completes the connection.
  connectOracWifi();

  menuBurtLoaded = loadBurtMenuSprites();
  // Menu artwork uses the same SD card.
  if (!SD.begin(SD_CS_PIN)) Serial.println("ORAC MENU SD: card not found");
  lifeSpritesLoaded = loadBurtLifeSprites();
  loadBurtStageSprites();
  resetMenuBurtTimer();

  // Choose one startup splash phrase. It remains unchanged until the next reboot.
  int startupIndex = random(0, sizeof(menuStartupPhrases) / sizeof(menuStartupPhrases[0]));
  menuStartupPhrase = menuStartupPhrases[startupIndex];

  setBrightness();

  // Short retro ORAC startup chirp.
  oracBootSound();

  drawMainMenu();
}

// ============================================================
// LOOP
// ============================================================

void loop() {

  static Screen lastScreen =
    SCREEN_MENU;

  static SettingsScreen lastSettings =
    SETTINGS_MENU;

  static TimeTool lastTool =
    TOOL_COUNTDOWN;

  int x;
  int y;

  // ----------------------------------------------------------
  // TOUCH
  // ----------------------------------------------------------

  if (
    getTouch(
      x,
      y
    )
  ) {

    delay(120);

    // When the display is off, ANY touch wakes it and the touch is
    // deliberately consumed so it cannot activate a menu item.
    if (displayIsOff) {
      wakeDisplay();
      while (digitalRead(TOUCH_IRQ) == LOW) delay(10);
      delay(60);
      return;
    }

    // Every accepted touchscreen action gets a small retro computer click.
    // Suppress the click specifically while selecting Footstep Volume so
    // the quiet footstep sample can be heard on its own during testing.
    bool footstepVolumeTouch =
      (currentScreen == SCREEN_SETTINGS &&
       currentSettings == SETTINGS_SOUND &&
       y >= 143 && y < 176);

    if (!footstepVolumeTouch) {
      oracClick();
    }

    if (
      currentScreen ==
      SCREEN_MENU
    ) {

      handleMenuTouch(
        x,
        y
      );

    } else if (
      currentScreen ==
      SCREEN_TIME_TOOLS
    ) {

      handleTimeToolsTouch(
        x,
        y
      );

    } else if (
      currentScreen ==
      SCREEN_AI
    ) {

      handleAiTouch(x, y);

    } else if (
      currentScreen ==
      SCREEN_ORAC
    ) {

      handleOracTouch(
        x,
        y
      );

    } else if (
      currentScreen ==
      SCREEN_SNAKE
    ) {

      handleSnakeTouch(x, y);

    } else if (
      currentScreen ==
      SCREEN_TAMAGOTCHI
    ) {

      handleTamagotchiTouch(x, y);

    } else if (
      currentScreen ==
      SCREEN_CELLULAR
    ) {

      handleCellularTouch(x, y);

    } else if (
      currentScreen ==
      SCREEN_PK_METER
    ) {

      handlePKTouch(x, y);

    } else if (
      currentScreen ==
      SCREEN_PSYCHIC
    ) {

      handlePsychicTouch(x, y);

    } else if (
      currentScreen ==
      SCREEN_RANDOM
    ) {

      handleRandomTouch(x, y);

    } else if (
      currentScreen ==
      SCREEN_SETTINGS
    ) {

      handleSettingsTouch(
        x,
        y
      );

    } else if (
      currentScreen ==
      SCREEN_MANDELBROT
    ) {

      handleMandelbrotTouch(
        x,
        y
      );

    } else if (
      currentScreen ==
      SCREEN_CALCULATOR
    ) {

      handleCalculatorTouch(
        x,
        y
      );

    } else if (
      currentScreen ==
      SCREEN_CONVERTER
    ) {

      handleConverterTouch(
        x,
        y
      );

    } else {

      // Bottom area is HOME

      if (
        y >= 200
      ) {

        currentScreen =
          SCREEN_MENU;
      }
    }

    // Consume this touch completely before the newly selected screen
    // gets another chance to interpret the same finger press.
    while (
      digitalRead(TOUCH_IRQ) == LOW
    ) {
      delay(10);
    }

    delay(60);
  }

  // If the display is asleep, keep the rest of the application quiet.
  // The touch controller remains active and the next tap will wake it.
  if (displayIsOff) {
    delay(20);
    return;
  }

  // ----------------------------------------------------------
  // ORAC AI NETWORK
  // ----------------------------------------------------------

  ensureOracWifi();

  // Update the visible main-menu status strip immediately when Wi-Fi connects
  // during boot, rather than waiting for a screen change.
  if (currentScreen == SCREEN_MENU) drawMenuStatusStrip();

  // Refresh the Wi-Fi settings screen when its connection state changes.
  // This fixes the old behaviour where CONNECT/TRY could remain on screen
  // even after success or timeout. The screen is only redrawn on a state
  // change, so there is no continuous flicker or heap churn.
  if (currentScreen == SCREEN_SETTINGS && currentSettings == SETTINGS_WIFI && !wifiSettingsEditing && !wifiScanShowing) {
    static bool lastWifiConnected = false;
    static bool lastWifiAttemptActive = false;
    static int lastWifiAttemptIndex = -99;
    static int lastWifiIndex = -99;
    static bool lastWifiManualHold = false;
    static String lastWifiResult = "";
    if (lastWifiConnected != oracWifiConnected ||
        lastWifiAttemptActive != oracWifiAttemptActive ||
        lastWifiAttemptIndex != oracWifiAttemptIndex ||
        lastWifiIndex != oracWifiIndex ||
        lastWifiManualHold != oracWifiManualHold ||
        lastWifiResult != oracWifiResult) {
      lastWifiConnected = oracWifiConnected;
      lastWifiAttemptActive = oracWifiAttemptActive;
      lastWifiAttemptIndex = oracWifiAttemptIndex;
      lastWifiIndex = oracWifiIndex;
      lastWifiManualHold = oracWifiManualHold;
      lastWifiResult = oracWifiResult;
      drawWifiSettings();
    }
  }

  // ORAC COMPUTER
  // ----------------------------------------------------------

  updateOracComputer();

  // ----------------------------------------------------------
  // MAIN MENU STATUS LAMPS
  // ----------------------------------------------------------

  if (currentScreen == SCREEN_MENU) {
    drawMenuStatusLeds();
    drawMenuWifiIndicator();
    updateMenuBurt();
  }

  // ----------------------------------------------------------
  // SCREEN / SUBSCREEN CHANGED
  // ----------------------------------------------------------

  if (
    currentScreen !=
      lastScreen ||

    currentSettings !=
      lastSettings ||

    currentTool !=
      lastTool
  ) {

    releaseOracScreenGraphics(lastScreen);
    drawCurrentScreen();

    if (currentScreen == SCREEN_MENU) {
      resetMenuBurtTimer();
    } else {
      for (int i = 0; i < BURT_MENU_MAX; i++) {
        menuThronglets[i].active = false;
        menuThronglets[i].action = BURT_WALKING;
      }
    }

    lastScreen =
      currentScreen;

    lastSettings =
      currentSettings;

    lastTool =
      currentTool;
  }

  // ----------------------------------------------------------
  // FIBONACCI
  // ----------------------------------------------------------

  if (
    currentScreen ==
    SCREEN_FIBONACCI
  ) {

    drawFibonacciClock(
      false
    );
  }

  // ----------------------------------------------------------
  // FLIP CLOCK
  // ----------------------------------------------------------

  else if (
    currentScreen ==
    SCREEN_FLIP
  ) {
    drawFlipClock(false);
  }

  // ----------------------------------------------------------
  // COUNTDOWN
  // ----------------------------------------------------------

  else if (
    currentScreen ==
      SCREEN_TIME_TOOLS &&
    currentTool ==
      TOOL_COUNTDOWN
  ) {

    drawCountdown(
      false
    );
  }

  // ----------------------------------------------------------
  // STOPWATCH
  // ----------------------------------------------------------

  else if (
    currentScreen ==
      SCREEN_TIME_TOOLS &&
    currentTool ==
      TOOL_STOPWATCH
  ) {

    drawStopwatch(
      false
    );
  }

  // ----------------------------------------------------------
  // SNAKE
  // ----------------------------------------------------------

  else if (currentScreen == SCREEN_SNAKE) {
    updateSnakeGame();
  }

  // ----------------------------------------------------------
  // TAMAGOTCHI
  // ----------------------------------------------------------

  else if (currentScreen == SCREEN_TAMAGOTCHI) {
    updateTamagotchi();
  }

  // CELLULAR AUTOMATA
  else if (currentScreen == SCREEN_CELLULAR) {
    if (cellularRunning && millis() - cellularLastStep >= cellularSpeedIntervals[cellularSpeed]) {
      cellularLastStep=millis();
      cellularStep();
      drawCellularGrid(false);
      drawCellularStats();
    }
  }

  // PK METER
  else if (currentScreen == SCREEN_PK_METER) {
    updatePKMeter();
  }

  // PSYCHIC RESEARCH
  else if (currentScreen == SCREEN_PSYCHIC) {
    updatePsychicResearch();
  }

  // FRACTAL LAB / GENERATOR
  else if (
    currentScreen == SCREEN_MANDELBROT
  ) {
    if(fractalRendering) {
      stepFractalRender();
    } else if(currentFractal == FRACTAL_GENERATOR) {
      if(generatorHolding) {
        if(millis() - generatorHoldStart >= GENERATOR_HOLD_MS) {
          generatorHolding=false;
          generatorFading=true;
          generatorFadeStart=millis();
          generatorFadeStep=0;
          updateFractalLabInfo(true);
        }
      } else if(generatorFading) {
        stepGeneratorFade();
        if(!generatorFading) {
          fractalNewGeneratorParameters();
          startFractalRender();
        }
      }
    }
  }

  delay(5);
}