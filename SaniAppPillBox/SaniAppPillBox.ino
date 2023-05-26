/*
  SD card read/write

   SD card attached to SPI bus as follows:
 ** MOSI  - pin 11
 ** MISO  - pin 12
 ** CLK   - pin 13
 ** CS    - pin 4 (for MKRZero SD: SDCARD_SS_PIN)

*/




#include "Firebase_Arduino_WiFiNINA.h"
#include <SPI.h>
#include <SD.h>
#include <RTCZero.h>

#define DATABASE_URL "---" //<databaseName>.firebaseio.com or <databaseName>.<region>.firebasedatabase.app
#define DATABASE_SECRET "---"

// Firebase
FirebaseData fbdo;
FirebaseData stream;
unsigned long sendDataPrevMillis = 0;
String path = "/Residences/";
int count = 0;

// SD
File myFile;
String WIFI_SSID;
String WIFI_PASSWORD;
String ARDUINOID;
String DATA;

// Time
RTCZero rtc;
int TIME_HOUR;
int TIME_MINUTE;
int TIME_SECONDS;
int TIME_DAY;
int TIME_MONTH;
int TIME_YEAR;
int TIME_WEEK_DAY;
String TIME_WEEK_DAY_STRING;
String TIME_DAYPART_STRING;
int keyIndex = 0;                           // your network key Index number (needed only for WEP)
int status = WL_IDLE_STATUS;
const int GMT = 1; //change this to adapt it to your time zone

// Buzzer
const int buzzerPin = 7;
bool buzzerAllow = true;

// Button
const int buttonPin = 5;
int lastStatePin = LOW;  // the previous state from the input pin
int currentStatePin;                // the current reading from the input pin

// General
int medication = 0; // If 0, it does not have to be taken, else 1.

// --------- FUNCTIONS ---------

// Split string.
// Parameters:
// - data: main string.
// - separator: delimeter.
// - index: substring from string.
String getValue(String data, char separator, int index);

// Make the buzzer beep.
// Parameters:
// - note: note that will be played.
// - duration: duration in ms.
void beep(int note, int duration);

// Test all LEDs
void testleds();

// Return day of the week.
// Parameters:
// - day: day of the year.
// - month: day of the year.
// - year: current year.
// Output:
// - 0: Sunday
// - 1: Monday
// - 2: Tuesday
// - 3: Wednesday
// - 4: Thursday
// - 5: Friday
// - 6: Saturday
int dayOfWeek(int day, int month, int year);

// Put every Taken field to No.
void resetWeek();

// Record an error and the date in errors.txt with
// Parameters:
// - description: description of the error.
void errorLog(String description);

// Make all LEDs blink and the buzzer sound if it is enabled.
void errorBlinkBuzzer();

// Get some data from the SD card:
// Get flags from flags.txt and set values.
//  - sound: turns on the buzzer (true) or turns it off (false).
// SSID: WiFi SSID from Wifi.txt.
// PASSWORD: Wifi password from Wifi.txt.
// ARDUINOID: Pillbox id.
// DATA: Residence id.
// Flags: get flags from flags.txt and set values.
void getSDdata();

void setup() {
  // Open serial communications and wait for port to open:
  Serial.begin(9600);
  
  // Set pins
  pinMode(buzzerPin, OUTPUT); // Set buzzer - pin 7 as an output
  pinMode(buttonPin, INPUT_PULLUP); // the pull-up input pin will be HIGH when the switch is open and LOW when the switch is closed.

  // Set LEDs
  pinMode(A0, OUTPUT); // Monday
  pinMode(A1, OUTPUT); // Tuesday
  pinMode(A2, OUTPUT); // Wednesday
  pinMode(A3, OUTPUT); // Thursday
  pinMode(A4, OUTPUT); // Friday
  pinMode(A5, OUTPUT); // Saturday
  pinMode(A6, OUTPUT); // Sunday

  testleds();

  getSDdata();

  // ------------- WiFi -------------

  // check if the WiFi module works
  if (WiFi.status() == WL_NO_SHIELD) {
    String errorWiFiShield = "WiFi shield not present";
    errorLog(errorWiFiShield);
    errorBlinkBuzzer();
    Serial.println("WiFi shield not present");
    // don't continue:
    while (true);
  }
  // attempt to connect to WiFi network:
  while ( status != WL_CONNECTED) {
    Serial.print("Attempting to connect to SSID: ");
    Serial.println(WIFI_SSID);
    // Connect to WPA/WPA2 network. Change this line if using open or WEP network:
    status = WiFi.begin(WIFI_SSID.c_str(), WIFI_PASSWORD.c_str());
    // wait 10 seconds for connection:
    delay(10000);
    if(status != WL_CONNECTED){
      String errorWiFiConnection = "WiFi not connected to SSID";
      errorLog(errorWiFiConnection);
      errorBlinkBuzzer();
      Serial.println(errorWiFiConnection);
    }
  }

  Serial.println("Connected!");
  
  // ------------- RTC -------------
  // you're connected now, so print out the status:
  rtc.begin();
  unsigned long epoch;
  int numberOfTries = 0, maxTries = 6;
  do {
    epoch = WiFi.getTime();
    numberOfTries++;
  }
  while ((epoch == 0) && (numberOfTries < maxTries));
  if (numberOfTries == maxTries) {
    errorLog("NTP unreachable!!");
    errorBlinkBuzzer();
    Serial.print("NTP unreachable!!");
    while (1);
  }
  else {
    Serial.print("Epoch received: ");
    Serial.println(epoch);
    rtc.setEpoch(epoch + GMT  * 60 *60);
    Serial.println();
  }

  // ------------- Firebase -------------
  //Provide the autentication data
  Firebase.begin(DATABASE_URL, DATABASE_SECRET, WIFI_SSID, WIFI_PASSWORD);
  Firebase.reconnectWiFi(true);

  if (!Firebase.beginStream(stream, path))
  {
    String errorFirebase = "Can't connect stream, " + stream.errorReason();
    errorLog(errorFirebase);
    errorBlinkBuzzer();
    Serial.println(errorFirebase);
  }
  
}

void loop() {
  
  TIME_HOUR = rtc.getHours();
  TIME_MINUTE = rtc.getMinutes();
  TIME_SECONDS = rtc.getSeconds();

  TIME_DAY = rtc.getDay();
  TIME_MONTH = rtc.getMonth();
  TIME_YEAR = rtc.getYear();

  TIME_WEEK_DAY = dayOfWeek(TIME_DAY, TIME_MONTH, TIME_YEAR);

  // Night: 00:00 - 6:59
  // Morning: 7:00 - 11:59
  // Afternoon: 12:00 - 17:59
  // Evening: 18:00 - 23:59

  if(TIME_HOUR >= 0 && TIME_HOUR <= 6){
    TIME_DAYPART_STRING = "Night";
  } else if(TIME_HOUR >= 7 && TIME_HOUR <= 11) {
    TIME_DAYPART_STRING = "Morning";
  } else if(TIME_HOUR >= 12 && TIME_HOUR <= 17) {
    TIME_DAYPART_STRING = "Afternoon";
  } else if(TIME_HOUR >= 18 && TIME_HOUR <= 23) {
    TIME_DAYPART_STRING = "Evening";
  }

  switch(TIME_WEEK_DAY){
    case 0:
      TIME_WEEK_DAY_STRING = "Sunday";
    break;

    case 1:
      TIME_WEEK_DAY_STRING = "Monday";
    break;

    case 2:
      TIME_WEEK_DAY_STRING = "Tuesday";
    break;

    case 3:
      TIME_WEEK_DAY_STRING = "Wednesday";
    break;

    case 4:
      TIME_WEEK_DAY_STRING = "Thursday";
    break;

    case 5:
      TIME_WEEK_DAY_STRING = "Friday";
    break;

    case 6:
      TIME_WEEK_DAY_STRING = "Saturday";
    break;
  }

  if(TIME_WEEK_DAY_STRING == "Monday" && TIME_HOUR == 0 && TIME_MINUTE == 0){
    resetWeek();
  }

  // Check if pill has to be taken
  if (millis() - sendDataPrevMillis > 700 || sendDataPrevMillis == 0){
    sendDataPrevMillis = millis();
    count++;

    currentStatePin = digitalRead(buttonPin);

    if(medication == 1 && lastStatePin == HIGH && currentStatePin == LOW){
      switch(TIME_WEEK_DAY){
        case 0:
          digitalWrite(A6, LOW);
        break;

        case 1:
          digitalWrite(A0, LOW);
        break;
        
        case 2:
          digitalWrite(A1, LOW);
        break;

        case 3:
          digitalWrite(A2, LOW);
        break;

        case 4:
          digitalWrite(A3, LOW);
        break;

        case 5:
          digitalWrite(A4, LOW);
        break;

        case 6:
          digitalWrite(A5, LOW);
        break;
      }
      if (Firebase.setString(fbdo, path + ARDUINOID + "/Medication/" + TIME_WEEK_DAY_STRING + "/" + TIME_DAYPART_STRING + "/Taken/", "Yes"))
      {
        Serial.println("Taken");
      }
      else
      {
        String errorFirebaseTaken = "Error, "+ fbdo.errorReason();
        errorLog(errorFirebaseTaken);
        errorBlinkBuzzer();
        Serial.println(errorFirebaseTaken);
      }
      delay(60000);
    }
    else
    {
      if(lastStatePin == LOW && currentStatePin == HIGH){
        Serial.println("The button is released");
      }
    }

    lastStatePin = currentStatePin;
  }

  printDate();

  // Check the hour of the pill that has to be taken at the specific part of the day
  // Serial.println(path + ARDUINOID + "/Medication/" + TIME_WEEK_DAY_STRING + "/" + TIME_DAYPART_STRING + "/Hour");
  // Serial.print("Get data... ");
  if (Firebase.getString(fbdo, path + ARDUINOID + "/Medication/" + TIME_WEEK_DAY_STRING + "/" + TIME_DAYPART_STRING + "/Hour")){
    Serial.print("Pill hour (HH:MM): ");
    Serial.print(fbdo.stringData());
    Serial.print("   ");
    Serial.println(TIME_DAYPART_STRING);

    String timestr = fbdo.stringData();
    String hourstr = getValue(timestr,':',0);
    String minutestr = getValue(timestr,':',1);
    int hourint = hourstr.toInt();
    int minuteint = minutestr.toInt();

    String medicinename;
    if(Firebase.getString(fbdo, path + ARDUINOID + "/Medication/" + TIME_WEEK_DAY_STRING + "/" + TIME_DAYPART_STRING + "/Medication")){
      medicinename = fbdo.stringData();
    }

    if(hourint == TIME_HOUR && minuteint == TIME_MINUTE && !medicinename.equals("---")){
      Serial.println("Need to take medicine\n");
      beep(880, 100);
      medication = 1;

      switch(TIME_WEEK_DAY){
        case 0:
          digitalWrite(A6, HIGH);   // turn the LED on (HIGH is the voltage level)
        break;

        case 1:
          digitalWrite(A0, HIGH);   // turn the LED on (HIGH is the voltage level)
        break;
        
        case 2:
          digitalWrite(A1, HIGH);   // turn the LED on (HIGH is the voltage level)
        break;

        case 3:
          digitalWrite(A2, HIGH);   // turn the LED on (HIGH is the voltage level)
        break;

        case 4:
          digitalWrite(A3, HIGH);   // turn the LED on (HIGH is the voltage level)
        break;

        case 5:
          digitalWrite(A4, HIGH);   // turn the LED on (HIGH is the voltage level)
        break;

        case 6:
          digitalWrite(A5, HIGH);   // turn the LED on (HIGH is the voltage level)
        break;
      }
    }
    else{
      Serial.println("No need to take medicine\n");
      digitalWrite(A0, LOW);    // turn the LED off by making the voltage LOW
      digitalWrite(A1, LOW);
      digitalWrite(A2, LOW);
      digitalWrite(A3, LOW);
      digitalWrite(A4, LOW);
      digitalWrite(A5, LOW);
      digitalWrite(A6, LOW);
      medication = 0;
    }
  }
  else{
    String errorFirebaseCheckHour = "Error, "+ fbdo.errorReason();
    errorLog(errorFirebaseCheckHour);
    errorBlinkBuzzer();
    Serial.println("Error, "+ fbdo.errorReason());
  }
  delay(400);
}

// --------- FUNCTIONS ---------

String print2digits(int number) {
  String numberstr = String(number);
  if (number < 10) {
    numberstr = "0" + numberstr;
  }
  return numberstr;
}

void printDate(){
  Serial.print("Current date (DD/MM/YY) (HH:MM:SS): ");
  Serial.print(print2digits(TIME_DAY));
  Serial.print("/");
  Serial.print(print2digits(TIME_MONTH));
  Serial.print("/");
  Serial.print(TIME_YEAR);
  Serial.print(" ");

  Serial.print(print2digits(TIME_HOUR));
  Serial.print(":");
  Serial.print(print2digits(TIME_MINUTE));
  Serial.print(":");
  Serial.print(print2digits(TIME_SECONDS));
  Serial.print(" ");

  switch(TIME_WEEK_DAY){
    case 0:
      Serial.println("Sunday");
    break;

    case 1:
      Serial.println("Monday");
    break;

    case 2:
      Serial.println("Tuesday");
    break;

    case 3:
      Serial.println("Wednesday");
    break;

    case 4:
      Serial.println("Thursday");
    break;

    case 5:
      Serial.println("Friday");
    break;

    case 6:
      Serial.println("Saturday");
    break;
  }

}

String getValue(String data, char separator, int index)
{
  int found = 0;
  int strIndex[] = {0, -1};
  int maxIndex = data.length()-1;

  for(int i=0; i<=maxIndex && found<=index; i++){
    if(data.charAt(i)==separator || i==maxIndex){
        found++;
        strIndex[0] = strIndex[1]+1;
        strIndex[1] = (i == maxIndex) ? i+1 : i;
    }
  }

  return found>index ? data.substring(strIndex[0], strIndex[1]) : "";
}

void beep(int note, int duration)
{
  if(buzzerAllow){
    tone(buzzerPin, note);
    delay(duration);
    noTone(buzzerPin);
  }
}

void testleds(){
  digitalWrite(A0, HIGH);   // turn the LED on (HIGH is the voltage level)
  delay(3000);
  digitalWrite(A0, LOW);    // turn the LED off by making the voltage LOW
  delay(300);

  digitalWrite(A1, HIGH);   // turn the LED on (HIGH is the voltage level)
  delay(3000);
  digitalWrite(A1, LOW);    // turn the LED off by making the voltage LOW
  delay(300);

  digitalWrite(A2, HIGH);   // turn the LED on (HIGH is the voltage level)
  delay(3000);
  digitalWrite(A2, LOW);    // turn the LED off by making the voltage LOW
  delay(300);

  digitalWrite(A3, HIGH);   // turn the LED on (HIGH is the voltage level)
  delay(3000);
  digitalWrite(A3, LOW);    // turn the LED off by making the voltage LOW
  delay(300);

  digitalWrite(A4, HIGH);   // turn the LED on (HIGH is the voltage level)
  delay(3000);
  digitalWrite(A4, LOW);    // turn the LED off by making the voltage LOW
  delay(300);

  digitalWrite(A5, HIGH);   // turn the LED on (HIGH is the voltage level)
  delay(3000);                       // wait for a second
  digitalWrite(A5, LOW);    // turn the LED off by making the voltage LOW
  delay(300);

  digitalWrite(A6, HIGH);   // turn the LED on (HIGH is the voltage level)
  delay(3000);
  digitalWrite(A6, LOW);    // turn the LED off by making the voltage LOW
  delay(300);
}

int dayOfWeek(int day, int month, int year){
    static const int offset[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};

    year -= month < 3;
    return (year + year / 4 - year / 100 + year / 400 + offset[month - 1] + day) % 7;
}

void resetWeek(){
  char weekdays[][10] = {"Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday", "Sunday"};
  char dayparts[][10] = {"Morning", "Afternoon", "Evening", "Night"};

  int sizeweekdays = sizeof(weekdays)/sizeof(weekdays[0]);
  int sizedayparts = sizeof(dayparts)/sizeof(dayparts[0]);
  bool error = false;
  Serial.print("Ressetting week... ");
  for(int i=0; i<sizeweekdays; i++){
    for(int j=0; j<sizedayparts; j++){
      do{
        if (Firebase.setString(fbdo, path + ARDUINOID + "/Medication/" + weekdays[i] + "/" + dayparts[j] + "/Taken/", "No")){
          error = false;
        }
        else{
          error = true;
        }
      }
      while(error);
    }
  }
  Serial.println("Done!\n");
}

void errorLog(String description){
  myFile = SD.open("errors.txt", FILE_WRITE);

  if (myFile) {
    Serial.print("Writing to errors.txt... ");
    myFile.print(print2digits(TIME_DAY));
    myFile.print("/");
    myFile.print(print2digits(TIME_MONTH));
    myFile.print("/");
    myFile.print(print2digits(TIME_YEAR));
    myFile.print("  ");
    myFile.print(print2digits(TIME_HOUR));
    myFile.print(":");
    myFile.print(print2digits(TIME_MINUTE));
    myFile.print(":");
    myFile.print(print2digits(TIME_SECONDS));
    myFile.print("->");
    myFile.println(description);

    myFile.close();
    Serial.print("Done!\n");
  } else {
    Serial.println("Error opening errors.txt");
  }
}

void errorBlinkBuzzer(){
  for(int i=0; i<5; i++){
    digitalWrite(A0, HIGH);
    digitalWrite(A1, HIGH);
    digitalWrite(A2, HIGH);
    digitalWrite(A3, HIGH);
    digitalWrite(A4, HIGH);
    digitalWrite(A5, HIGH);
    digitalWrite(A6, HIGH);
    beep(1080, 50);
    beep(1080, 50);
    digitalWrite(A0, LOW);
    digitalWrite(A1, LOW);
    digitalWrite(A2, LOW);
    digitalWrite(A3, LOW);
    digitalWrite(A4, LOW);
    digitalWrite(A5, LOW);
    digitalWrite(A6, LOW);
    beep(1080, 50);
    beep(1080, 50);
  }
}

void getSDdata(){
  Serial.print("Initializing SD card... ");

  if (!SD.begin(4)) {
    String errorSDInit = "SD initialization failed!";
    errorLog(errorSDInit);
    errorBlinkBuzzer();
    Serial.println(errorSDInit);
    while (1);
  }
  Serial.print("Done!\n");

  myFile = SD.open("flags.txt");
  if (myFile) {
    Serial.print("Reading flags.txt... ");
    String line = myFile.readStringUntil('\n');
    String flag = getValue(line,'=',0);
    String value = getValue(line,'=',1);
    if(flag == "sound"){
      if(value == "false"){
        buzzerAllow = false;
      }
    }
    // close the file:
    myFile.close();
  } else {
    // if the file didn't open, print an error:
    String errorSDFlags = "Error opening flags.txt";
    errorLog(errorSDFlags);
    errorBlinkBuzzer();
    Serial.println(errorSDFlags);
  }
  Serial.print("Done!\n");

  // Get Wi-Fi parameters
  myFile = SD.open("WiFi.txt");
  if (myFile) {
    Serial.print("Reading WiFi.txt... ");
    WIFI_SSID = myFile.readStringUntil('\n');
    int lastIndex = WIFI_SSID.length() - 1;
    WIFI_SSID.remove(lastIndex);
    WIFI_PASSWORD  = myFile.readStringUntil('\n');
    // close the file:
    myFile.close();
  } else {
    // if the file didn't open, print an error:
    String errorSDWifi = "Error opening WiFi.txt";
    errorLog(errorSDWifi);
    errorBlinkBuzzer();
    Serial.println(errorSDWifi);
  }
  Serial.print("Done!\n");

  // Get ArduinoID
  myFile = SD.open("id.txt");
  if (myFile) {
    Serial.print("Reading id.txt... ");
    ARDUINOID  = myFile.readStringUntil('\n');
    // close the file:
    myFile.close();
  } else {
    // if the file didn't open, print an error:
    String errorSDID = "Error opening id.txt";
    errorLog(errorSDID);
    errorBlinkBuzzer();
    Serial.println(errorSDID);
  }
  Serial.print("Done!\n");

  // Get data

  myFile = SD.open("data.txt");
  if (myFile) {
    Serial.print("Reading data.txt... ");
    DATA  = myFile.readStringUntil('\n');
    // close the file:
    myFile.close();
  } else {
    // if the file didn't open, print an error:
    String errorSDData = "Error opening data.txt";
    errorLog(errorSDData);
    errorBlinkBuzzer();
    Serial.println(errorSDData);
  }
  Serial.print("Done!\n");

  Serial.println("Data:");
  Serial.println("  SSID: " + WIFI_SSID);
  Serial.println("  PASSWORD: " + WIFI_PASSWORD);
  Serial.println("  ARDUINOID: " + ARDUINOID);
  Serial.println("  DATA: " + DATA);
  path = path + DATA + "/Residents/";
}