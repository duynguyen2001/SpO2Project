#include <SPI.h>
#include "nRF24L01.h"
#include "RF24.h"

RF24 radio (7,8);
const uint64_t address =  0xF0F0F0F0E1LL;
const int Button = 5;
int msg[4];
int LEDState = 1;
int ButtonState = 0;
int BuzzerState = 1;
int lastButtonState = 0;
int lastBuzzerState = 0;
void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600); 
  radio.begin();
  radio.setAutoAck(1);
  radio.setRetries(15,15);
  radio.setDataRate(RF24_2MBPS);
  radio.setPALevel(RF24_PA_MAX);
  radio.setChannel(10);
  radio.openReadingPipe(1,address);
  radio.startListening();
  pinMode(2,OUTPUT);
  pinMode(3,OUTPUT);
  pinMode(5,INPUT);
}

void loop() { 
  ButtonState = digitalRead(Button);
  // put your main code here, to run repeatedly:
  if (radio.available()) {
    radio.read(&msg,sizeof(msg));
    Serial.print("1234:\t");
    Serial.print(msg[0]);
    Serial.print("\t");
    Serial.print(msg[1]);
    Serial.print("\t");
    Serial.print(msg[2]);
    Serial.print("\t");
    Serial.println(msg[3]);
    if ( msg[0] < 60 || msg[0] > 120 || msg[1] < 70) {
      digitalWrite(2, LEDState);
      digitalWrite(3, BuzzerState);
      if (ButtonState != lastButtonState) {
        lastButtonState = ButtonState;
        if (ButtonState == LOW) {
          LEDState = (LEDState == HIGH) ? LOW : HIGH;
          digitalWrite(2, LEDState);
        }
        if (ButtonState == LOW) {
          BuzzerState= (BuzzerState== HIGH) ? LOW : HIGH;
          digitalWrite(3, BuzzerState);
        }
      }
    }
  }
}
