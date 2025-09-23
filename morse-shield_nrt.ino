/*
  Electric keyer for morse-shield on Arduino Uno
  designed by nksg JG5CBR/6
  2019/12/20

  http://www.nksg.net/usbif4cw/
  usbif4cw@nksg.net
*/

#include <Wire.h>
#include <FaBoLCDmini_AQM0802A.h>
#include <EEPROM.h>

String date = "19/12/20";
//#define DEBUG
#define SCROLL_ROW
//#define SERIAL_PRINT_DIT_DAH

#if 1 /* JG1WYW */
//String mod_date = "23/09/12";
String mod_date = "24/05/01";
/*
 * These options were added by JG1WYW
 * So far, off delay and look ahead memory is tested with improved Iambic_A.
 *
 * EEPROM:
 *  - Save settings by long press on Sw3
 *  - Load default by pressing on Sw4 during power on (not initializing EEPROM)
 *
 * Experimental features (need to be enabled explicitly):
 *  - Off delay
 *  - Look ahead memory
 *  - Avoid chattering
 *
 */
#define OPT_OFF_DELAY			/* off delay timers */
//#define OPT_ON_DELAY			/* on delay timers */
//#define OPT_REPEAT_ADD                  /* addition by repeat */
//#define OPT_LOOK_AHEAD_MEMORY           /* look ahead memory */
//#define OPT_TWO_TONE                    /* two tone for marking the trailing dot */
//#define OPT_ESP32                       /* ESP32 instead of Arduino Uno */
#define OPT_AVOID_CHATTERING            /* indend to avoid chattering */
//#define OPT_AVOID_CHATTERING_WATCH      /* indend to avoid chattering */
#define OPT_AUTO_KEYERTYPE              /* adapt to paddle/straight change */
#define OPT_INVADER_SOUND		/* invader sound */
//#define OPT_MEMORY_CLICK		/* memory click */
#define OPT_RUNNER			/* runner training of callsigns */
#define OPT_BETTER_DECODE		/* better decode */
#define OPT_BETTER_SRAND		/* random seed using timing */
#define OPT_PARAM_EEPROM                /* load/save params from/to EEPROM */
#define OPT_LCD_DISP                    /* on/off of LCD display */
#define OPT_TONE_FREQ                   /* selectable tone frequency */
#define OPT_LAST_MENUINDEX              /* memory the last menuIndex */
#define OPT_WORD_SPACE                  /* word space dots */
#define OPT_BETTER_IAMBIC_A             /* Improved behavior of Iambic_A */
#define OPT_DECODE_INVALID_IMMEDIATELY  /* '_' for invalid chars immediately when running */
//#define OPT_DECODE_DEBUG
//#define OPT_TIMING_DEBUG                /* Debug flag */
#ifdef OPT_ESP32
//#define OPT_VIBRATION  	                /* vibration feedback */
#endif

#define MOD_MISC      /* See comments for each modification */
//#define OPT_NO_SERIAL

#endif /* JG1WYW */

// keyer setting
#ifdef MOD_MISC
#define MIN_WPM 15
#define MAX_WPM 25
#else
#define MIN_WPM  5
#define MAX_WPM 50
#endif
#define L        0
#define R        1
#define UP       0
#define DOWN     1
int key = UP;

// default keyer value
int Wpm = 20;
float wait = 3.0;
int durationDot = 60;
int durationMark = 0;
int durationSpace = 0;
int durationLongPress = 1000;
int paddleRvs = 0; // paddle reverse

#ifdef OPT_RUNNER
int runner_mode = 0; // Runner mode instead of just callsigns
int running = 0; // means Now running
int runner_repeat = 0;
int runner_miss = 0;
int runner_accel = 0;
int runner_retry = 0;
int runnerDurationDot = 60;
int runner_count = 0;
#define MAX_RUNNER_ACCEL 20
#define HIGH_RUNNER_ACCEL 10
#endif

#if defined(OPT_RUNNER) || defined(OPT_INVADER_SOUND)
int runner_beep = 0;

#define RUNNER_HIT      1
#define RUNNER_MISS     2
#define INVADER_BEEP    3
#define INVADER_ERRBEEP 4

#endif

#ifdef OPT_WORD_SPACE
int wordSpace = 4; /* XXX 7 is too short */
int charSpace = 3;
int menuIndexWordSpace;
int menuIndexCharSpace;
#endif

#ifdef OPT_OFF_DELAY
int offDelay = 1; // off-delay disabled by default (Experimental)

int off_timer_ratio_dot = 50;     /* XXX extra percent (of dot) for dot */
int off_timer_ratio_dash = 150;   /* XXX extra percent (of dot) for dash */

#define MAX_DOT_RATIO  100
//#define MAX_LAM_RATIO  200
#define MAX_LAM_RATIO  100
#define MAX_DASH_RATIO 200
#ifdef OPT_REPEAT_ADD
#define MAX_REPEAT_ADD 100
#endif

int menuIndexOffDelay;
int menuIndexOffDelayDot;
int menuIndexOffDelayDash;

#ifdef OPT_REPEAT_ADD
int off_repeat_add = 10;
int menuIndexOffRepeatAdd = 0;
int repeatingL = 0;
int repeatingR = 0;
#endif
#endif

#ifdef OPT_ON_DELAY
int onDelay = 1; // on-delay disabled by default (Experimental)

int on_timer_ratio_dot = 50;     /* XXX extra percent (of dot) for dot */
int on_timer_ratio_dash = 150;   /* XXX extra percent (of dot) for dash */

int menuIndexOnDelay;
int menuIndexOnDelayDot;
int menuIndexOnDelayDash;

#endif

#ifdef OPT_LOOK_AHEAD_MEMORY
int lookAheadMemory = 1; // lookAheadMemory is disabled by default (Experimental)
int look_ahead_memory_ratio = 25; // XXX percent (of dot) for look ahead memory
int menuIndexLookAheadMemory;
int menuIndexLookAheadMemoryRatio;
#endif

#ifdef OPT_AVOID_CHATTERING
int avoidChattering = 1; // avoid chattering disabled by default (Experimental)
int avoidChatteringCount = 10; // Stable count

#define MAX_CHATTERING_COUNT 100

int menuIndexAvoidChattering;
int menuIndexAvoidChatteringCount;
#endif

#ifdef OPT_AUTO_KEYERTYPE
int autoKeyerType = 1; // auto keyer type disabled by default (Experimental)
#ifdef OPT_BETTER_IAMBIC_A
int autoKeyerTypeElekey = 0; // XXX IambicA, auto keyer type when elekey changed from straight key
#else
int autoKeyerTypeElekey = 2; // XXX Supr, auto keyer type when elekey changed from straight key
#endif
unsigned long auto_keyer_start = 0;
#define MAX_STRAIGHT_OR_PADDLE 3000 /* 3sec */
int menuIndexAutoKeyertype;
int menuIndexAutoKeyertypeElekey;
#endif

#ifdef OPT_VIBRATION
//#define VIBRATION_PERIOD  (durationDot / 3)
#define VIBRATION_PERIOD  20
unsigned long vibration_end = 0;
#endif

#ifdef OPT_INVADER_SOUND
int invaderSound = 1;
#endif

#ifdef OPT_BETTER_SRAND
int first_srand = 1;
#endif

#ifdef OPT_TONE_FREQ
int toneFreqIndex = 0; // must be adjust to toneFreq whenever changed
int menuIndexToneFreq;
#endif

// side tone
int sidetone = 1; // 0: disable, 1: enable
int defToneFreq[] = {
#ifdef OPT_TONE_FREQ
  220, 233, 247, 261, 277, 294, 311, 330, 349, 370, 392, 415,
#endif
  440, 466, 494, 523, 554, 587, 622, 659, 698, 740, 784, 831,
  880, 932, 988, 1047, 1109, 1175, 1245, 1319, 1397, 1480, 1568, 1661
};
#ifdef OPT_LCD_DISP
int menuIndexLcdDisp;
int lcdDisp = 1; // LCD display on/off flag
#endif
#ifdef OPT_TONE_FREQ
int toneFreq = 220; // side tone frequency 220Hz, A3
#else
int toneFreq = 440; // side tone frequency 440Hz, A4
#endif
int beepFreq = 880 * 2; // beep frequency 880Hz

#ifdef OPT_TWO_TONE
int twoTone = 0; // experimental
int toneTail = 0;
int durationTail = 0;
#define TONE_TAIL_RATIO 33
int menuIndexTwoTone;
#endif

#ifdef OPT_MEMORY_CLICK
static int memoryClick = 0;
int menuIndexMemoryClick;
#endif

// keyer selection
#define keyIambicA  0
#define keyIambicB  1
#define keySuper    2 // super keyer
#define keyStraight 3 // straight key
#define keyBug      4 // bug key
#ifdef OPT_BETTER_IAMBIC_A
int keyerType = keyIambicA;
#else
int keyerType = keySuper;
#endif
static int memory = -1;

char *keyName[] {
  "IbcA",
  "IbcB",
  "Supr",
  "Strt"
};

#ifdef OPT_ESP32

// I/O pins setting analog
#define pinVol      36
#define pinAudioIn  39
// I/O pins setting digital
#define pinPaddleL   4
#define pinPaddleR  16
#define pinBz       32 // audioOutPin
#ifdef OPT_VIBRATION
#define pinVb       33
#endif
#define pinLed      17
#define pinSw4      14
#define pinSw3      12
#define pinSw2      13
#define pinSw1      15

#else

// I/O pins setting analog
#define pinVol     A0
#define pinAudioIn A1
// I/O pins setting digital
#define pinPaddleL  6
#define pinPaddleR  7
#define pinBz       9 // audioOutPin
#define pinLed     13
#define pinSw4      2
#define pinSw3      3
#define pinSw2      4
#define pinSw1      5

#endif

int pinBtn[4] = {pinSw1, pinSw2, pinSw3, pinSw4};

// keyer language
#define langAbc   0
#define langWabun 1
int lang = langAbc;

#ifdef OPT_BETTER_IAMBIC_A
int paddleR_pressed = 0; // to check if re-pressed once after press detected
int paddleL_pressed = 0;
#endif
int lastPaddle = L; // memory key param
int endingChar = 0; // space after letter(1) or dit/dah(0)

static int pos_read = 0;
unsigned int nextCode = 0x01; // letter to send
unsigned int presetMessage[128]; // ruber stamp
int IsSendingPresetMessages;

// mark | space control
static unsigned long timeStart = 0;
static unsigned long timeStartSw[4];

// beacon cq
unsigned int defCqInterval = 2000;
static unsigned long timeCqInterval = 0;
static unsigned long timeStartCq = 0;
unsigned int defCqTimes = 10;
unsigned int CqTimes = defCqTimes;

// 8x2 LCD setting
FaBoLCDmini_AQM0802A lcd;
#define SPACE 0b00100000 // 32
const int colums = 8;
const int rows = 2;
int lcdIndex = 0;
int line1[colums];
int line2[colums];

// goertzel calculation param
float magnitude ;
int magnitudelimit = 100;
int magnitudelimit_low = 100;
int realstate = LOW;
int realstatebefore = LOW;
int filteredstate = LOW;
int filteredstatebefore = LOW;

#if 0 /* not used */
// The sampling frq will be 8928 on a 16 mhz
// without any prescaler etc
// because we need the tone in the center of the bins
// you can set the tone to 496, 558, 744 or 992
// then n the number of samples which give the bandwidth
// can be (8928 / tone) * 1 or 2 or 3 or 4 etc
// init is 8928/558 = 16 *4 = 64 samples
// try to take n = 96 or 128 ;o)
// 48 will give you a bandwidth around 186 hz
// 64 will give you a bandwidth around 140 hz
// 96 will give you a bandwidth around 94 hz
// 128 will give you a bandwidth around 70 hz
// BUT remember that high n take a lot of time
// so you have to find the compromice - i use 48
float coeff;
float Q1 = 0;
float Q2 = 0;
float sine;
float cosine;
float sampling_freq = 8928.0;
float target_freq = 558.0; // adjust for your needs see above
float n = 48.0; // if you change  her please change next line also
int testData[48];

// Noise Blanker time which shall be computed so this is initial
int nbtime = 6;  // ms noise blanker
#endif

long starttimehigh;
long highduration;
long lasthighduration;
#ifdef MOD_MISC
long hightimesavg = 60; /* to begin with moderate highduration */
#else
long hightimesavg;
#endif
long lowtimesavg;
long startttimelow;
long lowduration;
long laststarttime = 0;

char code = 1;//[20]; estimated code
int stop = LOW;
#ifdef MOD_MISC
#define INIT_DECODE_WPM 20
int wpm = INIT_DECODE_WPM; // initial wpm value of decoding
#else
int wpm;
#endif

// game setting
#define maxInvader 64
int restInvader = -1;
unsigned int presetInvader[maxInvader];
int gamePts = 0;
static int combo = 0;
int gameStarted = 0;

// setting menu
// ToDo: save param on EEPROM
char *setMenu[] {
  "Lang A/", // ABC or Wabun
  "Pdl Rev.", // paddle reverse
  "Key mode", // keyer mode
  "Wait 1:*", // dit dah wait
  "SideTone", // sidetone on, off / tone[Hz]
  "CQint:ms", // beacon interval
  "CQ times"  // beacon times
#ifdef OPT_LCD_DISP
  ,
  "LCD disp" // LCD display on/off flag
#endif
#ifdef OPT_TONE_FREQ
  ,
  "ToneFreq"  // sidetone frequency
#endif
#ifdef OPT_WORD_SPACE
  ,
  "Word Spc",  // word space dots
  "Char Spc"   // character space dots
#endif
#ifdef OPT_OFF_DELAY
  ,
  "OffDelay",  // off delay
  "OfDlyDot",  // off delay dot ratio
  "OfDlyDsh"   // off delay dash ratio
#ifdef OPT_REPEAT_ADD
  ,
  "OfRptAdd"   // off delay repeating add
#endif
#endif
#ifdef OPT_ON_DELAY
  ,
  "OnDelay",   // on delay
  "OnDlyDot",  // on delay dot ratio
  "OnDlyDsh"   // on delay dash ratio
#endif
#ifdef OPT_LOOK_AHEAD_MEMORY
  ,
  "LAMemory",  // look ahead memory
  "LAMratio"   // look ahead memory ratio
#endif
#ifdef OPT_TWO_TONE
  ,
  "Two-Tone"   // enable two tone sound
#endif
#ifdef OPT_MEMORY_CLICK
  ,
  "MemClick"   // memory click
#endif
#ifdef OPT_AVOID_CHATTERING
  ,
  "AvodChtr",  // to avoid chattering
  "StbleCnt"   // stable count
#endif
#ifdef OPT_AUTO_KEYERTYPE
  ,
  "AtoKyChg",  // auto keyer type change
  "AtoKyEle"   // auto keyer type when elekey
#endif
};

#define MENU_INDEX_EXTRA 7 // XXX the beginning of extra menu
#define MaxMenu (sizeof(setMenu) / sizeof(char *))

#ifdef OPT_PARAM_EEPROM

typedef struct PARAM_EEPROM {
  int cksum;
  int lang;
  int paddleRvs;
  int keyerType;
  float wait;
  int sidetone;
  unsigned int defCqInterval;
  unsigned int defCqTimes;
#ifdef OPT_LCD_DISP
  int lcdDisp;
#endif
#ifdef OPT_TONE_FREQ
  int toneFreq;
#endif
#ifdef OPT_WORD_SPACE
  int wordSpace;
  int charSpace;
#endif
#ifdef OPT_OFF_DELAY
  int offDelay;
  int off_timer_ratio_dot;
  int off_timer_ratio_dash;
#ifdef OPT_REPEAT_ADD
  int off_repeat_add;
#endif
#endif
#ifdef OPT_ON_DELAY
  int onDelay;
  int on_timer_ratio_dot;
  int on_timer_ratio_dash;
#endif
#ifdef OPT_LOOK_AHEAD_MEMORY
  int lookAheadMemory;
  int look_ahead_memory_ratio;
#endif
#ifdef OPT_TWO_TONE
  int twoTone;
#endif
#ifdef OPT_MEMORY_CLICK
  int memoryClick;
#endif
#ifdef OPT_AVOID_CHATTERING
  int avoidChattering;
  int avoidChatteringCount;
#endif
#ifdef OPT_AUTO_KEYERTYPE
  int autoKeyerType;
  int autoKeyerTypeElekey;
#endif
} param_eeprom_t;

#define PARAM_EEPROM_BASE (128 * 4)

int param_eeprom_load()
{
  param_eeprom_t param;
  byte *ptr;
  int cksum = 0;

#ifdef OPT_ESP32
  return 0;
#endif

  ptr = (byte *)&param;
  for (int i = 0 ; i < sizeof(param_eeprom_t) ; i++) {
    *ptr = EEPROM.read(PARAM_EEPROM_BASE + i);
    if (i >= sizeof(int)) { // excluding the first cksum
      cksum ^= *ptr;
      cksum <<= 1;
    }
    ptr++;
  }

  if (cksum == param.cksum) {
    lang = param.lang;
    paddleRvs = param.paddleRvs;
    keyerType = param.keyerType;
    wait = param.wait;
    sidetone = param.sidetone;
    defCqInterval = param.defCqInterval;
    defCqTimes = param.defCqTimes;
#ifdef OPT_LCD_DISP
    lcdDisp = param.lcdDisp;
#endif
#ifdef OPT_TONE_FREQ
    toneFreq = param.toneFreq;
#endif
#ifdef OPT_WORD_SPACE
    wordSpace = param.wordSpace;
    charSpace = param.charSpace;
#endif
#ifdef OPT_OFF_DELAY
    offDelay = param.offDelay;
    off_timer_ratio_dot = param.off_timer_ratio_dot;
    off_timer_ratio_dash = param.off_timer_ratio_dash;
#ifdef OPT_REPEAT_ADD
    off_repeat_add = param.off_repeat_add;
#endif
#endif
#ifdef OPT_ON_DELAY
    onDelay = param.onDelay;
    on_timer_ratio_dot = param.on_timer_ratio_dot;
    on_timer_ratio_dash = param.on_timer_ratio_dash;
#endif
#ifdef OPT_LOOK_AHEAD_MEMORY
    lookAheadMemory = param.lookAheadMemory;
    look_ahead_memory_ratio = param.look_ahead_memory_ratio;
#endif
#ifdef OPT_TWO_TONE
    twoTone = param.twoTone;
#endif
#ifdef OPT_MEMORY_CLICK
    memoryClick = param.memoryClick;
#endif
#ifdef OPT_AVOID_CHATTERING
    avoidChattering = param.avoidChattering;
    avoidChatteringCount = param.avoidChatteringCount;
#endif
#ifdef OPT_AUTO_KEYERTYPE
    autoKeyerType = param.autoKeyerType;
    autoKeyerTypeElekey = param.autoKeyerTypeElekey;
#endif

    return 0; // success
  }

  return -1; // return without setting params when fail
}

void param_eeprom_save()
{
  param_eeprom_t param;
  byte *ptr;
  int cksum = 0;

  param.lang = lang;
  param.paddleRvs = paddleRvs;
  param.keyerType = keyerType;
  param.wait = wait;
  param.sidetone = sidetone;
  param.defCqInterval = defCqInterval;
  param.defCqTimes = defCqTimes;
#ifdef OPT_LCD_DISP
  param.lcdDisp = lcdDisp;
#endif
#ifdef OPT_TONE_FREQ
  param.toneFreq = toneFreq;
#endif
#ifdef OPT_WORD_SPACE
  param.wordSpace = wordSpace;
  param.charSpace = charSpace;
#endif
#ifdef OPT_OFF_DELAY
  param.offDelay = offDelay;
  param.off_timer_ratio_dot = off_timer_ratio_dot;
  param.off_timer_ratio_dash = off_timer_ratio_dash;
#ifdef OPT_REPEAT_ADD
  param.off_repeat_add = off_repeat_add;
#endif
#endif
#ifdef OPT_ON_DELAY
  param.onDelay = onDelay;
  param.on_timer_ratio_dot = on_timer_ratio_dot;
  param.on_timer_ratio_dash = on_timer_ratio_dash;
#endif
#ifdef OPT_LOOK_AHEAD_MEMORY
  param.lookAheadMemory = lookAheadMemory;
  param.look_ahead_memory_ratio = look_ahead_memory_ratio;
#endif
#ifdef OPT_TWO_TONE
  param.twoTone = twoTone;
#endif
#ifdef OPT_MEMORY_CLICK
  param.memoryClick = memoryClick;
#endif
#ifdef OPT_AVOID_CHATTERING
  param.avoidChattering = avoidChattering;
  param.avoidChatteringCount = avoidChatteringCount;
#endif
#ifdef OPT_AUTO_KEYERTYPE
  param.autoKeyerType = autoKeyerType;
  param.autoKeyerTypeElekey = autoKeyerTypeElekey;
#endif

  ptr = (byte *)&param;
  for (int i = 0 ; i < sizeof(param_eeprom_t) ; i++) {
    if (i >= sizeof(int)) { // excluding the first cksum
      cksum ^= *ptr;
      cksum <<= 1;
    }
    ptr++;
  }
  param.cksum = cksum;

  ptr = (byte *)&param;
  for (int i = 0 ; i < sizeof(param_eeprom_t) ; i++) {
    EEPROM.write(PARAM_EEPROM_BASE + i, *ptr);
    ptr++;
  }
}

void tone_eeprom()
{
  buzzer(pinBz, 698, 100); // F4
  delay(100);
  buzzer(pinBz, 1397, 200); // F5
  delay(100);
}

#endif /* OPT_PARAM_EEPROM */

#ifdef OPT_TIMING_DEBUG
void serial_timing(char *s)
{
  Serial.print(millis());
  Serial.print(F(","));
  Serial.print(s);
  Serial.println();
}
#endif

// morse code
// dit=0, dah=1, start flag=1, 1char=1byte, max7 dit/dah
// example: "A" -> ".-" -> "0000 0101" -> 0x05
unsigned char BinAbc[] = {
  0b00000101, // A
  0b00011000, // B
  0b00011010, // C
  0b00001100, // D
  0b00000010, // E
  0b00010010, // F
  0b00001110, // G
  0b00010000, // H
  0b00000100, // I
  0b00010111, // J
  0b00001101, // K
  0b00010100, // L
  0b00000111, // M
  0b00000110, // N
  0b00001111, // O
  0b00010110, // P
  0b00011101, // Q
  0b00001010, // R
  0b00001000, // S
  0b00000011, // T
  0b00001001, // U
  0b00010001, // V
  0b00001011, // W
  0b00011001, // X
  0b00011011, // Y
  0b00011100, // Z
  0b00111111, // 0
  0b00101111, // 1
  0b00100111, // 2
  0b00100011, // 3
  0b00100001, // 4
  0b00100000, // 5
  0b00110000, // 6
  0b00111000, // 7
  0b00111100, // 8
  0b00111110, // 9
  0b01010101, // .
  0b01110011, // ,
  0b01001100, // ?
  0b00110010, // /
  0b00101010, // + [AR]
  0b00110001, // = [BT]
  0b01100001, // -
  0b01011110, // '
  0b01010010, // "
  0b01111000, // :
  0b00110110, // (
  0b01101101, // )
  0b01011010, // @
  0b01000101, // [VA]
  0b00101000  // [AS]
};

unsigned char CharAbc[] = {
  0b01000001, // A 0x41
  0b01000010, // B
  0b01000011, // C
  0b01000100, // D
  0b01000101, // E
  0b01000110, // F
  0b01000111, // G
  0b01001000, // H
  0b01001001, // I
  0b01001010, // J
  0b01001011, // K
  0b01001100, // L
  0b01001101, // M
  0b01001110, // N
  0b01001111, // O
  0b01010000, // P
  0b01010001, // Q
  0b01010010, // R
  0b01010011, // S
  0b01010100, // T
  0b01010101, // U
  0b01010110, // V
  0b01010111, // W
  0b01011000, // X
  0b01011001, // Y
  0b01011010, // Z
  0b00110000, // 0
  0b00110001, // 1
  0b00110010, // 2
  0b00110011, // 3
  0b00110100, // 4
  0b00110101, // 5
  0b00110110, // 6
  0b00110111, // 7
  0b00111000, // 8
  0b00111001, // 9
  0b00101110, // .
  0b00101100, // ,
  0b00111111, // ?
  0b00101111, // /
  0b00101011, // + [AR]
  0b00111101, // = [BT]
  0b00101101, // -
  0b00100111, // '
  0b00100010, // "
  0b00111010, // :
  0b00101000, // (
  0b00101001, // )
  0b01000000  // @
};

unsigned char BinKana[] = {
  0b00111011, // a
  0b00000101, // i
  0b00001001, // u
  0b00110111, // e
  0b00101000, // o
  0b00010100, // ka
  0b00110100, // ki
  0b00010001, // ku
  0b00011011, // ke
  0b00011111, // ko
  0b00110101, // sa
  0b00111010, // si
  0b00111101, // su
  0b00101110, // se
  0b00011110, // so
  0b00000110, // ta
  0b00010010, // ti
  0b00010110, // tu
  0b00101011, // te
  0b00100100, // to
  0b00001010, // na
  0b00011010, // ni
  0b00010000, // nu
  0b00011101, // ne
  0b00010011, // no
  0b00011000, // ha
  0b00111001, // hi
  0b00011100, // hu
  0b00000010, // he
  0b00001100, // ho
  0b00011001, // ma
  0b00100101, // mi
  0b00000011, // mu
  0b00110001, // me
  0b00110010, // mo
  0b00001011, // ya
  0b00110011, // yu
  0b00000111, // yo
  0b00001000, // ra
  0b00001110, // ri
  0b00110110, // ru
  0b00001111, // re
  0b00010101, // ro
  0b00001101, // wa
  0b00101001, // wi
  0b00101100, // we
  0b00010111, // wo
  0b00101010, // n
  0b00000100, // dakuten
  0b00100110, // handakuten
  0b00111111, // 0
  0b00101111, // 1
  0b00100111, // 2
  0b00100011, // 3
  0b00100001, // 4
  0b00100000, // 5
  0b00110000, // 6
  0b00111000, // 7
  0b00111100, // 8
  0b00111110, // 9
  0b00101101, // -
  0b01010101, // touten
  0b01010100, // tojikakko
  0b01101101, // (
  0b01010010, // )
  0b01100111, // hore
  0b00100010  // rata
};

unsigned char CharWabun[] = {
  0b10110001, // a
  0b10110010, // i
  0b10110011, // u
  0b10110100, // e
  0b10110101, // o
  0b10110110, // ka
  0b10110111, // ki
  0b10111000, // ku
  0b10111001, // ke
  0b10111010, // ko
  0b10111011, // sa
  0b10111100, // si
  0b10111101, // su
  0b10111110, // se
  0b10111111, // so
  0b11000000, // ta
  0b11000001, // ti
  0b11000010, // tu
  0b11000011, // te
  0b11000100, // to
  0b11000101, // na
  0b11000110, // ni
  0b11000111, // nu
  0b11001000, // ne
  0b11001001, // no
  0b11001010, // ha
  0b11001011, // hi
  0b11001100, // hu
  0b11001101, // he
  0b11001110, // ho
  0b11001111, // ma
  0b11010000, // mi
  0b11010001, // mu
  0b11010010, // me
  0b11010011, // mo
  0b11010100, // ya
  0b11010101, // yu
  0b11010110, // yo
  0b11010111, // ra
  0b11011000, // ri
  0b11011001, // ru
  0b11011010, // re
  0b11011011, // ro
  0b11011100, // wa
  0b00000100, // wi
  0b00000101, // we
  0b10100110, // wo
  0b11011101, // n
  0b11011110, // dakuten
  0b11011111, // handakuten
  0b00110000, // 0
  0b00110001, // 1
  0b00110010, // 2
  0b00110011, // 3
  0b00110100, // 4
  0b00110101, // 5
  0b00110110, // 6
  0b00110111, // 7
  0b00111000, // 8
  0b00111001, // 9
  0b00101101, // -
  0b10100100, // touten
  0b10100011, // tojikakko
  0b00101000, // (
  0b00101001  // )
};

// original characters
byte invader1L[8] = {
  0b00100,
  0b00010,
  0b00111,
  0b01101,
  0b11111,
  0b10111,
  0b10100,
  0b00011
};

byte invader1R[8] = {
  0b00100,
  0b01000,
  0b11100,
  0b10110,
  0b11111,
  0b11101,
  0b00101,
  0b11000
};

byte invader2L[8] = {
  0b00100,
  0b10010,
  0b10111,
  0b11101,
  0b01111,
  0b01111,
  0b00100,
  0b01000
};

byte invader2R[8] = {
  0b00100,
  0b01001,
  0b11101,
  0b10111,
  0b11110,
  0b11110,
  0b00100,
  0b00010
};

byte wi[8] = {
  0b00010,
  0b01111,
  0b01010,
  0b01010,
  0b11111,
  0b00010,
  0b00010
};

byte we[8] = {
  0b00000,
  0b11111,
  0b00101,
  0b00110,
  0b00100,
  0b11111,
  0b00000
};

#ifdef OPT_ESP32

void startVibration()
{
  ledcWriteTone(2, 120);
}

void stopVibration()
{
  ledcWrite(2, 0);
}

void buzzer(int dummyPin, int freq, int duration)
{
  ledcWriteTone(1, freq);
  delay(duration);
  ledcWrite(1, 0);
}

void tone(int dummyPin, int freq)
{
  ledcWriteTone(1, freq);
#ifdef OPT_VIBRATION
  startVibration();
  vibration_end = millis() + VIBRATION_PERIOD;
#endif
}

void noTone(int dummyPin)
{
  ledcWrite(1, 0);
  //stopVibration();
}

#else

void buzzer(int pin, int freq, int duration)
{
  tone(pin, freq, duration);
}

#endif

void setup() {

#ifdef OPT_ESP32
  /* tone setup */
  ledcSetup(1, 12000, 8); // Channel 1, Hz, 8bit
  ledcAttachPin(pinBz, 1); // Channel 1

#ifdef OPT_VIBRATION
  /* vibration setup */
  ledcSetup(2, 12000, 8); // Channel 2, Hz, 8bit
  ledcAttachPin(pinVb, 2); // Channel 2
#endif
#endif

#if 0 /* not used */
  // The basic goertzel calculation
  int  k;
  float omega;
  k = (int) (0.5 + ((n * target_freq) / sampling_freq));
  omega = (2.0 * PI * k) / n;
  sine = sin(omega);
  cosine = cos(omega);
  coeff = 2.0 * cosine;
#endif

  Serial.begin(115200);
  Serial.println(F("morse-shield for Arduino UNO"));
  Serial.println(F("Designed by nksg"));
  Serial.print(F("20"));
  Serial.println(date);

  // Create the random seed
  randomSeed(analogRead(pinVol));

  // I/O pins setup
  for (int i = 0; i < 4; i++) pinMode(pinBtn[i], INPUT_PULLUP);
  pinMode(pinPaddleL, INPUT_PULLUP);
  pinMode(pinPaddleR, INPUT_PULLUP);
  pinMode(pinBz, OUTPUT);
  pinMode(pinLed, OUTPUT);

#ifdef MOD_MISC
  /* turn off led */
  digitalWrite(pinLed, LOW);
#endif

  // clear 4x switchs hold time
  for (int i = 0; i < 4; i++) timeStartSw[i] = 0;

  // Set up the LCD
  lcd.begin();
  // Print messages to the LCD
#if 1
  lcd.home();
  lcd.print(F("WakuWaku"));
  lcd.setCursor(0, 1);
  lcd.print(mod_date); // yy/mm/dd
  delay(2000);
#endif
  lcd.home();
  lcd.print(F("MorseKey"));
  lcd.setCursor(0, 1);
  lcd.print(F("JG5CBR/6"));
  delay(1000);
  lcd.setCursor(0, 1);
  lcd.print(date); // yy/mm/dd
  delay(1000);
  lcd.setCursor(0, 1);
  lcd.print(F("        "));

  // Create a original character
  lcd.createChar(0, invader1L);
  lcd.createChar(1, invader1R);
  lcd.createChar(2, invader2L);
  lcd.createChar(3, invader2R);
  lcd.createChar(4, wi);
  lcd.createChar(5, we);

  // Clear both lines of the LCD
  for (int i = 0; i < colums; i++) {
    line1[i] = SPACE;
    line2[i] = SPACE;
  }

#if 1 /* XXX dynamic menu indexing */
  /*
   * set menuIndex dynamically
   */
  int menuIndex = MENU_INDEX_EXTRA;
#ifdef OPT_LCD_DISP
  menuIndexLcdDisp = menuIndex++;
#endif
#ifdef OPT_TONE_FREQ
  menuIndexToneFreq = menuIndex++;
#endif
#ifdef OPT_WORD_SPACE
  menuIndexWordSpace = menuIndex++;
  menuIndexCharSpace = menuIndex++;
#endif
#ifdef OPT_OFF_DELAY
  menuIndexOffDelay = menuIndex++;
  menuIndexOffDelayDot = menuIndex++;
  menuIndexOffDelayDash = menuIndex++;
#ifdef OPT_REPEAT_ADD
  menuIndexOffRepeatAdd = menuIndex++;
#endif
#endif
#ifdef OPT_ON_DELAY
  menuIndexOnDelay = menuIndex++;
  menuIndexOnDelayDot = menuIndex++;
  menuIndexOnDelayDash = menuIndex++;
#endif
#ifdef OPT_LOOK_AHEAD_MEMORY
  menuIndexLookAheadMemory = menuIndex++;
  menuIndexLookAheadMemoryRatio = menuIndex++;
#endif
#ifdef OPT_TWO_TONE
  menuIndexTwoTone = menuIndex++;
#endif
#ifdef OPT_MEMORY_CLICK
  menuIndexMemoryClick = menuIndex++;
#endif
#ifdef OPT_AVOID_CHATTERING
  menuIndexAvoidChattering = menuIndex++;
  menuIndexAvoidChatteringCount = menuIndex++;
#endif
#ifdef OPT_AUTO_KEYERTYPE
  menuIndexAutoKeyertype = menuIndex++;
  menuIndexAutoKeyertypeElekey = menuIndex++;
#endif
#endif /* 1 */

#ifdef OPT_PARAM_EEPROM
  if (digitalRead(pinSw4) == LOW) {
    /*
     * XXX push pinSw4 in order not to load EEPROM
     */
    tone_eeprom();
  } else {
    param_eeprom_load();
  }

  // reset toneFreqIndex (no matter which result returned)
  for (int i = 0 ; i < (sizeof(defToneFreq) / sizeof(int)) ; i++) {
    toneFreqIndex = 0;
    if (toneFreq <= defToneFreq[i]) {
      toneFreqIndex = i;
      break;
    }
  }
#endif
}

void loop() {
  // read Potentiometer value and set new WPM
  readPotValue();

  // read Paddle LR status
  readPaddleValue();

  // read Button status
  readButtonValue();

  // Put a new character when the keyer is waiting
  if (durationMark == 0 && durationSpace == 0) {
#ifndef OPT_NO_SERIAL
#ifdef OPT_LCD_DISP
   if (lcdDisp)
#endif
    readFromSerial();
#endif
    setNextChar();
  }

  // invader game
  morseInvader();

  // Messages from preset or keyboard
  if (pos_read > 0) return;

  // goertzel Calculation
  goertzel();
}

#if 0 /* not used */
void getMorseTone() {
  // The basic where we get the tone
  for (char index = 0; index < n; index++) {
    testData[index] = analogRead(pinAudioIn);
  }
  for (char index = 0; index < n; index++) {
    float Q0;
    Q0 = coeff * Q1 - Q2 + (float) testData[index];
    Q2 = Q1;
    Q1 = Q0;
  }
  // we do only need the real part
  float magnitudeSquared = (Q1 * Q1) + (Q2 * Q2) - Q1 * Q2 * coeff;
  magnitude = sqrt(magnitudeSquared);
  Q2 = 0;
  Q1 = 0;

  // here you can measure magnitude for setup..
  //Serial.println(magnitude);

  // here we will try to set the magnitude limit automatic
  if (magnitude > magnitudelimit_low) {
    // moving average filter
    magnitudelimit = (magnitudelimit + ((magnitude - magnitudelimit) / 6));
  }

  if (magnitudelimit < magnitudelimit_low)
    magnitudelimit = magnitudelimit_low;

  // now we check for the magnitude //
  // just to have some space up
  if (magnitude > magnitudelimit * 0.6)
    realstate = HIGH;
  else
    realstate = LOW;

  // here we clean up the state with a noise blanker
  if (realstate != realstatebefore) {
    laststarttime = millis();
  }
  if ((millis() - laststarttime) >= nbtime) {
    if (realstate != filteredstate) {
      filteredstate = realstate;
    }
  }
}
#endif

void goertzel() {
  //getMorseTone();
  filteredstate = key; // digitalRead(13);

  // Then we do want to have some durations on high and low
  if (filteredstate != filteredstatebefore) {
    if (filteredstate == HIGH) { /* key pushed */
      starttimehigh = millis();
      lowduration = (millis() - startttimelow);
#ifdef OPT_BETTER_DECODE
      stop = HIGH;
#endif
    }

    if (filteredstate == LOW) { /* key released */
      startttimelow = millis();
#ifdef OPT_BETTER_DECODE
      stop = LOW;
#endif
      highduration = (millis() - starttimehigh);
#ifdef OPT_BETTER_DECODE
      if ((highduration * 2) <= hightimesavg) {
        hightimesavg = highduration;  // if speed increased fast ..
      } else if (highduration < (2 * hightimesavg) && hightimesavg < (2 * highduration)) {
        hightimesavg = (highduration + 3 * hightimesavg) / 4; // now we know avg dit time (rolling 4 avg)
      } else if (highduration >= (2 * hightimesavg) && highduration < (4 * hightimesavg)) {
        hightimesavg = (highduration / wait + 3 * hightimesavg) / 4; // now we know avg dit time (rolling 4 avg)
      } else
#else
      if (highduration < (2 * hightimesavg) || hightimesavg == 0) {
        hightimesavg = (highduration + hightimesavg + hightimesavg) / 3; // now we know avg dit time ( rolling 3 avg)
      }
#endif
      if (highduration > (5 * hightimesavg) ) {
#ifdef OPT_BETTER_DECODE
        hightimesavg = highduration;  // if speed decreased fast ..
#else
        hightimesavg = highduration + hightimesavg;   // if speed decrease fast ..
#endif
      }
    }
  }

  // now we will check which kind of baud we have - dit or dah
  // and what kind of pause we do have 1 - 3 or 7 pause
  // we think that hightimeavg = 1 bit
  if (filteredstate != filteredstatebefore) {
#ifdef OPT_BETTER_DECODE
    if (filteredstate == LOW && (unsigned int)code < 0b10000000) { // we did end a HIGH
#else
    stop = LOW;
    if (filteredstate == LOW && code < 0b10000000) { // we did end a HIGH
#endif
      if (highduration < (hightimesavg * 2) && highduration > (hightimesavg * 0.6)) { /// 0.6 filter out false dits
        code = code << 1;//strcat(code, ".");
#ifdef SERIAL_PRINT_DIT_DAH
        Serial.print(F("."));
#endif
      }
#ifdef OPT_BETTER_DECODE
      if (highduration >= (hightimesavg * 2) && highduration < (hightimesavg * 6)) {
#else
      if (highduration > (hightimesavg * 2) && highduration < (hightimesavg * 6)) {
#endif
        code = code << 1;
        code += 1;
        //strcat(code, "-");
#ifdef SERIAL_PRINT_DIT_DAH
        Serial.print(F("-"));
#endif
        wpm = (wpm + (1200 / ((highduration) / 3))) / 2; // the most precise we can do ;o)
      }
    }
#if defined(OPT_RUNNER) && defined(OPT_DECODE_INVALID_IMMEDIATELY)
    else if (running && filteredstate == LOW) {
	// invoke decode expecting '_' during running to notify invalid keying (typically connecting more than two characters) immediately
#ifdef OPT_DECODE_DEBUG
	Serial.print(F("(1)"));
#endif
	decode();
	code = 1;
    }
#endif

    if (filteredstate == HIGH) { // we did end a LOW

#ifdef OPT_BETTER_DECODE
      float lacktime = 0.9; // a bit less pause when low speed under 25wpm
#else
      float lacktime = 1;
#endif
      if (wpm > 25)lacktime = 1.0; ///  when high speeds we have to have a little more pause before new letter or new word
      if (wpm > 30)lacktime = 1.2;
      if (wpm > 35)lacktime = 1.5;

      if (lowduration > (hightimesavg * (2 * lacktime)) && lowduration < hightimesavg * (5 * lacktime)) { // letter space
#ifdef OPT_DECODE_DEBUG
        Serial.print(F("(2)"));
#endif
        decode();
        code = 1;
#ifdef SERIAL_PRINT_DIT_DAH
        Serial.print(F("/"));
#endif
      }
      if (lowduration >= hightimesavg * (5 * lacktime)) { // word space
#ifdef OPT_DECODE_DEBUG
        Serial.print(F("(3)"));
#endif
        decode();
        code = 1;
#ifdef OPT_RUNNER
       if (!running) // to omit word space during running
#endif
        printascii(SPACE);
#ifdef SERIAL_PRINT_DIT_DAH
        Serial.println();
#endif
      }
    }
  }

  // write if no more letters
#ifdef OPT_BETTER_DECODE
  if (stop == LOW && (millis() - startttimelow) >= (hightimesavg * 6)) {
#else
  if ((millis() - startttimelow) >= (highduration * 6) && stop == LOW) {
#endif
#ifdef OPT_DECODE_DEBUG
    Serial.print(F("(4)"));
#endif
    decode();
    code = 1; //code[0] = '\0';
    stop = HIGH;
  }

  // we will turn on and off the LED and the speaker
  /*
    if (filteredstate == HIGH) {
      digitalWrite(pinLed, HIGH);
      tone(audioOutPin, target_freq);
    }
    else {
      digitalWrite(pinLed, LOW);
      noTone(audioOutPin);
    }
  */
  // the end of main loop clean up//
#ifdef OPT_RUNNER
 if (!running) // to keep lcd space as much as possible during running for verification purpose
#endif
  updateinfolinelcd();
  realstatebefore = realstate;
  lasthighduration = highduration;
  filteredstatebefore = filteredstate;
}

// Read Potentiometer value and set new WPM
void readPotValue() {
#ifdef MOD_MISC
#define INIT_POT_VAL -11 // to update the first Wpm value read from potentio meter
  static int potValueOld = INIT_POT_VAL;
#else
  static int potValueOld = 0;
#endif
  int potStep = 10;
  int potValue = analogRead(pinVol);

  if (potValue - potValueOld > potStep || potValue - potValueOld < -potStep) {
#ifdef OPT_ESP32
    int wpm = map(potValue, 0, 4095 - potStep, MIN_WPM, MAX_WPM);
#else
    int wpm = map(potValue, 0, 1023 - potStep, MIN_WPM, MAX_WPM);
#endif
#ifdef MOD_MISC
    if (wpm != Wpm || potValueOld == INIT_POT_VAL) setNewWpm(wpm);
#else
    if (wpm != Wpm) setNewWpm(wpm);
#endif
    
#ifdef DEBUG
    Serial.print(F("pot(0-1023)="));
    Serial.println(potValue);
#endif
    potValueOld = potValue;
  }
}

void setNewWpm(int wpm) {
  Wpm = wpm;
  durationDot = (60.0 * 1000) / (Wpm * 50);
#ifdef OPT_RUNNER
  runner_accel = 0; // reset acceleration when Wpm is modified manually
#endif

#ifndef OPT_NO_SERIAL
  Serial.print(F("WPM="));
  Serial.println(Wpm);
#ifdef DEBUG
  Serial.print(F("duration dot(ms)="));
  Serial.println(durationDot);
#endif
#endif

  // Print new WPM to the LCD
  lcd.home();
  if (Wpm < 10) lcd.print(F("0"));
  lcd.print(Wpm);
  lcd.print(F("WPM"));
  lcd.print(F("  "));
  if (lang == langAbc) {
    lcd.print(F("A"));
  } else if (lang == langWabun) {
    lcd.write(0b10110001); // wabun
  }
}

void beep() {
  buzzer(pinBz, beepFreq, 20);
}

void errBeep() {
  buzzer(pinBz, 220, 20);
}

#ifdef OPT_RUNNER
void tone_runner_mode()
{
  buzzer(pinBz, 349, 100); // F3
  delay(100);
  buzzer(pinBz, 698, 100); // F4
  delay(100);
}

void tone_hit()
{
  //buzzer(pinBz, 262, 100); // C3
  //buzzer(pinBz, 698, 100); // F3
  buzzer(pinBz, 392, 100); // G3
  //buzzer(pinBz, 698, 100); // F4
  delay(100);
  //buzzer(pinBz, 523, 100); // C4
  //buzzer(pinBz, 698, 100); // F4
  buzzer(pinBz, 784, 100); // G4
  //buzzer(pinBz, 1397, 200); // F5

  delay(100);
}

void tone_miss()
{
  buzzer(pinBz, 220, 100); // A2
  delay(100);
  buzzer(pinBz, 110, 200); // A1
  delay(100);
}
#endif

#ifdef OPT_INVADER_SOUND
void tone_invader_start()
{
  buzzer(pinBz, 880, 100); // A4
  delay(100);
  buzzer(pinBz, 1760, 100); // A5
  delay(100);
}

void tone_invader_clear()
{
  buzzer(pinBz, 784, 100); // G4
  delay(100);
  buzzer(pinBz, 1047, 100); // C5
  delay(100);
  buzzer(pinBz, 1568, 100); // G5
  delay(100);
}
#endif

#ifdef OPT_MEMORY_CLICK
static unsigned long timeEnd = 0;

#define MEMORY_CLICK_RATIO 50 /* % */

void memory_click_start()
{
  if (timeEnd == 0) {
    tone(pinBz, beepFreq * 3);
    //tone(pinBz, toneFreq);
    //tone(pinBz, beepFreq * 2);
    //tone(pinBz, toneFreq * 2);
    timeEnd = millis() + durationDot * MEMORY_CLICK_RATIO / 100;
    //timeEnd = millis() + durationMark - durationDot * MEMORY_CLICK_RATIO / 100;
    if (timeEnd == 0) // zero means not started
      timeEnd = 1;
  }
}

void memory_click_stop()
{
  if (durationMark != 0) {
    if (sidetone)
      tone(pinBz, toneFreq); // side tone
      //tone(pinBz, beepFreq * 3);
    else
      noTone(pinBz);
  } else {
    noTone(pinBz);
  }
  timeEnd = 0;
}
#endif

#ifdef OPT_OFF_DELAY
int paddleL_prev = HIGH;
int paddleR_prev = HIGH;
unsigned long paddleL_highStart = 0;
unsigned long paddleR_highStart = 0;

int paddleR_off_delay; /* off delay ms */
int paddleL_off_delay; /* off delay ms */
#endif

#ifdef OPT_ON_DELAY
int paddleL_on_prev = HIGH;
int paddleR_on_prev = HIGH;
unsigned long paddleL_on_highStart = 0;
unsigned long paddleR_on_highStart = 0;

int paddleR_on_delay; /* on delay ms */
int paddleL_on_delay; /* on delay ms */
#endif

#ifdef OPT_BETTER_IAMBIC_A
int paddleL_iambic_prev = HIGH;
int paddleR_iambic_prev = HIGH;
#endif

#ifdef OPT_AVOID_CHATTERING
int paddleL_stable = HIGH;
int paddleR_stable = HIGH;
#ifdef OPT_AVOID_CHATTERING_WATCH
int paddleL_watching = 0;
int paddleR_watching = 0;
#endif
int paddleL_trans = 0;
int paddleR_trans = 0;

//#define AVG_THRESHOLD 0.25
#define AVG_THRESHOLD 0.125
//#define AVG_THRESHOLD 0.5

float paddleL_avg = 0;
float paddleR_avg = 0;
#endif

#ifdef OPT_RUNNER
unsigned int runner_call[8];
int runner_call_len = 0;
int runner_ptr = 0;

void reset_durationDot()
{
  if (Wpm != 0)
    durationDot = (60.0 * 1000) / (Wpm * 50); // reset in case accelerated
}

void accel_durationDot()
{
  if (Wpm != 0)
    durationDot = (60.0 * 1000) / (Wpm * 50) * 100 / (100 + 5 * runner_accel);
}

void runner_stop()
{
  running = 0; // stop running
  runner_repeat = 0;
  reset_durationDot();
}

void print_runner_count()
{
  printascii(SPACE);
  printascii('(');

  if (runner_accel >= 10)
    printascii('0' + (runner_accel % 100) / 10);
  printascii('0' + (runner_accel % 10));

  printascii('/');

  if (runner_count >= 100)
    printascii('0' + (runner_count % 1000) / 100);
  if (runner_count >= 10)
    printascii('0' + (runner_count % 100) / 10);
  printascii('0' + (runner_count % 10));

  printascii(')');
  printascii(SPACE);
}
#endif

void readPaddleValue() {
  int paddleL = digitalRead(pinPaddleL);
  int paddleR = digitalRead(pinPaddleR);
#ifdef OPT_TIMING_DEBUG
  static int phys_paddleL_prev = HIGH;
  static int phys_paddleR_prev = HIGH;
  static int logi_paddleL_prev = HIGH;
  static int logi_paddleR_prev = HIGH;

  if (phys_paddleL_prev != paddleL) {
    if (paddleL == LOW) {
      serial_timing("pL-D,,,");
    } else {
      serial_timing("pL-U,,,");
    }
    phys_paddleL_prev = paddleL;
  }
  if (phys_paddleR_prev != paddleR) {
    if (paddleR == LOW) {
      serial_timing(",pR-D,,");
    } else {
      serial_timing(",pR-U,,");
    }
    phys_paddleR_prev = paddleR;
  }
#endif

#ifdef OPT_AUTO_KEYERTYPE
  if (autoKeyerType) {
    /*
     * automatic change between keyStraight and keySuper
     */
    if (keyerType != keyStraight) {
      if (paddleL == HIGH && paddleR == LOW) {
	if (auto_keyer_start + MAX_STRAIGHT_OR_PADDLE <= millis()) {
	  keyerType = keyStraight;
          auto_keyer_start = millis();
	}
      } else {
        auto_keyer_start = millis();
      }
    }
    if (keyerType == keyStraight) {
      if (paddleL == HIGH && paddleR == HIGH) {
	if (auto_keyer_start + MAX_STRAIGHT_OR_PADDLE <= millis()) {
	  keyerType = autoKeyerTypeElekey;
          auto_keyer_start = millis();
	}
      } else {
        auto_keyer_start = millis();
      }
    }
  }
#endif

#ifdef OPT_AVOID_CHATTERING
  if (avoidChattering) {
    float weight = 1.0 / avoidChatteringCount;

    /*
     * avoid chattering, paddle R
     */
#ifdef OPT_AVOID_CHATTERING_WATCH
    if (paddleR_watching == 0 && (paddleR_stable != paddleR))
      paddleR_watching = avoidChatteringCount;
#endif

    if (paddleR == LOW)
        paddleR_avg += paddleR_avg * (/*1.0*/ - weight) /* + 0 * weight */;
    else
        paddleR_avg += paddleR_avg * (/*1.0*/ - weight) + /* 1 * */ weight;

#ifdef OPT_AVOID_CHATTERING_WATCH
    if (paddleR_watching > 0) {
      paddleR_watching--;
      if (paddleR_watching == 0) {
        if (paddleR_avg < 0.5)
            paddleR_stable = LOW;
        if (paddleR_avg >= 0.5)
            paddleR_stable = HIGH;
        paddleR = paddleR_stable;
      }
    }
#endif

    if (paddleR_stable == HIGH && paddleR_avg < AVG_THRESHOLD)
        paddleR_stable = LOW;
    if (paddleR_stable == LOW && paddleR_avg > (1.0 - AVG_THRESHOLD))
        paddleR_stable = HIGH;
    paddleR = paddleR_stable;

    /*
     * avoid chattering, paddle L
     */
#ifdef OPT_AVOID_CHATTERING_WATCH
    if (paddleL_watching == 0 && (paddleL_stable != paddleL))
      paddleL_watching = avoidChatteringCount;
#endif

    if (paddleL == LOW)
        paddleL_avg += paddleL_avg * (/*1.0*/ - weight) /* + 0 * weight */;
    else
        paddleL_avg += paddleL_avg * (/*1.0*/ - weight) + /* 1 * */ weight;

#ifdef OPT_AVOID_CHATTERING_WATCH
    if (paddleL_watching > 0) {
      paddleL_watching--;
      if (paddleL_watching == 0) {
        if (paddleL_stable == HIGH && paddleL_avg < 0.5)
            paddleL_stable = LOW;
        if (paddleL_stable == LOW && paddleL_avg >= 0.5)
            paddleL_stable = HIGH;
        paddleL = paddleL_stable;
      }
    }
#endif

    if (paddleL_stable == HIGH && paddleL_avg < AVG_THRESHOLD)
        paddleL_stable = LOW;
    if (paddleL_stable == LOW && paddleL_avg > (1.0 - AVG_THRESHOLD))
        paddleL_stable = HIGH;
    paddleL = paddleL_stable;

#if 0
    /*
     * avoid chattering, paddle R
     */
    if (paddleR == LOW && paddleR_stable == HIGH) {
      if (++paddleR_trans >= avoidChatteringCount) {
	paddleR_stable = LOW;
	paddleR_trans = 0;
      } else {
	/* stays HIGH until stable count reach */
	paddleR = HIGH;
      }
    } else if (paddleR == HIGH && paddleR_stable == LOW) {
      if (--paddleR_trans <= -avoidChatteringCount) {
	paddleR_stable = HIGH;
	paddleR_trans = 0;
      } else {
	/* stays LOW until stable count reach */
	paddleR = LOW;
      }
    }

    /*
     * avoid chattering, paddle L
     */
    if (paddleL == LOW && paddleL_stable == HIGH) {
      if (++paddleL_trans >= avoidChatteringCount) {
	paddleL_stable = LOW;
	paddleL_trans = 0;
      } else {
	/* stays HIGH until stable count reach */
	paddleL = HIGH;
      }
    } else if (paddleL == HIGH && paddleL_stable == LOW) {
      if (--paddleL_trans <= -avoidChatteringCount) {
	paddleL_stable = HIGH;
	paddleL_trans = 0;
      } else {
	/* stays LOW until stable count reach */
	paddleL = LOW;
      }
    }
#endif

  }
#endif /* OPT_AVOID_CHATTERING */

#ifdef OPT_OFF_DELAY
  /*
   * off delay is only for electric keyer. (i.e., not incl. straight key)
   */
  if (offDelay && keyerType != keyStraight) {
    /*
     * off delay, paddle R
     */
    if (paddleR == LOW) {
      /* immediately active, clear off delay timer */
      paddleR_prev = LOW;
      paddleR_highStart = 0;
    } else if (/*paddleR == HIGH &&*/ paddleR_prev == LOW) {
      /* R's falling edge */
      paddleR_prev = HIGH;
      paddleR_highStart = millis();
      if (paddleRvs) {
        /* reversed to DOT */
#ifdef OPT_REPEAT_ADD
        paddleR_off_delay = durationDot * (off_timer_ratio_dot + off_repeat_add * repeatingR) / 100;
        repeatingR = 0;
#else
        paddleR_off_delay = durationDot * off_timer_ratio_dot / 100;
#endif
      } else {
        /* normal DASH */
#ifdef OPT_REPEAT_ADD
        paddleR_off_delay = durationDot * (off_timer_ratio_dash + off_repeat_add * repeatingR) / 100;
        repeatingR = 0;
#else
        paddleR_off_delay = durationDot * off_timer_ratio_dash / 100;
#endif
      }
      /* paddleR stays active during off delay timer */
      paddleR = LOW;
    } else if (/*paddleR == HIGH && paddleR_prev != LOW &&*/ paddleR_highStart != 0) {
      if ((millis() - paddleR_highStart) < paddleR_off_delay) {
        /* paddleR stays active during off delay timer */
        paddleR = LOW;
      } else {
        /* off delay expired */
        paddleR_highStart = 0;
      }
    }

    /*
     * off delay, paddle L
     */
    if (paddleL == LOW) {
      /* immediately active, clear off delay timer */
      paddleL_prev = LOW;
      paddleL_highStart = 0;
    } else if (/*paddleL == HIGH &&*/ paddleL_prev == LOW) {
      /* L's falling edge */
      paddleL_prev = HIGH;
      paddleL_highStart = millis();
      if (paddleRvs) {
        /* reversed to DASH */
#ifdef OPT_REPEAT_ADD
        paddleL_off_delay = durationDot * (off_timer_ratio_dash + off_repeat_add * repeatingL) / 100;
        repeatingL = 0;
#else
        paddleL_off_delay = durationDot * off_timer_ratio_dash / 100;
#endif
      } else {
        /* normal DOT */
#ifdef OPT_REPEAT_ADD
        paddleL_off_delay = durationDot * (off_timer_ratio_dot + off_repeat_add * repeatingL) / 100;
        repeatingL = 0;
#else
        paddleL_off_delay = durationDot * off_timer_ratio_dot / 100;
#endif
      }
      /* paddleL stays active during off delay timer */
      paddleL = LOW;
    } else if (/*paddleL == HIGH && paddleL_prev != LOW &&*/ paddleL_highStart != 0) {
      if ((millis() - paddleL_highStart) < paddleL_off_delay) {
        /* paddleL stays active during off delay timer */
        paddleL = LOW;
      } else {
        /* off delay expired */
        paddleL_highStart = 0;
      }
    }

  } /* (offDelay && keyerType != keyStraight) */
#endif

/**************************************/
#ifdef OPT_ON_DELAY
  /*
   * on delay is only for electric keyer. (i.e., not incl. straight key)
   */
  if (onDelay && keyerType != keyStraight) {
    /*
     * on delay, paddle R
     */
    if (paddleR == HIGH) {
      /* immediately active, clear on delay timer */
      paddleR_on_prev = HIGH;
      paddleR_on_highStart = 0;
    } else if (/*paddleR == LOW &&*/ paddleR_on_prev == HIGH) {
      /* R's rising edge */
      paddleR_on_prev = LOW;
      paddleR_on_highStart = millis();
      if (paddleRvs) {
        /* reversed to DOT */
#ifdef OPT_REPEAT_ADD
        paddleR_on_delay = durationDot * (on_timer_ratio_dot + on_repeat_add * repeatingR) / 100;
        repeatingR = 0;
#else
        paddleR_on_delay = durationDot * on_timer_ratio_dot / 100;
#endif
      } else {
        /* normal DASH */
#ifdef OPT_REPEAT_ADD
        paddleR_on_delay = durationDot * (on_timer_ratio_dash + on_repeat_add * repeatingR) / 100;
        repeatingR = 0;
#else
        paddleR_on_delay = durationDot * on_timer_ratio_dash / 100;
#endif
      }
      /* paddleR stays inactive during on delay timer */
      paddleR = HIGH;
    } else if (/*paddleR == LOW && paddleR_on_prev != HIGH &&*/ paddleR_on_highStart != 0) {
      if ((millis() - paddleR_on_highStart) < paddleR_on_delay) {
        /* paddleR stays inactive during on delay timer */
        paddleR = HIGH;
      } else {
        /* on delay expired */
        paddleR_on_highStart = 0;
      }
    }

    /*
     * on delay, paddle L
     */
    if (paddleL == HIGH) {
      /* immediately active, clear on delay timer */
      paddleL_on_prev = HIGH;
      paddleL_on_highStart = 0;
    } else if (/*paddleL == LOW &&*/ paddleL_on_prev == HIGH) {
      /* L's rising edge */
      paddleL_on_prev = LOW;
      paddleL_on_highStart = millis();
      if (paddleRvs) {
        /* reversed to DASH */
#ifdef OPT_REPEAT_ADD
        paddleL_on_delay = durationDot * (on_timer_ratio_dash + on_repeat_add * repeatingL) / 100;
        repeatingL = 0;
#else
        paddleL_on_delay = durationDot * on_timer_ratio_dash / 100;
#endif
      } else {
        /* normal DOT */
#ifdef OPT_REPEAT_ADD
        paddleL_on_delay = durationDot * (on_timer_ratio_dot + on_repeat_add * repeatingL) / 100;
        repeatingL = 0;
#else
        paddleL_on_delay = durationDot * on_timer_ratio_dot / 100;
#endif
      }
      /* paddleL stays inactive during on delay timer */
      paddleL = HIGH;
    } else if (/*paddleL == LOW && paddleL_on_prev != HIGH &&*/ paddleL_on_highStart != 0) {
      if ((millis() - paddleL_on_highStart) < paddleL_on_delay) {
        /* paddleL stays inactive during on delay timer */
        paddleL = HIGH;
      } else {
        /* on delay expired */
        paddleL_on_highStart = 0;
      }
    }

  } /* (onDelay && keyerType != keyStraight) */
#endif
/**************************************/

#ifdef OPT_BETTER_IAMBIC_A
  if (keyerType != keyStraight) {

    /*
     * paddle R
     */
    if (paddleR == LOW) {
      if (paddleR_iambic_prev == HIGH) {
         paddleR_pressed = 1;
      }
      paddleR_iambic_prev = LOW;
    } else {
      paddleR_iambic_prev = HIGH;
    }

    /*
     * paddle L
     */
    if (paddleL == LOW) {
      if (paddleL_iambic_prev == HIGH) {
         paddleL_pressed = 1;
      }
      paddleL_iambic_prev = LOW;
    } else {
      paddleL_iambic_prev = HIGH;
    }

  }
#endif

#ifdef MOD_MISC
  if (paddleR == LOW && keyerType == keyStraight) {
    /* ignore R when straight key */
    paddleR = HIGH;
  }
#endif

#ifdef OPT_TIMING_DEBUG
  if (logi_paddleL_prev != paddleL) {
    if (paddleL == LOW) {
      serial_timing(",,L-D,");
    } else {
      serial_timing(",,L-U,");
    }
    logi_paddleL_prev = paddleL;
  }
  if (logi_paddleR_prev != paddleR) {
    if (paddleR == LOW) {
      serial_timing(",,,R-D");
    } else {
      serial_timing(",,,R-U");
    }
    logi_paddleR_prev = paddleR;
  }
#endif

  // clear preset messages
  if ((paddleL == LOW || paddleR == LOW) && pos_read > 0) {
    clearPresetMessage();
    clearBeacon();
    return;
  }

#ifdef OPT_RUNNER
  if (running && (paddleR == LOW || paddleL == LOW)) {
    // normal speed for actual keying
    reset_durationDot();
  }
#endif

  /*
   * Paddle LOW = Pushed, HIGH = Released
   */

  // pressing both paddles
  if (durationSpace == 0 && paddleL == LOW && paddleR == LOW) {
    if (durationMark == 0 && keyerType != keyStraight) {
      memory = -1;
#ifdef OPT_BETTER_IAMBIC_A
      /* reset memory and pressed status at the beginning of every mark */
      paddleR_pressed = 0;
      paddleL_pressed = 0;
#endif
      if (lastPaddle != L) {
        durationMark = !paddleRvs ? durationDot : durationDot * wait;
        lastPaddle = L;
#ifdef OPT_TWO_TONE
	if (twoTone) {
	  if (!paddleRvs)
	    toneTail = 1;
	  else
	    toneTail = 0;
	  //durationTail = !paddleRvs ? durationDot * (100 - TONE_TAIL_RATIO) / 100 : durationDot * wait - durationDot * TONE_TAIL_RATIO / 100;
	  durationTail = durationMark * (100 - TONE_TAIL_RATIO) / 100;
	  toneTail = 0;
	}
#endif
      } else {
        durationMark = !paddleRvs ? durationDot * wait : durationDot;
        lastPaddle = R;
#ifdef OPT_TWO_TONE
	if (twoTone) {
	  if (!paddleRvs)
	    toneTail = 0;
	  else
	    toneTail = 1;
	  //durationTail = !paddleRvs ? durationDot * wait - durationDot * TONE_TAIL_RATIO / 100 : durationDot * (100 - TONE_TAIL_RATIO) / 100;
	  durationTail = durationMark * (100 - TONE_TAIL_RATIO) / 100;
	  toneTail = 0;
	}
#endif
      }
      digitalWrite(pinLed, HIGH);
      key = DOWN;
      timeStart = millis();
#ifdef OPT_TIMING_DEBUG
      serial_timing(",,,,t1-D,");
#endif
#ifdef OPT_TWO_TONE
      if (sidetone) {
	tone(pinBz, toneFreq); // side tone
	//if (twoTone)
	//toneTail = 0;
      }
#else
#ifdef OPT_MEMORY_CLICK
      if (memoryClick)
        memory_click_start();
      else
#endif
      if (sidetone) tone(pinBz, toneFreq); // side tone
#endif
    }
    // pressing the L paddle
  } else if (durationSpace == 0 && paddleL == LOW) {
    if (durationMark == 0) {
      if (keyerType == keyStraight) {
        durationMark = 10000; // key down straight key
      } else {
        durationMark = !paddleRvs ? durationDot : durationDot * wait;
#ifdef OPT_TWO_TONE
	if (twoTone) {
	  if (!paddleRvs)
	    toneTail = 1;
	  else
	    toneTail = 0;
	  //durationTail = !paddleRvs ? durationDot * (100 - TONE_TAIL_RATIO) / 100 : durationDot * wait - durationDot * TONE_TAIL_RATIO / 100;
	  durationTail = durationMark * (100 - TONE_TAIL_RATIO) / 100;
	  toneTail = 0;
	}
#endif
      }
      lastPaddle = L;
      memory = -1;
#ifdef OPT_BETTER_IAMBIC_A
      /* reset memory and pressed status at the beginning of every mark */
      paddleR_pressed = 0;
      paddleL_pressed = 0;
#endif
      digitalWrite(pinLed, HIGH);
      key = DOWN;
      timeStart = millis();
#ifdef OPT_TIMING_DEBUG
      serial_timing(",,,,t2-D,");
#endif
#ifdef OPT_TWO_TONE
      if (sidetone) {
	tone(pinBz, toneFreq); // side tone
	//if (twoTone)
	//toneTail = 0;
      }
#else
#ifdef OPT_MEMORY_CLICK
      if (memoryClick)
        memory_click_start();
      else
#endif
      if (sidetone) tone(pinBz, toneFreq); // side tone
#endif
    }
    // pressing the R paddle
  } else if (paddleR == LOW && durationSpace == 0) {
    if (durationMark == 0 && keyerType != keyStraight) {
      durationMark = !paddleRvs ? durationDot * wait : durationDot;
      lastPaddle = R;
      memory = -1;
#ifdef OPT_BETTER_IAMBIC_A
      /* reset memory and pressed status at the beginning of every mark */
      paddleR_pressed = 0;
      paddleL_pressed = 0;
#endif
#ifdef OPT_TWO_TONE
      if (twoTone) {
	if (!paddleRvs)
	  toneTail = 0;
	else
	  toneTail = 1;
	//durationTail = !paddleRvs ? durationDot * wait - durationDot * TONE_TAIL_RATIO / 100 : durationDot * (100 - TONE_TAIL_RATIO) / 100;
	durationTail = durationMark * (100 - TONE_TAIL_RATIO) / 100;
	toneTail = 0;
      }
#endif
      digitalWrite(pinLed, HIGH);
      key = DOWN;
      timeStart = millis();
#ifdef OPT_TIMING_DEBUG
      serial_timing(",,,,t3-D,");
#endif
#ifdef OPT_TWO_TONE
      if (sidetone) {
	tone(pinBz, toneFreq); // side tone
	//if (twoTone)
	//toneTail = 0;
      }
#else
#ifdef OPT_MEMORY_CLICK
      if (memoryClick)
        memory_click_start();
      else
#endif
      if (sidetone) tone(pinBz, toneFreq); // side tone
#endif
    }
  }

  if (durationSpace != 0) {
#ifdef OPT_BETTER_IAMBIC_A
    if (keyerType == keyIambicA) {
      if (memory == -1 && paddleL == LOW && paddleL_pressed) {
	memory = L;
        paddleL_pressed = 0;
      } else if (memory == -1 && paddleR == LOW && paddleR_pressed) {
	memory = R;
        paddleR_pressed = 0;
      }
#ifdef OPT_LOOK_AHEAD_MEMORY
      else if (lookAheadMemory && memory == -1 && (millis() - timeStart >= durationDot * (100 - min(100,look_ahead_memory_ratio)) / 100)) {
        // memory paddle LR
	if (paddleL == LOW && paddleR == LOW) {
          if (lastPaddle == R) {
            memory = L;
            paddleL_pressed = 0;
          } else {
            memory = R;
            paddleR_pressed = 0;
          }
	} else if (paddleL == LOW) {
          memory = L;
          paddleL_pressed = 0;
	} else if (paddleR == LOW) {
          memory = R;
          paddleR_pressed = 0;
	}
      }
#endif
    } else
#endif
    if (keyerType == keyIambicB || keyerType == keySuper) {

      // memory paddle LR
      if (memory == -1 && paddleL == LOW && lastPaddle == R) {
        memory = L;
      } else if (memory == -1 && paddleR == LOW && lastPaddle == L) {
        memory = R;
      }
#ifdef OPT_LOOK_AHEAD_MEMORY
      else if (lookAheadMemory && memory == -1 && (millis() - timeStart >= durationDot * (100 - min(100,look_ahead_memory_ratio)) / 100)) {
        // memory paddle LR
	if (paddleL == LOW && paddleR == LOW) {
          if (lastPaddle == R) {
            memory = L;
          } else {
            memory = R;
          }
	} else if (paddleL == LOW) {
          memory = L;
	} else if (paddleR == LOW) {
          memory = R;
	}
      }
#endif
    }
    // ending of space
    if (millis() - timeStart >= durationSpace) {
      durationSpace = 0;
      // output memory LR
#ifdef OPT_REPEAT_ADD
      if (off_repeat_add > 0) {
        if (memory == L) {
          if (lastPaddle == L) {
            if (repeatingL < 4) {
              repeatingL++;
            } else if (repeatingL >= 4) {
              repeatingL = 0;
              memory = -1;
              //Serial.print(F("MemoryClear"));
            }
            repeatingR = 0;
            //Serial.println();
            //Serial.print(F("repeatingL="));
            //Serial.println(repeatingL);
          } else {
            repeatingL = 0;
            repeatingR = 0;
          }
        } else if (memory == R) {
          if (lastPaddle == R) {
            if (repeatingR < 4) {
              repeatingR++;
            } else if (repeatingR >= 4) {
              repeatingR = 0;
              memory = -1;
              //Serial.print(F("MemoryClear"));
            }
            repeatingL = 0;
            //Serial.println();
            //Serial.print(F("repeatingR="));
            //Serial.println(repeatingR);
          } else {
            repeatingL = 0;
            repeatingR = 0;
          }
        }
      }
#endif
      if (memory != -1) {
        if (memory == L) {
          durationMark = !paddleRvs ? durationDot : durationDot * wait;
          lastPaddle = L;
#ifdef OPT_TWO_TONE
	  if (twoTone) {
	    if (!paddleRvs)
	      toneTail = 1;
	    else
	      toneTail = 0;
	    //durationTail = !paddleRvs ? durationDot * (100 - TONE_TAIL_RATIO) / 100 : durationDot * wait - durationDot * TONE_TAIL_RATIO / 100;
	    durationTail = durationMark * (100 - TONE_TAIL_RATIO) / 100;
	    toneTail = 0;
	  }
#endif
        } else if (memory == R) {
          durationMark = !paddleRvs ? durationDot * wait : durationDot;
          lastPaddle = R;
#ifdef OPT_TWO_TONE
	  if (twoTone) {
	    if (!paddleRvs)
	      toneTail = 0;
	    else
	      toneTail = 1;
	    //durationTail = !paddleRvs ? durationDot * wait - durationDot * TONE_TAIL_RATIO / 100 : durationDot * (100 - TONE_TAIL_RATIO) / 100;
	    durationTail = durationMark * (100 - TONE_TAIL_RATIO) / 100;
	    toneTail = 0;
	  }
#endif
        }
        memory = -1;
        digitalWrite(pinLed, HIGH);
        key = DOWN;
        timeStart = millis();
#ifdef OPT_TIMING_DEBUG
        serial_timing(",,,,t4-D,");
#endif
#ifdef OPT_TWO_TONE
	if (sidetone) {
	  tone(pinBz, toneFreq); // side tone
	  //if (twoTone)
	  //toneTail = 0;
	}
#else
#ifdef OPT_MEMORY_CLICK
        if (memoryClick)
          memory_click_start();
        else
#endif
        if (sidetone) tone(pinBz, toneFreq); // side tone
#endif
#ifdef OPT_REPEAT_ADD
      } else {
        repeatingL = 0;
        repeatingR = 0;
#endif
      }
    }
  }
  if (durationMark != 0) {
#ifdef OPT_TWO_TONE
    if (twoTone && sidetone && toneTail == 0 && (millis() - timeStart) >= durationTail) {
      toneTail = 1;
      noTone(pinBz);
      tone(pinBz, toneFreq*5/8);
    }
#endif
    // memory paddle LR
#ifdef OPT_BETTER_IAMBIC_A
    if (keyerType == keyIambicA) {
      if (memory == -1 && paddleL_pressed && lastPaddle == R) {
	memory = L;
        paddleL_pressed = 0;
      } else if (memory == -1 && paddleR_pressed && lastPaddle == L) {
	memory = R;
        paddleR_pressed = 0;
      }
#ifdef OPT_LOOK_AHEAD_MEMORY
      else if (lookAheadMemory && look_ahead_memory_ratio > 100 && memory == -1 && (timeStart + durationMark - millis() < durationDot * (look_ahead_memory_ratio - 100) / 100)) {
        // memory paddle LR
	if (paddleL == LOW && paddleR == LOW) {
          if (lastPaddle == R) {
            memory = L;
            paddleL_pressed = 0;
          } else {
            memory = R;
            paddleR_pressed = 0;
          }
	} else if (paddleL == LOW) {
          memory = L;
          paddleL_pressed = 0;
	} else if (paddleR == LOW) {
          memory = R;
          paddleR_pressed = 0;
	}
      }
#endif
    } else
#endif
    if (keyerType == keyIambicB || ((keyerType == keySuper) && (millis() - timeStart >= durationDot))) {
      if (memory == -1 && paddleL == LOW && lastPaddle == R) {
        memory = L;
      } else if (memory == -1 && paddleR == LOW && lastPaddle == L) {
        memory = R;
      }
#ifdef OPT_LOOK_AHEAD_MEMORY
      else if (lookAheadMemory && look_ahead_memory_ratio > 100 && memory == -1 && (timeStart + durationMark - millis() < durationDot * (look_ahead_memory_ratio - 100) / 100)) {
        // memory paddle LR
	if (paddleL == LOW && paddleR == LOW) {
          if (lastPaddle == R) {
            memory = L;
          } else {
            memory = R;
          }
	} else if (paddleL == LOW) {
          memory = L;
	} else if (paddleR == LOW) {
          memory = R;
	}
      }
#endif
    }
    // ending of mark
    if (millis() - timeStart >= durationMark) {
#ifdef OPT_WORD_SPACE
      durationSpace = endingChar ? durationDot * charSpace : durationDot;
#else
      durationSpace = endingChar ? durationDot * 3 : durationDot;
#endif
      durationMark = 0;
      endingChar = 0;
      noTone(pinBz);
      timeStart = millis();
      digitalWrite(pinLed, LOW);
      key = UP;
#ifdef OPT_TIMING_DEBUG
      serial_timing(",,,,,t1-U");
#endif
    }
    // key up straight key
    if (keyerType == keyStraight) {
#ifdef MOD_MISC
      if (paddleL == HIGH && (pos_read == 0) && (millis() - timeStart >= durationDot)) { /* pos_read condition prevents from all-dots message output */
#else
      if (paddleL == HIGH && (millis() - timeStart >= durationDot)) {
#endif
        durationSpace = durationDot;
        durationMark = 0;
        noTone(pinBz);
        timeStart = millis();
        digitalWrite(pinLed, LOW);
        key = UP;
#ifdef OPT_TIMING_DEBUG
        serial_timing(",,,,,t2-U");
#endif
      }
    }
  }
#if defined(OPT_RUNNER) || defined(OPT_INVADER_SOUND)
  /*
   * to avoid conflict among tone()
   */
  if (runner_beep != 0 && durationMark == 0 && filteredstate == LOW) {
    /* intending no impact on sidetone and highduration */
    switch (runner_beep) {
#ifdef OPT_RUNNER
    case RUNNER_HIT:
      tone_hit();
      break;
    case RUNNER_MISS:
      tone_miss();
      break;
#endif
#ifdef OPT_INVADER_SOUND
    case INVADER_BEEP:
      beep();
      break;
    case INVADER_ERRBEEP:
      errBeep();
      break;
#endif
    }
    runner_beep = 0;
  }
#endif /* defined(OPT_RUNNER) || defined(OPT_INVADER_SOUND) */
#ifdef OPT_MEMORY_CLICK
  if (timeEnd && timeEnd <= millis()) {
    memory_click_stop();
  }
#endif

#ifdef OPT_VIBRATION
  if (vibration_end > 0 && vibration_end <= millis()) {
    stopVibration();
    vibration_end = 0;
  }
#endif
}

void readButtonValue() {
  int sw[4];

  for (int i = 0; i < 4; i++) {
    sw[i] = digitalRead(pinBtn[i]);
    // button was pressed
    if (sw[i] == LOW && timeStartSw[i] == 0) {
#ifdef OPT_BETTER_SRAND
      if (first_srand) {
        first_srand = 0;
        srand(millis()); /* seed for rand() */
      }
#endif
      //digitalWrite(pinLed, HIGH);
      timeStartSw[i] = millis();
      beep();
      // button was released
    } else if (sw[i] == HIGH && timeStartSw[i] != 0) {
      //digitalWrite(pinLed, LOW);
      if (millis() - timeStartSw[i] >= durationLongPress)
        fireLongFunction(i); // long press
      else
        fireFunction(i); // short press
      timeStartSw[i] = 0;
      // detect long press
    } else if (millis() - timeStartSw[i] >= durationLongPress && millis() - timeStartSw[i] < durationLongPress + 10) {
      //digitalWrite(pinLed, LOW);
      beep();
#ifdef OPT_RUNNER
      runner_mode = 0;
    } else if (millis() - timeStartSw[i] >= durationLongPress*2 && millis() - timeStartSw[i] < durationLongPress*2 + 10) { /* much longer press */
      //digitalWrite(pinLed, LOW);
      tone_runner_mode();
      runner_mode = 1;
      runner_count = 0;
#endif
    }
  }
}

void fireFunction(int no) {
  Serial.println();
  Serial.print(F("Press="));
  Serial.println(no + 1);

#if 1 // XXX stop side tone
  noTone(pinBz);
#endif
#ifdef OPT_RUNNER
  runner_stop(); /* pushing button to disable runner */
#endif
  // clear preset messages
  if (pos_read > 0) {
    clearPresetMessage();
    clearBeacon();
    return;
  }
  // stop game
  if (restInvader >= 0) {
    restInvader = 0;
    return;
  }

  // play preset messages
  lcd.setCursor(0, 0);
  lcd.print(F("Msg"));
  lcd.print(no + 1);
  lcd.print(F("    "));
  setPresetMessage(no);
  printascii(SPACE);
  IsSendingPresetMessages = true;
}

void fireLongFunction(int no) {
  Serial.println();
  Serial.print(F("LongPress="));
  Serial.println(no + 1);

#if 1 // XXX stop side tone
  noTone(pinBz);
#endif
#ifdef OPT_RUNNER
  runner_stop(); /* pushing button to disable runner */
#endif
  // clear preset messages
  if (pos_read > 0) {
    clearPresetMessage();
    clearBeacon();
    return;
  }
  // stop game
  if (restInvader >= 0) {
    restInvader = 0;
    return;
  }

  int pressedSw;
  long lasttime = millis();
  switch (no) {
    case 0: // beacon cq
      setPresetMessage(0);
      printascii(SPACE);
      timeCqInterval = defCqInterval;
      lcd.setCursor(0, 0);
      lcd.print(F("Beacon"));
      lcd.print(defCqTimes);
      CqTimes = defCqTimes;
      break;
    case 1: // recv training mode
      lcd.setCursor(0, 0);
      lcd.print(F("Training"));
      lcd.setCursor(0, 1);
      lcd.print(F("A "));
      lcd.write(0b10110001);
#ifdef OPT_RUNNER
      if (runner_mode) {
        lcd.print(F(" r "));
      } else {
        lcd.print(F(" c "));
      }
#else
      lcd.print(F(" c "));
#endif
      lcd.write(0); // invader
      lcd.write(1); // invader
      // wait for the selection
      lcd.setCursor(5, 0);
      for (;;) {
        lcd.setCursor(6, 1);
        if ((millis() - lasttime) >= 1000) {
          lcd.write((millis() / 1000) % 3 ? 0 : 2);
          lcd.write((millis() / 1000) % 2 ? 1 : 3);
          lasttime = millis();
        }
        if (digitalRead(pinSw1) == LOW) {
          pressedSw = pinSw1;
          lcd.setCursor(0, 0);
          lcd.print(F("5x10 ABC"));
          break;
        } else if (digitalRead(pinSw2) == LOW) {
          pressedSw = pinSw2;
          lcd.setCursor(0, 0);
          lcd.print(F("5x10"));
          lcd.write(0b11011100); // wabun
          lcd.write(0b11001100);
          lcd.write(0b11011110);
          lcd.write(0b11011101);
          break;
        } else if (digitalRead(pinSw3) == LOW) {
          pressedSw = pinSw3;
          lcd.setCursor(0, 0);
#ifdef OPT_RUNNER
          lcdIndex = 0;
          if (runner_mode) {
            lcd.print(F("Runner  "));
	  } else {
            lcd.print(F("10x CALL"));
	  }
#else
          lcd.print(F("10x CALL"));
#endif
          break;
        } else if (digitalRead(pinSw4) == LOW) {
          pressedSw = pinSw4;
#ifdef OPT_INVADER_SOUND
          if (invaderSound) {
            beep();
	  }
#endif
          invaderAnimation();
          break;
        }
      }
      lcd.setCursor(0, 1);
      lcd.print(F("        "));
#ifdef OPT_INVADER_SOUND
     if (!invaderSound) {
#endif
      beep();
      delay(200);
#ifdef OPT_INVADER_SOUND
     }
#endif
      for (;;) {
        if (digitalRead(pressedSw) == HIGH) break;
      }
      trainingMode(pressedSw);
      break;
    case 2: // record messages
      //ToDo: save original messages on EEPROM
      Serial.println(F("recording mode..."));
#ifdef OPT_PARAM_EEPROM
      param_eeprom_save();
      lcd.setCursor(0, 0);
      lcd.print(F("Saved   "));
      tone_eeprom();
#endif
      break;
    case 3: // setting
      Serial.println(F("setting mode..."));
      editSetting();
      break;
    default:
      Serial.println(F("default"));
      break;
  }
}

// recv training mode (abc, kana, ja-callsign, invader)
void trainingMode(int pressedSw) {
  int i, j, k = 0;
  if (pressedSw == pinSw1) {
    // 5x10 random letters ABC
    lang = langAbc;
    for (i = 0; i < 10; i++) {
      for (j = 0; j < 5; j++) {
        presetMessage[k] = CharAbc[rand() / (RAND_MAX / 26 + 1)] ;
        k++;
      }
      presetMessage[k] = ' ';
      k++;
    }
  } else if (pressedSw == pinSw2) {
    // 5x10 random letters KANA
    lang = langWabun;
    for (i = 0; i < 10; i++) {
      for (j = 0; j < 5; j++) {
        presetMessage[k] = CharWabun[rand() / (RAND_MAX / 48 + 1)] ;
#if 0 /* XXX debug (made wabun same to abc) */
        Serial.print(presetMessage[k]);
        Serial.print(F(","));
#endif
        k++;
      }
#if 0 /* XXX debug (made wabun same to abc) */
      Serial.println();
#endif
      presetMessage[k] = ' ';
      k++;
    }
  } else if (pressedSw == pinSw3) {
    // 10 random callsigns JA
    lang = langAbc;
    for (i = 0; i < 10; i++) {
#ifdef MOD_MISC
#ifdef OPT_RUNNER
     int nr = 0;
     if (runner_mode == 0 /*rand() / (RAND_MAX / 2 + 1) == 0*/) { /* contest number */
      runner_repeat = 1;
      int ch;
      if (rand() / (RAND_MAX / 2 + 1) == 0) {
        ch = 'M';
      } else if (rand() / (RAND_MAX / 2 + 1) == 0) {
        ch = 'H';
      } else {
        ch = 'L';
      }

      nr = 1;
      presetMessage[k] = '0';
      k++;
      presetMessage[k] = '0' + rand() / (RAND_MAX / 10 + 1);
      k++;

      if (rand() / (RAND_MAX / 2 + 1) == 0) { /* JCC/JCG */
        /* JCC */
        presetMessage[k] = '0';
        k++;
        presetMessage[k] = '0' + rand() / (RAND_MAX / 15 + 1);
        k++;

        if (rand() / (RAND_MAX / 2 + 1) == 0) {
          presetMessage[k] = '0';
          k++;
          presetMessage[k] = '0' + rand() / (RAND_MAX / 15 + 1);
          k++;

          presetMessage[k] = ch;
          k++;

          runner_call_len = 7;
        } else {
          presetMessage[k] = ch;
          k++;

          runner_call_len = 5;
        }
      } else {
        /* JCG */
        presetMessage[k] = '0';
                k++;
        presetMessage[k] = '0';
        k++;
        presetMessage[k] = '0' + rand() / (RAND_MAX / 15 + 1);
        k++;

        presetMessage[k] = ch;
        k++;

        runner_call_len = 6;
      }

      int j;
      for (j = 0 ; j < k ; j++) {
        if (presetMessage[j] == '0')
          presetMessage[j] = 'T';
        else if (presetMessage[j] >= '9' && presetMessage[j] <= '>')
          presetMessage[j] = 'N';
      }
     } else
#endif /* OPT_RUNNER */
     if (rand() / (RAND_MAX / 4 + 1) == 0) { /* begin with 7/8 */
      presetMessage[k] = '7' + rand() / (RAND_MAX / 2 + 1); /* 7-8 */
      k++;
      presetMessage[k] = 'J' + rand() / (RAND_MAX / 5 + 1); /* J-N */
      k++;
     } else { /* begin with J */
#endif /* MOD_MISC */
      presetMessage[k] = 'J';
      k++;
      presetMessage[k] = 'A' + rand() / (RAND_MAX / 17 + 1) ; /* JA, JD-JS */
      if (presetMessage[k] == 'B' || presetMessage[k] == 'C') {
        presetMessage[k] += 16;
      }
      k++;
#ifdef MOD_MISC
     } /* end of if 7/8 or J */
#endif
#ifndef OPT_RUNNER /* !OPT_RUNNDER */
      if (presetMessage[k - 1] == 'S') {
        presetMessage[k] = '6';
      } else if (presetMessage[k - 1] == 'D') {
        presetMessage[k] = '1';
      } else {
#endif
        presetMessage[k] = '0' + rand() / (RAND_MAX / 10 + 1) ;
#ifndef OPT_RUNNER /* !OPT_RUNNDER */
      }
#endif
      k++;
#ifdef OPT_RUNNER
      if (nr == 1) {
        k--;
        presetMessage[k] = 0x00;
      } else
      if (presetMessage[k - 3] == '7') {
	if (presetMessage[k - 2] >= 'K' && presetMessage[k - 2] <= 'N') { // omitting 7{K-N}{0|5-9}
	  presetMessage[k - 1] = '1' + rand() / (RAND_MAX / 4 + 1) ; /* 1-4 */
	}
      } else if (presetMessage[k - 3] == '8') {
	if (presetMessage[k - 2] == 'K') { // omitting 8K
	  presetMessage[k - 2] = 'J';
	} else if (presetMessage[k - 2] == 'L') { // omitting 8L
	  presetMessage[k - 2] = 'N';
	}
      } else if (presetMessage[k - 2] == 'D') { // omitting JD{0|2-9}
	presetMessage[k - 1] = '1';
      }
      if (nr == 0)
#endif
      for (j = 0; j < 3; j++) {
        presetMessage[k] = 'A' + rand() / (RAND_MAX / 26 + 1) ;
        k++;
      }
#ifdef OPT_RUNNER
      if (runner_mode) { /* runner */
        int idx;

	running = 1;
	runner_ptr = 0;

        if (nr == 1)
          ; /* fall through */
        else
        if (rand() / (RAND_MAX / 5 + 1) == 0) {
          presetMessage[k] = '/'; /* stroke */
          k++;
          presetMessage[k] = '0' + rand() / (RAND_MAX / 10 + 1) ;
          k++;
	  runner_call_len = 8;
	} else {
	  runner_call_len = 6;
	}

	for (idx = 0 ; idx < runner_call_len ; idx++) {
	  runner_call[idx] = presetMessage[k - runner_call_len + idx];
	}

        break;
      }
#endif
      presetMessage[k] = ' ';
      k++;
    }
  } else if (pressedSw == pinSw4) {
    // morse-invader
    SetupMorseInvader();
  }
}

// translate cw code to ascii //
void decode() {
  int i, j, k;

#ifdef MOD_MISC
  if (code == 1) // there is no decoded characters
    return;
#endif

  if (lang == langAbc) {
    if (code == 0b01000101) { // [VA]
      printascii(0b01011011);
      printascii(0b01010110);
      printascii(0b01000001);
      printascii(0b01011101);
    } else if (code == 0b00101000) { // [AS]
      printascii(0b01011011);
      printascii(0b01000001);
      printascii(0b01010011);
      printascii(0b01011101);
    } else if (code == 0b01100111) { // hore
      printascii(0b01011011);
      printascii(0b11001110);
      printascii(0b11011010);
      printascii(0b01011101);
      lang = langWabun;
      Serial.println(F("(switching to Wabun)"));
    } else {
      for (i = 0; i < sizeof(CharAbc) / sizeof(unsigned char); i++) {
        if (code == BinAbc[i]) {
          if (restInvader < 0) {
            printascii(CharAbc[i]);
#ifdef OPT_TIMING_DEBUG
            Serial.println();
#endif
#ifdef OPT_RUNNER
            if (running == 1) {
              if (code == 0b01001100) { /* '?' */
	        int idx;
		clearPresetMessage();
		for (idx = 0 ; idx < runner_call_len ; idx++) {
		  presetMessage[idx] = runner_call[idx];
		}
                runner_retry++;
                // retrying 2 attempts (total 3 calls) are allowed.
                // 3rd retry (total 4th attempts) becomes 1 penalty,
                // and runner_accel will be immediately decreaased (slowed).
		if (runner_retry > 2 && runner_accel > 0) {
		   runner_accel--; // '?' becomes 5% slower
		   //runner_miss++;
		   runner_retry = 0;
		}
                accel_durationDot(); // accelerated speed for callsigns
		runner_ptr = 0;
                print_runner_count();
		//printascii(SPACE);
              } else if (CharAbc[i] == runner_call[runner_ptr]) {
	        if (++runner_ptr == runner_call_len) {
		  //if (runner_miss == 0) {
                  // 1 miss is allowed.
		  if (runner_miss < 2) {
		    if (runner_accel < MAX_RUNNER_ACCEL) {
                      runner_accel++; // no miss becomes 5% faster
		    }
		  } else {
		    if (runner_accel > 0) {
		      runner_accel--; // miss(es) becomes 5% slower
		    }
		  }

                  runner_count++;
                  if (runner_accel >= HIGH_RUNNER_ACCEL) {
                    tone_invader_clear();
                  } else {
                    tone_hit();
                  }
                  print_runner_count();

		  accel_durationDot(); // accelerated speed for callsigns
		  runner_miss = 0;
                  runner_retry = 0;
		  delay(1000);
		  printascii(SPACE);
                  trainingMode(pinSw3);
		}
	      } else {
	        //tone_miss();
		runner_beep = RUNNER_MISS;
		runner_miss++;
		runner_ptr = 0;
		printascii(SPACE);
	      }
	    }
#endif
          } else { // Invader
            gameStarted = 1;
            lcd.setCursor(0, 1);
            lcd.write(CharAbc[i]);
            lcd.noCursor();
            for (j = 1; j < colums; j++) {
              if (line1[j] == CharAbc[i]) { // hit
                gamePts += (1 + combo);
                if (combo < 4) combo++;
                lcd.write(SPACE);
#ifdef OPT_INVADER_SOUND
                if (invaderSound) {
		  runner_beep = INVADER_BEEP;
		}
#endif
                for (k = j; k > 1; k--) {
                  line1[k] = line1[k - 1];
                  lcd.setCursor(k, 1);
                  lcd.write(line1[k]);
                }
                break;
              }
            }
#ifdef OPT_INVADER_SOUND
            if (j == colums) {
	      combo = 0; // wrong
	      if (invaderSound) {
                runner_beep = INVADER_ERRBEEP;
	      }
	    }
#else
            if (j == colums) combo = 0; // wrong
#endif
          }
          break;
        }
      }
#ifdef OPT_INVADER_SOUND
      if (restInvader >=0 && i == sizeof(CharAbc) / sizeof(unsigned char)) {
        if (invaderSound) {
          runner_beep = INVADER_ERRBEEP;
	}
      }
#endif
#ifdef OPT_RUNNER
      if (running == 1 && i == sizeof(CharAbc) / sizeof(unsigned char)) {
        //tone_miss();
	runner_beep = RUNNER_MISS;
	runner_miss++;
	runner_ptr = 0;
	printascii('_');
      }
#endif
    }
  } else if (lang == langWabun) {
    if (code == 0b00100010) { // rata
      printascii(0b01011011);
      printascii(0b11010111);
      printascii(0b11000000);
      printascii(0b01011101);
      lang = langAbc;
      Serial.println(F("(switching to Abc)"));
    } else {
      for (i = 0; i < sizeof(CharWabun) / sizeof(unsigned char); i++) {
        if (code == BinKana[i]) {
          if (restInvader < 0) {
            printascii(CharWabun[i]);
          } else { // Invader
            gameStarted = 1;
            lcd.setCursor(0, 1);
            lcd.write(CharWabun[i]);
            lcd.noCursor();
#if 1 /* XXX made wabun same to abc */
            for (j = 1; j < colums; j++) {
              if (line1[j] == CharWabun[i]) { // hit
#else
            for (j = 1; j < colums - 1; j++) {
              if (line1[j] == CharWabun[i]) {
#endif
                gamePts += (1 + combo);
                if (combo < 4) combo++;
                lcd.write(SPACE);
#ifdef OPT_INVADER_SOUND
                if (invaderSound) {
		  runner_beep = INVADER_BEEP;
		}
#endif
                for (k = j; k > 1; k--) {
                  line1[k] = line1[k - 1];
                  lcd.setCursor(k, 1);
                  lcd.write(line1[k]);
                }
                break;
              }
            }
#ifdef OPT_INVADER_SOUND /* made wabun same to abc */
            if (j == colums) {
	      combo = 0; // wrong
              if (invaderSound) {
                runner_beep = INVADER_ERRBEEP;
	      }
	    }
#else
            if (j == colums) combo = 0; // wrong
#endif
          }
          break;
        }
      }
#ifdef OPT_INVADER_SOUND
      if (restInvader >=0 && i == sizeof(CharWabun) / sizeof(unsigned char)) {
        if (invaderSound) {
          runner_beep = INVADER_ERRBEEP;
	}
      }
#endif
    }
  }
}

// print the ascii code to the lcd one a time
void printascii(int asciinumber) {
  // Do nothing when playing the game
  if (restInvader >= 0) return;

  static int lastLetter;
  if (lastLetter == SPACE && asciinumber == SPACE) return;
  lastLetter = asciinumber;

#ifndef OPT_NO_SERIAL
#ifdef OPT_LCD_DISP
 if (lcdDisp)
#endif
  Serial.write(asciinumber); // Monitor characters
#endif

#ifdef OPT_LCD_DISP
  if (lcdDisp == 0)
    return;
#endif

#ifdef SCROLL_ROW
  if (lcdIndex > colums - 1) {
    lcdIndex = 0;
    for (int i = 0; i <= colums - 1 ; i++) {
      lcd.setCursor(i, rows - 2);
      lcd.write(line1[i]);
      lcd.setCursor(i, rows - 1);
      lcd.write(SPACE);
    }
  }
  line1[lcdIndex] = asciinumber;
  lcd.setCursor(lcdIndex, rows - 1);
  lcd.write(asciinumber);
  lcdIndex += 1;
#endif

#ifndef SCROLL_ROW
  if (lcdIndex > colums - 1) {
    lcdIndex = 7;
    lcd.setCursor(0, rows - 2);
    for (int i = 0; i < colums - 1 ; i++) {
      lcd.write(line2[i + 1]);
      line2[i] = line2[i + 1];
    }
    line2[7] = line1[0];
    lcd.write(line2[7]);
    lcd.setCursor(0, rows - 1);
    for (int i = 0; i < colums - 1 ; i++) {
      lcd.write(line1[i + 1]);
      line1[i] = line1[i + 1];
    }
    //lcd.write(SPACE);
  }
  line1[lcdIndex] = asciinumber;
  lcd.setCursor(lcdIndex, rows - 1);
  lcd.write(asciinumber);
  lcdIndex += 1;
#endif SCROLL_ROW
}

// update the upper line with the speed
void updateinfolinelcd() {
#ifdef MOD_MISC
  static int oldWpm = INIT_DECODE_WPM; // to surpress initial "wpm" (for initial "WPM" displayed, instead)
#else
  static int oldWpm;
#endif
  if (oldWpm == wpm) return;
  lcd.home();
  if (wpm < 10) lcd.print(F("0"));
  lcd.print(wpm);
  lcd.print(F("wpm"));
  oldWpm = wpm;
}

void editSetting() {
  lcd.setCursor(0, 1);
  lcd.write(0b00000110);
  lcd.print(F("<    >"));
  lcd.write(0b11110111);
#ifdef OPT_LAST_MENUINDEX
  static int menuIndex = -1; // memory the last menuIndex
  int right_after_invoked = 1;
  int backward = 0;
#define DURATION_SEMILONG_PRESS 300
#else
  int menuIndex = -1;
#endif

  lcd.setCursor(0, 0);
  lcd.print(F("Set Mode"));
  for (;;) {
    if (digitalRead(pinSw4) == LOW) break;
#ifdef OPT_LAST_MENUINDEX
    if (backward || (menuIndex != -1 && right_after_invoked) || digitalRead(pinSw1) == LOW) {
      if (digitalRead(pinSw1) == LOW) {
	backward = 0;
	timeStart = millis();
	while (digitalRead(pinSw1) == LOW) {
	  if (millis() - timeStart >= DURATION_SEMILONG_PRESS) {
	    backward = 1;
	    buzzer(pinBz, beepFreq * 2, 20);
	    break;
	  }
	}
	while (digitalRead(pinSw1) == LOW)
	  /* wait until pinSw1 becomes HIGH */;
      }
      if (menuIndex == -1 || !right_after_invoked) {
	beep();
	if (backward) {
	  backward = 0;
	  menuIndex--;
	  if (menuIndex < 0) menuIndex = MaxMenu -1;
	} else {
	  menuIndex++;
	  if (menuIndex == MaxMenu) menuIndex = 0;
	}
      } else {
	right_after_invoked = 0;
	// menuIndex is not incremented
      }
#else
    if (digitalRead(pinSw1) == LOW) {
      beep();
      menuIndex++;
      if (menuIndex == MaxMenu) menuIndex = 0;
#endif
      delay(200);
      lcd.setCursor(0, 0);
      lcd.print(setMenu[menuIndex]);
      if (menuIndex == 0) lcd.write(0b10110001); // a
      lcd.setCursor(2, 1);
      if (menuIndex == 0) {
        if (lang == langAbc)
          lcd.print(F("ABC "));
        else {
          lcd.write(0b11011100); // wabun
          lcd.write(0b11001100);
          lcd.write(0b11011110);
          lcd.write(0b11011101);
        }
      } else if (menuIndex == 1) {
        if (paddleRvs)
          lcd.print(F(" on "));
        else
          lcd.print(F("off "));
      } else if (menuIndex == 2) {
        lcd.print(keyName[keyerType]);
      } else if (menuIndex == 3) {
        lcd.print(wait);
      } else if (menuIndex == 4) {
        if (sidetone)
          lcd.print(F(" on "));
        else
          lcd.print(F("off "));
      } else if (menuIndex == 5) {
        lcd.print(defCqInterval);
      } else if (menuIndex == 6) {
        if (defCqTimes < 10) lcd.print(F("  "));
        else lcd.print(F(" "));
        lcd.print(defCqTimes);
        lcd.print(F(" "));
#ifdef OPT_LCD_DISP
      } else if (menuIndex == menuIndexLcdDisp) {
        if (lcdDisp)
          lcd.print(F(" on "));
        else
          lcd.print(F("off "));
#endif
#ifdef OPT_TONE_FREQ
      } else if (menuIndex == menuIndexToneFreq) {
        lcd.print(toneFreq);
	if (toneFreq < 1000)
          lcd.print(F(" "));
#endif
#ifdef OPT_WORD_SPACE
      } else if (menuIndex == menuIndexWordSpace) {
        lcd.print(wordSpace);
	if (wordSpace < 10)
          lcd.print(F(" "));
	lcd.print(F("  "));
      } else if (menuIndex == menuIndexCharSpace) {
        lcd.print(charSpace);
	if (charSpace < 10)
          lcd.print(F(" "));
	lcd.print(F("  "));
#endif
#ifdef OPT_OFF_DELAY
      } else if (menuIndex == menuIndexOffDelay) {
        if (offDelay)
          lcd.print(F(" on "));
        else
          lcd.print(F("off "));
      } else if (menuIndex == menuIndexOffDelayDot) {
        lcd.print(off_timer_ratio_dot);
	if (off_timer_ratio_dot < 10)
          lcd.print(F(" "));
	if (off_timer_ratio_dot < 100)
          lcd.print(F(" "));
        lcd.print(F(" "));
      } else if (menuIndex == menuIndexOffDelayDash) {
        lcd.print(off_timer_ratio_dash);
	if (off_timer_ratio_dash < 10)
          lcd.print(F(" "));
	if (off_timer_ratio_dash < 100)
          lcd.print(F(" "));
        lcd.print(F(" "));
#ifdef OPT_REPEAT_ADD
      } else if (menuIndex == menuIndexOffRepeatAdd) {
        lcd.print(off_repeat_add);
	if (off_repeat_add < 10)
          lcd.print(F(" "));
	if (off_repeat_add < 100)
          lcd.print(F(" "));
        lcd.print(F(" "));
#endif
#endif
#ifdef OPT_ON_DELAY
      } else if (menuIndex == menuIndexOnDelay) {
        if (onDelay)
          lcd.print(F(" on "));
        else
          lcd.print(F("off "));
      } else if (menuIndex == menuIndexOnDelayDot) {
        lcd.print(on_timer_ratio_dot);
	if (on_timer_ratio_dot < 10)
          lcd.print(F(" "));
	if (on_timer_ratio_dot < 100)
          lcd.print(F(" "));
        lcd.print(F(" "));
      } else if (menuIndex == menuIndexOnDelayDash) {
        lcd.print(on_timer_ratio_dash);
	if (on_timer_ratio_dash < 10)
          lcd.print(F(" "));
	if (on_timer_ratio_dash < 100)
          lcd.print(F(" "));
        lcd.print(F(" "));
#endif
#ifdef OPT_LOOK_AHEAD_MEMORY
      } else if (menuIndex == menuIndexLookAheadMemory) {
        if (lookAheadMemory)
          lcd.print(F(" on "));
        else
          lcd.print(F("off "));
      } else if (menuIndex == menuIndexLookAheadMemoryRatio) {
        lcd.print(look_ahead_memory_ratio);
	if (look_ahead_memory_ratio < 10)
          lcd.print(F(" "));
	if (look_ahead_memory_ratio < 100)
          lcd.print(F(" "));
        lcd.print(F(" "));
#endif
#ifdef OPT_TWO_TONE
      } else if (menuIndex == menuIndexTwoTone) {
        if (twoTone)
          lcd.print(F(" on "));
        else
          lcd.print(F("off "));
#endif
#ifdef OPT_MEMORY_CLICK
      } else if (menuIndex == menuIndexMemoryClick) {
        if (memoryClick)
          lcd.print(F(" on "));
        else
          lcd.print(F("off "));
#endif
#ifdef OPT_AVOID_CHATTERING
      } else if (menuIndex == menuIndexAvoidChattering) {
        if (avoidChattering)
          lcd.print(F(" on "));
        else
          lcd.print(F("off "));
      } else if (menuIndex == menuIndexAvoidChatteringCount) {
        lcd.print(avoidChatteringCount);
	if (avoidChatteringCount < 10)
	  lcd.print(F(" "));
	if (avoidChatteringCount < 100)
	  lcd.print(F(" "));
	lcd.print(F(" "));
#endif
#ifdef OPT_AUTO_KEYERTYPE
      } else if (menuIndex == menuIndexAutoKeyertype) {
        if (autoKeyerType)
          lcd.print(F(" on "));
        else
          lcd.print(F("off "));
      } else if (menuIndex == menuIndexAutoKeyertypeElekey) {
        lcd.print(keyName[autoKeyerTypeElekey]);
#endif
      }
    }
    if (digitalRead(pinSw2) == LOW) {
      beep();
      delay(200);
      lcd.setCursor(2, 1);
      if (menuIndex == 0) {
        if (lang == langWabun) {
          lang = langAbc;
          lcd.print(F("ABC "));
        } else {
          lang = langWabun;
          lcd.write(0b11011100); // wabun
          lcd.write(0b11001100);
          lcd.write(0b11011110);
          lcd.write(0b11011101);
        }
      } else if (menuIndex == 1) {
        paddleRvs = !paddleRvs;
        if (paddleRvs)
          lcd.print(F(" on "));
        else
          lcd.print(F("off "));
      } else if (menuIndex == 2) {
        keyerType -= 1;
#ifdef OPT_BETTER_IAMBIC_A
        if (keyerType < 0) keyerType = 0;
#else
        if (keyerType < 1) keyerType = 1;
#endif
        lcd.print(keyName[keyerType]);
      } else if (menuIndex == 3) {
        wait -= 0.1;
        if (wait < 2.0) wait = 3.0;
        lcd.print(wait);
      } else if (menuIndex == 4) {
        sidetone = !sidetone;
        if (sidetone)
          lcd.print(F(" on "));
        else
          lcd.print(F("off "));
      } else if (menuIndex == 5) {
        defCqInterval -= 200;
        if (defCqInterval < 1000) defCqInterval = 1000;
        lcd.print(defCqInterval);
      } else if (menuIndex == 6) {
        defCqTimes--;
        if (defCqTimes < 3) defCqTimes = 3;
        if (defCqTimes < 10) lcd.print(F("  "));
        else lcd.print(F(" "));
        lcd.print(defCqTimes);
        lcd.print(F(" "));
#ifdef OPT_LCD_DISP
      } else if (menuIndex == menuIndexLcdDisp) {
        lcdDisp = !lcdDisp;
        if (lcdDisp)
          lcd.print(F(" on "));
        else
          lcd.print(F("off "));
#endif
#ifdef OPT_TONE_FREQ
      } else if (menuIndex == menuIndexToneFreq) {
	if (toneFreqIndex > 0)
	  toneFreqIndex--;
	toneFreq = defToneFreq[toneFreqIndex];
	buzzer(pinBz, toneFreq, 100);
	delay(100);
        lcd.print(toneFreq);
	if (toneFreq < 1000)
          lcd.print(F(" "));
#endif
#ifdef OPT_WORD_SPACE
      } else if (menuIndex == menuIndexWordSpace) {
	if (wordSpace > 1)
	  wordSpace--;
        lcd.print(wordSpace);
	if (wordSpace < 10)
          lcd.print(F(" "));
	lcd.print(F("  "));
      } else if (menuIndex == menuIndexCharSpace) {
	if (charSpace > 1)
	  charSpace--;
        lcd.print(charSpace);
	if (charSpace < 10)
          lcd.print(F(" "));
	lcd.print(F("  "));
#endif
#ifdef OPT_OFF_DELAY
      } else if (menuIndex == menuIndexOffDelay) {
        offDelay = !offDelay;
        if (offDelay)
          lcd.print(F(" on "));
        else
          lcd.print(F("off "));
      } else if (menuIndex == menuIndexOffDelayDot) {
	if (off_timer_ratio_dot > 0)
	  off_timer_ratio_dot -= 5;
        lcd.print(off_timer_ratio_dot);
	if (off_timer_ratio_dot < 10)
          lcd.print(F(" "));
	if (off_timer_ratio_dot < 100)
          lcd.print(F(" "));
        lcd.print(F(" "));
      } else if (menuIndex == menuIndexOffDelayDash) {
	if (off_timer_ratio_dash > 0)
	  off_timer_ratio_dash -= 5;
        lcd.print(off_timer_ratio_dash);
	if (off_timer_ratio_dash < 10)
          lcd.print(F(" "));
	if (off_timer_ratio_dash < 100)
          lcd.print(F(" "));
        lcd.print(F(" "));
#ifdef OPT_REPEAT_ADD
      } else if (menuIndex == menuIndexOffRepeatAdd) {
	if (off_repeat_add > 0)
	  off_repeat_add -= 1;
	if (off_repeat_add < 0)
	  off_repeat_add = 0;
        lcd.print(off_repeat_add);
	if (off_repeat_add < 10)
          lcd.print(F(" "));
	if (off_repeat_add < 100)
          lcd.print(F(" "));
        lcd.print(F(" "));
#endif
#endif
#ifdef OPT_ON_DELAY
      } else if (menuIndex == menuIndexOnDelay) {
        onDelay = !onDelay;
        if (onDelay)
          lcd.print(F(" on "));
        else
          lcd.print(F("off "));
      } else if (menuIndex == menuIndexOnDelayDot) {
	if (on_timer_ratio_dot > 0)
	  on_timer_ratio_dot -= 5;
        lcd.print(on_timer_ratio_dot);
	if (on_timer_ratio_dot < 10)
          lcd.print(F(" "));
	if (on_timer_ratio_dot < 100)
          lcd.print(F(" "));
        lcd.print(F(" "));
      } else if (menuIndex == menuIndexOnDelayDash) {
	if (on_timer_ratio_dash > 0)
	  on_timer_ratio_dash -= 5;
        lcd.print(on_timer_ratio_dash);
	if (on_timer_ratio_dash < 10)
          lcd.print(F(" "));
	if (on_timer_ratio_dash < 100)
          lcd.print(F(" "));
        lcd.print(F(" "));
#endif
#ifdef OPT_LOOK_AHEAD_MEMORY
      } else if (menuIndex == menuIndexLookAheadMemory) {
        lookAheadMemory = !lookAheadMemory;
        if (lookAheadMemory)
          lcd.print(F(" on "));
        else
          lcd.print(F("off "));
      } else if (menuIndex == menuIndexLookAheadMemoryRatio) {
	if (look_ahead_memory_ratio > 0)
	  look_ahead_memory_ratio -= 5;
        lcd.print(look_ahead_memory_ratio);
	if (look_ahead_memory_ratio < 10)
          lcd.print(F(" "));
	if (look_ahead_memory_ratio < 100)
          lcd.print(F(" "));
        lcd.print(F(" "));
#endif
#ifdef OPT_TWO_TONE
      } else if (menuIndex == menuIndexTwoTone) {
	twoTone = !twoTone;
        if (twoTone)
          lcd.print(F(" on "));
        else
          lcd.print(F("off "));
#endif
#ifdef OPT_MEMORY_CLICK
      } else if (menuIndex == menuIndexMemoryClick) {
        memoryClick = !memoryClick;
        if (memoryClick)
          lcd.print(F(" on "));
        else
          lcd.print(F("off "));
#endif
#ifdef OPT_AVOID_CHATTERING
      } else if (menuIndex == menuIndexAvoidChattering) {
	avoidChattering = !avoidChattering;
        if (avoidChattering)
          lcd.print(F(" on "));
        else
          lcd.print(F("off "));
      } else if (menuIndex == menuIndexAvoidChatteringCount) {
	if (avoidChatteringCount > 5)
	  avoidChatteringCount -= 5;
        lcd.print(avoidChatteringCount);
	if (avoidChatteringCount < 10)
	  lcd.print(F(" "));
	if (avoidChatteringCount < 100)
	  lcd.print(F(" "));
	lcd.print(F(" "));
#endif
#ifdef OPT_AUTO_KEYERTYPE
      } else if (menuIndex == menuIndexAutoKeyertype) {
	autoKeyerType = !autoKeyerType;
        if (autoKeyerType)
          lcd.print(F(" on "));
        else
          lcd.print(F("off "));
      } else if (menuIndex == menuIndexAutoKeyertypeElekey) {
        if (autoKeyerTypeElekey > 0) {
	  autoKeyerTypeElekey--;
	}
        lcd.print(keyName[autoKeyerTypeElekey]);
#endif
      }
    }
    if (digitalRead(pinSw3) == LOW) {
      beep();
      delay(200);
      lcd.setCursor(2, 1);
      if (menuIndex == 0) {
        if (lang == langWabun) {
          lang = langAbc;
          lcd.print(F("ABC "));
        } else {
          lang = langWabun;
          lcd.write(0b11011100); // wabun
          lcd.write(0b11001100);
          lcd.write(0b11011110);
          lcd.write(0b11011101);
        }
      } else if (menuIndex == 1) {
        paddleRvs = !paddleRvs;
        if (paddleRvs)
          lcd.print(F(" on "));
        else
          lcd.print(F("off "));
      } else if (menuIndex == 2) {
        keyerType += 1;
        if (keyerType > 3) keyerType = 3;
        lcd.print(keyName[keyerType]);
      } else if (menuIndex == 3) {
        wait += 0.1;
        if (wait > 4.0) wait = 4.0;
        lcd.print(wait);
      } else if (menuIndex == 4) {
        sidetone = !sidetone;
        if (sidetone)
          lcd.print(F(" on "));
        else
          lcd.print(F("off "));
      } else if (menuIndex == 5) {
        defCqInterval += 200;
        if (defCqInterval > 9000) defCqInterval = 9000;
        lcd.print(defCqInterval);
      } else if (menuIndex == 6) {
        defCqTimes++;
        if (defCqTimes > 30) defCqTimes = 30;
        if (defCqTimes < 10) lcd.print(F("  "));
        else lcd.print(F(" "));
        lcd.print(defCqTimes);
        lcd.print(F(" "));
#ifdef OPT_LCD_DISP
      } else if (menuIndex == menuIndexLcdDisp) {
        lcdDisp = !lcdDisp;
        if (lcdDisp)
          lcd.print(F(" on "));
        else
          lcd.print(F("off "));
#endif
#ifdef OPT_TONE_FREQ
      } else if (menuIndex == menuIndexToneFreq) {
	if (toneFreqIndex < (sizeof(defToneFreq) / sizeof(int)) - 1)
	  toneFreqIndex++;
	toneFreq = defToneFreq[toneFreqIndex];
	buzzer(pinBz, toneFreq, 100);
	delay(100);
        lcd.print(toneFreq);
	if (toneFreq < 1000)
          lcd.print(F(" "));
#endif
#ifdef OPT_WORD_SPACE
      } else if (menuIndex == menuIndexWordSpace) {
	if (wordSpace < 10)
	  wordSpace++;
        lcd.print(wordSpace);
	if (wordSpace < 10)
          lcd.print(F(" "));
	lcd.print(F("  "));
      } else if (menuIndex == menuIndexCharSpace) {
	if (charSpace < 10)
	  charSpace++;
        lcd.print(charSpace);
	if (charSpace < 10)
          lcd.print(F(" "));
	lcd.print(F("  "));
#endif
#ifdef OPT_OFF_DELAY
      } else if (menuIndex == menuIndexOffDelay) {
        offDelay = !offDelay;
        if (offDelay)
          lcd.print(F(" on "));
        else
          lcd.print(F("off "));
      } else if (menuIndex == menuIndexOffDelayDot) {
	if (off_timer_ratio_dot < MAX_DOT_RATIO)
	  off_timer_ratio_dot += 5;
        lcd.print(off_timer_ratio_dot);
	if (off_timer_ratio_dot < 10)
          lcd.print(F(" "));
	if (off_timer_ratio_dot < 100)
          lcd.print(F(" "));
        lcd.print(F(" "));
      } else if (menuIndex == menuIndexOffDelayDash) {
	if (off_timer_ratio_dash < MAX_DASH_RATIO)
	  off_timer_ratio_dash += 5;
        lcd.print(off_timer_ratio_dash);
	if (off_timer_ratio_dash < 10)
          lcd.print(F(" "));
	if (off_timer_ratio_dash < 100)
          lcd.print(F(" "));
        lcd.print(F(" "));
#ifdef OPT_REPEAT_ADD
      } else if (menuIndex == menuIndexOffRepeatAdd) {
	if (off_repeat_add < MAX_REPEAT_ADD)
	  off_repeat_add += 1;
        lcd.print(off_repeat_add);
	if (off_repeat_add < 10)
          lcd.print(F(" "));
	if (off_repeat_add < 100)
          lcd.print(F(" "));
        lcd.print(F(" "));
#endif
#endif
#ifdef OPT_ON_DELAY
      } else if (menuIndex == menuIndexOnDelay) {
        onDelay = !onDelay;
        if (onDelay)
          lcd.print(F(" on "));
        else
          lcd.print(F("off "));
      } else if (menuIndex == menuIndexOnDelayDot) {
	if (on_timer_ratio_dot < MAX_DOT_RATIO)
	  on_timer_ratio_dot += 5;
        lcd.print(on_timer_ratio_dot);
	if (on_timer_ratio_dot < 10)
          lcd.print(F(" "));
	if (on_timer_ratio_dot < 100)
          lcd.print(F(" "));
        lcd.print(F(" "));
      } else if (menuIndex == menuIndexOnDelayDash) {
	if (on_timer_ratio_dash < MAX_DASH_RATIO)
	  on_timer_ratio_dash += 5;
        lcd.print(on_timer_ratio_dash);
	if (on_timer_ratio_dash < 10)
          lcd.print(F(" "));
	if (on_timer_ratio_dash < 100)
          lcd.print(F(" "));
        lcd.print(F(" "));
#endif
#ifdef OPT_LOOK_AHEAD_MEMORY
      } else if (menuIndex == menuIndexLookAheadMemory) {
        lookAheadMemory = !lookAheadMemory;
        if (lookAheadMemory)
          lcd.print(F(" on "));
        else
          lcd.print(F("off "));
      } else if (menuIndex == menuIndexLookAheadMemoryRatio) {
	if (look_ahead_memory_ratio < MAX_LAM_RATIO)
	  look_ahead_memory_ratio += 5;
        lcd.print(look_ahead_memory_ratio);
	if (look_ahead_memory_ratio < 10)
          lcd.print(F(" "));
	if (look_ahead_memory_ratio < 100)
          lcd.print(F(" "));
        lcd.print(F(" "));
#endif
#ifdef OPT_TWO_TONE
      } else if (menuIndex == menuIndexTwoTone) {
	twoTone = !twoTone;
        if (twoTone)
          lcd.print(F(" on "));
        else
          lcd.print(F("off "));
#endif
#ifdef OPT_MEMORY_CLICK
      } else if (menuIndex == menuIndexMemoryClick) {
        memoryClick = !memoryClick;
        if (memoryClick)
          lcd.print(F(" on "));
        else
          lcd.print(F("off "));
#endif
#ifdef OPT_AVOID_CHATTERING
      } else if (menuIndex == menuIndexAvoidChattering) {
	avoidChattering = !avoidChattering;
        if (avoidChattering)
          lcd.print(F(" on "));
        else
          lcd.print(F("off "));
      } else if (menuIndex == menuIndexAvoidChatteringCount) {
	if (avoidChatteringCount < MAX_CHATTERING_COUNT)
	  avoidChatteringCount += 5;
        lcd.print(avoidChatteringCount);
	if (avoidChatteringCount < 10)
	  lcd.print(F(" "));
	if (avoidChatteringCount < 100)
	  lcd.print(F(" "));
	lcd.print(F(" "));
#endif
#ifdef OPT_AUTO_KEYERTYPE
      } else if (menuIndex == menuIndexAutoKeyertype) {
	autoKeyerType = !autoKeyerType;
        if (autoKeyerType)
          lcd.print(F(" on "));
        else
          lcd.print(F("off "));
      } else if (menuIndex == menuIndexAutoKeyertypeElekey) {
	if (autoKeyerTypeElekey < (sizeof(keyName)/sizeof(char *)-2)) { // except for the last index (straight key)
	  autoKeyerTypeElekey++;
	}
        lcd.print(keyName[autoKeyerTypeElekey]);
#endif
      }
    }
  }

  for (;;) {
    if (digitalRead(pinSw4) == HIGH) break;
  }
  lcd.setCursor(0, 1);
  lcd.print(F("        "));
  beep();
  delay(200);
  setNewWpm(Wpm);
}

void readFromSerial() {
  static bool combineChar = false; // [AR] etc.
  int readChar = 0;
  static int posWriting = 0, bank = -1;
  static bool memoryWriting = false;

  // generator is still working
  if (nextCode != 0x01) return;

  // Read 1 character from serial port
  readChar = Serial.read();
  if (readChar >= 'a' && readChar <= 'z') {
    readChar -= 0x20; // 'a'->'A'
  }
  // Succeeded in reading 1 character from serial port
  if (readChar > 0) {
    //Serial.write(readChar);
    if (IsSendingPresetMessages) {
      // clear preset messages
      if (pos_read > 0) {
        clearPresetMessage();
        clearBeacon();
        return;
      }
      IsSendingPresetMessages = false;
    }
    // stop game
    if (restInvader >= 0) {
      restInvader = 0;
      return;
    }
    if (!memoryWriting && readChar == '[') {
      combineChar = true;
      printascii('[');
    } else if (!memoryWriting && readChar == ']') {
      combineChar = false;
      printascii(']');
    } else if (readChar == '#') {
      memoryWriting = true;
      posWriting = 0;
      bank = -1;
      Serial.println(F("\nwriting mode"));
      Serial.println(F("bank? (1-4)"));
      return;
      // CR or LF
    } else if (readChar == 10 || readChar == 13) {
      return;
    }
    if (memoryWriting) {
      if (bank == -1) { // Start writing to memory
        if (readChar >= '1' && readChar <= '4') {
          bank = readChar - '1';
          Serial.print(F("\nbank = "));
          Serial.println(bank + 1);
          Serial.println(F("type ';' to finish"));
          for (int i = 0; i < 128; i++) EEPROM.write(i + 128 * bank, 0x00);
        } else { // Bank number is wrong
          Serial.println(F("\nbank unknown"));
          memoryWriting = false;
          Serial.print(readChar);
        }
        // Find terminal character
      } else if (readChar == ';' ) {
        Serial.println(F("\nfinish writing"));
        memoryWriting = false;
      } else {
        EEPROM.write(posWriting + 128 * bank, readChar);
        Serial.print(readChar);
        posWriting++;
      }
      return;
    }
  }
  if (readChar > 0 && !memoryWriting) {
    presetMessage[pos_read] = readChar;
  }
}

void setNextChar() {
  static bool combineChar = false; // [AR] etc.
  int readChar = 0;

  if (nextCode == 0x01) {
    // Get next character from preset message
    readChar = presetMessage[pos_read];
    //Serial.write(readChar);
    //Serial.println(pos_read);
    presetMessage[pos_read] = 0;
    if (pos_read < 126) pos_read++;

    // Preset message has not been prepared
    if (readChar == 0) {
      readChar = -1;
      clearPresetMessage();
      //Serial.println(F("no preset messages"));

#ifdef OPT_RUNNER
      if (runner_repeat == 1) {
        trainingMode(pinSw3);
      }
#endif
      // set next beacon message
      if (timeCqInterval > 0) {
        if (timeStartCq == 0) { // 1st time
          timeStartCq = millis();
        } else {
          if ((millis() - timeStartCq) >= timeCqInterval && CqTimes > 1) {
            setPresetMessage(0);
            printascii(SPACE);
            timeStartCq = 0;
            CqTimes--;
            Serial.print(F(" #beacon"));
            Serial.println(CqTimes);
          }
        }
      }
    }
  }

  // Put a new character into the keyer
  if (readChar != -1 && nextCode == 0x01) {
    int i;
    if (lang == langAbc) {
      if (readChar >= 'a' && readChar <= 'z') {
        readChar -= 0x20;
      }
      for (i = 0; i < sizeof(CharAbc) / sizeof(unsigned char); i++) {
        if (readChar == CharAbc[i]) {
          nextCode = BinAbc[i]; // Convert to morse code
          printascii(CharAbc[i]);
        }
      }
      if (readChar == '[') {
        combineChar = true;
        printascii('[');
      } else if (readChar == ']') {
        combineChar = false;
        printascii(']');
#ifdef OPT_TIMING_DEBUG
        Serial.println();
#endif
      }
      switch (readChar) {
        case ' ':
          nextCode = 0xFF;
          printascii(SPACE);
          break;
      }
#ifdef OPT_RUNNER
      if (running && presetMessage[pos_read] == 0) {
        // space after callsign
        printascii(SPACE);
      }
#endif
    } else if (lang == langWabun) {
      for (i = 0; i < sizeof(CharWabun) / sizeof(unsigned char); i++) {
        if (readChar == CharWabun[i]) {
          nextCode = BinKana[i];
          printascii(CharWabun[i]);
          break;
        }
      }
      if (readChar == '[') {
        combineChar = true;
        printascii('[');
      } else if (readChar == ']') {
        combineChar = false;
        printascii(']');
      }
      switch (readChar) {
        case ' ':
          nextCode = 0xFF;
          printascii(SPACE);
          break;
      }
    }
  }
  unsigned int tmpChar, cnt = 0;

  // send space
  if (nextCode == 0xFF) {
#ifdef OPT_WORD_SPACE
    durationSpace = durationDot * wordSpace;
#else
    durationSpace = durationDot * 7;
#endif
    timeStart = millis();
    nextCode = 0x01;
    // An effective code
  } else if (nextCode >= B00000010) {
    tmpChar = nextCode;//, cnt = 0;
    while (tmpChar != 0x01) {
      tmpChar = tmpChar >> 1;
      cnt++;
    }
    tmpChar = nextCode & (0x01 << cnt - 1);
    //cntSpace = 0;
    if (tmpChar > 0) {
      if (durationSpace > 0) {
        memory = R;
      } else {
        durationMark = durationDot * wait;
        digitalWrite(pinLed, HIGH);
        key = DOWN;
        timeStart = millis();
        if (sidetone) tone(pinBz, toneFreq); // side tone
      }
    } else {
      if (durationSpace > 0) {
        memory = L;
      } else {
        durationMark = durationDot;
        digitalWrite(pinLed, HIGH);
        key = DOWN;
        timeStart = millis();
        if (sidetone) tone(pinBz, toneFreq); // side tone
      }
    }
    nextCode = nextCode ^ (0x01 << cnt);
    nextCode = nextCode | (0x01 << cnt - 1);
    if (nextCode == 0x01) {
      if (combineChar) {
        endingChar = 0;
      } else {
        endingChar = 1;
      }
    }
  }
  //Serial.print(F("Ready\n"));
}

void clearPresetMessage() {
  for (int i = 0; i < 128; i++) {
    presetMessage[i] = 0x00;
  }
  nextCode = 0x01;
  pos_read = 0;
}

void clearBeacon() {
  CqTimes = 0;
  timeStartCq = 0;
  timeCqInterval = 0;
}

// preset message have 4 banks (offset 0-3)
void setPresetMessage(int offset) {
  // read messages from eeprom
  for (int i = 0; i < 128; i++) {
    presetMessage[i] = EEPROM.read(i + 128 * offset);
    //Serial.print(presetMessage[i]);
    //Serial.print(F(","));
  }
  // last colum is language option
  if (presetMessage[127] == langAbc ||
      presetMessage[127] == langAbc) {
    lang = presetMessage[127];
  }
}

void invaderAnimation() {
  lcd.setCursor(0, 0);
  lcd.print(F("Ready..3")); // count down 3s
  lcd.setCursor(5, 1);
  lcd.write(0); // invader
  lcd.write(1); // invader
  lcd.write(SPACE);
  delay(500);
  lcd.setCursor(7, 0);
  lcd.print(F("3"));
  lcd.setCursor(4, 1);
  lcd.write(0); // invader
  lcd.write(1); // invader
  lcd.write(SPACE);
  lcd.write(2); // invader
  delay(500);
#ifdef OPT_INVADER_SOUND
  if (invaderSound) {
    beep();
  }
#endif
  lcd.setCursor(7, 0);
  lcd.print(F("2"));
  lcd.setCursor(3, 1);
  lcd.write(0); // invader
  lcd.write(1); // invader
  lcd.write(SPACE);
  lcd.write(2); // invader
  lcd.write(1); // invader
  delay(500);
  lcd.setCursor(7, 0);
  lcd.print(F("2"));
  lcd.setCursor(2, 1);
  lcd.write(0); // invader
  lcd.write(1); // invader
  lcd.write(SPACE);
  lcd.write(2); // invader
  lcd.write(1); // invader
  lcd.write(SPACE);
  delay(500);
#ifdef OPT_INVADER_SOUND
  if (invaderSound) {
    beep();
  }
#endif
  lcd.setCursor(7, 0);
  lcd.print(F("1"));
  lcd.setCursor(1, 1);
  lcd.write(0); // invader
  lcd.write(1); // invader
  lcd.write(SPACE);
  lcd.write(2); // invader
  lcd.write(1); // invader
  lcd.write(SPACE);
  lcd.write(2); // invader
  delay(500);
  lcd.setCursor(7, 0);
  lcd.print(F("1"));
  lcd.setCursor(0, 1);
  lcd.write(0); // invader
  lcd.write(1); // invader
  lcd.write(SPACE);
  lcd.write(2); // invader
  lcd.write(1); // invader
  lcd.write(SPACE);
  lcd.write(2); // invader
  lcd.write(3); // invader
  delay(500);
#ifdef OPT_INVADER_SOUND
  if (invaderSound) {
    tone_invader_start();
    delay(500);
  }
#endif
  lcd.setCursor(0, 0);
  lcd.print(F("        "));
}

void SetupMorseInvader() {
  int i;
  gamePts = 0;
  restInvader = maxInvader;
  combo = 0;
  lcdIndex = 0;
  gameStarted = 0;

  // clear display and buffers
  lcd.setCursor(0, 1);
  for (i = 0; i < colums; i++) {
    line1[i] = SPACE; // fill with space
    lcd.write(line1[i]); // clear colum
  }
  // keying position
  lcd.setCursor(0, 1);
  lcd.write(0b00010000);
}

void morseInvader() {
  // other functoin is running
  if (restInvader == -1) return;

  // this is refresh interval
  static long lastInvaderTime;
  if ((millis() - lastInvaderTime) <= (1500 / maxInvader * restInvader + 500)) {
    return;
  }

  int i;
  lcd.setCursor(1, 1);

  // move left 1 colum
  for (i = 0; i < colums - 1; i++) {
    line1[i] = line1[i + 1];
    if (i < 1) continue; // estimated code
    lcd.write(line1[i]);
  }

  // still invaders are waiting
  if (restInvader > 0) {
    if (lang == langAbc) { // ABC
      line1[colums - 1] = CharAbc[rand() / (RAND_MAX / 26 + 1)];
    } else if (lang == langWabun) { // Wabun
      line1[colums - 1] = CharWabun[rand() / (RAND_MAX / 48 + 1)];
    }
    // no more invaders
  } else {
    line1[colums - 1] = SPACE; // fill with 1 space
  }
  lcd.write(line1[colums - 1]);

  // display pts
  lcd.setCursor(5, 0);
  if (gamePts < 100) lcd.print(F("0"));
  if (gamePts < 10) lcd.print(F("0"));
  lcd.print(gamePts);

  // Game Over
  if (line1[0] != SPACE) {
    lcd.setCursor(0, 1);
    lcd.print(F("GameOver"));
    lcd.noCursor();
    restInvader = -1;
    lastInvaderTime = 0;
    for (i = 0; i < 5; i++) {
      errBeep();
      delay(50);
    }
    // no letters in line1
  } else if (SPACE == (line1[1] | line1[2] | line1[3] | line1[4] | line1[5] | line1[6] | line1[7])) {
    lcd.setCursor(2, 1);
    lcd.print(F("Clear!"));
#ifdef OPT_INVADER_SOUND
    if (invaderSound) {
      tone_invader_clear();
    }
#endif
    restInvader = -1;
    lastInvaderTime = 0;
    // still invaders are waiting
  } else {
    if (restInvader > 0) restInvader--;
    lastInvaderTime = millis();
    if (!gameStarted) {
      lcd.cursor();
      lcd.setCursor(0, 1);
    }
  }
}
