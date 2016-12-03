#include "ManchesterRF.h"	// https://github.com/cano64/ManchesterRF
#include "EEPROM.h"
#include "EEPROMAnything.h"

#define DEBUG

// Configuration format
// --------------------
//
// CHANNEL
// 8

#define CONFIG_ADDRESS	0x00	// EEPROM address to store configuration
#define CONFIG_SIZE		0x0F 	// configuration array size in bytes
#define CONFIG_BACKUPS 	3 		// number of configuration copies to store

typedef struct
{
	uint8_t b[CONFIG_SIZE];
	byte crc;
} tx_config_t;

tx_config_t tx_config;

#define TX_PIN 11				// TX connects to this pin
#define LED_PIN 13				// LED connects to this pin
#define	PROG_ENABLE_PIN 4 		// if this pin is LOW, enable TX programming mode. Pins 5-9 are used to set channel
#define PROG_PIN_COUNT 5 		// number of programming pins
#define LOW_BRIGHTNESS_PIN 2 	// PCINT
#define HIGH_BRIGHTNESS_PIN 3 	// PCINT

// Dimmer 3-byte message format
// ----------------------------
//
// HEADER-MSG_TYPE-CHANNEL-DATA-TRAILER
// 4      3        5       8    4

// 4 bits at the beginning and at the end of message
#define DATA_START 		0xF0	// packet start
#define DATA_STOP 		0x0A	// packet end

// 3 bits for different message types
#define MSG_BRIGHTNESS	0x00	// brighness adjustment
#define MSG_SET_LOW		0x01	// set low dimmable brighness value (multiplied by 10 at RX)
#define MSG_SET_HIGH	0x02	// set high dimmable brighness value (multiplied by 10 at RX)
#define MSG_SET_SPEED	0x03	// set dimming speed, 0-255, 0 = instant switching

#define BRIGHTNESS_HIGH	0xAA	// value to be sent for high brigtness
#define BRIGHTNESS_LOW	0x3C	// value to be sent for low brigtness

uint8_t channel = 0;			// channel to transmit information, 5 bits are used (31 channels supported, channel 0 impossible)

ManchesterRF rf(MAN_2400);		//link speed, try also MAN_300, MAN_600, MAN_1200, MAN_2400, MAN_4800, MAN_9600, MAN_19200, MAN_38400

//CRC-8 - based on the CRC8 formulas by Dallas/Maxim
//code released under the therms of the GNU GPL 3.0 license
byte crc8(const byte *data, byte len)
{
	byte crc = 0x00;
	while (len--)
	{
		byte extract = *data++;
		for (byte tempI = 8; tempI; tempI--)
		{
			byte sum = (crc ^ extract) & 0x01;
			crc >>= 1;
			if (sum)
			{
				crc ^= 0x8C;
			}
			extract >>= 1;
		}
	}
	return crc;
}

void zero_unused_config()
{
	for (uint8_t i = 1; i < CONFIG_SIZE; i++)
	{
		tx_config.b[i] = 0;
	}
}

void write_config()
{
	uint8_t j = 0;
	for (uint8_t i = (PROG_PIN_COUNT + PROG_ENABLE_PIN); i > PROG_ENABLE_PIN; i--)
	{
		pinMode(i, INPUT_PULLUP);
		bitWrite(channel, j, ((digitalRead(i) == HIGH) ? 0 : 1));
#ifdef DEBUG
		Serial.print("write_config() bit=");
		Serial.print(j);
		Serial.print(" value=");
		Serial.println(((digitalRead(i) == HIGH) ? 0 : 1));
#endif
		j++;
	}
	tx_config.b[0] = channel;
	zero_unused_config();
	tx_config.crc = crc8(tx_config.b, CONFIG_SIZE);
#ifdef DEBUG
	Serial.print("write_config() tx_config.b[0]=");
	Serial.print(tx_config.b[0]);
	Serial.print(" tx_config.crc=");
	Serial.println(tx_config.crc);
#endif
	for (uint8_t i = 0; i < CONFIG_BACKUPS; i++)
	{
		EEPROM_writeAnything(CONFIG_ADDRESS + (i * (CONFIG_SIZE + 1)), tx_config);
	}
	channel = 0;
}

void read_config()
{
	for (uint8_t i = 0; i < CONFIG_BACKUPS; i++)
	{
		EEPROM_readAnything(CONFIG_ADDRESS + (i * (CONFIG_SIZE + 1)), tx_config);
#ifdef DEBUG
		Serial.print("read_config() attempt ");
		Serial.print(i);
		Serial.print(" tx_config.b[0]=");
		Serial.print(tx_config.b[0]);
		Serial.print(" tx_config.crc=");
		Serial.print(tx_config.crc);
		Serial.print(" crc8()=");
		Serial.println(crc8(tx_config.b, CONFIG_SIZE));
#endif
		if (crc8(tx_config.b, CONFIG_SIZE) == tx_config.crc)
		{
			channel = tx_config.b[0];
#ifndef DEBUG
			break;
#endif
		}
	}

	if (channel == 0)	// CONFIG_BACKUPS attempts to read EEPROM were ALL unsuccessful => EEPROM corrupted or not programmed
	{
#ifdef DEBUG
		Serial.println("read_config() failure");
#endif
		while (true)
		{
			digitalWrite(LED_PIN, LOW);
			delay(100);
			digitalWrite(LED_PIN, HIGH);
			delay(100);
		}
	}
}

void send_message(uint8_t message_type, uint8_t message_value)
{
	uint8_t b1, b2, b3;				// ManchesterRF 3-byte transmitByte() bytes;
	b1 = (DATA_START | message_type | (channel >> 4));
	b2 = ((channel << 4) | (message_value >> 4));
	b3 = ((message_value << 4) | DATA_STOP);

#ifdef DEBUG
	Serial.print("send_message(");
	Serial.print(message_type);
	Serial.print(", ");
	Serial.print(message_value);
	Serial.print(") b1=");
	Serial.print(b1, BIN);
	Serial.print(", b2=");
	Serial.print(b2, BIN);
	Serial.print(", b3=");
	Serial.println(b3, BIN);
#endif
	// do things 3 times since delivery is not guaranteed
	rf.transmitByte(b1, b2, b3);
	rf.transmitByte(b1, b2, b3);
	rf.transmitByte(b1, b2, b3);
}

void setup()
{
#ifdef DEBUG
	Serial.begin(57600);
#endif
	pinMode(LED_PIN, OUTPUT);
	pinMode(PROG_ENABLE_PIN, INPUT_PULLUP);
	if (digitalRead(PROG_ENABLE_PIN) == LOW)
	{
		write_config();
	}
	rf.TXInit(TX_PIN);
	read_config();
}

void loop()
{
	uint16_t wait = 5000;

	digitalWrite(LED_PIN, LOW);
	send_message(MSG_BRIGHTNESS, BRIGHTNESS_LOW);
	delay(wait);

	digitalWrite(LED_PIN, HIGH);
	send_message(MSG_BRIGHTNESS, BRIGHTNESS_HIGH);
	delay(wait);
}