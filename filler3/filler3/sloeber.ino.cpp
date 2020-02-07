#ifdef __IN_ECLIPSE__
//This is a automatic generated file
//Please do not modify this file
//If you touch this file your change will be overwritten during the next build
//This file has been generated on 2020-02-07 18:06:17

#include "Arduino.h"
#include "Arduino.h"
#define OUT_FILL1_PIN 3
#define OUT_FILL2_PIN 4
#define OUT_FILL3_PIN 5
#define OUT_DONE_PIN  6
#define IN_START_PIN  8
#define IN_UP_PIN     7
#define IN_DOWN_PIN   9
#include <SoftwareSerial.h>
extern SoftwareSerial swSerial;
#include <EEPROM.h>
#include <Wire.h>
#include "LiquidCrystal_I2C.h"

int printConfirm(String s) ;
int getInt(long *pi) ;
int getCharArray(char *p) ;

void printHeader(Stream &port) ;
void printInfo(Stream &port) ;
char* formatWeight(char* buf, long i) ;
void lcdPrintWeight() ;
void lcdPrintWeights() ;
void printHelp(Stream &port) ;
void setup() ;
void loop() ;

#include "filler3.ino"


#endif
