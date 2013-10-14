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

byte  minWPM = 5;
byte  maxWPM = 45;
short  potRange = 255;

#define PTT_DELAY 63    //  6 times dit time
#define DEBOUNCE  20
#define BUFFER_SIZE 32

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
unsigned char note = 535;
unsigned char ptt = false;
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


///////////////////////////////////////////////////////////////////////////////
//
//  State Machine Defines

enum KSTYPE {
  IDLE, CHK_DIT, CHK_DAH, KEYED_TRANS, KEYED, 
  INTER_ELEMENT, SK_DOWN 
};

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
  
  if ( digitalRead(LPin) == LOW )
    isStrait = true;
  
  digitalWrite(gLedPin, LOW);
  digitalWrite(rLedPin, LOW);
  tone(tonePin, note);
  delay(100);
  digitalWrite(gLedPin, HIGH);
  digitalWrite(rLedPin, HIGH);
  noTone(tonePin);
  delay(100);
  digitalWrite(gLedPin, LOW);
  digitalWrite(rLedPin, LOW);
  tone(tonePin, note);
  delay(300);
  digitalWrite(gLedPin, HIGH);
  digitalWrite(rLedPin, HIGH);
  noTone(tonePin);
  delay(100);
  digitalWrite(gLedPin, LOW);
  digitalWrite(rLedPin, LOW);
  tone(tonePin, note);
  delay(100);
  digitalWrite(gLedPin, HIGH);
  digitalWrite(rLedPin, HIGH);
  noTone(tonePin);
  delay(100);
  
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

long ktimer;  //  timer for keydown
long ptimer;  //  timer for ptt
long rtimer;  //  timer for rx/tx trans

void loop()
{
  // Basic Iambic Keyer
  // keyerControl contains processing flags and keyer mode bits
  // Supports Iambic A and B
  // State machine based, uses calls to millis() for timing.
  checkSpeed();
  checkSerial();
  
  switch (keyerState) {
  case IDLE:
    // Wait for direct or latched paddle press
    if ( keyerControl & STRAITKEY ) {
      if ( digitalRead(RPin) == LOW ) {
        if ( !ptt ) {
          ptt = true;
          digitalWrite(pttPin, LOW);         // enable PTT
          digitalWrite(gLedPin, LOW);
        }
        digitalWrite(rLedPin, LOW);         // turn the LED on
        tone( tonePin, note );
        ktimer = millis()+DEBOUNCE;
        keyerState = SK_DOWN;
      }
    } else if ((digitalRead(LPin) == LOW) ||
        (digitalRead(RPin) == LOW) ||
        (keyerControl & 0x03) ) {
      update_PaddleLatch();
      keyerState = CHK_DIT;
    }
    if ( ptt && (millis() >= ptimer) ) {
      digitalWrite(pttPin, HIGH);  // turn PTT off
      digitalWrite(gLedPin, HIGH);
      ptt = false;
    }
    break;
  case SK_DOWN:
    if ( millis() > ktimer && digitalRead(RPin) == HIGH ) {
      Serial.println("hr");
      digitalWrite(rLedPin, HIGH);         // turn the LED off
      noTone( tonePin);
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
      ktimer = ditTime * 3 * (weight/50.0);
      keyedPrep();
      keyerState = KEYED_TRANS;
    }
    else {
      keyerState = IDLE;
      ptimer = millis()+pttdelay;
    }
    break;
  case KEYED_TRANS:
    if ( ptt || millis() >= rtimer+rtxdelay ) {
      ptt = true;
      digitalWrite(rLedPin, LOW);         // turn the LED on
      tone( tonePin, note );
      ktimer += millis();                 // set ktimer to interval end time
      keyerControl &= ~(DIT_L + DAH_L);   // clear both paddle latch bits
      keyerState = KEYED;                 // next state
    }
    break;
  case KEYED:
    // Wait for timer to expire
    if (millis() > ktimer) {            // are we at end of key down ?
      digitalWrite(rLedPin, HIGH);      // turn the LED off
      noTone( tonePin );
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
        
///////////////////////////////////////////////////////////////////////////////
//
//    Latch dit and/or dah press
//
//    Called by keyer routine
//
///////////////////////////////////////////////////////////////////////////////

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
  digitalWrite(pttPin, LOW);         // enable PTT
  digitalWrite(gLedPin, LOW);
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

void bufPutin(byte b)
{
  buffer[bufLocPut++] = ch;
  if ( bufLocPut == BUFFER_SIZE ) {
    bufLocPut = 0;
  }
  //  TODO: roll back to LocGet
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
  static void (*cmdfun[33])(byte buf[]) = {
    cmdAdmin, cmdSidetone, cmdSpeed, cmdWeight, cmdPTTTime,
    cmdSpeedRange, cmdPause, cmdGetSpeed, cmdBackspace,
    cmdPinConfig, cmdClearBuffer
  };

  if ( Serial.available() >0 ) {
    byte ch = Serial.read();
    if ( serialLoc == 0 ) {
      if ( ch >=0 && ch < 32 ) {
        serialLen = cmdLen[ch];
        serialBuf[serialLoc++] = ch;
      } else {
        bufPutin(ch);
      }
    } else {
      serialBuf[serialLoc++] = ch;
      if  ( serialBuf[0] == 0x16 && serialLoc == 2 && ch == 0x03 ||
            serialBuf[0] == 0x00 && serialLoc == 2 && ch == 0x00 ||
            serialBuf[0] == 0x00 && serialLoc == 2 && ch == 0x04 
          ) {
        serialLen++;
      }
      serialLen--;
    }
    if ( serialLoc > 0 && serialLen == 0 ) {
      (*cmdfun[serialBuf[0]])(serialBuf+1);
      serialLoc = 0;
    }
  } //  if Serial.available() >0
}

void cmdAdmin(byte buf[]) 
{
  switch (buf[0]) {
    case 0x00:break;  //  do nothing for calibrating
    case 0x01:break;  //  reset, can't do now
    case 0x02:break;  //  host on, return version to host
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
    case 0x07:  //  return all parameters
      break;
    case 0x09:  //  return calibrating value
      Serial.write(0x00);
      break;
  }
}

void cmdSidetone(byte buf[])
{
  static short sidetone[] = {
    3759, 1879, 1252, 940, 752, 625, 535, 469, 417, 375
  };

  if ( buf[0] >=1 && buf[0] <=10 ) {
    note = sidetone[buf[0]-1];
  }
}

void cmdSpeed(byte buf[])
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

}

void cmdClearBuffer(byte buf[])
{
  bufLocGet = bufLocPut = 0;
}