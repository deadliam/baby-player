// #include <Adafruit_NeoPixel.h>
#include <GyverPortal.h>
#include <LittleFS.h>
#include <TimeLib.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>
#include "NTPClient.h"
#include "WiFiUdp.h"
#include <EEPROM.h>
#include "SoftwareSerial.h"    // pour les communications series avec le DFplayer
#include "DFRobotDFPlayerMini.h"  // bibliotheque pour le DFPlayer

SoftwareSerial mySoftwareSerial(14, 12); // Rx, Tx ( wemos D5,D6 ou 14,12 GPIO )  ou Tx,RX ( Dfplayer )
DFRobotDFPlayerMini myDFPlayer;  // init player

unsigned long previousMillis = 0UL;
unsigned long timeInterval = 1000UL;
time_t currentEpochTime;
time_t operationEpochTime = 0;

String serverDnsName = "babyplayer";

#define MAX_LOG_LINES 200
String logs[MAX_LOG_LINES] = {};

GyverPortal ui(&LittleFS);

int volume = 10;
int minVal = 0;
int maxVal = 30;

// void SPINNER(const String& name, float value = 0, float min = NAN, float max = NAN, float step = 1, uint16_t dec = 0, PGM_P st = GP_GREEN, const String& w = "", bool dis = 0) {
GP_SPINNER sp1("sp1", volume, minVal, maxVal, 1, 0, GP_BLUE, "", 0);
GP_AREA ar("ar", MAX_LOG_LINES, "", "");
GPtime valTime;

// #define NUM_LEDS 38
// #define DATA_PIN D4
// #define BRIGHTNESS 50
// #define LED_TYPE NEO_GRB // Adafruit_NeoPixel uses a different naming convention
// #define FRAMES_PER_SECOND 120
// Adafruit_NeoPixel strip = Adafruit_NeoPixel(NUM_LEDS, DATA_PIN, LED_TYPE);

const long utcOffsetInSeconds = 19800;
// Define NTP Client to get time
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", utcOffsetInSeconds);

#define EEPROM_SIZE 512
int eepromTarget;
int eepromVolumeAddress = 0;

bool isPlaying = false;
bool isLooped = true;
int currentFileNumber = 1;

// enum folders {JAM, ROBOT, CARTOONS, SLEEP};

void build() {
  GP.BUILD_BEGIN();

  GP.THEME(GP_DARK);
  //GP.THEME(GP_LIGHT);
  GP.UPDATE("txt1,txt2,sp1");
  GP.TITLE("HEDGEHOG PLAYER");
  GP.HR();

  GP.NAV_TABS("Action,List", GP_BLUE_B);

  GP.NAV_BLOCK_BEGIN();
  // M_NAV_BLOCK(
  M_BLOCK_THIN(
    M_BOX(
      GP.LABEL("Elapsed:");
      GP.TIME("time", valTime);
    );
  );
  M_BLOCK_THIN(
    M_BOX(
      GP.BUTTON_MINI("loop", "🔁", "", GP_GRAY, "100", 0, 0);
      GP.BUTTON_MINI("prev", "⏮️", "", GP_GRAY, "100", 0, 0); 
      GP.BUTTON_MINI("play", "⏯️", "", GP_GRAY, "100", 0, 0); 
      GP.BUTTON_MINI("next", "⏭️", "", GP_GRAY, "100", 0, 0); 
    );

    GP.BREAK();
    GP.BUTTON("jam", "JAM", "", GP_GRAY, "100", 0, 0); 
    GP.BUTTON("robot", "ROBOT", "", GP_GRAY, "100", 0, 0); 
    GP.BUTTON("sleep", "SLEEP", "", GP_GRAY, "100", 0, 0); 
    GP.BUTTON("cartoons", "CARTOONS", "", GP_GRAY, "100", 0, 0); 
  );

  M_BOX(
    GP.LABEL("Volume:");
    GP.SPINNER(sp1);
  );
  
  GP.NAV_BLOCK_END();

  GP.NAV_BLOCK_BEGIN();
    GP.AREA(ar); GP.BREAK();
  GP.NAV_BLOCK_END();

  GP.BUILD_END();
}

void setup() {
  mySoftwareSerial.begin(9600);
  Serial.begin(115200);

  // WiFiManager
  // Local intialization. Once its business is done, there is no need to keep it around
  WiFiManager wifiManager;
  // Uncomment and run it once, if you want to erase all the stored information
  // wifiManager.resetSettings();
  
  // set custom ip for portal
  //wifiManager.setAPConfig(IPAddress(10,0,1,1), IPAddress(10,0,1,1), IPAddress(255,255,255,0));
  wifiManager.setHostname(serverDnsName);

  // fetches ssid and pass from eeprom and tries to connect
  // if it does not connect it starts an access point with the specified name
  // here  "AutoConnectAP"
  // and goes into a blocking loop awaiting configuration
  wifiManager.autoConnect("AutoConnectAP");
  // or use this for auto generated name ESP + ChipID
  //wifiManager.autoConnect();
  
  // if you get here you have connected to the WiFi
  Serial.println("Connected.");

  // Set up mDNS responder:
  // - first argument is the domain name, in this example
  //   the fully-qualified domain name is "esp8266.local"
  // - second argument is the IP address to advertise
  //   we send our IP address on the WiFi network

  if (!MDNS.begin(serverDnsName)) {
    Serial.println("Error setting up MDNS responder!");
    while (1) {
      delay(1000);
    }
  }
  Serial.println("mDNS responder started");
  MDNS.addService("http", "tcp", 80);
  
  // ========================================================
  //Init EEPROM
  EEPROM.begin(EEPROM_SIZE);

  EEPROM.get(eepromVolumeAddress, eepromTarget);
  Serial.print("eepromTarget: ");
  Serial.println(eepromTarget);
  if (isnan(eepromTarget) || eepromTarget == 255) {
    eepromTarget = volume;
    Serial.print("Set eepromTarget to: ");
    Serial.println(eepromTarget);

    EEPROM.put(eepromVolumeAddress, eepromTarget);
    EEPROM.commit();
  } else {
    volume = eepromTarget;
    Serial.print("eepromTarget is: ");
    Serial.println(eepromTarget);
  }
  
  // ========================================================
  timeClient.begin();
  // Set offset time in seconds to adjust for your timezone, for example:
  // GMT +1 = 3600
  // GMT +8 = 28800
  // GMT -1 = -3600
  // GMT 0 = 0
  timeClient.setTimeOffset(7200);

  // ========================================================
  // strip.begin();
  // strip.setBrightness(BRIGHTNESS);
  // pixelsOff();
  // strip.show(); // Initialize all pixels to 'off'

  ui.attachBuild(build);
  ui.attach(action);
  ui.start();
  ui.log.start(30);
  ui.enableOTA();

  // ========================================================
  if (!LittleFS.begin()) Serial.println("FS Error");
  ui.downloadAuto(true);

  // ========================================================
  if (!myDFPlayer.begin(mySoftwareSerial)) {
    Serial.println(F("Unable to begin:"));
    Serial.println(F("1.Please recheck the connection!"));
    Serial.println(F("2.Please insert the SD card!"));
    while(true){
      delay(0); // Code to compatible with ESP8266 watch dog.
    }
  }
  Serial.println(F("DFPlayer Mini online."));
  myDFPlayer.setTimeOut(500);
  myDFPlayer.outputDevice(DFPLAYER_DEVICE_SD);

  myDFPlayer.volume(volume);  //Set volume value. From 0 to 30
  // myDFPlayer.playFolder(1, 1);
  playRobot();
  // myDFPlayer.start();
  // loopCurrent();
}

void action() {

  if (ui.update()) {
    fillLogArea();
    ui.update(ar);
  }

  if (ui.click()) {
    if (ui.click("jam")) {
      Serial.println("jam");
      playJam();
    }
    if (ui.click("robot")) {
      Serial.println("robot");
      playRobot();
    }
    if (ui.click("sleep")) {
      Serial.println("sleep");
      playSleep();
    }
    if (ui.click("cartoons")) {
      Serial.println("cartoons");
      playCartoons();
    }
    if (ui.click("play")) {
      Serial.println("Play");
      playAndPause();
    }
    if (ui.click("loop")) {
      Serial.println("Loop");
      loopCurrent();
    }
    if (ui.click("prev")) {
      Serial.println("Previous");
      previous();
    }
    if (ui.click("next")) {
      Serial.println("Next");
      next();
    }
    if (ui.click(sp1)) {
      Serial.print("Spinner value: ");
      Serial.println(sp1.value);
      volume = sp1.value;
      EEPROM.put(eepromVolumeAddress, volume);
      EEPROM.commit();
      myDFPlayer.volume(volume);
    }
  }
}

void loop() {

  ui.tick();

  ui.updateTime("time", valTime);
  ui.update("sp1");

  MDNS.update();
  timeClient.update();

  unsigned long currentMillis = millis();
    /* The Arduino executes this code once every second
    *  (timeInterval = 1000 (ms) = 1 second).
    */

  if (currentMillis - previousMillis > timeInterval) {
    currentEpochTime = timeClient.getEpochTime();
    currentFileNumber = myDFPlayer.readCurrentFileNumber();
    updateElapsedTime();
    previousMillis = currentMillis;
  }

  // if (myDFPlayer.available()) {
  //   printDetail(myDFPlayer.readType(), myDFPlayer.read()); 
  // }
}

void loopCurrent() {
  if (isLooped) {
    Serial.println("DISABLE LOOP");
    myDFPlayer.disableLoop();
    isLooped = false;
  } else {
    Serial.println("LOOP CURRENT");
    myDFPlayer.loop(currentFileNumber);
    isLooped = true;
  }
  delay(1000);
}

void playRobot() {
  // playFolder(01, 1, volume);
  myDFPlayer.loopFolder(1);
  delay(2000);
  myDFPlayer.playLargeFolder(1, 1);  //play specific mp3 in SD:/15/004.mp3; Folder Name(1~99); File Name(1~255)
  isPlaying = true;
  delay(1000);
}
void playJam() {
  // playFolder(02, 1, volume);
  myDFPlayer.loopFolder(2);
  delay(2000);
  myDFPlayer.playLargeFolder(2, 1);  //play specific mp3 in SD:/15/004.mp3; Folder Name(1~99); File Name(1~255)
  isPlaying = true;
  delay(1000);
}
void playSleep() {
  // playFolder(03, 1, volume);
  myDFPlayer.loopFolder(4);
  delay(2000);
  myDFPlayer.playLargeFolder(4, 1);  //play specific mp3 in SD:/15/004.mp3; Folder Name(1~99); File Name(1~255)
  isPlaying = true;
  delay(1000);
}
void playCartoons() {
  // playFolder(04, 1, volume);
  myDFPlayer.loopFolder(3);
  delay(2000);
  myDFPlayer.playLargeFolder(3, 1);  //play specific mp3 in SD:/15/004.mp3; Folder Name(1~99); File Name(1~255)
  isPlaying = true;
  delay(1000);
}

void playTrackX (byte x) {
  
  mySoftwareSerial.write((byte)0x7E);
  mySoftwareSerial.write((byte)0xFF);
  mySoftwareSerial.write((byte)0x06);
  mySoftwareSerial.write((byte)0x03);
  mySoftwareSerial.write((byte)0x00);
  mySoftwareSerial.write((byte)0x00);
  mySoftwareSerial.write((byte)x);
  mySoftwareSerial.write((byte)0xEF);
}

void playAndPause() {
  if (isPlaying) {
    Serial.println("PAUSE");
    myDFPlayer.pause();
    isPlaying = false;
  } else {
    Serial.println("PLAY");
    myDFPlayer.play(currentFileNumber); //currentFileNumber
    isPlaying = true;
  }
  delay(1000);
  
  Serial.print("readState: ");
  Serial.println(myDFPlayer.readState());
}
void previous() {
  Serial.println("PREVIOUS");
  myDFPlayer.previous();
  delay(1000);
  currentEpochTime = operationEpochTime;
}
void next() {
  Serial.println("NEXT");
  myDFPlayer.next();  //Play next mp3
  delay(1000);
  currentEpochTime = operationEpochTime;
}

void updateElapsedTime() {
  
  if (isPlaying) {
    operationEpochTime += timeInterval;
  }
  time_t epochTimeInterval = currentEpochTime - operationEpochTime;
  char format[] = "hh:mm:ss";
  tm tm;
  gmtime_r(&epochTimeInterval, &tm);
  valTime.set(tm.tm_hour, tm.tm_min, tm.tm_sec);
}

void fillLogArea() {
  String textAreaString = "";
  for (int i = 0; i < MAX_LOG_LINES; i++) {
    if (logs[i].length() == 0) {
      continue;
    }
    textAreaString += "#";
    textAreaString += String(i + 1);
    textAreaString += "\t";
    textAreaString += logs[i];
    textAreaString += "\n";
  }
  ar.text = textAreaString;
}

void printDetail(uint8_t type, int value){
  switch (type) {
    case TimeOut:
      Serial.println(F("Time Out!"));
      break;
    case WrongStack:
      Serial.println(F("Stack Wrong!"));
      break;
    case DFPlayerCardInserted:
      Serial.println(F("Card Inserted!"));
      break;
    case DFPlayerCardRemoved:
      Serial.println(F("Card Removed!"));
      break;
    case DFPlayerCardOnline:
      Serial.println(F("Card Online!"));
      break;
    case DFPlayerUSBInserted:
      Serial.println("USB Inserted!");
      break;
    case DFPlayerUSBRemoved:
      Serial.println("USB Removed!");
      break;
    case DFPlayerPlayFinished:
      Serial.print(F("Number:"));
      Serial.print(value);
      Serial.println(F(" Play Finished!"));
      break;
    case DFPlayerError:
      Serial.print(F("DFPlayerError:"));
      switch (value) {
        case Busy:
          Serial.println(F("Card not found"));
          break;
        case Sleeping:
          Serial.println(F("Sleeping"));
          break;
        case SerialWrongStack:
          Serial.println(F("Get Wrong Stack"));
          break;
        case CheckSumNotMatch:
          Serial.println(F("Check Sum Not Match"));
          break;
        case FileIndexOut:
          Serial.println(F("File Index Out of Bound"));
          break;
        case FileMismatch:
          Serial.println(F("Cannot Find File"));
          break;
        case Advertise:
          Serial.println(F("In Advertise"));
          break;
        default:
          break;
      }
      break;
    default:
      break;
  }
}