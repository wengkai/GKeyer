///////////////////////////////////////////////////////////////////////////////
//
//  Golf Morse Code Keyer Sketch
//  ex. Iambic Morse Code Keyer Sketch
//  Copyright (c) 2009 Steven T. Elliott
//  Copyright (c) 2013 Weng Kai
//
//  This library is free software; you can redistribute it and/or
//  modify it under the terms of the GNU Lesser General Public
//  License as published by the Free Software Foundation; either
//  version 2.1 of the License, or (at your option) any later version.
//
//  This library is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
//  Lesser General Public License for more details:
//
//  Free Software Foundation, Inc., 59 Temple Place, Suite 330,
//  Boston, MA  02111-1307  USA
//
//  http://openqrp.org/?p=343
//
//  "Trimmed" by Bill Bishop - wrb[at]wrbishop.com
//  "Remixed" By Weng Kai - ba5ag[at]qq.com
//
///////////////////////////////////////////////////////////////////////////////
//
//                         openQRP CPU Pin Definitions
//
///////////////////////////////////////////////////////////////////////////////

#include <EEPROM.h>

//
// Digital Pins
//
const int tonePin  = 15;  // Tone output pin
const int LPin     = 2;   // Left paddle input
const int RPin     = 3;   // Right paddle input
const int rLedPin  = 5;   // Red LED
const int gLedPin  = 6;   // Greed LED
const int pttPin   = 16;  // PTT
const int keyPin   = 17;  // key out
//
////////////////////////////////////////////////////////////////////////////////
//
//  keyerControl bit definitions
//
#define DIT_L     0x01  //  Dit latch
#define DAH_L     0x02  //  Dah latch
#define DIT_PROC  0x04  //  Dit is being processed
#define PDLSWAP   0x08  //  0 for normal, 1 for swap
#define IAMBICB   0x10  //  0 for Iambic A, 1 for Iambic B
#define STRAITKEY 0x20  //  0 for paddle, 1 for strait key
#define PTT_DELAY 6     //  6 times dit time
#define DEBOUNCE  20
#define BUFFER_SIZE 64
#define KEY_WD_TIMER  100*1000L

//
////////////////////////////////////////////////////////////////////////////////
//
//  Library Instantiations
//
////////////////////////////////////////////////////////////////////////////////
//
///////////////////////////////////////////////////////////////////////////////
//
//  Global Variables
//

unsigned long ditTime;        // No. milliseconds per dit
unsigned char keyerControl;
unsigned char keyerState;
unsigned char note = 600;
boolean       isPTTDown = false;
unsigned int  pttdelay = 500;
unsigned int  rtxdelay = 175;
int           speedPot;
boolean       isSpeedOverride = false;  //  true if speed is set by host
int           weight = 50;
boolean       isPause = false;  //  true to pause the queue
byte          currentWPM = 25;
byte          buffer[BUFFER_SIZE];
byte          bufLocGet = 0;
byte          bufLocPut = 0;
boolean       isKeyDown = false;
long          ktimer;  //  timer for keydown
long          ptimer;  //  timer for ptt
long          rtimer;  //  timer for rx/tx trans
byte          minWPM = 5;
byte          maxWPM = 45;
byte          farmWPM = 0;
short         potRange = 255;
boolean       isCutMode = false;
float         spaceRatio = 1.0;
boolean       isSidetone = true;
long          keyWDTimer;
byte          wk2mode = 0;
byte          firstExtensionTime = 0;
byte          keyingCompensation = 0;
float         didahRatio = 3.0;

enum {
  WKM_CTSPACE   = 0x01,
  WKM_AUTOSAPCE = 0x02,
  WKM_ECHO      = 0x04,
  WKM_PADDLESWAP= 0x08,
  WKM_PADDLEECHO= 0x40,
  WKM_NOPADDLEWD= 0x80,
};

struct {
  byte len;
  byte code;
} MorseCode[] = {
  {6, B10101100}, //  ! = 0x21
  {6, B01001000}, //  "
  {0, 0}, //  #
  {7, B00010010}, //  $
  {0, 0}, //  %
  {5, B01000000}, //  &
  {6, B01111000}, //  '
  {5, B10110000}, //  (
  {6, B10110100}, //  )
  {0, 0}, //  *
  {5, B01010000}, //  +
  {6, B11001100}, //  ,
  {6, B10000100}, //  -
  {6, B01010100}, //  .
  {5, B10010000}, //  /
  {5, B11111000}, //  0
  {5, B01111000}, //  1
  {5, B00111000}, //  2
  {5, B00011000}, //  3
  {5, B00001000}, //  4
  {5, B00000000}, //  5
  {5, B10000000}, //  6
  {5, B11000000}, //  7
  {5, B11100000}, //  8
  {5, B11110000}, //  9
  {5, B10110000}, //  :
  {4, B10100000}, //  ;
  {5, B01010000}, //  <
  {5, B10001000}, //  =
  {6, B00010100}, //  >
  {6, B00110000}, //  ?
  {6, B01101000}, //  @
  {2, B01000000}, //  A
  {4, B10000000}, //  B
  {4, B10100000}, //  C
  {3, B10000000}, //  D
  {1, B00000000}, //  E
  {4, B00100000}, //  F
  {3, B11000000}, //  G
  {4, B00000000}, //  H
  {2, B00000000}, //  I
  {4, B01110000}, //  J
  {3, B10100000}, //  K
  {4, B01000000}, //  L
  {2, B11000000}, //  M
  {2, B10000000}, //  N
  {3, B11100000}, //  O
  {4, B01100000}, //  P
  {4, B11010000}, //  Q
  {3, B01000000}, //  R
  {3, B00000000}, //  S
  {1, B10000000}, //  T
  {3, B00100000}, //  U
  {4, B00010000}, //  V
  {3, B01100000}, //  W
  {4, B10010000}, //  X
  {4, B10110000}, //  Y
  {4, B11000000}, //  Z
  {0, 0}, //  [
  {0, 0}, //  \
  {0, 0}, //  ]
  {0, 0}, //  ^
  {6, B00110100}, //  _
  {6, B11001100}, //  ` as ' = 0x60
};

enum KSTYPE {
  IDLE, CHK_DIT, CHK_DAH, KEYED_TRANS, KEYED, 
  INTER_ELEMENT, SK_DOWN 
};

enum AKSTATE {
  AK_NONE, AK_KD, AK_KU, AK_INTER,
};

byte akState = AK_NONE;
byte akChar;
byte akBit;
byte akMask;
byte akEcho;
long akTimer;

///////////////////////////////////////////////////////////////////////////////
//
//  System Initialization
//
///////////////////////////////////////////////////////////////////////////////

void setup() {
  boolean isStrait = false;
    
  // Setup outputs
  pinMode(rLedPin, OUTPUT);     // sets the digital pin as output
  pinMode(gLedPin, OUTPUT);
  
  // Setup control input pins
  pinMode(LPin, INPUT_PULLUP);  // sets Left Paddle digital pin as input with pullup
  pinMode(RPin, INPUT_PULLUP);  // sets Right Paddle digital pin as input with pullup
  pinMode(pttPin, OUTPUT);    
  digitalWrite(pttPin, LOW);    // ptt off
  digitalWrite(rLedPin, HIGH);  // turn the LED off
  digitalWrite(gLedPin, HIGH);  // turn the LED off
  
  if ( digitalRead(LPin) == LOW ) {
    isStrait = true;
  }
  
  bufPut('R');
 
  if ( digitalRead(LPin) == HIGH )
    isStrait = false;
  
  Serial.begin(9600);
  
  keyerState = IDLE;
  keyerControl = IAMBICB;      // Or 0 for IAMBICA
  if ( isStrait )
    keyerControl |= STRAITKEY;
  
  checkSpeed();
  
  prompt();
}
  
///////////////////////////////////////////////////////////////////////////////
//
//  Main Work Loop
//
///////////////////////////////////////////////////////////////////////////////

void loop()
{
  // Basic Iambic Keyer
  // keyerControl contains processing flags and keyer mode bits
  // Supports Iambic A and B
  // State machine based, uses calls to millis() for timing.
  checkSpeed();
  checkSerial();
  if ( isKeyDown ) {
    if ( millis() > keyWDTimer ) {
      keyUp();
      pttUp();
    }
  }
  switch (keyerState) {
  case IDLE:
    checkAK();
    // Wait for direct or latched paddle press
    if ( keyerControl & STRAITKEY ) {
      if ( digitalRead(RPin) == LOW ) {
        if ( !isPTTDown ) {
          pttDown();
        }
        keyDown();
        ktimer = millis()+DEBOUNCE;
        keyerState = SK_DOWN;
      }
    } else if ((digitalRead(LPin) == LOW) ||
        (digitalRead(RPin) == LOW) ||
        (keyerControl & 0x03) ) {
      if ( isKeyDown ) {
        keyUp();
      }
      update_PaddleLatch();
      keyerState = CHK_DIT;
    }
    if ( isPTTDown && (millis() >= ptimer) ) {
      pttUp();
    }
    break;
  case SK_DOWN:
    if ( millis() > ktimer && digitalRead(RPin) == HIGH ) {
      keyUp();
      ptimer = millis()+pttdelay;
      keyerState = IDLE;
    }
    break;
  case CHK_DIT:
    // See if the dit paddle was pressed
    if (keyerControl & DIT_L) {
      keyerControl |= DIT_PROC;
      ktimer = ditTime * (weight/50.0);
      keyedPrep();
      keyerState = KEYED_TRANS;
    }
    else {
      keyerState = CHK_DAH;
    }
    break;
  case CHK_DAH:
    // See if dah paddle was pressed
    if (keyerControl & DAH_L) {
      ktimer = ditTime * didahRatio * (weight/50.0);
      keyedPrep();
      keyerState = KEYED_TRANS;
    }
    else {
      keyerState = IDLE;
      ptimer = millis()+pttdelay;
    }
    break;
  case KEYED_TRANS:
    if ( isPTTDown || millis() >= rtimer+rtxdelay ) {
      isPTTDown = true;
      keyDown();
      ktimer += millis();                 // set ktimer to interval end time
      keyerControl &= ~(DIT_L + DAH_L);   // clear both paddle latch bits
      keyerState = KEYED;                 // next state
    }
    break;
  case KEYED:
    // Wait for timer to expire
    if (millis() > ktimer) {            // are we at end of key down ?
      keyUp();
      ktimer = millis() + ditTime 
        * (2-weight/50.0);    // inter-element time
      keyerState = INTER_ELEMENT;     // next state
    }
    else if (keyerControl & IAMBICB) {
      update_PaddleLatch();           // early paddle latch in Iambic B mode
    }
    break;
  case INTER_ELEMENT: // Insert time between dits/dahs
    update_PaddleLatch();               // latch paddle state
    if (millis() > ktimer) {            // are we at end of inter-space ?
      if (keyerControl & DIT_PROC) {             // was it a dit or dah ?
          keyerControl &= ~(DIT_L + DIT_PROC);   // clear two bits
          keyerState = CHK_DAH;                  // dit done, check for dah
      }
      else {
        keyerControl &= ~(DAH_L);              // clear dah latch
        keyerState = IDLE;                     // go idle
        ptimer = millis()+pttdelay;
      }
    }
    break;
  }
}
      
void pttDown()
{
  // Serial.println("ptt down");
  digitalWrite(pttPin, LOW);         // enable PTT
  digitalWrite(gLedPin, LOW);
  isPTTDown = true;
  ptimer = millis() + 1e10;
} 

void pttUp()
{
  // Serial.println("ptt up");
  digitalWrite(pttPin, HIGH);  // turn PTT off
  digitalWrite(gLedPin, HIGH);
  isPTTDown = false;
} 

void keyDown()
{
  digitalWrite(rLedPin, LOW);         // turn the LED on
  digitalWrite(keyPin, HIGH); 
  if ( isSidetone) tone( tonePin, note );
  if ( !isKeyDown ) {
    isKeyDown = true;
    keyWDTimer = millis() + KEY_WD_TIMER;
  }
}

void keyUp()
{
  digitalWrite(rLedPin, HIGH);      // turn the LED off
  digitalWrite(keyPin, HIGH); 
  noTone( tonePin );
  isKeyDown = false;
}

void checkAK()
{
  int ch;
  switch ( akState ) {
  case AK_NONE:
    if ( isPause ) return;
    ch = bufGet();
    if ( ch >-1 ) {
      if ( ch >= 'a' && ch <= 'z' ) {
        ch = ch - ('a'-'A');
      }
      if ( ch == ' ' || ch == '|' ) {
        int basetime;
        if ( farmWPM == 0 ) {
          basetime = ditTime * (2-weight/50.0);
        } else {
          basetime = 1200/farmWPM * (2-weight/50.0);
        }
        if ( ch == ' ' ) {
          akTimer = millis() + 
            (wk2mode&WKM_CTSPACE?6:7)
            * basetime;
        } else {
          akTimer = millis() + 0.5 * basetime;
        }
        akState = AK_INTER;
      } else {
        akEcho = ch;
        if ( isCutMode ) {
          if ( ch == '0' )  ch = 'T';
          else if ( ch == '9' ) ch = 'N';
        }
        akChar = ch - 0x21;
        if ( MorseCode[akChar].len >0 ) {
          akBit = 1;
          akMask = 0x80;
          pttDown();
          keyDown();
          calAKTimer();
          akTimer += firstExtensionTime;
          akState = AK_KD;
        }
      }
    } 
    break;
  case AK_KD:
    if ( millis() >= akTimer ) {
      keyUp();
      akTimer = millis() + 
        spaceRatio * ditTime * (2-weight/50.0)-
        keyingCompensation;
      akState = AK_KU;
    }
    break;
  case AK_KU:
    if ( millis() >= akTimer ) {
      akBit++;
      akMask >>= 1;
      if ( akBit > MorseCode[akChar].len ) {
        long space;
        if ( farmWPM == 0 ) {
          space = 2 * ditTime * (2-weight/50.0);
        } else {
          space = 3 * 1200/farmWPM * (2-weight/50.0) 
            - ditTime * (2-weight/50.0);
        }
        akTimer = millis() + space;
        akState = AK_INTER;
        if ( wk2mode & WKM_ECHO ) {
          Serial.write(akEcho);
        }
      } else {
        keyDown();
        calAKTimer();
        akState = AK_KD;
      }
    }
    break;
  case AK_INTER:
    if ( millis() >= akTimer ) {
      ptimer = millis() + pttdelay;
      akState = AK_NONE;
    }
    break;
  }
}

void calAKTimer()
{
  if ( MorseCode[akChar].code & akMask ) {
    // Serial.print("-");
    akTimer = millis() + 
      didahRatio * ditTime * (weight/50.0) +
      keyingCompensation;
  } else {
    // Serial.print(".");
    akTimer = millis() + 
      ditTime * (weight/50.0) +
      keyingCompensation;
  }
}

void update_PaddleLatch()
{
  if (digitalRead(RPin) == LOW) {
    keyerControl |= DIT_L;
  }
  if (digitalRead(LPin) == LOW) {
    keyerControl |= DAH_L;
  }
}

///////////////////////////////////////////////////////////////////////////////
//
//    Calculate new time constants based on wpm value
//
///////////////////////////////////////////////////////////////////////////////

void checkSpeed()
{
  if ( !isSpeedOverride ) {
    int a0;
    a0 = analogRead(A0);
    if ( abs(speedPot - a0) >20 ) {
      int wpm;
      speedPot = a0;
      a0 = map(a0, 0, 1023, 0, potRange *4 );
      wpm = map(a0, 0, 1023, minWPM, maxWPM);
      setSpeed(wpm);
      pttdelay = PTT_DELAY * ditTime;
      if ( wpm >= 30 ) 
        rtxdelay = 5;
      else 
        rtxdelay = 175;
    }
  }
}

void setSpeed(byte wpm)
{
  currentWPM = wpm;
  ditTime = 1200/wpm;
}

void keyedPrep()
{
  if ( !isPTTDown ) {
    pttDown();
  }
  rtimer = millis();
}

void prompt()
{
  Serial.println("GKeyer v0.1\t(C)BA5AG 2013");
  if ( keyerControl & STRAITKEY ) {
    Serial.print("Strait Key Mode");
  } else {
    Serial.print("Paddle Iambic Mode ");
    if ( keyerControl & IAMBICB )
      Serial.print("B");
    else
      Serial.print("A");
  }
  Serial.println();
}

void bufLocGetInc()
{
  bufLocGet++;
  if ( bufLocGet == BUFFER_SIZE )
    bufLocGet = 0;
}

void bufLocGetDec()
{
  bufLocGet--;
  if ( bufLocGet <0 )
    bufLocGet = BUFFER_SIZE -1;
}

void bufPut(byte b)
{
  buffer[bufLocPut++] = b;
  if ( bufLocPut == BUFFER_SIZE ) {
    bufLocPut = 0;
  }
  //  TODO: roll back to LocGet
}

//  return -1 if no char in buffer
int bufGet()
{
  int ret = -1;
  if ( bufLocGet != bufLocPut ) {
    ret = buffer[bufLocGet];
    bufLocGetInc();
  } 
  return ret;
}

void setPaddle(byte conf)
{
  if ( conf & WKM_PADDLESWAP ) 
    keyerControl |= PDLSWAP;
  else
    keyerControl &= ~PDLSWAP;
  conf = conf & 0x30;
  switch ( conf ) {
  case 0x00:
    keyerControl |= IAMBICB;
    keyerControl &= ~STRAITKEY;
    break;
  case 0x01:
    keyerControl &= ~IAMBICB;
    keyerControl &= ~STRAITKEY;
    break;
  case 0x11:
    keyerControl |= STRAITKEY;
    break;
  }
}

void checkSerial()
{
  static byte serialLen = 0;
  static byte serialBuf[16];
  static byte serialLoc = 0;
  static byte cmdLen[32] = {
    1, 1, 1, 1, 2, 3, 1, 0, 0, 1, 0, 1, 1, 1, 1, 15, 
    1, 1, 1, 0, 1, 0, 1, 1, 1, 1, 1, 2, 1, 1, 0, 0 
  };  //  number of bytes after the first byte
  static void (*cmdfun[])(byte buf[]) = {
    //  00-03
    cmdAdmin, cmdSidetone, cmdSetSpeed, cmdWeight, 
    //  04-07
    cmdPTTTime, cmdSpeedRange, cmdPause, cmdGetSpeed, 
    //  08-0B
    cmdBackspace, cmdPinConfig, cmdClearBuffer,
    cmdKeyImd, 
    //  0C-0F
    cmdHSCW, cmdFarnWPM, cmdSetMode, cmdDefault,
    //  10-13
    cmdSetExt, cmdKeyComp, cmdPadSw, cmdNOP,
    //  14-17
    cmdSoftPad, cmdGetStatus, cmdPointer, cmdSetRatio,
  };

  if ( Serial.available() >0 ) {
    byte ch = Serial.read();
    if ( serialLoc == 0 ) {
      if ( ch >=0 && ch < 32 ) {
        serialLen = cmdLen[ch];
        serialBuf[serialLoc++] = ch;
      } else if ( ch >= 0x20 && ch <= 0x80 ) {
        bufPut(ch);
      }
    } else {
      serialBuf[serialLoc++] = ch;
      if  ( serialBuf[0] == 0x16 && serialLoc == 2 && ch == 0x03 ||
            serialBuf[0] == 0x00 && serialLoc == 2 && ch == 0x00 ||
            serialBuf[0] == 0x00 && serialLoc == 2 && ch == 0x0e ||
            serialBuf[0] == 0x00 && serialLoc == 2 && ch == 0x0f ||
            serialBuf[0] == 0x00 && serialLoc == 2 && ch == 0x04 
          ) {
        serialLen++;
      }
      serialLen--;
    }
    if ( serialLoc > 0 && serialLen == 0 ) {
      if ( serialBuf[0] < 0x18 ) {
        (*cmdfun[serialBuf[0]])(serialBuf+1);
      } else {
        //  put it into buffer
      }
      serialLoc = 0;
    }
  } //  if Serial.available() >0
}

void cmdAdmin(byte buf[]) 
{
  int i;
  switch (buf[0]) {
    case 0x00:break;  //  do nothing for calibrating
    case 0x01:break;  //  reset, can't do now
    case 0x02:        //  host on, return version to host
      Serial.write(23); //  version 2.3
      break;  
    case 0x03:break;  //  host off
    case 0x04:  //  echo
      Serial.write(buf[1]);
      break;  
    case 0x05:  //  return paddle ad value
      Serial.write(0x00);
      break;
    case 0x06:  //  return speed ad value
      Serial.write(map(speedPot,0,1023,0,63));
      break;
    case 0x07:  
      //  TODO!
      break;  //  return all parameters
    case 0x08:  break;  //  reserved
    case 0x09:  //  return calibrating value
      Serial.write(0x00);
      break;
    case 0x0a:  break; // set wk1 mode
    case 0x0b:  break; // set wk2 mode
    case 0x0c:  // upload EEPROM 256 bytes to PC
      for ( i = 0; i < 256; i++) {
        delay(20);
        Serial.write(EEPROM.read(i));
      }
      break;
    case 0x0d:  //  download EEPROM 256 bytes from PC
      for ( i=0;  i < 256; i++) {
        int ch;
        delay(20);
        ch = Serial.read();
        if ( ch == -1 ) continue;
        EEPROM.write(i, ch);
      }
      break;
    case 0x0e:  break;  //  send msg
    case 0x0f:  //  XMODE
      if ( buf[1] & 0x80 ) {
        isCutMode = true;
      } else {
        isCutMode = false;
      }
      spaceRatio = 1.0 + (buf[1]&0x0F) * 0.02;
      break;
    case 0x10: // reserved
      Serial.write(0);
      break;
    case 0x11: // set high baud rate
      Serial.end();
      Serial.begin(9600);
      break;
    case 0x12: // set low baud rate
      Serial.end();
      Serial.begin(1200);
      break;
  }
}

void cmdSidetone(byte buf[])
{
  static short sidetone[] = {
    4000, 2000, 1333, 1000, 800, 666, 571, 500, 444, 400
  };

  byte b = buf[0];
  if ( b & 0x80 ) isSidetone = false;
  else isSidetone = true;
  b &= 0x0F;
  if ( b>=1 && b<=10 )
    note = sidetone[b-1];
}

void cmdSetSpeed(byte buf[])
{
  if ( buf[0] >=5 && buf[0] <=99 ) {
    isSpeedOverride = true;
    setSpeed(buf[0]);
  } else if ( buf[0] == 0 ) {
    isSpeedOverride = false;
  }
}

void cmdWeight(byte buf[])
{
  if ( buf[0] >= 10 && buf[0] <= 90 ) {
    weight = buf[0];
  }
}

void cmdPTTTime(byte buf[])
{
  rtxdelay = buf[0] * 10;
  pttdelay = buf[1] * 10;
}

void cmdSpeedRange(byte buf[])
{
  minWPM = buf[0];
  maxWPM = minWPM + buf[1];
  potRange = buf[2];
}

void cmdPause(byte buf[])
{
  if ( buf[0] == 0x00 ) {
    isPause = false;
  } else if ( buf[0] == 0x01 ) {
    isPause = true;
  }
}

void cmdGetSpeed(byte buf[])
{
  Serial.write(0x80 | (currentWPM - minWPM) );
}

void cmdBackspace(byte buf[])
{
  bufLocGetDec();
  if ( bufLocGet == bufLocPut ) {
    bufLocGetInc();
  }
}

void cmdPinConfig(byte buf[])
{
  //  do nothing in this box
}

void cmdClearBuffer(byte buf[])
{
  bufLocGet = bufLocPut = 0;
  isPause = false;
}

void cmdKeyImd(byte buf[])  //  0x0B
{
  if ( buf[0] ) {
    pttDown();
    keyDown();
  } else {
    keyUp();
    pttUp();
  }
}

void cmdHSCW(byte buf[])  //  0x0C
{
  setSpeed((int)buf[0]*100/5);
}

void cmdFarnWPM(byte buf[])  //  0x0D
{
  farmWPM = buf[0];
}

void cmdSetMode(byte buf[])  //  0x0E
{
  wk2mode = buf[0];
  setPaddle(wk2mode);
}

void cmdDefault(byte buf[])  //  0x0F
{
  cmdSetMode(buf);
  cmdSetSpeed(buf+1);
  cmdSidetone(buf+2);
  cmdWeight(buf+3);
  cmdPTTTime(buf+4);
  cmdSpeedRange(buf+6);
  cmdSetExt(buf+8);
  cmdKeyComp(buf+9);
  cmdFarnWPM(buf+10);
  cmdPadSw(buf+11);
  cmdSetRatio(buf+12);
  cmdPinConfig(buf+13);
}

void cmdSetExt(byte buf[])  //  0x10
{
  firstExtensionTime = buf[0];
}

void cmdKeyComp(byte buf[])  //  0x11
{
  keyingCompensation = buf[0];
}

void cmdPadSw(byte buf[])  //  0x12
{
  //  do nothing in this box
}

void cmdNOP(byte buf[])  //  0x13
{
}

void cmdSoftPad(byte buf[])  //  0x14
{
  //  do nothing in this box
}

void cmdGetStatus(byte buf[])  //  0x15
{
  byte b = B11000010;
  byte d;
  if (  keyerState == KEYED_TRANS ||
        keyerState == KEYED ||
        keyerState == INTER_ELEMENT ||
        (keyerState == IDLE && akState != AK_NONE )) {
    b |= 0x10;
  }
  if ( keyerState == IDLE && akState != AK_NONE )
    b |= 0x04;
  d = bufLocPut - bufLocGet;
  if ( d>0 ) {
    if ( d>BUFFER_SIZE/3 ) b |= 0x01;
  } else if ( d<0 ) {
    if ( -d<BUFFER_SIZE/3 ) b |= 0x01;
  }
  Serial.write(b);
}

void cmdPointer(byte buf[])  //  0x16
{
  int i;
  switch ( buf[0] ) {
  case 0:bufLocGet = bufLocPut = 0;
    break;
  case 1:bufLocPut = bufLocGet;
    break;
  case 2:
    break;
  case 3:
    for ( i = 0; i<buf[1]; i++ )
      bufPut(0);
    break;
  }
}

void cmdSetRatio(byte buf[])  //  0x17
{
  didahRatio = 3.0*(buf[0]/50.0);
}

void cmdBPTT(byte buf[])  //  0x18
{

}

void cmdBKey(byte buf[])  //  0x19
{

}

void cmdBWait(byte buf[])  //  0x1A
{

}

void cmdBMerge(byte buf[])  //  0x1B
{

}

void cmdBSpeed(byte buf[])  //  0x1C
{

}

void cmdBHSpeed(byte buf[])  //  0x1D
{

}

void cmdBCancelSpeed(byte buf[])  //  0x1E
{

}

void cmdBNOP(byte buf[])  //  0x1F
{
}

/*
void winkey_admin_get_values_command() {

  byte byte_to_send;

  // 1 - mode register
  byte_to_send = 0;
  if (length_wordspace != default_length_wordspace) {
    byte_to_send = byte_to_send | 1;
  }
  #ifdef FEATURE_AUTOSPACE
  if (autospace_active) {
    byte_to_send = byte_to_send | 2;
  }
  #endif
  if (winkey_serial_echo) {
    byte_to_send = byte_to_send | 4;
  }
  if (paddle_mode == PADDLE_REVERSE) {
    byte_to_send = byte_to_send | 8;
  }
  switch (keyer_mode) {
    case IAMBIC_A: byte_to_send = byte_to_send | 16; break;
    case ULTIMATIC: byte_to_send = byte_to_send | 32; break;
    case BUG: byte_to_send = byte_to_send | 48; break;
  }
  if (winkey_paddle_echo_activated) {
    byte_to_send = byte_to_send | 64;
  }
  #ifdef FEATURE_DEAD_OP_WATCHDOG
  if (dead_op_watchdog_active) {
    byte_to_send = byte_to_send | 128;
  }
  #endif //FEATURE_DEAD_OP_WATCHDOG
  Serial.write(byte_to_send);

  // 2 - speed
  if (wpm > 99) {
    Serial.write(99);
  } else {
    byte_to_send = wpm;
    Serial.write(byte_to_send);
  }

  // 3 - sidetone
  switch(hz_sidetone) {
    case WINKEY_SIDETONE_1 : Serial.write(1); break;
    case WINKEY_SIDETONE_2 : Serial.write(2); break;
    case WINKEY_SIDETONE_3 : Serial.write(3); break;
    case WINKEY_SIDETONE_4 : Serial.write(4); break;
    case WINKEY_SIDETONE_5 : Serial.write(5); break;
    case WINKEY_SIDETONE_6 : Serial.write(6); break;
    case WINKEY_SIDETONE_7 : Serial.write(7); break;
    case WINKEY_SIDETONE_8 : Serial.write(8); break;
    case WINKEY_SIDETONE_9 : Serial.write(9); break;
    case WINKEY_SIDETONE_10 : Serial.write(10); break;
    default: Serial.write(5); break;
  }

  // 4 - weight
  Serial.write(weighting);

  // 5 - ptt lead
  Serial.write(zero);   // TODO - backwards calculate this

  // 6 - ptt tail
  Serial.write(zero);   // TODO - backwards calculate this

  // 7 - pot min wpm
  #ifdef FEATURE_POTENTIOMETER
  Serial.write(pot_wpm_low_value);
  #endif
  #ifndef FEATURE_POTENTIOMETER
  Serial.write(15);
  #endif

  // 8 - pot wpm range
  #ifdef FEATURE_POTENTIOMETER
  byte_to_send = pot_wpm_high_value - pot_wpm_low_value;
  Serial.write(byte_to_send);
  #endif
  #ifndef FEATURE_POTENTIOMETER
  Serial.write(20);
  #endif

  // 9 - 1st extension
  Serial.write(first_extension_time);

  // 10 - compensation
  Serial.write(keying_compensation);

  // 11 - farnsworth wpm
  #ifdef FEATURE_FARNSWORTH
  byte_to_send = wpm_farnsworth;
  Serial.write(byte_to_send);
  #endif
  #ifndef FEATURE_FARNSWORTH
  Serial.write(zero);
  #endif

  // 12 - paddle setpoint
  Serial.write(50);  // default value

  // 13 - dah to dit ratio
  Serial.write(50);  // TODO -backwards calculate

  // 14 - pin config
  #ifdef OPTION_WINKEY_2_SUPPORT
  byte_to_send = 0;
  if (current_ptt_line != 0) {byte_to_send = byte_to_send | 1;}
  if ((sidetone_mode == SIDETONE_ON) || (sidetone_mode == SIDETONE_PADDLE_ONLY)) {byte_to_send | 2;}
  if (current_tx_key_line == tx_key_line_1) {byte_to_send = byte_to_send | 4;}
  if (current_tx_key_line == tx_key_line_2) {byte_to_send = byte_to_send | 8;}
  if (wk2_both_tx_activated) {byte_to_send = byte_to_send | 12;}
  if (ultimatic_mode == ULTIMATIC_DIT_PRIORITY) {byte_to_send = byte_to_send | 128;}
  if (ultimatic_mode == ULTIMATIC_DAH_PRIORITY) {byte_to_send = byte_to_send | 64;}  
  if (ptt_hang_time_wordspace_units == 1.33) {byte_to_send = byte_to_send | 16;}
  if (ptt_hang_time_wordspace_units == 1.66) {byte_to_send = byte_to_send | 32;}
  if (ptt_hang_time_wordspace_units == 2.0) {byte_to_send = byte_to_send | 64;}
  Serial.write(byte_to_send);
  #else
  Serial.write(5); // default value
  #endif

  // 15 - pot range
  #ifdef OPTION_WINKEY_2_SUPPORT
  Serial.write(zero);
  #else
  Serial.write(0xFF);
  #endif

}
*/

