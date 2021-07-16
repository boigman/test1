#include <Arduino.h>
#include <WiFi.h>
#include <time.h>

#define DEBUG
const char* ssid       = "BoigieNetx";
const char* password   = "F1r3Engin3Sauc3";

const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = -6 * 60 * 60;
const int   daylightOffset_sec = 3600;
const int EVENT_LIMIT = 5;
int event_count;
int array_count;
int curr_level;
int prev_level;
int pump_prev_level;

char timeVar[128];
char timeStringBuff[72]; //72 chars should be enough

String levels[20] = {"LOW","ELEVATED","HIGH","CRITICAL","EMERGENCY"};

struct event_rec
{
  int event_type; //0=Water_level, 1=Pump_status
  char timeStringBuff[72];
  char description[100];
  int pre_event_level;
  int post_event_level;
};

event_rec events[EVENT_LIMIT];

void getLocalTime(boolean doPrint)
{
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){
    Serial.println("Failed to obtain time");
    return;
  }
//  Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
  strftime(timeStringBuff, sizeof(timeStringBuff), "%x %H:%M:%S", &timeinfo);
  //print like "const char*"
  if(doPrint) {
    Serial.print(timeStringBuff);
  }
}

String SendHTML(int pCurrPumpState,int pCurrLevel){
  String ptr = "<!DOCTYPE html> <html>\n";
  ptr +="<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0, user-scalable=no\">\n";
  ptr +="<title>LED Control</title>\n";
  ptr +="<style>html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}\n";
  ptr +="body{margin-top: 50px;} h1 {color: #444444;margin: 50px auto 30px;} h3 {color: #444444;margin-bottom: 50px;}\n";
  ptr +=".button {display: block;width: 80px;background-color: #3498db;border: none;color: white;padding: 13px 30px;text-decoration: none;font-size: 25px;margin: 0px auto 35px;cursor: pointer;border-radius: 4px;}\n";
  ptr +=".button-on {background-color: #3498db;}\n";
  ptr +=".button-on:active {background-color: #2980b9;}\n";
  ptr +=".button-off {background-color: #34495e;}\n";
  ptr +=".button-off:active {background-color: #2c3e50;}\n";
  ptr +="p {font-size: 14px;color: #888;margin-bottom: 10px;}\n";
  ptr +="</style>\n";
  ptr +="</head>\n";
  ptr +="<body>\n";
  ptr +="<h1>ESP32 Web Server</h1>\n";
    ptr +="<h3>Using Station(STA) Mode</h3>\n";
  
  if(pCurrPumpState)
    {ptr +="<p>Sump Pump is: ON</p>\n";}
  else
    {ptr +="<p>Sump Pump is: OFF</p>\n";}
  ptr +="<p>Water Level is: " + levels[pCurrLevel] + "</p>\n";}
  return ptr;
}

void showTime(tm localTime) {
  Serial.print(localTime.tm_mday);
  Serial.print('/');
  Serial.print(localTime.tm_mon + 1);
  Serial.print('/');
  Serial.print(localTime.tm_year - 100);
  Serial.print('-');
  Serial.print(localTime.tm_hour);
  Serial.print(':');
  Serial.print(localTime.tm_min);
  Serial.print(':');
  Serial.print(localTime.tm_sec);
  Serial.print(" Day of Week ");
  if (localTime.tm_wday == 0)   Serial.println(7);
  else Serial.println(localTime.tm_wday);
}

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
unsigned long printStartMillis;
unsigned long nextPrintMillis;

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

int getWaterLevel(boolean doPrint) {
  for(int i = 3;i>-1;i--) {
    switch_val = digitalRead(switches[i]);
    if(!switch_val) {
      if(doPrint) {
        Serial.print("Water level is: ");
        Serial.print(levels[i+1]);
      }
      return i+1;
    }

  }
  if(doPrint) {
    Serial.print("Water level is: ");
    Serial.print(levels[0]);
  }
  return 0;
}

void printHistory() {
  for(int ii=event_count; ii > max(event_count - EVENT_LIMIT, -1);ii--) {
    array_count = ii % EVENT_LIMIT;
    Serial.println((String) events[array_count].timeStringBuff + ": " + events[array_count].description);
  }
}

void addEvent(int pEventType) {
  event_count++;
  array_count = event_count % EVENT_LIMIT;
//  Serial.print("event_count = ");
//  Serial.println(event_count);
//  Serial.print("array_count = ");
//  Serial.println(array_count);
  events[array_count].event_type = pEventType;

  getLocalTime(false);
  for(int ii=0;ii<sizeof(timeStringBuff);ii++) {
	events[array_count].timeStringBuff[ii] = timeStringBuff[ii];
  }
  events[array_count].post_event_level = curr_level;

  if(pEventType) {	// Pump event
      events[array_count].pre_event_level = pump_prev_level;
      String description = "Sump Pump ran for " + convertMillis(currMillis - pumpStartMillis) + 
		" (Water Level " + levels[events[array_count].pre_event_level] + 
		" to " + levels[events[array_count].post_event_level] + ")";
      description.toCharArray(events[array_count].description, sizeof(events[array_count].description));
      Serial.println((String) events[array_count].timeStringBuff + ": " + events[array_count].description);
  } else {	// Water level event
	  events[array_count].pre_event_level = prev_level;
	  String description = "Water level is: " + levels[events[array_count].post_event_level] + " (from " + levels[events[array_count].pre_event_level] + ")";
	  description.toCharArray(events[array_count].description, sizeof(events[array_count].description));
	  Serial.println((String) events[array_count].timeStringBuff + ": " + events[array_count].description);
  }
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
  
  //connect to WiFi
  Serial.printf("Connecting to %s ", ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.print(".");
  }
  Serial.println(" CONNECTED");
  Serial.print("Got IP: ");  Serial.println(WiFi.localIP());
  
  //init and get the time
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  getLocalTime(true);
  Serial.print("\n");

  //disconnect WiFi as it's no longer needed
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);  

    Serial.println("Sump Pump Monitor");
    for(int i = 0;i<4;i++) {
        switch_val = digitalRead(switches[i]);
//        Serial.print("Switch[");
//        Serial.print(i);
//        Serial.print("] is ");
//        Serial.println(switch_val?"HIGH":"LOW");
        last_switch_val[i] = switch_val;

    }

/**/
    for(int i = 0;i<4;i++) {
        digitalWrite(green_led[i], LOW);
        delay(125);

    }
    for(int i = 0;i<4;i++) {
        digitalWrite(red_led[i], HIGH);
        delay(125);
        digitalWrite(red_led[i], LOW);
        delay(125);

    }

    for(int i = 0;i<4;i++) {
        digitalWrite(HB_led, HIGH);
        delay(125);
        digitalWrite(HB_led, LOW);
        delay(125);

    }

    for(int i = 0;i<4;i++) {
        digitalWrite(red_led[i], LOW);
        digitalWrite(green_led[i], LOW);
    }
    digitalWrite(HB_led, LOW);

    curMonVal = analogRead(curMonPin);
    currPumpState = (curMonVal > pumpOnLevel?1:0);

#ifdef DEBUG	
	Serial.print("Sump Pump is ");
	Serial.print(currPumpState?"ON":"OFF");
    	Serial.print(" (" + curMonVal);
	Serial.println(")");
#endif	
	prevPumpState = currPumpState;
	if(currPumpState) {
		digitalWrite(HB_led, HIGH);
		pumpStartMillis = millis();
		nextPumpCheck = getNextPumpCheck(pumpStartMillis, pumpStartMillis);
	} else {
		digitalWrite(HB_led, LOW);
	}

  getLocalTime(false);
  curr_level = getWaterLevel(false);
  prev_level = curr_level;
  events[0].event_type = 0;
  for(int ii=0;ii<sizeof(timeStringBuff);ii++) {
    events[0].timeStringBuff[ii] = timeStringBuff[ii];
  }
  String description = "Initial Water level is: " + levels[curr_level] + " | Sump Pump is: " + (currPumpState?"ON":"OFF") ;
  description.toCharArray(events[0].description, sizeof(events[0].description));
  events[0].pre_event_level = prev_level;
  events[0].post_event_level = curr_level;
#ifdef DEBUG	
  Serial.println((String) events[0].timeStringBuff + ": " + events[0].description);
#endif	
  printStartMillis = millis();
  nextPrintMillis = printStartMillis + 3 * 60 * 1000;

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
            getLocalTime(false);
            prev_level = curr_level;
            curr_level = getWaterLevel(false);
            if(!currPumpState) {
			  addEvent(0);
            }
        }
        last_switch_val[i] = switch_val;

    }
 curMonVal = analogRead(curMonPin);
 if(curMonVal > pumpOnLevel) currPumpState = 1;
 if(curMonVal < pumpOffLevel) currPumpState = 0;
// currPumpState = (curMonVal > pumpOnLevel?1:0);
 if(currPumpState != prevPumpState) {
     prevPumpState = currPumpState;
     getLocalTime(true);
#ifdef DEBUG		 
    Serial.print(": Sump Pump is ");
    Serial.print(currPumpState?"ON":"OFF");
    Serial.print(" (");
    Serial.print(curMonVal);
    Serial.println(")");
#endif	
    if(currPumpState) {
      pump_prev_level = curr_level;
      digitalWrite(HB_led, HIGH);
      pumpStartMillis = millis();
      nextPumpCheck = getNextPumpCheck(pumpStartMillis, pumpStartMillis);
    } else {
      digitalWrite(HB_led, LOW);
      curr_level = getWaterLevel(false);
      currMillis = millis();      
	  addEvent(1);
    }
 } else {
	currMillis = millis();
	if(currPumpState && currMillis > nextPumpCheck) { 
      getLocalTime(true);
#ifdef DEBUG		 
      Serial.print(": *** WARNING *** Sump Pump has been running for ");
      Serial.print((currMillis - pumpStartMillis) / (60 * 1000));
      Serial.println(" minutes.");
#endif	
	nextPumpCheck = getNextPumpCheck(pumpStartMillis, nextPumpCheck);
  }
  if(currMillis >  nextPrintMillis) {
#ifdef DEBUG
    getLocalTime(true);		 
    Serial.println(": Printing History...");
    printHistory();
#endif	
    nextPrintMillis = currMillis + 3 * 60 * 1000;
  }
 }
 server.handleClient();


}
