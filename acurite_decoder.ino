#include <SPI.h>
#include "decoders.h"
#include "rfm69_constants.h"

#define RX_PIN 14    // 14 == A0. Must be one of the analog pins, b/c of the analog comparator being used.

#undef DEBUG

AcuRiteDecoder adx;

byte packetBuffer[60], packetFill;

volatile word pulse_width;
word last_width;
word last_poll = 0;

SPISettings SPI_settings(2000000, MSBFIRST, SPI_MODE0);

#define intPin 2
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
  RegBitrateSet(8000); // 4.8kb/s
  RegFrfSet(433920000); // fundamental frequency = 433.92MHz (really 433.920044 MHz)
  
  rfm69_write(RegRxBw, RegRxBwDccFreq4 | RegRxBwOOK50k); // 4% DC cancellation; 50k bandwidth in OOK mode
  rfm69_write(RegLna, RegLnaZ200 | RegLnaGainSelect12db); // 200 ohm, -6db

  rfm69_write(RegOokPeak,
   RegOokThreshPeak | RegOokThreshPeakStep0d5 | RegOokThreshPeakDec1c );

  rfm69_write(RegOpMode, RegOpModeRX);
  rfm69_write(RegAfcFei, RegAfcFeiAfcClear);
}

void setup () {
    Serial.begin(57600);
    Serial.println("\n[AcuRite decoder]");
    
    pinMode(intPin, INPUT);
    digitalWrite(intPin, LOW); // turn off pull-up until it's running properly
    
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
    for (int i=1; i<=5; i++) {
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
#ifdef DEBUG
      Serial.print("Checksum failure - calcd 0x");
      Serial.print(cksum, HEX);
      Serial.print(", expected 0x");
      Serial.println(data[6], HEX);
#endif
      return;
    }

    Serial.print("Source ID: ");
    unsigned long id = ((data[0] & 0x3f) << 7) | (data[1] & 0x7f);
    Serial.println(id);
    
    Serial.print("Channel: ");
    switch (data[0] & 0xC0) {
      case 0xc0:
        Serial.println("A");
        break;
        case 0x80:
        Serial.println("B");
        break;
        case 0x00:
        Serial.println("C");
        break;
        default:
        Serial.println("x");
        break;
    }
    if ((data[2] & 0x7F) != 0x44) {
      Serial.println("Device signature does not match AcuRite signature 0x44");
    }

    if ((data[3] & 0x7F) != 0x7F) {
      Serial.print("Humidity: ");
      Serial.print(data[3] & 0x7f);
      Serial.println("%");
    }
    
    unsigned long t1 = ((data[4] & 0x0F) << 7) | (data[5] & 0x7F);
    float temp = ((float)t1 - (float) 1024) / 10.0;
    
    // add manual calibration values to sensors...
    if (id == 382) temp += 2.5; // reads humidity as 0x7f (127%)
    if (id == 4454) temp += 2.5; // reads humidity as 0x10 (16%)
    
    Serial.print("Temperature: ");
    Serial.print(temp);
    Serial.print(" C (");
    Serial.print(temp * 9.0 / 5.0 + 32.0);
    Serial.println(" F)");
}

void loop () {
    runPulseDecoders(pulse_width);
    
    while (packetFill >= 7) {
      byte dbuf[7];
      removeData(dbuf, 7);
      DecodePacket(dbuf);
    }
}
