#define RegFifo 0x00

#define RegOpMode 0x01
  #define RegOpModeSequencer   0x80
  #define RegOpModeListen      0x40
  #define RegOpModeListenAbort 0x20
  #define RegOpModeSleep       0x00
  #define RegOpModeStandby     0x04
  #define RegOpModeFS          0x08
  #define RegOpModeTX          0x0C
  #define RegOpModeRX          0x10

#define RegDataModul 0x02
  #define RegDataModulPacketMode     0x00
  #define RegDataModulContinuousSync 0x40
  #define RegDataModulContinuous     0x60
  #define RegDataModulFSK            0x00
  #define RegDataModulOOK            0x08
  #define RegDataModulNoShaping      0x00
  #define RegDataModulBT10           0x01
  #define RegDataModulBR             0x01
  #define RegDataModulBT05           0x02
  #define RegDataModul2BR            0x02
  #define RegDataModulBT03           0x03

#define RegBitrateMsb 0x03
#define RegBitrateLsb 0x04
#define RegBitrateSet(x) { rfm69_write(RegBitrateMsb, (32000000/(x))/256); \
    rfm69_write(RegBitrateLsb, (32000000/(x))%256); }

#define RegFdevMsb 0x05
#define RegFdevLsb 0x06

#define RegFrfMsb 0x07
#define RegFrfMid 0x08
#define RegFrfLsb 0x09
#define RegFrfSet(x) { \
    unsigned long _l = ((float)(x) / 61.03515625); \
    rfm69_write(RegFrfMsb, (_l >> 16) & 0xFF); \
    rfm69_write(RegFrfMid, (_l >> 8) & 0xFF); \
    rfm69_write(RegFrfLsb, _l & 0xFF); }

#define RegOsc1 0x0a

#define RegAfcCtrl 0x0b

#define RegListen1 0x0d
#define RegListen2 0x0e
#define RegListen3 0x0f

#define RegVersion 0x10

#define RegPaLevel 0x11

#define RegOcp 0x13

#define RegLna 0x18
  #define RegLnaZ50  0x00 // 50 ohm impedance
  #define RegLnaZ200 0x80 // 200 ohm impedance
  #define RegLnaGainSelectAGC  0x00
  #define RegLnaGainSelect0db  0x01 // highest possible gain (-0db)
  #define RegLnaGainSelect6db  0x02 // -6db
  #define RegLnaGainSelect12db 0x03 // -12db
  #define RegLnaGainSelect24db 0x04 // -24db
  #define RegLnaGainSelect36db 0x05 // -36db
  #define RegLnaGainSelect48db 0x06 // -48db

#define RegRxBw 0x19
  #define RegRxBwDccFreq16 0x00 // DC cut-off at 15.92% of rxbw
  #define RegRxBwDccFreq8  0x20 // 7.96%
  #define RegRxBwDccFreq4  0x40 // 3.98%
  #define RegRxBwDccFreq2  0x60 // 1.99%
  #define RegRxBwDccFreq1  0x80 // 1.00%
  #define RegRxBwDccFreq05 0xa0 // 0.50%
  #define RegRxBwDccFreq03 0xc0 // 0.25%
  #define RegRxBwDccFreq01 0xe0 // 0.12%

  #define RegRxBwFSK500k 0b00000 // 500000
  #define RegRxBwFSK250k 0b00001 // 250000
  #define RegRxBwFSK125k 0b00010 // 125000
  #define RegRxBwFSK63k  0b00011 // 62500
  #define RegRxBwFSK31k  0b00100 // 31250
  #define RegRxBwFSK16k  0b00101 // 15625
  #define RegRxBwFSK8k   0b00110 // 7812.5
  #define RegRxBwFSK4k   0b00111 // 3906.25
  #define RegRxBwFSK400k 0b01000 // 400000
  #define RegRxBwFSK200k 0b01001 // 200000
  #define RegRxBwFSK100k 0b01010 // 100000
  #define RegRxBwFSK50k  0b01011 // 50000
  #define RegRxBwFSK25k  0b01100 // 25000
  #define RegRxBwFSK13k  0b01101 // 12500
  #define RegRxBwFSK6k   0b01110 // 6250
  #define RegRxBwFSK3k   0b01111 // 3125
  #define RegRxBwFSK333k 0b10000 // 333333.3
  #define RegRxBwFSK167k 0b10001 // 166666.7
  #define RegRxBwFSK83k  0b10010 // 83333.3
  #define RegRxBwFSK42k  0b10011 // 41666.7
  #define RegRxBwFSK21k  0b10100 // 20883.3
  #define RegRxBwFSK10k  0b10101 // 10416.7
  #define RegRxBwFSK5k   0b10110 // 5208.3
  #define RegRxBwFSK2k6  0b10111 // 2604.2

  #define RegRxBwOOK250k 0b00000 // 250k
  #define RegRxBwOOK125k 0b00000 // 125k
  #define RegRxBwOOK63k  0b00000 // 62500
  #define RegRxBwOOK31k  0b00000 // 31250
  #define RegRxBwOOK16k  0b00000 // 15625
  #define RegRxBwOOK7k   0b00000 // 7182.5
  #define RegRxBwOOK4k   0b00000 // 3906.25
  #define RegRxBwOOK2k   0b00000 // 1953.125
  #define RegRxBwOOK200k 0b01000 // 200000
  #define RegRxBwOOK100k 0b01001 // 100000
  #define RegRxBwOOK50k  0b01010 // 50000
  #define RegRxBwOOK25k  0b01011 // 25000
  #define RegRxBwOOK13k  0b01100 // 12500
  #define RegRxBwOOK6k   0b01101 // 6250
  #define RegRxBwOOK3k   0b01110 // 3125
  #define RegRxBwOOK2k   0b01111 // 1562.5
  #define RegRxBwOOK167k 0b10000 // 166666.7
  #define RegRxBwOOK83k  0b10001 // 83333.3
  #define RegRxBwOOK42k  0b10010 // 41666.7
  #define RegRxBwOOK21k  0b10011 // 20883.3
  #define RegRxBwOOK10k  0b10100 // 10416.7
  #define RegRxBwOOK5k   0b10101 // 5208.3
  #define RegRxBwOOK2k6  0b10110 // 2604.2
  #define RegRxBwOOK1k3  0b10111 // 1302.1

#define RegAfcBw 0x1a

#define RegOokPeak 0x1b
  #define RegOokThreshFixed 0x00
  #define RegOokThreshPeak  0x40
  #define RegOokThreshAverage 0x80
  #define RegOokThreshPeakStep0d5 0x00 // 0.5db
  #define RegOokThreshPeakStep1d0 0x04 // 1.0db
  #define RegOokThreshPeakStep1d5 0x08 // 1.0db
  #define RegOokThreshPeakStep2d0 0x0c // 1.0db
  #define RegOokThreshPeakStep3d0 0x10 // 1.0db
  #define RegOokThreshPeakStep4d0 0x14 // 1.0db
  #define RegOokThreshPeakStep5d0 0x08 // 1.0db
  #define RegOokThreshPeakStep6d0 0x1c // 1.0db
  #define RegOokThreshPeakDec1c   0x00 // once per chip
  #define RegOokThreshPeakDecc2   0x01 // once every other chips
  #define RegOokThreshPeakDecc4   0x02 // once every four chips
  #define RegOokThreshPeakDecc8   0x03 // once every eight chips
  #define RegOokThreshPeakDec2c   0x04 // 2x every chip
  #define RegOokThreshPeakDec4c   0x05 // 4x every chip
  #define RegOokThreshPeakDec8c   0x06 // 8x every chip
  #define RegOokThreshPeakDec16c  0x07 // 16x every chip

#define RegOokAvg 0x1c

#define RegOokFix 0x1d

#define RegAfcFei 0x1e
  #define RegAfcFeiDone         0x40 // read-only
  #define RegAfcFeiStart        0x20 // write-only
  #define RegAfcFeiAfcDone      0x10 // read-only
  #define RegAfcFeiAfcAutoclear 0x08
  #define RegAfcFeiAutoOn       0x04
  #define RegAfcFeiAfcClear     0x02 // write-only
  #define RegAfcFeiAfcStart     0x01 // write-only

#define RegAfcMsb 0x1f
#define RegAfcLsb 0x20

#define RegFeiMsb 0x21
#define RegFeiLsb 0x22

#define RegRssiConfig 0x23

#define RegRssiValue 0x24

#define RegDioMapping1 0x25
#define RegDioMapping2 0x26

#define RegIrqFlags1 0x27
#define RegIrqFlags2 0x28

#define RegRssiThresh 0x29

#define RegRxTimeout1 0x2a
#define RegRxTimeout2 0x2b

#define RegPreambleMsb 0x2c
#define RegPreambleLsb 0x2d

#define RegSyncConfig 0x2e

#define RegSyncValue1 0x2f
#define RegSyncValue2 0x30
#define RegSyncValue3 0x31
#define RegSyncValue4 0x32
#define RegSyncValue5 0x33
#define RegSyncValue6 0x34
#define RegSyncValue7 0x35
#define RegSyncValue8 0x36

#define RegPacketConfig1 0x37

#define RegPayloadLength 0x38

#define RegNodeAdrs 0x39

#define RegBroadcastAdrs 0x3a

#define RegAutoModes 0x3b

#define RegFifoThresh 0x3c

#define RegPacketConfig2 0x3d

#define RegAesKey1 0x3e
#define RegAesKey2 0x3f
#define RegAesKey3 0x40
#define RegAesKey4 0x41
#define RegAesKey5 0x42
#define RegAesKey6 0x43
#define RegAesKey7 0x44
#define RegAesKey8 0x45
#define RegAesKey9 0x46
#define RegAesKey10 0x47
#define RegAesKey11 0x48
#define RegAesKey12 0x49
#define RegAesKey13 0x4a
#define RegAesKey14 0x4b
#define RegAesKey15 0x4c
#define RegAesKey16 0x4d

#define RegTemp1 0x4e
#define RegTemp2 0x4f

#define RegTestLna 0x58
#define RegTestPa1 0x5a
#define RegTestPa2 0x5c
#define RegTestDagc 0x6f
#define RegTestAfc 0x71
#define RegTest 0x50


