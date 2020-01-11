#include "A4988RotatorController.h"
#include "WiFi.h"
#include <SPI.h>
#include <Wire.h>
#include <MechaQMC5883.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

MechaQMC5883 qmc;

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 32 // OLED display height, in pixels

// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
#define OLED_RESET     4 // Reset pin # (or -1 if sharing Arduino reset pin)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
char ip_address[16];

int moveSpeed = 1;
int azPos;
int elPos;
String inputString;
bool stringComplete;

const char* ssid = "karen";
const char* password =  "12a12b12c12d1";

WiFiServer wifiServer(4533);

#define BufferSize 256

# define X_EN  18 // DONE 3 //ENA+(+5V) stepper motor enable , active low     Orange
# define X_DIR 14 // DONE 5 //DIR+(+5v) axis stepper motor direction control  Brown
# define X_STP 13 // DONE 7 //PUL+(+5v) axis stepper motor step control       RED
// microstep
# define X_MICRO_1 19 // DONE 
# define X_MICRO_2 5  // DONE 
# define X_MICRO_3 12 // DONE 

# define EL_EN  32 // DONE 3 //ENA+(+5V) stepper motor enable , active low     Orange
# define EL_DIR 25 // DONE 5 //DIR+(+5v) axis stepper motor direction control  Brown
# define EL_STP 27 // DONE 7 //PUL+(+5v) axis stepper motor step control       RED
// microstep
# define EL_MICRO_1 4 // DONE 
# define EL_MICRO_2 33 // DONE 
# define EL_MICRO_3 26 // DONE 

// Speed
# define TRACKING 600
# define INTERMEDIATE 400
# define FAST 300


A4988RotatorController xRot; // Rotato rController
A4988RotatorController elRot; // Rotato rController

void serialEvent()    // Read data from serial port
{
  while (Serial.available()) 
  {
    char inChar = (char)Serial.read();            // get the new byte:
    if (inChar > 0)     {inputString += inChar;}  // add it to the inputString:
    if (inChar == '\n') { stringComplete = true;} // if the incoming character is a newline, set a flag so the main loop can do something about it: 
  }
}

boolean isNumber(char *input)
{
  for (int i = 0; input[i] != '\0'; i++)
  {
    if (isalpha(input[i]))
      return false;
  }
   return true;
}

void unwind()
{
  xRot.unwind();
}

void slideUp(int distance)
{
  Serial.println("Up: ");
  elRot.slide(0-distance); 
}

void slideDown(int distance)
{
  Serial.println("Down: ");
  elRot.slide(distance); 
}

void slideRight(int distance)
{
  Serial.print("Slide right ");
  Serial.println(distance);
  xRot.on();
  xRot.slide(0-distance); 
}

void slideLeft(int distance)
{
  Serial.print("Slide left ");
  Serial.println(distance);
  xRot.on();
  xRot.slide(distance); 
}

// Find North
int home()
{
  int dir;
  int newHeading;
  
  delay(200);
  Serial.println("Find North...");
  display.clearDisplay();

  azPos = getHeading();
  xRot.slideTo(azPos);   // Leave the 'rotator' at 0 and move the antennas (or, move but don't update the rotator's position)
  
  display.setTextSize(2);
  display.setTextColor(WHITE);
  display.setCursor(0,0);

  display.println("AZ: " + String(azPos));
  display.setCursor(0,24);
  display.setTextSize(1);
  display.println(ip_address);
  display.display();

  if(azPos > 180) {
    displayTurnRight();
  } else {
    displayTurnLeft();
  }

  Serial.println("Start: " + azPos);
  xRot.setNewPosition(0);
  xRot.move();
  Serial.println("End: " + azPos);

  newHeading = getHeading();
  
  display.setTextSize(2);
  display.setTextColor(WHITE);
  display.setCursor(0,0);

  display.println("AZ: " + String(newHeading) + "  ");
  display.setCursor(0,24);
  display.setTextSize(1);
  display.println(ip_address);
  display.display();

  return newHeading;
}

void displayTurnRight()
{
  display.setCursor(0,16);
  display.setTextSize(1);
  display.println("Turn right.");
}

void displayTurnLeft()
{
  display.setCursor(0,16);
  display.setTextSize(1);
  display.println("Turn left.");
}

void displayFoundNorth()
{
  display.setCursor(0,16);
  display.setTextSize(1);
  display.println("Pointing North.");
}

int getHeading()
{
  int x, y, z;
  int azimuth;

  xRot.off();
  elRot.off();
  delay(200);
  //float azimuth; //is supporting float too
  qmc.read(&x, &y, &z,&azimuth);
  xRot.on();
  elRot.on();

//  return qmc.azimuth(&y,&x);
  return azimuth;
}

int getDirectonToTurn(int heading)
{
  if(heading > 358 || heading < 2) {
    return 0;
  } else if(heading <= 180) {
    return -1;
  } else if(heading > 180) {
    return 1;
  }
}

void setup() {// *************************************************************     setup
  Serial.begin(115200);         // Start the Serial communication to send messages to the computer
  delay(10);

  pinMode (X_EN, OUTPUT);
  pinMode (X_DIR, OUTPUT);
  pinMode (X_STP, OUTPUT);
  pinMode (EL_EN, OUTPUT);
  pinMode (EL_DIR, OUTPUT);
  pinMode (EL_STP, OUTPUT);
  pinMode(X_MICRO_1, OUTPUT);
  pinMode(X_MICRO_2, OUTPUT);
  pinMode(X_MICRO_3, OUTPUT);
  pinMode(EL_MICRO_1, OUTPUT);
  pinMode(EL_MICRO_2, OUTPUT);
  pinMode(EL_MICRO_3, OUTPUT);
  
  digitalWrite (X_EN, HIGH); //ENA low=enabled
  digitalWrite (X_DIR, LOW); //DIR
  digitalWrite (X_STP, LOW); //PUL
  digitalWrite (EL_EN, HIGH); //ENA low=enabled
  digitalWrite (EL_DIR, LOW); //DIR
  digitalWrite (EL_STP, LOW); //PUL

  xRot.setDefaults();
  xRot.setPins(X_EN, X_DIR, X_STP, X_MICRO_1, X_MICRO_2, X_MICRO_3);        // setPins(int enable_pin, int direction_pin, int step_pin, int microstep_pin_1, int microstep_pin_2, int microstep_pin_3);
  xRot.setTrackingSpeed(TRACKING);
  xRot.setIntermediateSpeed(INTERMEDIATE);
  xRot.setFastSpeed(FAST);
  xRot.setMicrostep(1, 0, 0);
  xRot.flipRotation(1);

  elRot.setDefaults();
  elRot.setPins(EL_EN, EL_DIR, EL_STP, EL_MICRO_1, EL_MICRO_2, EL_MICRO_3);       // setPins(int en_5v, int dir_5v, int stp_5v, EL_micro_1, EL_micro_2, EL_micro_3);
  elRot.setTrackingSpeed(TRACKING);
  elRot.setIntermediateSpeed(INTERMEDIATE);
  elRot.setFastSpeed(FAST);
  elRot.flipRotation(1);

// Low  Low    Low      Full step
// High Low    Low      Half step
// Low  High   Low      Quarter step
// High High   Low      Eighth step
// High High   High     Sixteenth step
  xRot.setMicrostep(1, 1, 1);
  elRot.setMicrostep(1, 1, 1);


  azPos = 0;
  elPos = 0;

  // Connect to Wi-Fi
  delay(1000);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi..");
  }

  // IP address needs to be a string to display on the oLED display
  sprintf(ip_address, "IP:%d.%d.%d.%d", WiFi.localIP()[0], WiFi.localIP()[1], WiFi.localIP()[2], WiFi.localIP()[3] );
  
//  Serial.println("Connected to the WiFi network");
//  Serial.print("My IP address is: ");
//  Serial.println(ip_address);
//  Serial.println("Set up Gpredict to connect to this IP address using port 4533.");

  wifiServer.begin();

  // Compass
  Wire.begin();
  qmc.init();
  qmc.setMode(Mode_Continuous,ODR_100Hz,RNG_2G,OSR_256);
/*  
  Mode : Mode_Standby / Mode_Continuous

  ODR : ODR_10Hz / ODR_50Hz / ODR_100Hz / ODR_200Hz
  ouput data update rate

  RNG : RNG_2G / RNG_8G
  magneticfield measurement range

  OSR : OSR_512 / OSR_256 / OSR_128 / OSR_64
  over sampling rate
*/

//   SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { // Address 0x3C for 128x32
    Serial.println(F("SSD1306 allocation failed"));
    for(;;); // Don't proceed, loop forever
  }
  display.display();
  delay(5000);
  while(home() > 0){}
  elRot.off();                // Take a rest
  xRot.off();
  displayFoundNorth();
}

void loop()
{
    double angleAz;
    double angleEl;
    String data;
    
    WiFiClient client = wifiServer.available();
    if (client) {
      while (client.connected()) {
  
          if (client.available()) {
               data = client.readStringUntil('\n');
//              data.trim();
//              data.toUpperCase();
              Serial.println("Data: " + data + "END");
//              display.setTextSize(2);
//              display.setTextColor(WHITE);
//              display.setCursor(0,0);
//              display.println(data);
 
              if(data.indexOf("hm") != -1) {
                while(home() > 0){}
                displayFoundNorth();
              } else if(data == "u10") {
                slideUp(10);
              } else if(data.indexOf("u5") != -1) {
                slideUp(5);
              } else if(data.indexOf("d10") != -1) {
                slideDown(10);
              } else if(data.indexOf("d5") != -1) {
                slideDown(5);
              } else if(data.indexOf("l10") != -1) {
                slideLeft(10);
              } else if(data.indexOf("l5") != -1) {
                slideLeft(5);
              } else if(data.indexOf("r10") != -1) {
                slideRight(10);
              } else if(data.indexOf("r5") != -1) {
                slideRight(5);
              } else if(data.indexOf("q") != -1 || data.indexOf(" hm") != -1) {
                Serial.println("Zeroing elevation.");
                elRot.setNewPosition(0);    // Home elevation
                elRot.move();
                elPos = 0;
                Serial.println("Done.");
                int nowPos = elRot.getPosition();
                Serial.print("Elevation is now at: ");
                Serial.println(nowPos);
                unwind();
                while(home() > 0){}         // Home azimuth
                elRot.off();                // Take a rest
                xRot.off();
              } else if(data.indexOf(" uw") != -1) {
                Serial.println("Unwinding.");
                elRot.setNewPosition(0);    // Home elevation
                elRot.move();
                elPos = 0;
                int nowPos = elRot.getPosition();
                Serial.print("Elevation is now at: ");
                Serial.println(nowPos);
                unwind();
              }
  
              if (data.length() > 3) {
                  String az = data.substring(2);
                  az = az.substring(0, az.indexOf(' '));
                  String el = data.substring(data.indexOf(' ')); 
                  el = el.substring(el.indexOf(' ') + 1);
                  el = el.substring(el.indexOf(' ') + 1);
  
                  if(el.length() > 3) {
//                      Serial.print("Azimuth: ");            
//                      Serial.println(az);
//                      Serial.print("Elevation: ");            
//                      Serial.println(el);

                  
                      angleAz = az.toFloat();
                      azPos = (int)angleAz;
                      Serial.print("Converted ");
                      Serial.print(az);
                      Serial.print(" to ");
                      Serial.println(azPos);
                      xRot.on();
                      xRot.setNewPosition(azPos);
                      xRot.move();

                      angleEl = el.toFloat();
                      elPos = (int)angleEl;
                      Serial.print("Converted ");
                      Serial.print(el);
                      Serial.print(" to ");
                      Serial.println(elPos);
                      elRot.on();
                      elRot.setNewPosition(elPos);
                      elRot.move();
                }
            }
  
            client.print("0");                                      // Send the all is good response back to Gpredict
        }
    }

    client.stop();
    }
}
