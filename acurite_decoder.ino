#include <SPI.h>
#include "decoders.h"

#define RX_PIN 14    // 14 == A0. Must be one of the analog pins, b/c of the analog comparator being used.

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
  Serial.print("addData: ");
    for (byte i = 0; i < len; ++i) {
        Serial.print(' ');
        Serial.print((int) buf[i], HEX);
    }
    Serial.println();
  
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


// see http://jeelabs.org/2011/01/27/ook-reception-with-rfm12b-2/
// ... although this has been modified somewhat in order to receive from the AcuRite sensors; see
// see http://hackaday.io/project/4490-re-purposing-acurite-temperature-sensors/log/14666-receiving-the-data
static void rf12_init_OOK () {
    xfer16(0x8017); // 8027    868 Mhz;disabel tx register; disable RX
                          //         fifo buffer; xtal cap 12pf, same as xmitter
                          // 8017    433 MHz; disable tx; disable rx fifo; xtal cap 12pf

    xfer16(0x82c0); // 82C0    enable receiver; enable basebandblock 
    xfer16(0xA68a); // A68A    jorj: a620
    xfer16(0xc655); // C691    c691 datarate 2395 kbps 0xc647 = 4.8kbps
    xfer16(0x946a); // 9489    VDI; FAST;270khz;GAIn -6db; DRSSI 91dbm
    
    xfer16(0xC220); // C220    datafiltercommand; ** not documented cmd 
    xfer16(0xCA00); // CA00    FiFo and resetmode cmd; FIFO fill disabeld
    xfer16(0xC473); // C473    AFC run only once; enable AFC; enable
                          //         frequency offset register; +3 -4
    xfer16(0xCC67); // CC67    pll settings command
    xfer16(0xB800); // TX register write command not used
    xfer16(0xC800); // disable low dutycycle 
    xfer16(0xC040); // 1.66MHz,2.2V not used see 82c0  
}

void setup () {
    Serial.begin(57600);
    Serial.println("\n[AcuRite decoder]");
    
    pinMode(intPin, INPUT);
    digitalWrite(intPin, LOW); // turn off pull-up until it's running properly
    
    //    rf12_initialize(0, RF12_433MHZ);
    rf12_init_OOK();
    
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
  Serial.print("DecodePacket: ");
    for (byte i = 0; i < 7; i++) {
        Serial.print(' ');
        Serial.print((int) data[i], HEX);
    }
    Serial.println();

  
  
  // Check parity bits. (Byte 0 and byte 7 have no parity bits.)              
    for (int i=1; i<=5; i++) {
      if (calcParity(data[i]) != (data[i] & 0x80)) {
        Serial.print("Parity failure in byte ");
        Serial.println(i);
        return;
      }
    }
    
        // Check checksum.                                                          
    unsigned char cksum = 0;
    for (int i=0; i<=5; i++) {
      cksum += data[i];
    }
    if ((cksum & 0xFF) != data[6]) {
      Serial.print("Checksum failure - calcd 0x");
      Serial.print(cksum, HEX);
      Serial.print(", expected 0x");
      Serial.println(data[6], HEX);
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
