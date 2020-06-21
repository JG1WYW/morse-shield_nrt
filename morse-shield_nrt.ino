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

#define OPT_PADDLE_REVERSE		/* paddle reversed by default */
#define OPT_OFF_DELAY			/* off delay timer for super keyer */
#define OPT_AVOID_CHATTERING		/* indend to avoid chattering */
#define OPT_AUTO_SWITCH_KEYERTYPE	/* adapt to paddle/straight change */
#define OPT_RUNNER			/* morse runner training */
#define OPT_TOOLONG                     /* '_' for too long chars */

// keyer setting
#define MIN_WPM  5
#define MAX_WPM 50
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
#ifdef OPT_PADDLE_REVERSE /* JG1WYW */
int paddleRvs = 1; // paddle reverse (paddle on left hand side)
#else
int paddleRvs = 0; // paddle reverse
#endif

#ifdef OPT_RUNNER /* JG1WYW */
int runnerMode = 1; // Runner mode instead of just callsigns
#endif

#ifdef OPT_OFF_DELAY /* JG1WYW */
int offDelay = 1; // off delay enabled
#endif

#ifdef OPT_AUTO_SWITCH_KEYERTYPE
int count_straight_or_paddle = 0;
int changed_to_straight = 0;
#define MAX_STRAIGHT_OR_PADDLE 12000 /* 3sec */
#endif

// side tone
int sidetone = 1; // 0: disable, 1: enable
int defToneFreq[] = {
  440, 466, 494, 523, 554, 587, 622, 659, 698, 740, 784, 831,
  880, 932, 988, 1047, 1109, 1175, 1245, 1319, 1397, 1480, 1568, 1661
};
int toneFreq = 440; // side tone frequency 440Hz
int beepFreq = 880 * 2; // beep frequency 880Hz

// keyer selection
#define keyIambicA  0
#define keyIambicB  1
#define keySuper    2 // super keyer
#define keyStraight 3 // straight key
#define keyBug      4 // bug key
int keyerType = keySuper;
static int memory = -1;

char *keyName[] {
  "IbcA",
  "IbcB",
  "Supr",
  "Strt"
};

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
int pinBtn[4] = {pinSw1, pinSw2, pinSw3, pinSw4};

// keyer language
#define langAbc   0
#define langWabun 1
int lang = langAbc;

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

long starttimehigh;
long highduration;
long lasthighduration;
#if 1 /* JG1WYW */
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
#if 1 /* JG1WYW */
int wpm = 20;
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
#if defined(OPT_OFF_DELAY) /* JG1WYW */
#define MaxMenu 9
#elif defined(OPT_RUNNER) /* JG1WYW */
#define MaxMenu 8
#else
#define MaxMenu 7
#endif
char *setMenu[] {
  "Lang A/", // ABC or Wabun
  "Pdl Rev.", // paddle reverse
  "Key mode", // keyer mode
  "Wait 1:*", // dit dah wait
  "SideTone", // sidetone on, off / tone[Hz]
  "CQint:ms", // beacon interval
  "CQ times"  // beacon times
#ifdef OPT_RUNNER /* JG1WYW */
  ,
  "Runner  "  // Runner mode
#endif
#ifdef OPT_OFF_DELAY /* JG1WYW */
  ,
  "Offdelay"  // off delay
#endif
};

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

void setup() {
  // The basic goertzel calculation
  int  k;
  float omega;
  k = (int) (0.5 + ((n * target_freq) / sampling_freq));
  omega = (2.0 * PI * k) / n;
  sine = sin(omega);
  cosine = cos(omega);
  coeff = 2.0 * cosine;

  Serial.begin(115200);
  Serial.println(F("morse-shield for Arduino UNO"));
  Serial.println(F("Designed by nksg"));
  Serial.print(F("20"));
  Serial.println(date);

  // Create the random seed
  randomSeed(analogRead(pinVol));
#if 1 /* JG1WYW */
  srand(analogRead(pinVol)); /* seed for rand() */
#endif

  // I/O pins setup
  for (int i = 0; i < 4; i++) pinMode(pinBtn[i], INPUT_PULLUP);
  pinMode(pinPaddleL, INPUT_PULLUP);
  pinMode(pinPaddleR, INPUT_PULLUP);
  pinMode(pinBz, OUTPUT);
  pinMode(pinLed, OUTPUT);

  // clear 4x switchs hold time
  for (int i = 0; i < 4; i++) timeStartSw[i] = 0;

  // Set up the LCD
  lcd.begin();
  // Print messages to the LCD
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
    readFromSerial();
    setNextChar();
  }

  // invader game
  morseInvader();

  // Messages from preset or keyboard
  if (pos_read > 0) return;

  // goertzel Calculation
  goertzel();
}

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
  if ((millis() - laststarttime) > nbtime) {
    if (realstate != filteredstate) {
      filteredstate = realstate;
    }
  }
}

void goertzel() {
  //getMorseTone();
  filteredstate = key; // digitalRead(13);

  // Then we do want to have some durations on high and low
  if (filteredstate != filteredstatebefore) {
    if (filteredstate == HIGH) {
      starttimehigh = millis();
      lowduration = (millis() - startttimelow);
    }

    if (filteredstate == LOW) {
      startttimelow = millis();
      highduration = (millis() - starttimehigh);
      if (highduration < (2 * hightimesavg) || hightimesavg == 0) {
        hightimesavg = (highduration + hightimesavg + hightimesavg) / 3; // now we know avg dit time ( rolling 3 avg)
      }
      if (highduration > (5 * hightimesavg) ) {
        hightimesavg = highduration + hightimesavg;   // if speed decrease fast ..
      }
    }
  }

  // now we will check which kind of baud we have - dit or dah
  // and what kind of pause we do have 1 - 3 or 7 pause
  // we think that hightimeavg = 1 bit
  if (filteredstate != filteredstatebefore) {
    stop = LOW;
#ifdef OPT_TOOLONG /* JG1WYW */
    if (filteredstate == LOW && code < 0b01000000) { // we did end a HIGH
#else
    if (filteredstate == LOW && code < 0b10000000) { // we did end a HIGH
#endif
      if (highduration < (hightimesavg * 2) && highduration > (hightimesavg * 0.6)) { /// 0.6 filter out false dits
        code = code << 1;//strcat(code, ".");
#ifdef SERIAL_PRINT_DIT_DAH
        Serial.print(F("."));
#endif
      }
      if (highduration > (hightimesavg * 2) && highduration < (hightimesavg * 6)) {
        code = code << 1;
        code += 1;
        //strcat(code, "-");
#ifdef SERIAL_PRINT_DIT_DAH
        Serial.print(F("-"));
#endif
        wpm = (wpm + (1200 / ((highduration) / 3))) / 2; // the most precise we can do ;o)
      }
    }
#ifdef OPT_TOOLONG /* JG1WYW */
    else if (filteredstate == LOW) {
      printascii('_');
      code = 1;
    }
#endif

    if (filteredstate == HIGH) { // we did end a LOW

      float lacktime = 1;
      if (wpm > 25)lacktime = 1.0; ///  when high speeds we have to have a little more pause before new letter or new word
      if (wpm > 30)lacktime = 1.2;
      if (wpm > 35)lacktime = 1.5;

      if (lowduration > (hightimesavg * (2 * lacktime)) && lowduration < hightimesavg * (5 * lacktime)) { // letter space
        decode();
        code = 1;
#ifdef SERIAL_PRINT_DIT_DAH
        Serial.print(F("/"));
#endif
      }
      if (lowduration >= hightimesavg * (5 * lacktime)) { // word space
        decode();
        code = 1;
        printascii(SPACE);
#ifdef SERIAL_PRINT_DIT_DAH
        Serial.println();
#endif
      }
    }
  }

  // write if no more letters
  if ((millis() - startttimelow) > (highduration * 6) && stop == LOW) {
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
  updateinfolinelcd();
  realstatebefore = realstate;
  lasthighduration = highduration;
  filteredstatebefore = filteredstate;
}

// Read Potentiometer value and set new WPM
void readPotValue() {
  static int potValueOld = 0;
  int potStep = 10;
  int potValue = analogRead(pinVol);

  if (potValue - potValueOld > potStep || potValue - potValueOld < -potStep) {
    int wpm = map(potValue, 0, 1023 - potStep, MIN_WPM, MAX_WPM);
    if (wpm != Wpm) setNewWpm(wpm);

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

  Serial.print(F("WPM="));
  Serial.println(Wpm);
#ifdef DEBUG
  Serial.print(F("duration dot(ms)="));
  Serial.println(durationDot);
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
  tone(pinBz, beepFreq, 20);
}

void errBeep() {
  tone(pinBz, 220, 20);
}

#ifdef OPT_AVOID_CHATTERING /* JG1WYW */
int paddleL_stable = HIGH;
int paddleR_stable = HIGH;
int paddleL_trans = 0;
int paddleR_trans = 0;

//#define STABLE_COUNT 3
#define STABLE_COUNT 5
#endif

#ifdef OPT_OFF_DELAY /* JG1WYW */
int paddleL_prev = HIGH;
int paddleR_prev = HIGH;
unsigned long paddleL_highStart = 0;
unsigned long paddleR_highStart = 0;

#define OFF_TIMER_RATIO_DASH 150 /* percent of dash () */
//#define OFF_TIMER_RATIO_DASH 125 /* percent of dash () */
//#define OFF_TIMER_RATIO_DASH 120 /* percent of dash (not sufficient) */
//#define OFF_TIMER_RATIO_DASH 90 /* percent of dash (too short) */

//#define OFF_TIMER_RATIO_DOT  150 /* percent of dot (too over repeat) */
//#define OFF_TIMER_RATIO_DOT  120 /* percent of dot (too over repeat) */
//#define OFF_TIMER_RATIO_DOT  110 /* percent of dot (over repeat) */
//#define OFF_TIMER_RATIO_DOT  100 /* percent of dot (over) */
//#define OFF_TIMER_RATIO_DOT  90 /* percent of dot (??) */
//#define OFF_TIMER_RATIO_DOT  80 /* percent of dot () */
//#define OFF_TIMER_RATIO_DOT  75 /* percent of dot () */
#define OFF_TIMER_RATIO_DOT  50 /* percent of dot (a bit insufficient??) */
//#define OFF_TIMER_RATIO_DOT  40 /* percent of dot (over repeat?) */
//#define OFF_TIMER_RATIO_DOT  30 /* percent of dot (still a bit over) */
//#define OFF_TIMER_RATIO_DOT  25 /* percent of dot */
//#define OFF_TIMER_RATIO_DOT  20 /* percent of dot */
//#define OFF_TIMER_RATIO_DOT  15 /* percent of dot (not sufficient) */
//#define OFF_TIMER_RATIO_DOT  10 /* percent of dot */

int paddleR_off_delay; /* off delay ms */
int paddleL_off_delay; /* off delay ms */
#endif

#ifdef OPT_RUNNER /* JG1WYW */
int runner = 0;
unsigned int runner_call[8];
int runner_call_len = 0;
int runner_ptr = 0;
#endif

void readPaddleValue() {
  int paddleL = digitalRead(pinPaddleL);
  int paddleR = digitalRead(pinPaddleR);

#ifdef OPT_AUTO_SWITCH_KEYERTYPE /* JG1WYW */
  /*
   * automatic change between keyStraight and keySuper
   */
  if (keyerType == keySuper) {
    if (paddleL == HIGH && paddleR == LOW) {
      if (++count_straight_or_paddle > MAX_STRAIGHT_OR_PADDLE) {
        keyerType = keyStraight;
        changed_to_straight = 1;
        count_straight_or_paddle = 0;
      }
    } else {
      count_straight_or_paddle = 0;
    }
  }
  if (keyerType == keyStraight) {
    if (changed_to_straight == 1 && paddleL == HIGH && paddleR == HIGH) {
      if (++count_straight_or_paddle > MAX_STRAIGHT_OR_PADDLE) {
        keyerType = keySuper;
        changed_to_straight = 0;
        count_straight_or_paddle = 0;
      }
    } else {
      count_straight_or_paddle = 0;
    }
  }
#endif /* JG1WYW */

#ifdef OPT_AVOID_CHATTERING /* JG1WYW */
  /*
   * avoid chattering, paddle R
   */
  if (paddleR == LOW && paddleR_stable == HIGH) {
    if (++paddleR_trans >= STABLE_COUNT) {
      paddleR_stable = LOW;
      paddleR_trans = 0;
    } else {
      /* stays HIGH until stable count reach */
      paddleR = HIGH;
    }
  } else if (paddleR == HIGH && paddleR_stable == LOW) {
    if (--paddleR_trans <= -STABLE_COUNT) {
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
    if (++paddleL_trans >= STABLE_COUNT) {
      paddleL_stable = LOW;
      paddleL_trans = 0;
    } else {
      /* stays HIGH until stable count reach */
      paddleL = HIGH;
    }
  } else if (paddleL == HIGH && paddleL_stable == LOW) {
    if (--paddleL_trans <= -STABLE_COUNT) {
      paddleL_stable = HIGH;
      paddleL_trans = 0;
    } else {
      /* stays LOW until stable count reach */
      paddleL = LOW;
    }
  }
#endif /* OPT_AVOID_CHATTERING */

#ifdef OPT_OFF_DELAY /* JG1WYW */
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
        paddleR_off_delay = durationDot * OFF_TIMER_RATIO_DOT / 100;
      } else {
        /* normal DASH */
        paddleR_off_delay = durationDot * OFF_TIMER_RATIO_DASH / 100;
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
        paddleL_off_delay = durationDot * OFF_TIMER_RATIO_DASH / 100;
      } else {
        /* normal DOT */
        paddleL_off_delay = durationDot * OFF_TIMER_RATIO_DOT / 100;
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
#endif /* JG1WYW */

#if 1 /* JG1WYW */
  if (paddleR == LOW && keyerType == keyStraight) {
    /* ignore R when straight key */
    paddleR = HIGH;
  }
#endif

  // clear preset messages
  if ((paddleL == LOW || paddleR == LOW) && pos_read > 0) {
    clearPresetMessage();
    clearBeacon();
    return;
  }

  // pressing both paddles
  if (paddleL == LOW && paddleR == LOW && durationSpace == 0) {
    if (durationMark == 0 && keyerType != keyStraight) {
      if (lastPaddle != L) {
        durationMark = !paddleRvs ? durationDot : durationDot * wait;
        lastPaddle = L;
      } else {
        durationMark = !paddleRvs ? durationDot * wait : durationDot;
        lastPaddle = R;
      }
      digitalWrite(pinLed, HIGH);
      key = DOWN;
      timeStart = millis();
      if (sidetone) tone(pinBz, toneFreq); // side tone
    }
    // pressing the L paddle
  } else if (paddleL == LOW && durationSpace == 0) {
    if (durationMark == 0) {
      if (keyerType == keyStraight) {
        durationMark = 10000; // key down straight key
      } else {
        durationMark = !paddleRvs ? durationDot : durationDot * wait;
      }
      lastPaddle = L;
      digitalWrite(pinLed, HIGH);
      key = DOWN;
      timeStart = millis();
      if (sidetone) tone(pinBz, toneFreq); // side tone
    }
    // pressing the R paddle
  } else if (paddleR == LOW && durationSpace == 0) {
    if (durationMark == 0 && keyerType != keyStraight) {
      durationMark = !paddleRvs ? durationDot * wait : durationDot;
      lastPaddle = R;
      digitalWrite(pinLed, HIGH);
      key = DOWN;
      timeStart = millis();
      if (sidetone) tone(pinBz, toneFreq); // side tone
    }
  }

  if (durationSpace != 0) {
    if (keyerType == keyIambicB || keyerType == keySuper) {

      // memory paddle LR
      if (paddleL == LOW && lastPaddle == R) {
        memory = L;
      } else if (paddleR == LOW && lastPaddle == L) {
        memory = R;
      }
    }
    // ending of space
    if (millis() - timeStart > durationSpace) {
      durationSpace = 0;
      // output memory LR
      if (memory != -1) {
        if (memory == L) {
          durationMark = !paddleRvs ? durationDot : durationDot * wait;
          lastPaddle = L;
        } else if (memory == R) {
          durationMark = !paddleRvs ? durationDot * wait : durationDot;
          lastPaddle = R;
        }
        memory = -1;
        digitalWrite(pinLed, HIGH);
        key = DOWN;
        timeStart = millis();
        if (sidetone) tone(pinBz, toneFreq); // side tone
      }
    }
  }
  if (durationMark != 0) {
    // memory paddle LR
    if (keyerType == keyIambicB || ((keyerType == keySuper) && (millis() - timeStart > durationDot))) {
      if (paddleL == LOW && lastPaddle == R) {
        memory = L;
      } else if (paddleR == LOW && lastPaddle == L) {
        memory = R;
      }
    }
    // ending of mark
    if (millis() - timeStart > durationMark) {
      durationSpace = endingChar ? durationDot * 3 : durationDot;
      durationMark = 0;
      endingChar = 0;
      noTone(pinBz);
      timeStart = millis();
      digitalWrite(pinLed, LOW);
      key = UP;
    }
    // key up straight key
    if (keyerType == keyStraight) {
#if 1 /* JG1WYW */
      if (paddleL == HIGH && (pos_read == 0) && (millis() - timeStart > durationDot)) { /* pos_read condition prevents from all dots message output */
#else
      if (paddleL == HIGH && (millis() - timeStart > durationDot)) {
#endif
        durationSpace = durationDot;
        durationMark = 0;
        noTone(pinBz);
        timeStart = millis();
        digitalWrite(pinLed, LOW);
        key = UP;
      }
    }
  }
}

void readButtonValue() {
  int sw[4];

  for (int i = 0; i < 4; i++) {
    sw[i] = digitalRead(pinBtn[i]);
    // button was pressed
    if (sw[i] == LOW && timeStartSw[i] == 0) {
      //digitalWrite(pinLed, HIGH);
      timeStartSw[i] = millis();
      beep();
      // button was released
    } else if (sw[i] == HIGH && timeStartSw[i] != 0) {
      //digitalWrite(pinLed, LOW);
      if (millis() - timeStartSw[i] > durationLongPress)
        fireLongFunction(i); // long press
      else
        fireFunction(i); // short press
      timeStartSw[i] = 0;
      // detect long press
    } else if (millis() - timeStartSw[i] > durationLongPress && millis() - timeStartSw[i] < durationLongPress + 10) {
      //digitalWrite(pinLed, LOW);
      beep();
    }
  }
}

void fireFunction(int no) {
  Serial.println();
  Serial.print(F("Press="));
  Serial.println(no + 1);

#ifdef OPT_RUNNER /* JG1WYW */
  runner = 0; /* pushing button to disable runner */
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

#ifdef OPT_RUNNER /* JG1WYW */
  runner = 0; /* pushing button to disable runner */
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
      if (runnerMode) {
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
        if ((millis() - lasttime) > 1000) {
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
          if (runnerMode) {
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
          invaderAnimation();
          break;
        }
      }
      lcd.setCursor(0, 1);
      lcd.print(F("        "));
      beep();
      delay(200);
      for (;;) {
        if (digitalRead(pressedSw) == HIGH) break;
      }
      trainingMode(pressedSw);
      break;
    case 2: // record messages
      //ToDo: save original messages on EEPROM
      Serial.println(F("recording mode..."));
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
        Serial.print(presetMessage[k]);
        Serial.print(F(","));
        k++;
      }
      Serial.println();
      presetMessage[k] = ' ';
      k++;
    }
  } else if (pressedSw == pinSw3) {
    // 10 random callsigns JA
    lang = langAbc;
    for (i = 0; i < 10; i++) {
#if 1 /* JG1WYW */
     if (rand() / (RAND_MAX / 4) == 0) {
      presetMessage[k] = '7' + rand() / (RAND_MAX / 2 + 1); /* 7-8 */
      k++;
      presetMessage[k] = 'J' + rand() / (RAND_MAX / 5 + 1); /* J-N */
      k++;
     } else {
#endif
      presetMessage[k] = 'J';
      k++;
      presetMessage[k] = 'A' + rand() / (RAND_MAX / 17 + 1) ; /* JA, JD-JS */
      if (presetMessage[k] == 'B' || presetMessage[k] == 'C') {
        presetMessage[k] += 16;
      }
      k++;
#if 1 /* JG1WYW */
     }
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
      for (j = 0; j < 3; j++) {
        presetMessage[k] = 'A' + rand() / (RAND_MAX / 26 + 1) ;
        k++;
      }
#ifdef OPT_RUNNER /* !OPT_RUNNDER */
      if (runnerMode) { /* runner */
        int idx;

	runner = 1;
	runner_ptr = 0;

        if (rand() / (RAND_MAX / 5 + 1) == 0) {
          presetMessage[k] = '/'; /* Portable */
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
#ifdef OPT_RUNNER /* JG1WYW */
            if (runner == 1) {
              if (code == 0b01001100) { /* ? */
	        int idx;
		clearPresetMessage();
		for (idx = 0 ; idx < runner_call_len ; idx++) {
		  presetMessage[idx] = runner_call[idx];
		}
		runner_ptr = 0;
		printascii(SPACE);
              } else if (CharAbc[i] == runner_call[runner_ptr]) {
	        if (++runner_ptr == runner_call_len) {
                  tone(pinBz, 720, 100);
		  delay(100);
                  tone(pinBz, 1440, 200);
		  delay(1000);
		  printascii(SPACE);
                  trainingMode(pinSw3);
		}
	      } else {
	        tone(pinBz, 220, 100);
		delay(100);
	        tone(pinBz, 110, 200);
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
                for (k = j; k > 1; k--) {
                  line1[k] = line1[k - 1];
                  lcd.setCursor(k, 1);
                  lcd.write(line1[k]);
                }
                break;
              }
            }
            if (j == colums) combo = 0; // wrong
          }
          break;
        }
      }
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
            for (j = 1; j < colums - 1; j++) {
              if (line1[j] == CharWabun[i]) {
                gamePts += (1 + combo);
                if (combo < 4) combo++;
                lcd.write(SPACE);
                for (k = j; k > 1; k--) {
                  line1[k] = line1[k - 1];
                  lcd.setCursor(k, 1);
                  lcd.write(line1[k]);
                }
                break;
              }
            }
          }
          break;
        }
      }
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

  Serial.write(asciinumber); // Monitor characters

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
  static int oldWpm;
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
  int menuIndex = -1;

  lcd.setCursor(0, 0);
  lcd.print(F("Set Mode"));
  for (;;) {
    if (digitalRead(pinSw4) == LOW) break;
    if (digitalRead(pinSw1) == LOW) {
      beep();
      menuIndex++;
      if (menuIndex == MaxMenu) menuIndex = 0;
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
#ifdef OPT_RUNNER /* JG1WYW */
      } else if (menuIndex == 7) {
        if (runnerMode)
          lcd.print(F(" on "));
        else
          lcd.print(F("off "));
#endif
#ifdef OPT_OFF_DELAY /* JG1WYW */
      } else if (menuIndex == 8) {
        if (offDelay)
          lcd.print(F(" on "));
        else
          lcd.print(F("off "));
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
        if (keyerType < 0) keyerType = 0;
        lcd.print(keyName[keyerType]);
      } else if (menuIndex == 3) {
        wait -= 0.1;
        if (wait < 3.0) wait = 3.0;
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
#ifdef OPT_RUNNER /* JG1WYW */
      } else if (menuIndex == 7) {
        runnerMode = !runnerMode;
        if (runnerMode)
          lcd.print(F(" on "));
        else
          lcd.print(F("off "));
#endif
#ifdef OPT_OFF_DELAY /* JG1WYW */
      } else if (menuIndex == 8) {
        offDelay = !offDelay;
        if (offDelay)
          lcd.print(F(" on "));
        else
          lcd.print(F("off "));
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
#ifdef OPT_RUNNER /* JG1WYW */
      } else if (menuIndex == 7) {
        runnerMode = !runnerMode;
        if (runnerMode)
          lcd.print(F(" on "));
        else
          lcd.print(F("off "));
#endif
#ifdef OPT_OFF_DELAY /* JG1WYW */
      } else if (menuIndex == 8) {
        offDelay = !offDelay;
        if (offDelay)
          lcd.print(F(" on "));
        else
          lcd.print(F("off "));
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

      // set next beacon message
      if (timeCqInterval > 0) {
        if (timeStartCq == 0) { // 1st time
          timeStartCq = millis();
        } else {
          if ((millis() - timeStartCq) > timeCqInterval && CqTimes > 1) {
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
      }
      switch (readChar) {
        case ' ':
          nextCode = 0xFF;
          printascii(SPACE);
          break;
      }
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
    durationSpace = durationDot * 7;
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
