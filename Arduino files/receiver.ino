/* This is the peripheral part of the Project
 * This code will take the data received by the Wi-Fi module and process it 
 * The data will help decide to turn on the emergency signal or not (buzzer and LED) 
 */

// library
#include <SPI.h>
#include "nRF24L01.h"
#include "RF24.h"

// variables declaration for the peripheral
RF24 radio (7,8);
const uint64_t address =  0xF0F0F0F0E1LL;
const int Button = 5;
int msg[4];
int LEDState = 1;
int ButtonState = 0;
int BuzzerState = 1;
int lastButtonState = 0;
int lastBuzzerState = 0;

// set up function
void setup() {:
  Serial.begin(9600);
  // setting up the wifi module
  radio.begin();
  radio.setAutoAck(1);
  radio.setRetries(15,15);
  radio.setDataRate(RF24_2MBPS);
  radio.setPALevel(RF24_PA_MAX);
  radio.setChannel(10);
  radio.openReadingPipe(1,address);
  radio.startListening();
  // set pin mode for the buzzer, LED and the button
  pinMode(2,OUTPUT); // LED
  pinMode(3,OUTPUT); // buzzer
  pinMode(5,INPUT); // button
}

void loop() {
  // readin the data for button
  ButtonState = digitalRead(Button);
  if (radio.available()) {
    // read the array of data from the transmitter
    radio.read(&msg,sizeof(msg));
    // display the value of the array from the transmitter onto the serial display 
    Serial.print("1234:\t");
    Serial.print(msg[0]);
    Serial.print("\t");
    Serial.print(msg[1]);
    Serial.print("\t");
    Serial.print(msg[2]);

    // if the heart rate per minute are below 60 or over 120
    // or if the level of SpO2 goes below 70%
    // we turn on the buzzer and the LED for alarm
    if ( msg[0] < 60 || msg[0] > 120 || msg[1] < 70) {
      // writein the value for LED and buzzer
      digitalWrite(2, LEDState);
      digitalWrite(3, BuzzerState);
      // check last button state to turn off the buzzer and the LED properly
      if (ButtonState != lastButtonState) {
        lastButtonState = ButtonState;
        if (ButtonState == LOW) {
          LEDState = (LEDState == HIGH) ? LOW : HIGH;
          digitalWrite(2, LEDState);
        }
        if (ButtonState == LOW) {
          BuzzerState = (BuzzerState == HIGH) ? LOW : HIGH;
          digitalWrite(3, BuzzerState);
        }
      }
    }
  }
}
