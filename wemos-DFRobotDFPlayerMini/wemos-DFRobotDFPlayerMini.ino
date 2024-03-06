// Bibliothèque à inclure au minimum
#include "SoftwareSerial.h"    // pour les communications series avec le DFplayer
#include "DFRobotDFPlayerMini.h"  // bibliotheque pour le DFPlayer

// PIN qui serviront pour la communication série sur le WEMOS
SoftwareSerial mySoftwareSerial(14, 12); // Rx, Tx ( wemos D5,D6 ou 14,12 GPIO )  ou Tx,RX ( Dfplayer )
DFRobotDFPlayerMini myDFPlayer;  // init player

void setup() {
  mySoftwareSerial.begin(9600);
  Serial.begin(115200);
  
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

  myDFPlayer.volume(10);  //Set volume value. From 0 to 30
  Serial.println(myDFPlayer.readFileCounts());
  Serial.println(myDFPlayer.readCurrentFileNumber());

  Serial.println(myDFPlayer.readState());

  // myDFPlayer.play(1);  //Play the first mp3
  // delay(10000);
  // myDFPlayer.pause();
}

void loop() {
}