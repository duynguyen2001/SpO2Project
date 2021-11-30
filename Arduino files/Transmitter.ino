/*
  Optical SP02 Detection (SPK Algorithm) using the MAX30105 Breakout
  By: Nathan Seidle @ SparkFun Electronics
  Date: October 19th, 2016
  https://github.com/sparkfun/MAX30105_Breakout

  This demo shows heart rate and SPO2 levels.

  It is best to attach the sensor to your finger using a rubber band or other tightening 
  device. Humans are generally bad at applying constant pressure to a thing. When you 
  press your finger against the sensor it varies enough to cause the blood in your 
  finger to flow differently which causes the sensor readings to go wonky.

  This example is based on MAXREFDES117 and RD117_LILYPAD.ino from Maxim. Their example
  was modified to work with the SparkFun MAX30105 library and to compile under Arduino 1.6.11
  Please see license file for more info.

  Hardware Connections (Breakoutboard to Arduino):
  -5V = 5V (3.3V is allowed)
  -GND = GND
  -SDA = A4 (or SDA)
  -SCL = A5 (or SCL)
  -INT = Not connected
 
  The MAX30105 Breakout can handle 5V or 3.3V I2C logic. We recommend powering the board with 5V
  but it will also run at 3.3V.
*/
#include <LiquidCrystal.h>
#include <Wire.h>
#include <RF24.h>
#include <nRF24L01.h>
#include <SPI.h>
#include "MAX30105.h"
#include "spo2_algorithm.h"
#include "heartRate.h"

/**************************************
 * Variable for SpO2 Sensor
 **************************************/
MAX30105 particleSensor; // connect to default RCI legs of : SCL 21 and SDA 20

#define MAX_BRIGHTNESS 255

#if defined(__AVR_ATmega328P__) || defined(__AVR_ATmega168__)
//Arduino Uno doesn't have enough SRAM to store 100 samples of IR led data and red led data in 32-bit format
//To solve this problem, 16-bit MSB of the sampled data will be truncated. Samples become 16-bit data.
uint16_t irBuffer[100]; //infrared LED sensor data
uint16_t redBuffer[100];  //red LED sensor data
#else
uint32_t irBuffer[100]; //infrared LED sensor data
uint32_t redBuffer[100];  //red LED sensor data
#endif

int32_t bufferLength; //data length
int32_t spo2; //SPO2 value
int8_t validSPO2; //indicator to show if the SPO2 calculation is valid
int32_t heartRate; //heart rate value
int8_t validHeartRate; //indicator to show if the heart rate calculation is valid


const byte RATE_SIZE = 6; //Increase this for more averaging. 4 is good.
byte rates[RATE_SIZE]; //Array of heart rates
byte rateSpot = 0;
long lastHeartRate;
int32_t beatAvg;


/**************************************
 * Variable for Wifi transmitter
 **************************************/
 
RF24 radio (7,8); // use leg 7, 8 tto connect to the wifi module
const uint64_t address = 0xF0F0F0F0E1LL;
int msg[2];

/**************************************
 * Variable for interrupt
 **************************************/
bool interruptState = true;
int interruptLegON = 19; 
int interruptLegOFF = 18; 


/**************************************
 * Variable for displaying data for display 16x2
 **************************************/

// initialize the library by associating any needed LCD interface pin
// with the arduino pin number it is connected to
const int rs = 42, en = 43, d4 = 34, d5 = 35, d6 = 36, d7 = 37;
LiquidCrystal lcd(rs, en, d4, d5, d6, d7);

/**************************************
 * Main start and loop
 **************************************/
void setup()
{
   setupDisplay();
   setupInterrupt();
   setupWifi();
   Serial.begin(9600); // initialize serial communication at 9600 bits per second:
   setupSpo2Sensor();
}

void loop()
{
  calculatingSpO2andHeartBeat();
}

/**************************************
 * Code for displaying data to 16x2 display
 **************************************/
void setupDisplay() {
  // set up the LCD's number of columns and rows:
  lcd.begin(16, 2);
}
void displayData(){
  // set the cursor to column 0, line 1
  // (note: line 1 is the second row, since counting begins with 0):
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("BPM=");
  lcd.print(msg[0]);
  lcd.setCursor(0, 1);
  lcd.print("SPO2= ");
  lcd.print(msg[1]);  
}
void displayMessage(String s1, String s2){
  // set the cursor to column 0, line 1
  // (note: line 1 is the second row, since counting begins with 0):
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(s1);
  lcd.setCursor(0, 1);
  lcd.print(s2);
  lcd.print(msg[1]);
  
}
/**************************************
 * Code for data transmitting by Wifi
 **************************************/
void setupWifi() {
  // put your setup code here, to run once:
  radio.begin();
  radio.setAutoAck(1);               
  radio.setRetries(15,15);             
  radio.setDataRate(RF24_2MBPS);    // Tốc độ truyền
  radio.setPALevel(RF24_PA_MAX);      // Dung lượng tối đa
  radio.setChannel(10);               // Đặt kênh
  radio.openWritingPipe(address); 
  radio.stopListening();
}
void transmitData() {
  if(beatAvg >= 50)
    msg[0]= (int) beatAvg;
  if(validSPO2 == 1)
    msg[1]= (int) spo2;
  radio.write(&msg,sizeof(msg));  
}

/**************************************
 * Code for SpO2 sensor and Heart Beat
 **************************************/
void setupSpo2Sensor() {

  // Initialize sensor
  if (!particleSensor.begin(Wire, I2C_SPEED_FAST)) //Use default I2C port, 400kHz speed
  {
    Serial.println(F("MAX30105 was not found. Please check wiring/power."));
    while (1);
  }

 
  byte ledBrightness = 60; //Options: 0=Off to 255=50mA
  byte sampleAverage = 4; //Options: 1, 2, 4, 8, 16, 32
  byte ledMode = 2; //Options: 1 = Red only, 2 = Red + IR, 3 = Red + IR + Green
  byte sampleRate = 100; //Options: 50, 100, 200, 400, 800, 1000, 1600, 3200
  int pulseWidth = 411; //Options: 69, 118, 215, 411
  int adcRange = 4096; //Options: 2048, 4096, 8192, 16384

  particleSensor.setup(ledBrightness, sampleAverage, ledMode, sampleRate, pulseWidth, adcRange); //Configure sensor with these settings
}
void calculatingSpO2andHeartBeat() {
    bufferLength = 50; //buffer length of 50 stores 4 seconds of samples running at 25sps
 
    Serial.println(F("Attach sensor to finger with rubber band. Press button to start measuring SpO2"));
    displayMessage("Press yellow button", "to measure SpO2");
    while (interruptState == 1) {
      delay(50);
      //  Serial.println("still in loop");
     } ; //wait until user presses the button

  //read the first 150 samples, and determine the signal range
  for (byte i = 0 ; i < bufferLength ; i++)
  {
    while (particleSensor.available() == false) //do we have new data?
      particleSensor.check(); //Check the sensor for new data

    redBuffer[i] = particleSensor.getRed();
    irBuffer[i] = particleSensor.getIR();
    particleSensor.nextSample(); //We're finished with this sample so move to next sample
    lastHeartRate = 0;
    Serial.print(F("red="));
    Serial.print(redBuffer[i], DEC);
    Serial.print(F(", ir="));
    Serial.println(irBuffer[i], DEC);
  }

  //calculate heart rate and SpO2 after first 100 samples (first 4 seconds of samples)
  maxim_heart_rate_and_oxygen_saturation(irBuffer, bufferLength, redBuffer, &spo2, &validSPO2, &heartRate, &validHeartRate);

  //Continuously taking samples from MAX30102.  Heart rate and SpO2 are calculated every 1 second
  while (interruptState == 0)
  {
    //dumping the first 25 sets of samples in the memory and shift the last 75 sets of samples to the top
    for (byte i = 25; i < 100; i++)
    {
      redBuffer[i - 25] = redBuffer[i];
      irBuffer[i - 25] = irBuffer[i];
    }

    //take 25 sets of samples before calculating the heart rate.
    for (byte i = 75; i < 100; i++)
    {
      while (particleSensor.available() == false) //do we have new data?
        particleSensor.check(); //Check the sensor for new data


      redBuffer[i] = particleSensor.getRed();
      irBuffer[i] = particleSensor.getIR();
      particleSensor.nextSample(); //We're finished with this sample so move to next sample


      if(irBuffer[i] < 50000) {
        
        Serial.println(F("Please put your finger on the sensor"));
        displayMessage("Please put your", "finger on sensor");
        continue;
      }
      //send samples and calculation result to terminal program through UART
      Serial.print(F("red="));
      Serial.print(redBuffer[i], DEC);
      Serial.print(F(", ir="));
      Serial.print(irBuffer[i], DEC);

      Serial.print(F(", HR="));
      Serial.print(heartRate, DEC);
      Serial.print(F(", HRvalid="));
      Serial.print(validHeartRate, DEC);

      Serial.print(F(", SPO2="));
      Serial.print(spo2, DEC);

      Serial.print(F(", SPO2Valid="));
      Serial.print(validSPO2, DEC);
      
      Serial.print(", Avg BPM=");
      Serial.print(beatAvg);  
      

      Serial.print(", last BPM=");
      Serial.println(lastHeartRate);

      calculateAvgHeartBeat(heartRate, validHeartRate);
      transmitData();
      displayData();
    }

    //After gathering 25 new samples recalculate HR and SP02
    maxim_heart_rate_and_oxygen_saturation(irBuffer, bufferLength, redBuffer, &spo2, &validSPO2, &heartRate, &validHeartRate);
  }
  
}

void calculateAvgHeartBeat(uint32_t heartRate, int8_t validHeartRate) {
  
    Serial.print(F("Go inside"));
    if (validHeartRate <= 0 || heartRate == lastHeartRate) {
      return;
    }

    if (heartRate < 255 && heartRate > 20)
    {
      rates[rateSpot++] = (byte)heartRate; //Store this reading in the array
      rateSpot %= RATE_SIZE; //Wrap variable
      
      lastHeartRate = heartRate;
      //Take average of readings
      beatAvg = 0;
      for (byte x = 0 ; x < RATE_SIZE ; x++)
        beatAvg += rates[x];
      beatAvg /= RATE_SIZE * 1.9;
    }
}

/**************************************
 * Code for interrupt component
 **************************************/
void setupInterrupt(){
  pinMode(interruptLegON, INPUT_PULLUP);
  pinMode(interruptLegOFF, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(interruptLegON), interruptHandler1, CHANGE);
  attachInterrupt(digitalPinToInterrupt(interruptLegOFF), interruptHandler2, CHANGE);
}
// Change interrupt State to false
void interruptHandler1() {
  interruptState = false;
  Serial.println("Interrupt set");
  Serial.println(interruptState);
}
// Change interrupt State to true
void interruptHandler2() {
  interruptState = true;
  Serial.println("Interrupt set");
  Serial.println(interruptState);
  displayMessage("Stop measuring", "Press yellow to start");
  delay(1000);
}
