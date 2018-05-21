#include <SPI.h>
#include "decoders.h"
#include "rfm69_constants.h"
#include <RFM69.h>
#include "local_sprintf.h"

#define RX_PIN 14    // 14 == A0. Must be one of the analog pins, b/c of the analog comparator being used.

#undef DEBUG

#define VERSION "20180520"

AcuRiteDecoder adx;

byte packetBuffer[60], packetFill;

volatile word pulse_width;
word last_width;
word last_poll = 0;

SPISettings SPI_settings(2000000, MSBFIRST, SPI_MODE0);

RFM69 radio;

//#define intPin 9
#define selPin 10
// MOSI is 11; MISO is 12; SCK is 13.

ISR(ANALOG_COMP_vect) {
    word now = micros();
    pulse_width = now - last_width;
    last_width = now;
}


static void setupPinChangeInterrupt () {
    pinMode(RX_PIN, INPUT);
    digitalWrite(RX_PIN, HIGH); // enable pullup
    
    // enable analog comparator with fixed voltage reference.
    // This will trigger an interrupt on any change of value on RX_PIN.
    ACSR = _BV(ACBG) | _BV(ACI) | _BV(ACIE);
    ADCSRA &= ~ _BV(ADEN);
    ADCSRB |= _BV(ACME);
    ADMUX = RX_PIN - 14; // Note that this is specific to the analog pins...
}

void addData(const byte *buf, byte len)
{
#ifdef DEBUG
  Serial.print("addData: ");
    for (byte i = 0; i < len; ++i) {
        Serial.print(' ');
        Serial.print((int) buf[i], HEX);
    }
    Serial.println();
#endif
  
    if (packetFill + len < sizeof(packetBuffer)) {
        memcpy(packetBuffer + packetFill, buf, len);
        packetFill += len;
    }
}

byte removeData(byte *buf, byte len_requested)
{
  // If there's no data in the buffer, then just return
  if (packetFill == 0)
    return 0;
  
  // If the buffer is the same size as (or smaller than) what's requested, then return what's in the buffer
  if (packetFill <= len_requested) {
    memcpy(buf, packetBuffer, packetFill);
    len_requested = packetFill;
    packetFill = 0;
    return len_requested;
  }
  
  // The buffer must have more data in it than the request.
  //  copy the data to return in to the return buffer
  memcpy(buf, packetBuffer, len_requested);
  //  move the remaining data down in packetBuffer
  for (int i=0; i<packetFill - len_requested; i++) {
    packetBuffer[i] = packetBuffer[i + len_requested];
  }
  packetFill -= len_requested;
  //  return how many bytes we copied (same as the requested length)
  return len_requested;
}

static void runPulseDecoders (volatile word& pulse) {
  // get next pulse with and reset it - need to protect against interrupts
  cli();
  word p = pulse;
  pulse = 0;
  sei();

  // if we had a pulse, go through each of the decoders
  if (p != 0) { 
    if (adx.nextPulse(p)) {
      byte size;
      const byte *data = adx.getData(size);
      addData(data, size);
      adx.resetDecoder();
    }
  }
}

uint16_t xfer16(uint16_t cmd)
{
  uint16_t reply;

  SPI.beginTransaction(SPI_settings);

  if (selPin != -1)
    digitalWrite(selPin, LOW);
  reply = SPI.transfer(cmd >> 8) << 8;
  reply |= SPI.transfer(cmd & 0xFF);
  if (selPin != -1)
    digitalWrite(selPin, HIGH);

  SPI.endTransaction();

  return reply;
}

uint8_t rfm69_read(uint8_t reg)
{
  uint8_t reply;
  
  SPI.beginTransaction(SPI_settings);
  if (selPin != -1)
    digitalWrite(selPin, LOW);
    
  SPI.transfer(reg);
  reply = SPI.transfer(0x00);
    
  if (selPin != -1)
    digitalWrite(selPin, HIGH);
    
  SPI.endTransaction();
  
  return reply;
}

void rfm69_write(uint8_t reg, uint8_t val)
{
  SPI.beginTransaction(SPI_settings);
  if (selPin != -1)
    digitalWrite(selPin, LOW);
    
  SPI.transfer(reg | 0x80); // write bit
  SPI.transfer(val);
    
  if (selPin != -1)
    digitalWrite(selPin, HIGH);
    
  SPI.endTransaction();
}

static void rf69_init_OOK () {
//  uint8_t dev_id = rfm69_read(RegVersion);
//  if (dev_id != 0x00 || dev_id != 0xff)
//    return;

  rfm69_write(RegOpMode, RegOpModeStandby);
  rfm69_write(RegDataModul, RegDataModulContinuous | RegDataModulOOK); // Set continuous OOK mode
  RegBitrateSet(8000); // 8.0kb/s
  RegFrfSet(433920000); // fundamental frequency = 433.92MHz (really 433.920044 MHz)
  
  rfm69_write(RegRxBw, RegRxBwDccFreq4 | RegRxBwOOK50k); // 4% DC cancellation; 50k bandwidth in OOK mode
  rfm69_write(RegLna, RegLnaZ200 | RegLnaGainSelect12db); // 200 ohm, -12db

  rfm69_write(RegOokPeak,
   RegOokThreshPeak | RegOokThreshPeakStep0d5 | RegOokThreshPeakDec1c );

  rfm69_write(RegOpMode, RegOpModeRX);
  rfm69_write(RegAfcFei, RegAfcFeiAfcClear);
}

void setup () {
    Serial.begin(115200);
    Serial.print("\n[AcuRite decoder version ");
    Serial.print(VERSION);
    Serial.println("]");
    
//    pinMode(intPin, INPUT);
//    digitalWrite(intPin, LOW); // turn off pull-up until it's running properly
    
    rf69_init_OOK();
    
    setupPinChangeInterrupt();
}

byte calcParity(byte b)
{
  byte result = 0;
  for (byte i=0; i<=6; i++) {
    result ^= (b & (1<<i)) ? 1 : 0;
  }
  return result ? 0x80 : 0x00;
}

void PrintSource(const byte *data)
{
  Serial.print("Source: ");
  unsigned long id = ((data[0] & 0x3f) << 7) | (data[1] & 0x7f);
  Serial.print(id);
  
  Serial.print("/");
  switch (data[0] & 0xC0) {
  case 0xc0:
    Serial.print("A");
    break;
  case 0x80:
    Serial.print("B");
    break;
  case 0x00:
    Serial.print("C");
    break;
  default:
    Serial.print("x");
    break;
  }
}

void DecodePacket(const byte *data)
{
#ifdef DEBUG
  Serial.print("DecodePacket: ");
    for (byte i = 0; i < 7; i++) {
        Serial.print(' ');
        Serial.print((int) data[i], HEX);
    }
    Serial.println();
#endif
  
  
    // Check parity bits. (Byte 0 and byte 7 have no parity bits.)
    // ... Byte 1 might also not be parity-ing correctly. A new device
    // I bought in early 2018 has a parity error in byte 1, but
    // otherwise seems to work correctly.
    for (int i=2; i<=5; i++) {
      if (calcParity(data[i]) != (data[i] & 0x80)) {
#ifdef DEBUG
        Serial.print("Parity failure in byte ");
        Serial.println(i);
#endif
        return;
      }
    }

    // Check checksum.
    unsigned char cksum = 0;
    for (int i=0; i<=5; i++) {
      cksum += data[i];
    }
    if ((cksum & 0xFF) != data[6]) {
      PrintSource(data);
      Serial.print(" checksum failure - calcd 0x");
      Serial.print(cksum, HEX);
      Serial.print(", expected 0x");
      Serial.println(data[6], HEX);
      return;
    }

    // If data[2] & 0x80 is set, that's a low battery condition.  At
    // the same time, data[2] & 0x40 is *not* set (in my sample size
    // of 1). I'm not sure if that means they always happen together
    // or not.
    int battery_low = 0;
    if ((data[2] & 0xC0) != 0x40) {
      if (data[2] & 0xC0 == 0x80) {
	battery_low = 1;
      } else {
	PrintSource(data);
	Serial.print(" abnormal status 0x");
	Serial.println(data[2] & 0xC0, HEX);
      }
    }
    
    // Check the signature and warn if it doesn't match.
    // (I'm assuming this is a signature; it might not be!)
    if ((data[2] & 0x3F) != 0x04) {
      PrintSource(data);
      Serial.print(" Device signature 0x");
      Serial.print(data[2] & 0x3F, HEX);
      Serial.println(" does not match AcuRite signature 0x04");
    }

    PrintSource(data);
    if ((data[3] & 0x7F) != 0x7F) {
      Serial.print(" Humidity: ");
      Serial.print(data[3] & 0x7f);
      Serial.print("%");
    }
    
    unsigned long t1 = ((data[4] & 0x0F) << 7) | (data[5] & 0x7F);
    float temp = ((float)t1 - (float) 1024) / 10.0;
    
    // add manual calibration values to sensors...
#if 0
    unsigned long id = ((data[0] & 0x3f) << 7) | (data[1] & 0x7f);
    if (id == 382) temp += 3.0;
    if (id == 4454) temp += 2.6;
    if (id == 59 || id == 2676) temp += 2.5; // temperature varies substantially from the others
    if (id == 5296) temp += 3.0;
#endif
    
    Serial.print(" Temperature: ");
    Serial.print(temp);
    Serial.print(" C (");
    Serial.print(temp * 9.0 / 5.0 + 32.0);
    Serial.print(" F) battery ");

    if (battery_low) {
      Serial.println("low");
    } else {
      Serial.println("ok");
    }
    
    // relay data we want to see
     /*
    if ((data[0] & 0xC0) == 0xC0) { // channel A?
      byte buf[50];
      char msg[50];
      float F = temp * 9.0 / 5.0 + 32.0;
      cr_sprintf(msg, "A: %f deg", F);
      buf[0] = 'D';
      buf[1] = strlen(msg);
      memcpy(&buf[2], msg, strlen(msg));

      Serial.println("relaying"); 
      radio.initialize(RF69_433MHZ, 2, 210);
      radio.receiveDone();
      radio.send(1, buf, strlen(msg) + 2, false);
      rf69_init_OOK();
    }
      */
      
}

void loop () {
    runPulseDecoders(pulse_width);
    
    while (packetFill >= 7) {
      byte dbuf[7];
      removeData(dbuf, 7);
      DecodePacket(dbuf);
    }
}
