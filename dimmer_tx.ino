/*

  Manchester Receiver example
  
  In this example transmitter will transmit two 8 bit numbers per transmittion
    
*/


#include "ManchesterRF.h" //https://github.com/cano64/ManchesterRF



#define TX_PIN 11 //any pin can transmit
#define LED_PIN 13

ManchesterRF rf(MAN_2400); //link speed, try also MAN_300, MAN_600, MAN_1200, MAN_2400, MAN_4800, MAN_9600, MAN_19200, MAN_38400

uint8_t channel = 1;
uint8_t bh = 3;
uint8_t bl = 255;
uint16_t z = 450;

void setup() {
  Serial.begin(57600);
  pinMode(LED_PIN, OUTPUT);  
  digitalWrite(LED_PIN, HIGH);
  //Serial.println("PreInit");
  rf.TXInit(TX_PIN);
//  Serial.println("Init");

}

void loop() 
{
  for (uint16_t b = z; b <= z; b++)
  {
    rf.transmitByte(channel, (uint8_t)(b>>8), (uint8_t)b);
  //  Serial.println(b); 
//    delay(1000);
  }
//  digitalWrite(LED_PIN, digitalRead(LED_PIN)); //blink the LED on receive
  //delay(100);
  //Serial.println("Sent");
}


