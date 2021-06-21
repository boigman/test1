#include <Arduino.h>

int red_led[4];
int green_led[4];
int switches[4];
int switch_val;
int last_switch_val[4];
int HB_led = 19;
const int curMonPin = 25;
int curMonVal;
unsigned long prevMillis = 0;
unsigned long currMillis;
unsigned long pumpStartMillis;
unsigned long nextPumpCheck;
unsigned interval = 300;
int prevPumpState = 0;
int currPumpState;
int pumpOnLevel = 250;  //min AnalogRead level is 250 for Pump On
int pumpOffLevel = 75;  //max AnalogRead level is 75 for Pump Off

unsigned long getNextPumpCheck(unsigned long pStart, unsigned long pNextCheck) {
	unsigned long interval;
	unsigned long hourInterval = 60 * 60 * 1000;
	unsigned long tenMinInterval = 10 * 60 * 1000;
	unsigned long twoMinInterval = 2 * 60 * 1000;
	interval = millis() - pStart;
	if (interval > hourInterval) return pNextCheck + hourInterval;
	if (interval > tenMinInterval) return pNextCheck + 50 * 60 * 1000;
	if (interval > twoMinInterval) return pNextCheck + 8 * 60 * 1000;
	return pStart + 2 * 60 * 1000;
}	
String convertMillis(unsigned long pMillis) {
  char timeString[20];
  unsigned long currentMillis = pMillis;
  int seconds = currentMillis / 1000;
  int minutes = seconds / 60;
  int hours = minutes / 60;
//  unsigned long days = hours / 24;
  currentMillis %= 1000;
  seconds %= 60;
  minutes %= 60;
  hours %= 24; 
  sprintf_P(timeString, PSTR("%d:%02d:%02d"), hours, minutes, seconds);
  return timeString;   
}

void setup() {
    pinMode (15, OUTPUT);
    pinMode (2, OUTPUT);
    pinMode (0, OUTPUT);
    pinMode (4, OUTPUT);
    pinMode (16, OUTPUT);
    pinMode (17, OUTPUT);
    pinMode (5, OUTPUT);
    pinMode (18, OUTPUT);
    pinMode (HB_led, OUTPUT);
    pinMode(35, INPUT_PULLDOWN);
    pinMode(34, INPUT_PULLDOWN);
    pinMode(39, INPUT_PULLDOWN);
    pinMode(36, INPUT_PULLDOWN);
    green_led[0] = 15;
    red_led[0] = 2;
    green_led[1] = 0;
    red_led[1] = 4;
    green_led[2] = 16;
    red_led[2] = 17;
    green_led[3] = 5;
    red_led[3] = 18;
    switches[0] = 35;
    switches[1] = 34;
    switches[2] = 39;
    switches[3] = 36;
    //digitalWrite(red_led[0], HIGH);
    //delay(2500);
    //digitalWrite(red_led[0], LOW);
    Serial.begin(115200);
    Serial.println("Sump Pump Monitor");
    for(int i = 0;i<4;i++) {
        switch_val = digitalRead(switches[i]);
        Serial.print("Switch[");
        Serial.print(i);
        Serial.print("] is ");
        Serial.println(switch_val?"HIGH":"LOW");
        last_switch_val[i] = LOW;

    }

/**/
    for(int i = 0;i<4;i++) {
        digitalWrite(green_led[i], HIGH);
        delay(250);
        digitalWrite(green_led[i], LOW);
        delay(250);

    }
    for(int i = 0;i<4;i++) {
        digitalWrite(red_led[i], HIGH);
        delay(250);
        digitalWrite(red_led[i], LOW);
        delay(250);

    }

    for(int i = 0;i<4;i++) {
        digitalWrite(HB_led, HIGH);
        delay(250);
        digitalWrite(HB_led, LOW);
        delay(250);

    }

    for(int i = 0;i<4;i++) {
        digitalWrite(red_led[i], LOW);
        digitalWrite(green_led[i], LOW);
    }
    digitalWrite(HB_led, LOW);

	curMonVal = analogRead(curMonPin);
    currPumpState = (curMonVal > pumpOnLevel?1:0);
//	currPumpState = (curMonVal > pumpOnLevel?1:0);
	Serial.print("Sump Pump is ");
	Serial.print(currPumpState?"ON":"OFF");
    Serial.print(" (" + curMonVal);
	Serial.println(")");
	prevPumpState = currPumpState;
	if(currPumpState) {
		digitalWrite(HB_led, HIGH);
		pumpStartMillis = millis();
		nextPumpCheck = getNextPumpCheck(pumpStartMillis, pumpStartMillis);
	} else {
		digitalWrite(HB_led, LOW);
	}
    
}

void loop() {


    for(int i = 0;i<4;i++) {
        switch_val = digitalRead(switches[i]);
        if(!switch_val) {
        digitalWrite(green_led[i], LOW);
        delay(20);
        digitalWrite(red_led[i], HIGH);
        } else {
        digitalWrite(red_led[i], LOW);
        delay(20);
        digitalWrite(green_led[i], HIGH);
        }
        if(switch_val!=last_switch_val[i]) {
            Serial.print("Switch[");
            Serial.print(i);
            Serial.print("] is ");
            Serial.println(switch_val?"HIGH":"LOW");
        }
        last_switch_val[i] = switch_val;

    }
 curMonVal = analogRead(curMonPin);
 if(curMonVal > pumpOnLevel) currPumpState = 1;
 if(curMonVal < pumpOffLevel) currPumpState = 0;
// currPumpState = (curMonVal > pumpOnLevel?1:0);
 if(currPumpState != prevPumpState) {
     prevPumpState = currPumpState;
     Serial.print("Sump Pump is ");
     Serial.print(currPumpState?"ON":"OFF");
    Serial.print(" (");
    Serial.print(curMonVal);
    Serial.println(")");
    if(currPumpState) {
        digitalWrite(HB_led, HIGH);
		pumpStartMillis = millis();
		nextPumpCheck = getNextPumpCheck(pumpStartMillis, pumpStartMillis);
    } else {
		digitalWrite(HB_led, LOW);
		currMillis = millis();
		Serial.print("Sump Pump ran for ");
		Serial.print(convertMillis(currMillis - pumpStartMillis));
		Serial.println(" (h:m:s).");
    }
 } else {
	currMillis = millis();
	if(currPumpState && currMillis > nextPumpCheck) { 
      Serial.print("*** WARNING *** Sump Pump has been running for ");
      Serial.print((currMillis - pumpStartMillis) / (60 * 1000));
      Serial.println(" minutes.");
	  nextPumpCheck = getNextPumpCheck(pumpStartMillis, nextPumpCheck);
	}
 }
 
/*
 currMillis = millis();
 if(currMillis - prevMillis >= interval) {
     prevMillis = currMillis;
     curMonVal = analogRead(curMonPin);
     Serial.print("Current Mon Value: ");
     Serial.println(curMonVal);
 }
*/    
//        digitalWrite(HB_led, HIGH);
//        delay(125);
//        digitalWrite(HB_led, LOW);
        delay(20);


/**/
}