#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include "decoders.h"

// At time 't', we transition to level 'l'
typedef struct { float t; unsigned char l; } data;

int datap = 0;
int datacount = 0;
data datapoints[1024];

unsigned long curr_millis = 0;
// micros rolls over a 32-bit number every 70 minutes or so...
unsigned long curr_micros = 0;

int readLine(FILE *f, char *buf, const int bufsize)
{
  int numRead;
  char toRead;
  int totalRead = 0;
  int ret = 1;

  while (totalRead < bufsize-1) {
    numRead = fread(&toRead, 1, 1, f);
    if (numRead != 1) {
      ret = 0;
      break;
    }
    if (toRead == 0)
      continue;
    
    if (toRead == 0x0A || toRead == 0x0D)
      break;

    buf[totalRead] = toRead;
    totalRead++;
  }

  buf[totalRead] = 0;
  return ret;
}

void read_data(const char *fn)
{
  char buf[1024];
  int bufsize = sizeof(buf);
  FILE *f = fopen(fn, "r");
  if (!f) {
    perror("can't open");
    exit(-1);
  }

  while (readLine(f, buf, bufsize)) {
    // double(seconds), logic level (0/1)
    char *p = buf;
    char *start = p;
    while (*p) {
      if (*p == ' '){ // skip
	p++;
	continue;
      }
      if (*p == ',') {
	// time
	datapoints[datacount].t = atof(start);
	start = p+1;
      }
      p++;
    }
    // logic value
    datapoints[datacount].l = atoi(start);
    datacount++;
  }
}

unsigned long millis()
{
  return curr_millis;
}

unsigned long micros()
{
  return curr_micros;
}

byte calcParity(byte b)
{
  byte result = 0;
  for (char i=0; i<=6; i++) {
    result ^= (b & (1<<i)) ? 1 : 0;
  }
  return result ? 0x80 : 0x00;
}

void interpretData(const byte *data, int size)
{
  if (size == 7) {
    printf("\n");
    for (int i=0; i<size; i++) {
      printf("0x%.2X ", data[i]);
    }
    printf("\n");

    // Check parity bits. (Byte 0 and byte 7 have no parity bits.)
    for (int i=1; i<=5; i++) {
      if (calcParity(data[i]) != (data[i] & 0x80)) {
	printf("Parity failure in byte %d\n", i);
	return;
      }
    }
    // Check checksum.
    unsigned char cksum = 0;
    for (int i=0; i<=5; i++) {
      cksum += data[i];
    }
    if ((cksum & 0xFF) != data[6]) {
      printf("Checksum failure (0x%X vs. 0x%X)\n", cksum, data[6]);
      return;
    }

    // Packet is good! Display information.

    // Source A/B/C and ID
    printf("Source ID: 0x%X\n", (data[0] & 0x3F) << 7 | (data[1] & 0x7F));

    printf("Channel: %c\n", (data[0] & 0xC0) == 0xC0 ? 'A' : 
	   (data[0] & 0xC0) == 0x80 ? 'B' :
	   (data[0] & 0xC0) == 0x00 ? 'C' :
	   'x');

    if ((data[2] & 0x7F) != 0x44) {
      printf("Device signature does not match AcuRite signature: 0x%X (expected 0x44)\n", data[2] & 0x7F);
    }

    // Humidity. Value of 0x7F means "no humidity sensor installed"
    if ((data[3] & 0x7F) != 0x7F) {
      printf("Humidity: %d%%\n", data[3] & 0x7f);
    }

    // Temperature
    unsigned long t1 = ((data[4] & 0x0F) << 7) | (data[5] & 0x7F);
    float temp = ((float)t1 - (float) 1024) / 10.0;
    printf("Temperature: %f C (%f F)\n", temp, temp * 9.0 / 5.0 + 32.0);
  }
}

int main(int argc, char *argv[])
{
  AcuRiteDecoder decoder;
  
  read_data(argv[1]);

  // Begin a test pass - load the decoder and start passing it data
  datap = 0;
  while (datap < datacount) {
    curr_millis = datapoints[datap].t * 1000.0; // convert seconds to milliseconds
    unsigned long new_micros = datapoints[datap].t * 1000000.0;
    //printf(" [@ %lu = %d] ", new_micros, datapoints[datap].l);

    unsigned long elapsed_micros = new_micros - curr_micros;
    curr_micros = new_micros;

    if (decoder.nextPulse(elapsed_micros)) {
      byte size;
      const byte* data = decoder.getData(size);

      interpretData(data, size);
      decoder.resetDecoder();
    }

    datap++;
  }

  return 0;
}
