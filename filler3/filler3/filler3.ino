#include "Arduino.h"

#define OUT_FILL1_PIN 6
#define OUT_FILL2_PIN 5
#define OUT_FILL3_PIN 4
#define OUT_DONE_PIN  3

#define IN_START_PIN  8
#define IN_UP_PIN     7
#define IN_DOWN_PIN   9

#include <SoftwareSerial.h>
SoftwareSerial swSerial(11, 10); // RX, TX

#include <EEPROM.h>

#include <Wire.h>
#include "LiquidCrystal_I2C.h"
#define LCD_I2CADDR 0x27 //0x3F
const byte LCD_ROWS = 4; //2;
const byte LCD_COLS = 20; //16;
LiquidCrystal_I2C lcd(LCD_I2CADDR, 2, 1, 0, 4, 5, 6, 7, 3, POSITIVE);

struct Program {
	long params[3];
	char name[12];
};
#define PROGRAMS_NUM	42
Program program;

long weight;
long weights[4];

#define STATE_READY	0
#define STATE_FILL1	1
#define STATE_FILL2	2
#define STATE_FILL3	3
#define STATE_DONE	4
#define STATE_EDIT	10

int programNo;
int state = STATE_READY;
unsigned long stateMillis;
unsigned long buttonMillis;
bool noButton;

bool commStart;
int updatePos = 0;
bool lcdUpdate;

#define ENTER '\n'

int printConfirm(String s) {
	while(1) {
		Serial.print(s);
		Serial.print(F(" Y/N?:"));
		while(!Serial.available());
		String ss = Serial.readStringUntil(ENTER);
		Serial.println(ss);
		if(ss.equalsIgnoreCase("Y")) {
			return 1;
		}
		if(ss.equalsIgnoreCase("N")) {
			return 0;
		}
	}
	return 0;
}

int getInt(long *pi) {
	while(!Serial.available());
	String s = Serial.readStringUntil(ENTER);
	if(s.length()) {
		*pi = s.toInt();
		Serial.println(*pi);
		return 1;
	}
	Serial.println(*pi);
	return 0;
}

int getCharArray(char *p) {
	while(!Serial.available());
	String s = Serial.readStringUntil(ENTER);
	if(s.length()) {
		s.toCharArray(p, 12);
		p[11] = 0;
		Serial.println(p);
		return 1;
	}
	Serial.println(p);
	return 0;
}

Program editProgram(Stream &port, Program p) {
	port.print(F("NAME:"));
	getCharArray(p.name);
	port.print(F("SUBS1:"));
	getInt(&p.params[0]);
	port.print(F("SUBS2:"));
	getInt(&p.params[1]);
	port.print(F("SUBS3:"));
	getInt(&p.params[2]);
	return p;
}

void printHeader(Stream &port) {
	port.print(F("ID"));
	port.print('\t');
	port.print(F("NAME"));
	port.print('\t');
	port.print(F("SUBS1"));
	port.print('\t');
	port.print(F("SUBS2"));
	port.print('\t');
	port.println(F("SUBS3"));
}

void printInfo(Stream &port) {
	port.print(F("STATE:"));
	port.println(state);
	port.print(F("WEIGHT:"));
	port.println(weight);
	port.print(F("TARA:"));
	port.println(weights[0]);
	port.print(F("SUBS1:"));
	port.println(weights[1]);
	port.print(F("SUBS2:"));
	port.println(weights[2]);
	port.print(F("SUBS3:"));
	port.println(weights[3]);
}

char buf[8];
char* formatWeight(char* buf, long i) {
	sprintf(buf, "%6ld", i);
	return buf;
}

void lcdPrintProgram(Program &p, int n = -1) {
	lcd.clear();
	lcd.setCursor(0, 0);
	if(n < 10)
		lcd.print(' ');
	lcd.print(n);
	lcd.setCursor(2, 0);
	lcd.print(F(":"));
	//lcd.print(F("NAME:"));
	lcd.print(p.name);

	lcd.setCursor(14, 0);
	lcd.print(formatWeight(buf, weight));

	lcd.setCursor(0, 1);
	lcd.print(F("SUBS1: SUBS2: SUBS3:"));
	lcd.setCursor(0, 3);
	for(int i = 0; i < 3; i++) {
		lcd.print(formatWeight(buf, program.params[i]));
		if(i < 2)
			lcd.print(' ');
	}
	/*
	lcd.setCursor(0, 1);
	lcd.print(F("S1:"));
	lcd.print(formatWeight(buf, p.params[0]));
	lcd.setCursor(0, 2);
	lcd.print(F("S2:"));
	lcd.print(formatWeight(buf, p.params[1]));
	lcd.setCursor(0, 3);
	lcd.print(F("S2:"));
	lcd.print(formatWeight(buf, p.params[2]));
	*/
}

//void lcdPrint8spaces() {
//	lcd.print(F("        "));
//}

void lcdPrintWeight() {
	lcd.setCursor(14, 0);
	lcd.print(formatWeight(buf, weight));
}

void lcdPrintWeights() {
	lcdPrintWeight();

	lcd.setCursor(0, 2);
	long w;
	if(state == STATE_FILL1)
		w = (program.params[0] + weights[0]) - weight;
	else
		w = weights[1];
	lcd.print(formatWeight(buf, w));
	lcd.print(' ');

	if(state == STATE_FILL2)
		w = (program.params[1] + weights[0] + weights[1]) - weight;
	else
		w = weights[2];
	lcd.print(formatWeight(buf, w));
	lcd.print(' ');

	if(state == STATE_FILL3)
		w = (program.params[2] + weights[0] + weights[1] + weights[2]) - weight;
	else
		w = weights[3];
	lcd.print(formatWeight(buf, w));

	lcd.setCursor(0, 1);
	if(state == STATE_READY)
		lcd.print(F(">READY:PRESS START  "));
	if(state == STATE_FILL1)
		lcd.print(F("FILL1:              "));
	if(state == STATE_FILL2)
		lcd.print(F("       FILL2:       "));
	if(state == STATE_FILL3)
		lcd.print(F("              FILL3:"));
	if(state == STATE_DONE)
		lcd.print(F(">DONE:REMOVE BARREL "));
}

void printProgram(Stream &port, Program &p, int n = -1) {
	port.print(n);
	port.print('\t');
	p.name[15] = 0;
	port.print(p.name);
	port.print('\t');
	port.print(p.params[0]);
	port.print('\t');
	port.print(p.params[1]);
	port.print('\t');
	port.println(p.params[2]);
}

void printHelp(Stream &port) {
	port.println(F("---------------------"));
	port.println(F("P:PRESET PROGRAM"));
	port.println(F("D:DISPLAY PROGRAMS"));
	port.println(F("R:REMOVE PROGRAMS"));
}

void setup() {
	Serial.begin(9600);
	Serial.println(F("FILLER3"));
	Serial.println(F("GROWMAT.CZ"));

	swSerial.begin(9600);

	digitalWrite(OUT_FILL1_PIN, true);
	digitalWrite(OUT_FILL2_PIN, true);
	digitalWrite(OUT_FILL3_PIN, true);
	digitalWrite(OUT_DONE_PIN, true);
	pinMode(OUT_FILL1_PIN, OUTPUT);
	pinMode(OUT_FILL2_PIN, OUTPUT);
	pinMode(OUT_FILL3_PIN, OUTPUT);
	pinMode(OUT_DONE_PIN, OUTPUT);
	pinMode(IN_START_PIN, INPUT_PULLUP);
	pinMode(IN_UP_PIN, INPUT_PULLUP);
	pinMode(IN_DOWN_PIN, INPUT_PULLUP);

	EEPROM.get(sizeof(Program) * PROGRAMS_NUM, programNo);
	Serial.print(F("PROGRAM:"));
	Serial.println(programNo);

	printHeader(Serial);
	for(int i = 0; i < PROGRAMS_NUM; i++) {
		EEPROM.get(sizeof(Program) * i, program);
		printProgram(Serial, program, i);
	}
	EEPROM.get(sizeof(Program) * programNo, program);

	Wire.begin( );
	lcd.begin(LCD_COLS, LCD_ROWS);
	lcd.clear();
	lcd.print(F("FILLER3   GROWMAT.CZ"));
	delay(1000);
	lcd.clear();

	lcd.noAutoscroll();
	lcdPrintProgram(program, programNo);
	lcdPrintWeights();

	printHelp(Serial);

	Serial.println(F("READY"));
}

void loop() {
	if(swSerial.available()) {
	//	weight = swSerial.parseFloat();
		while(swSerial.available()) {
			Serial.write(swSerial.read());
		}
	}

	if(Serial.available()) {
		String s = Serial.readStringUntil(ENTER);
		//Serial.println(s);

		if(state == STATE_READY) {
			if(s.length()) {
				if(s.equalsIgnoreCase("D")) {
					Serial.print(F("PROGRAM:"));
					Serial.println(programNo);
					printHeader(Serial);
					for(int i = 0; i < PROGRAMS_NUM; i++) {
						EEPROM.get(sizeof(Program) * i, program);
						printProgram(Serial, program, i);
					}
					s = "";
				}
				if(s.equalsIgnoreCase("E")) {
					Serial.println(F("ID?:"));
					long n;
					getInt(&n);
					EEPROM.get(sizeof(Program) * n, program);
					printHeader(Serial);
					printProgram(Serial, program, n);
					//if(printConfirm(F("EDIT"))) {;
						program = editProgram(Serial, program);
						printHeader(Serial);
						printProgram(Serial, program, n);
						if(printConfirm(F("SAVE"))) {
							EEPROM.put(sizeof(Program) * n, program);
							//printProgram(Serial, program, n);
						}
					//}
					s = "";
				}
				if(s.equalsIgnoreCase("P")) {
					Serial.println(F("ID?:"));
					long n;
					getInt(&n);
					programNo = n;
					printHeader(Serial);
					EEPROM.get(sizeof(Program) * programNo, program);
					printProgram(Serial, program, programNo);
					EEPROM.update(sizeof(Program) * PROGRAMS_NUM, programNo);
					s = "";
				}

				if(s.equalsIgnoreCase("S")) {
					commStart = true;
					Serial.println(F("WEIGHT?:"));
					getInt(&weight);
					s = "";
				}

				if(s.equalsIgnoreCase("R")) {
					if(printConfirm(F("REMOVE ALL"))) {
						for(int i = 0; i < 12; i++)
							program.name[i] = 0;
						for(int i =0; i < 3; i++)
							program.params[i] = 0;
						printHeader(Serial);
						for(int i = 0; i < PROGRAMS_NUM; i++) {
							EEPROM.put(sizeof(Program) * i, program);
							printProgram(Serial, program, i);
						}
					}
				}
			}

			//while(Serial.available()) {
			//	Serial.read();
			//}
		}

		if(s.equalsIgnoreCase("?")) {
			printInfo(Serial);
			s = "";
		}

		if(s.length()) {
			//weight = Serial.parseFloat();
			weight = s.toFloat();
			Serial.print(F("WEIGHT:"));
			Serial.println(weight);
			while(Serial.available()) {
				Serial.read();
			}
			if(state != STATE_READY && state != STATE_EDIT)
				lcdPrintWeights();
			else
				lcdPrintWeight();
		}
	}

	if(digitalRead(IN_UP_PIN) && digitalRead(IN_DOWN_PIN)) {
		noButton = true;
	}

	if(state == STATE_EDIT) {
		if(updatePos < 3)
			lcd.setCursor(updatePos * 7 + 5, 3);
		else
			lcd.setCursor(updatePos, 0);
		lcd.cursor();

		lcdUpdate = false;
		if(millis() - buttonMillis > 500) {
			if(!digitalRead(IN_UP_PIN)) {
				if(noButton) {
					buttonMillis = millis();
					noButton = false;
				}
				if(updatePos < 3)
					program.params[updatePos]++;
				if(updatePos >= 3) {
					program.name[updatePos - 3]++;
					program.name[updatePos - 3] = min(program.name[updatePos - 3], '~');
				}
				lcdUpdate = true;
			}
			if(!digitalRead(IN_DOWN_PIN)) {
				if(noButton) {
					buttonMillis = millis();
					noButton = false;
				}
				if(updatePos < 3)
					program.params[updatePos]--;
					program.name[updatePos - 3] = min(program.name[updatePos - 3], '~');
				if(updatePos >= 3) {
					program.name[updatePos - 3]--;
					program.name[updatePos - 3] = max(program.name[updatePos - 3], ' ');
				}
				lcdUpdate = true;
			}
			if(!digitalRead(IN_START_PIN)) {
				buttonMillis = millis();
				program.name[11] = 0;
				updatePos++;
				if(updatePos >= 3 && updatePos < 11) {
					program.name[updatePos - 3] = max(program.name[updatePos - 3], ' ');
					program.name[updatePos - 3] = min(program.name[updatePos - 3], '~');
				}
				if(updatePos == 3 + 11) {
					//save
					lcd.noCursor();
					updatePos = 0;
					state = STATE_READY;
					EEPROM.put(sizeof(program) * programNo, program);
					lcdPrintProgram(program, programNo);
					lcdPrintWeights();
				}
			}
		}

		if(lcdUpdate) {
			if(updatePos < 3) {
				//lcd.setCursor(updatePos * 7, 2);
				//lcd.print(F("______"));
				lcd.setCursor(updatePos * 7, 3);
				lcd.print(formatWeight(buf, program.params[updatePos]));
			}
			else {
				lcd.setCursor(3, 0);
				lcd.print(program.name);
			}
		}

	}

	if(state == STATE_READY) {
		if(!digitalRead(IN_UP_PIN) && !digitalRead(IN_DOWN_PIN)) {
			if(millis() - buttonMillis > 500) {
				buttonMillis = millis();
				state = STATE_EDIT;

				lcd.setCursor(0, 2);
				lcd.print(F(">EDIT PROGRAM       "));
				lcdUpdate = true;
			}
		}
	}

	if(state == STATE_READY) {
		if(millis() - buttonMillis > 500) {
			if(!digitalRead(IN_UP_PIN)) {
				buttonMillis = millis();
				programNo++;
				if(programNo >= PROGRAMS_NUM)
					programNo = 0;
				lcd.setCursor(0, 18);
				EEPROM.get(sizeof(Program) * programNo, program);
				lcdPrintProgram(program, programNo);
			}
			if(!digitalRead(IN_DOWN_PIN)) {
				buttonMillis = millis();
				programNo--;
				if(programNo < 0)
					programNo = PROGRAMS_NUM - 1;
				EEPROM.get(sizeof(Program) * programNo, program);
				lcdPrintProgram(program, programNo);
			}
		}


		weights[0] = weight;
		if(!digitalRead(IN_START_PIN) || commStart) {
			if(millis() - buttonMillis > 500) {
				buttonMillis = millis();

				commStart = false;
				EEPROM.update(sizeof(Program) * PROGRAMS_NUM, programNo);
				EEPROM.get(sizeof(Program) * programNo, program);
				for(int i = 0; i < 3; i++) {
					weights[i + 1] = program.params[i];
				}

				printHeader(Serial);
				printProgram(Serial, program, programNo);

				state = STATE_FILL1;
				stateMillis = millis();
				Serial.print(F("TARA:"));
				Serial.println(weights[0]);

				lcdPrintProgram(program, programNo);
			}
		}

		//TODO:
		//if(!digitalRead(IN_UP_PIN) && !digitalRead(IN_DOWN_PIN)) {
		//	buttonMillis = millis();
		//	state = STATE_EDIT;
		//	lcdUpdate = true;
		//}
	}
	else {
		if(state != STATE_EDIT) {
			if(!digitalRead(IN_UP_PIN) && !digitalRead(IN_DOWN_PIN)) {
				buttonMillis = millis();
				state = STATE_READY;
				stateMillis = millis();
				Serial.println(F("STOP"));

				lcdPrintProgram(program, programNo);
				lcdPrintWeights();
			}
		}
	}

	if(state == STATE_FILL1) {
		if(weight - weights[0] >= program.params[0]) {
			state = STATE_FILL2;
			stateMillis = millis();
			weights[1] = weight - weights[0];
			Serial.print(F("SUBS1:"));
			Serial.println(weights[1]);
		}
	}
	if(state == STATE_FILL2) {
		if(weight - weights[1] - weights[0] >= program.params[1]) {
			state = STATE_FILL3;
			stateMillis = millis();
			weights[2] = weight - weights[1] - weights[0];
			Serial.print(F("SUSB2:"));
			Serial.println(weights[2]);
		}
	}
	if(state == STATE_FILL3) {
		if(weight - weights[2] - weights[1] - weights[0] >= program.params[2]) {
			state = STATE_DONE;
			stateMillis = millis();
			weights[3] = weight - weights[2] - weights[1] - weights[0];
			Serial.print(F("SUSBS3:"));
			Serial.println(weights[3]);
		}
	}

	if(state == STATE_DONE) {
		if(weight <= weights[0]) {
			state = STATE_READY;
			stateMillis = millis();
			Serial.print(F("TARA:"));
			Serial.println(weight);
			Serial.println(F("READY"));

			lcdPrintProgram(program, programNo);
			lcdPrintWeights();
		}
	}

	if(state != STATE_READY && state != STATE_EDIT) {
		lcdPrintWeights();
	}

	digitalWrite(OUT_FILL1_PIN, state != STATE_FILL1);
	digitalWrite(OUT_FILL2_PIN, state != STATE_FILL2);
	digitalWrite(OUT_FILL3_PIN, state != STATE_FILL3);
	digitalWrite(OUT_DONE_PIN, state != STATE_DONE);
}
