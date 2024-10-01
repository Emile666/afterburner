#ifndef _AFTERBURNER_H_
#define _AFTERBURNER_H_
/*                                    (banner font: aciiart.eu)
  _____________________________________________________________
 |       _    __ _            _                                \
 |      / \  / _| |_ ___  _ _| |__  _   _ _ __ ___  ___  _ _   |\
 |     / _ \| |_| '_/ _ \| '_/ '_ \| | | | '_/  _ \/ _ \| '_/  ||
 |    / ___ \  _| |_| __/| | | |_) | |_| | | | | | | __/| |    ||
 |   /_/   \_\|  \__\___||_| |____/\___,_|_| |_| |_|___||_|    ||     
 \_____________________________________________________________||
   '------------------------------------------------------------'

   Afterburner: GAL IC Programmer for Arduino by -= olin =-

   Based on ATFblast 3.1 by Bruce Abbott 
     http://www.bhabbott.net.nz/atfblast.html
   
   Based on GALBLAST by Manfred Winterhoff
     http://www.armory.com/%7Erstevew/Public/Pgmrs/GAL/_ClikMe1st.htm

   Based on GALmate by Yorck Thiele
     https://www.ythiee.com/2021/06/06/galmate-hardware/

   Supports:
   * National GAL16V8
   * Lattice GAL16V8A, GAL16V8B, GAL16V8D
   * Lattice GAL22V10B
   * Lattice GAL20V8
   * Atmel ATF16V8B, ATF16V8C, ATF22V10B, ATF22V10CQZ 

   Requires:
   * afterburner PC program to upload JED fuse map, erase, read etc.
   * simple programming circuit. See: https://github.com/ole00/afterburner

   * 2024-02-02 Fixed: Command 'B9' (Calibration Offset = 0,25V) doesn't work
                Note: Also requires elimination of a  in the PC program afterburner.c
                Added: 10.0V measurement in measureVppValues(()
*/
#define VERSION "0.6.1EL"

//#define DEBUG_PES
//#define DEBUG_VERIFY

//ARDUINO UNO pin mapping
//    GAL PIN NAME | ARDUINO UNO PIN NUMBER

//programing voltage control pin
#define PIN_VPP        11
#define PIN_SDOUT      12
#define PIN_STROBE     13
#define PIN_PV         9
#define PIN_SDIN       8

#define PIN_RA0         10
#define PIN_RA1         2
#define PIN_RA2         3
#define PIN_RA3         4
#define PIN_RA4         5
#define PIN_RA5         6
#define PIN_SCLK        7

// pin multiplex: ZIF_PIN <----> ARDUINO PIN or Shift register pin (0b1xxx)
#define PIN_ZIF3          2
#define PIN_ZIF4          0b1
#define PIN_ZIF5          0b1000
#define PIN_ZIF6          0b100
#define PIN_ZIF7          0b10
#define PIN_ZIF8          5
#define PIN_ZIF9          6
#define PIN_ZIF10         7
#define PIN_ZIF11         8
#define PIN_ZIF13         12
#define PIN_ZIF14         11
#define PIN_ZIF15         10
#define PIN_ZIF16         9
#define PIN_ZIF20         0b100000
#define PIN_ZIF21         0b10000
#define PIN_ZIF22         4
#define PIN_ZIF23         3
#define PIN_ZIF_GND_CTRL  13

#if CONFIG_IDF_TARGET_ESP32S2 == 1
//A0: VPP sense
//A3: DIGI_POT CS
#define A0   14
#define A1   15
#define A2   16
#define A3   17
//clk and dat is shared SPI bus
#define A4  18
#define A5  21
#endif

// AVR, or UNO R4
//A0: VPP sense
//A3: DIGI_POT CS
#define PIN_SHR_EN   A1
#define PIN_SHR_CS   A2
//clk and dat is shared SPI bus
#define PIN_SHR_CLK  A4
#define PIN_SHR_DAT  A5

#define COMMAND_NONE 0
#define COMMAND_UNKNOWN 1
#define COMMAND_IDENTIFY_PROGRAMMER '*'
#define COMMAND_HELP 'h'
#define COMMAND_UPLOAD 'u'
#define COMMAND_DEBUG 'd'
#define COMMAND_READ_PES 'p'
#define COMMAND_WRITE_PES 'P'
#define COMMAND_READ_FUSES 'r'
#define COMMAND_WRITE_FUSES 'w'
#define COMMAND_VERIFY_FUSES 'v'
#define COMMAND_ERASE_GAL 'c'
#define COMMAND_ERASE_GAL_ALL '~'
#define COMMAND_UTX '#'
#define COMMAND_ECHO 'e'
#define COMMAND_TEST_VOLTAGE 't'
#define COMMAND_SET_GAL_TYPE 'g'
#define COMMAND_ENABLE_CHECK_TYPE 'f'
#define COMMAND_DISABLE_CHECK_TYPE 'F'
#define COMMAND_ENABLE_SECURITY 's'
#define COMMAND_ENABLE_APD 'z'
#define COMMAND_DISABLE_APD 'Z'
#define COMMAND_MEASURE_VPP 'm'
#define COMMAND_CALIBRATE_VPP 'b'
#define COMMAND_CALIBRATION_OFFSET 'B'
#define COMMAND_JTAG_PLAYER 'j'

#define READGAL 0
#define VERIFYGAL 1
#define READPES 2
#define SCLKTEST 3
#define WRITEGAL 4
#define ERASEGAL 5
#define ERASEALL 6
#define BURNSECURITY 7
#define WRITEPES 8
#define VPPTEST 9
#define INIT 100

//check GAL type before starting an operation
#define FLAG_BIT_TYPE_CHECK (1 << 0)

// ATF16V8C flavour
#define FLAG_BIT_ATF16V8C (1 << 1)

// Keep the power-down feature enabled for ATF C GALs
#define FLAG_BIT_APD (1 << 2)

// contents of pes[3]
// Atmel PES is text string eg. 1B8V61F1 or 3Z01V22F1
//                                 ^           ^
#define LATTICE 0xA1
#define NATIONAL 0x8F
#define SGSTHOMSON 0x20
#define ATMEL16 'V'
#define ATMEL22 '1'
#define ATMEL750 'C'

typedef enum {
  UNKNOWN,
  GAL16V8,
  GAL18V10,
  GAL20V8,
  GAL20RA10,
  GAL20XV10,
  GAL22V10,
  GAL26CV12,
  GAL26V12,
  GAL6001,
  GAL6002,
  ATF16V8B,
  ATF20V8B,
  ATF22V10B,
  ATF22V10C,
  ATF750C,
  LAST_GAL_TYPE //dummy
} GALTYPE;

typedef enum {
  PINOUT_UNKNOWN,
  PINOUT_16V8,
  PINOUT_18V10,
  PINOUT_20V8,
  PINOUT_22V10,
  PINOUT_600,
} PINOUT;

#define BIT_NONE 0
#define BIT_ZERO 1
#define BIT_ONE  2

// config bit numbers

#define CFG_BASE_16   2048
#define CFG_BASE_18   3456
#define CFG_BASE_20   2560
#define CFG_BASE_20RA 3200
#define CFG_BASE_20XV 1600
#define CFG_BASE_22   5808
#define CFG_BASE_26CV 6344
#define CFG_BASE_26V  7800
#define CFG_BASE_600  8154
#define CFG_BASE_750 14364

#define CFG_STROBE_ROW 0
#define CFG_SET_ROW 1
#define CFG_STROBE_ROW2 3

// Atmel power-down row
#define CFG_ROW_APD 59

// Naive detection of the board's RAM size - for support of big Fuse map:
// PIN_A11 - present on MEGA (8kB) or Leonardo (2.5kB SRAM)
//  _RENESAS_RA_ - Uno R4 (32kB)
#if defined(PIN_A11) || defined(_RENESAS_RA_)
#define RAM_BIG
#endif

//ESP32-S2
#if CONFIG_IDF_TARGET_ESP32S2 == 1
#define RAM_BIG
#endif

#ifdef RAM_BIG
// for ATF750C
// MAXFUSES = (((171 * 84 bits)  + uesbits + (10*3 + 1 + 10*4 + 5)) + 7) / 8
//               (14504 + 7) / 8 = 1813
#define MAXFUSES 1813
#else
// Boards with small RAM (< 2.5kB) do not support ATF750C
// MAXFUSES calculated as the biggest required space to hold the fuse bitmap
// MAXFUSES = GAL6002 8330 bits = 8330/8 = 1041.25 bytes rounded up to 1042 bytes
//#define MAXFUSES 1042
//extra space added for sparse fusemap (just enough to fit erased ATF750C)
#define MAXFUSES 1332
#define USE_SPARSE_FUSEMAP
#endif

//   UES     user electronic signature
//   PES     programmer electronic signature (ATF = text string, others = Vendor/Vpp/timing)
//   cfg     configuration bits for OLMCs

// GAL info 
typedef struct
{
    GALTYPE type;
    unsigned char id0,id1;          /* variant 1, variant 2 (eg. 16V8=0x00, 16V8A+=0x1A)*/
    short fuses;                    /* total number of fuses              */
    char pins;                      /* number of pins on chip             */
    char rows;                      /* number of fuse rows                */
    unsigned char bits;             /* number of fuses per row            */
    char uesrow;                    /* UES row number                     */
    short uesfuse;                  /* first UES fuse number              */
    char uesbytes;                  /* number of UES bytes                */
    char eraserow;                  /* row adddeess for erase             */
    char eraseallrow;               /* row address for erase all (also PES) */
    char pesrow;                    /* row address for PES read/write     */
    char pesbytes;                  /* number of PES bytes                */
    char cfgrow;                    /* row address of config bits (ACW)   */
    unsigned short cfgbase;         /* base address of the config bit numbers */
    const unsigned char *cfg;       /* pointer to config bit numbers      */
    unsigned char cfgbits;          /* number of config bits              */
    unsigned char cfgmethod;        /* strobe or set row for reading config */
    PINOUT pinout;
} galinfo_t;

#endif /* _AFTERBURNER_H_ */
