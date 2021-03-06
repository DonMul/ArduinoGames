#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <SPI.h>
#include <MFRC522.h>

// PIN DEFINES
#define RGB_RED 0
#define RGB_GREEN 2
#define SS_PIN  4
#define RST_PIN  5
#define MISO 12
#define MOSI 13
#define SCK 14
#define RGB_BLUE 15

// GAME DEFINES
#define GAME_UNSET 0
#define GAME_FOXHUNT 1
#define GAME_LIVING_STRATEGO 2

// ROLE DEFINES
#define ROLE_FOXHUNT_HUNTER 0
#define ROLE_FOXHUNT_FOX 1

#define ROLE_LS_BOMB 0
#define ROLE_LS_SPY 1
#define ROLE_LS_SCOUT 2
#define ROLE_LS_MINER 3
#define ROLE_LS_SERGEANT 4
#define ROLE_LS_LIEUTENANT 5
#define ROLE_LS_CAPTAIN 6
#define ROLE_LS_MAJOR 7
#define ROLE_LS_COLONEL 8
#define ROLE_LS_GENERAL 9
#define ROLE_LS_MARSHALL 10

// GAME AND ROLE VARIABLES
int game = GAME_UNSET;
int role = 0;
int team = 0;

// WIFI SETTINGS
const char *foxHuntSSID = "FHAP";
const char *foxHuntPassword = "Fh!1";
int connections = 0;

WiFiEventHandler stationConnectedHandler;
WiFiEventHandler stationDisconnectedHandler;

// RFID SETTINGS
MFRC522 mfrc522(SS_PIN, RST_PIN);

/**
 * Setup leds and initialises the MFRC522 module
 */
void setup() {
  Serial.begin(9600);
  pinMode(RGB_RED, OUTPUT);
  pinMode(RGB_GREEN, OUTPUT);
  pinMode(RGB_BLUE, OUTPUT);
  
  delay(250);
  SPI.begin();
  mfrc522.PCD_Init();
}

/**
 * Main Game loop
 */
void loop() {
  if (game == GAME_UNSET) {
    initGame();
    return;
  }

  // If the game is foxhunt
  if (game == GAME_FOXHUNT) {
    // When this node is set as HUNTER
    if (ROLE_FOXHUNT_HUNTER) {
      // Set leds to red if no Foxes are nearby and set leds to green when foxes are around
      if (WiFi.status() != WL_CONNECTED) {
        setLeds(1,0,0);
      } else if (WiFi.status() == WL_CONNECTED) {
        setLeds(0,1,0);
      }

      delay(500);
    }
  } else if (game == GAME_LIVING_STRATEGO) {
    if (!mfrc522.PICC_IsNewCardPresent()) {
      delay(50);
      return;
    }
    
    if (!mfrc522.PICC_ReadCardSerial()) {
      delay(50);
      return;
    }

    int opponentGame = mfrc522.uid.uidByte[0];
    int opponentRole = mfrc522.uid.uidByte[1];
    int opponentTeam = mfrc522.uid.uidByte[2];

    if (opponentGame != game) {
      return;
    }

    if (opponentTeam == team) {
      teamLedBlink(5);
      return;
    }

    bool gameOver = false;
    if (opponentRole == ROLE_LS_BOMB && role != ROLE_LS_MINER) {
      gameOver = true;
    } else if (opponentRole == ROLE_LS_MARSHALL && role != ROLE_LS_SPY) {
      gameOver = true;
    } else if (opponentRole > role) {
      gameOver = true;
    }

    if (gameOver == true) {
      setLeds(0,0,0);
      game = 0;
      return;
    } else {
      teamLedBlink(5);
    }
  }
}

/**
 * Set the RGB led channels
 */
void setLeds(int redStatus, int greenStatus, int blueStatus)
{
  digitalWrite(RGB_RED, redStatus);
  digitalWrite(RGB_GREEN, greenStatus);
  digitalWrite(RGB_BLUE, blueStatus);
}

/**
 * Checks wether or not an RFID tag is present and reads the UID
 */
void initGame() {
  if (!mfrc522.PICC_IsNewCardPresent()) {
    delay(50);
    return;
  }
  
  if (!mfrc522.PICC_ReadCardSerial()) {
    delay(50);
    return;
  }
  
  int gameByte = mfrc522.uid.uidByte[0];
  if (gameByte == GAME_FOXHUNT) {
    game = GAME_FOXHUNT;
    role = mfrc522.uid.uidByte[1];
    
    if (role != ROLE_FOXHUNT_HUNTER && role != ROLE_FOXHUNT_FOX) {
      role = ROLE_FOXHUNT_HUNTER;
    }

    initFoxHunt();
  } else if (gameByte == GAME_LIVING_STRATEGO) {
    game = GAME_LIVING_STRATEGO;
    role = mfrc522.uid.uidByte[1];
    team = mfrc522.uid.uidByte[2];

    initLS();
  }
}

/**
 * Initialises the FoxHunt game
 */
void initFoxHunt() {
  // If the users role is a FOX
  if (role == ROLE_FOXHUNT_FOX) {
    // Enable the WiFi AP
    WiFi.mode(WIFI_AP);
    WiFi.softAP(foxHuntSSID, foxHuntPassword);

    // Clear leds to "Safe" (Green)
    setLeds(0,1,0);
    delay(500);

    // Set wifi connection listeners
    stationConnectedHandler = WiFi.onSoftAPModeStationConnected(&onWifiConnect);
    stationDisconnectedHandler = WiFi.onSoftAPModeStationDisconnected(&onWifiDisconnect);
  }
  // If the role is a HUNTER
  else if (role == ROLE_FOXHUNT_HUNTER) {
    WiFi.mode(WIFI_STA);
    // Start trying to connect to a fox's WiFi netowrk
    WiFi.begin(foxHuntSSID, foxHuntPassword);
  }
  
  delay(500);
}

/**
 * Initializes Living Stratego
 */
void initLS() {
  initTeamLeds();
}

/**
 * Ensures the correct team collors
 */
void initTeamLeds()
{
  if (team == 0) {
    setLeds(1,0,0);
  } else if (team == 1) {
    setLeds(0,1,0);
  } else if (team == 2) {
    setLeds(0,0,1);
  } else if (team == 4) {
    setLeds(1,1,0);
  }
}

/**
 * Blinks an given amount of times
 */
void teamLedBlink(int amount)
{
  for (int i = 0; i < amount; i++) {
    setLeds(0,0,0);
    delay(100);
    initTeamLeds();
    delay(100);
  }
}

/**
 * Increase WiFi connection counter and switch leds to red if the amount of connections are larger than 0
 */
void onWifiConnect(const WiFiEventSoftAPModeStationConnected& evt) {
  // Increase WiFi connections
  connections++;

  // Set leds to Unsafe (Red) when there are HUNTERs connected
  if (connections > 0) {
    setLeds(1,0,0);
  }
}

/**
 * Descrease wifi connection counter and switch leds to green if no connections are available
 */
void onWifiDisconnect(const WiFiEventSoftAPModeStationDisconnected& evt) {
  // Decrease WiFi connections
  connections--;

  // Set leds to Safe (Green) when there are not HUNTERs connected
  if (connections < 0) {
    setLeds(0,1,0);
  }
}

