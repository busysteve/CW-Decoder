
// ============================================================
// decoder.ino :: An Iambic Keyer and Decoder (4x20 LCD)
//
// (c) Scott Baker KJ7NLA
// Modified and Updated by Stephen Mathews K4SDM
// ============================================================

#include <Arduino.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <EEPROM.h>

const uint8_t pinDit  = 2;  // dit key input
const uint8_t pinDah  = 3;  // dah key input
const uint8_t pinSw1  = 7;  // push-button switch
const uint8_t pinBuzz = 9;  // buzzer/speaker pin

#define VERSION   "3.5.0"
const byte ver = 35;
//#define DEBUG 1           // uncomment for debug

// Morse-to-ASCII lookup table
const char m2a[129] PROGMEM =
  {'~',' ','E','T','I','A','N','M','S','U','R','W','D','K','G','O',
   'H','V','F','*','L','*','P','J','B','X','C','Y','Z','Q','*','*',
   '5','4','S','3','*','*','*','2','&','*','+','*','*','*','J','1',
   '6','=','/','*','*','#','(','*','7','*','G','*','8','*','9','0',
   '^','*','*','*','*','*','*','*','*','*','*','*','?','_','*','*',
   '*','\\','"','*','*','.','*','*','*','*','@','*','*','*','\'','*',
   '*','-','*','*','*','*','*','*','*','*',';','!','*',')','*','*',
   '*','*','*',',','*','*','*','*',':','*','*','*','*','*','*','*','^'};

// ASCII-to-Morse lookup table
const uint8_t a2m[64] PROGMEM =
  {0x01,0x6b,0x52,0x4c,0x89,0x4c,0x28,0x5e,  //   ! " # $ % & '
   0x36,0x6d,0x4c,0x2a,0x73,0x61,0x55,0x32,  // ( ) * + , - . /
   0x3f,0x2f,0x27,0x23,0x21,0x20,0x30,0x38,  // 0 1 2 3 4 5 6 7
   0x3c,0x3e,0x78,0x6a,0x4c,0x31,0x4c,0x4c,  // 8 9 : ; < = > ?
   0x5a,0x05,0x18,0x1a,0x0c,0x02,0x12,0x0e,  // @ A B C D E F G
   0x10,0x04,0x17,0x0d,0x14,0x07,0x06,0x0f,  // H I J K L M N O
   0x16,0x1d,0x0a,0x08,0x03,0x09,0x11,0x0b,  // P Q R S T U V W
   0x19,0x1b,0x1c,0x4c,0x40,0x4c,0x80,0x4d}; // X Y Z [ \ ] ^ _

// user interface
#define NBP  0  // no-button-pushed
#define BSC  1  // button-single-click
#define BPL  2  // button-push-long

#define LONGPRESS  500  // long button press

#define RUN_MODE  0  // user practice mode
#define SETUP     1  // change settings

volatile uint8_t  event    = NBP;
volatile uint8_t  menumode = RUN_MODE;

#define DITCONST  1200       // dit time constant
#define MAXWPM    40         // max keyer speed
#define INITWPM   18         // startup keyer speed  // SDM
#define MINWPM    10         // min keyer speed

#define MINTONE 450
#define MAXTONE 850

#define MINLESSONMODE 0
#define MAXLESSONMODE 1

#define MINLESSON 1
#define MAXLESSON 39

#define MINLESSONCNT 1
#define MAXLESSONCNT 80


// 4x20 LCD
#define COLSIZE   20
#define ROWSIZE    4
#define MAXCOL    COLSIZE-1  // max display column
#define MAXLINE   ROWSIZE-1  // max display row
#define SPACE10   "          "
#define SPACE20   "                    "

//#define BAUDRATE 9600
#define BAUDRATE 115200
#define LCD_ADDRESS 0x27

// Class instantiation
LiquidCrystal_I2C lcd(LCD_ADDRESS, COLSIZE, ROWSIZE);

uint8_t maddr = 1;
uint8_t myrow = 0;
uint8_t mycol = 0;
char tmpstr[12];

char lookup_cw(uint8_t x);
void print_cw(void);
void print_line(uint8_t row, char *str);
void clear_line(uint8_t row);
void printchar(char ch);
void maddr_cmd(uint8_t cmd);
inline void read_switch(void);
void read_paddles(void);
void iambic_keyer(void);
void straight_key(void);
void send_cwchr(char ch);
void send_cwmsg(char *str, uint8_t prn);
void back2run(void);
void ditcalc(void);
void change_wpm(uint8_t wpm);
void menu_wpm(void);
void menu_tone(void);
void menu_mode(void);
void menu_msg(void);
void doError(void);
void(*resetFunc) (void) = 0;

#define SKHZ    220    // error beep   = @A4 note
#define DDHZ    659    // dit/dah beep = @E5 note
char xmit_buf[620] = {0};
uint8_t xmit_cnt=0;


char realtime_xmit = 0;

short keyertone = 650;
short remotetone = 750;

byte lesson = 1;
byte lesson_size = MAXLESSONCNT;
byte lesson_mode = 0;

byte farns = 0;

volatile uint8_t  keyermode  = 0;   // keyer mode
volatile uint8_t  keyswap    = 0;   // key swap


// table lookup for CW decoder
char lookup_cw(uint8_t addr) {
  char ch = '*';
  if (addr < 129) ch = pgm_read_byte(m2a + addr);

  #ifdef DEBUG
  Serial.println(addr);
  #endif  // DEBUG

  return ch;
}

// print a line
void print_line(uint8_t row, char *str) {
  lcd.setCursor(0,row);
  lcd.print(str);
  lcd.print(SPACE10);
}

// clear a line
void clear_line(uint8_t row) {
  lcd.setCursor(0,row);
  lcd.print(SPACE20);
}


// print the ascii char
void printchar(char ch) {
  if ((myrow <= MAXLINE) & (mycol <= MAXCOL)) {
    lcd.setCursor(mycol,myrow);
    if (ch == '^') {
      // clear screen if 6-dit code is received
      lcd.clear();
      myrow = 0;
      mycol = 0;
    } else {
      lcd.print(ch);
      mycol++;
    }
  }
  if (mycol > MAXCOL) {
    mycol = 0;
    myrow++;
    if (myrow > MAXLINE) myrow = 0;
    clear_line(myrow);
  }
}

/*
char scrollbuf [81] = {' '};


// print the ascii char
void printchar(char ch) {
  //if ((myrow <= MAXLINE) & (mycol <= MAXCOL)) {
    lcd.setCursor(0,0);
    if (ch == '^') {
      // clear screen if 6-dit code is received
      lcd.clear();
      myrow = 3;
      mycol = 20;
    } else {
      for(char i=0; i<79; i++)
        scrollbuf[i] = scrollbuf[i+1];
      scrollbuf[79] = ch;
      scrollbuf[80] = 0;
      lcd.println(&scrollbuf[0]);
      lcd.println(&scrollbuf[20]);
      lcd.println(&scrollbuf[40]);
      lcd.println(&scrollbuf[60]);
    }
  //}
}
*/

// convert morse to ascii and print
void print_cw() {
  char ch = lookup_cw(maddr);
  printchar(ch);

}

// update the morse code table address
void maddr_cmd(uint8_t cmd) {
  if (cmd == 2) {
    // print the tranlated ascii
    // and reset the table address
    print_cw();
    maddr = 1;
  }
  else {
    // update the table address
    tone(pinBuzz, keyertone);
    maddr = maddr<<1;
    maddr |= cmd;
  }
}

// An Iambic (mode A/B) morse code keyer
// Copyright 2021 Scott Baker KJ7NLA


// keyerinfo bit definitions
#define DIT_REG     0x01     // dit pressed
#define DAH_REG     0x02     // dah pressed
#define KEY_REG     0x03     // dit or dah pressed
#define WAS_DIT     0x04     // last key was dit
#define BOTH_REG    0x08     // both dit/dah pressed

// keyermode bit definitions
#define IAMBICA   0        // iambic A keyer mode
#define IAMBICB   1        // iambic B keyer mode

// keyerstate machine states
#define KEY_IDLE    0
#define CHK_KEY     1
#define KEY_WAIT    2
#define IDD_WAIT    3
#define LTR_GAP     4
#define WORD_GAP    5

// more key info
#define GOTKEY  (keyerinfo & KEY_REG)
#define NOKEY   !GOTKEY
#define GOTBOTH  GOTKEY == KEY_REG

uint8_t  keyerstate = KEY_IDLE;
uint8_t  keyerinfo  = 0;

uint16_t dittime;        // dit time
uint16_t dahtime;        // dah time
uint16_t lettergap1;     // letter space for decode
uint16_t lettergap2;     // letter space for send
uint16_t wordgap1;       // word space for decode
uint16_t wordgap2;       // word space for send
uint8_t  sw1Pushed = 0;  // button pushed

// read and debounce switch
inline void read_switch() {
  sw1Pushed = !digitalRead(pinSw1);
}

// read and debounce paddles
void read_paddles() {
  uint8_t ditv1 = !digitalRead(pinDit);
  uint8_t dahv1 = !digitalRead(pinDah);
  uint8_t ditv2 = !digitalRead(pinDit);
  uint8_t dahv2 = !digitalRead(pinDah);
  if (ditv1 && ditv2) {
    if (keyswap) keyerinfo |= DAH_REG;
    else keyerinfo |= DIT_REG;
  }
  if (dahv1 && dahv2) {
    if (keyswap) keyerinfo |= DIT_REG;
    else keyerinfo |= DAH_REG;
  }
  if (GOTBOTH) keyerinfo |= BOTH_REG;
}

// iambic keyer state machine
void iambic_keyer() {
  static uint32_t ktimer;
  switch (keyerstate) {
    case KEY_IDLE:
      read_paddles();
      if (GOTKEY) {
        keyerstate = CHK_KEY;
      } else {
        keyerinfo = 0;
      }
      break;
    case CHK_KEY:
      switch (keyerinfo & 0x07) {
        case 0x05:  // dit and prev dit
        case 0x03:  // dit and dah
        case 0x01:  // dit
          keyerinfo &= ~DIT_REG;
          keyerinfo |= WAS_DIT;
          ktimer = millis() + dittime;
          maddr_cmd(0);
          if( realtime_xmit == 0 )
            Serial.print(".");
          else if( realtime_xmit != 0 && xmit_cnt < sizeof( xmit_buf )-1 )
            xmit_buf[xmit_cnt++] = '.';
          keyerstate = KEY_WAIT;
          break;
        case 0x07:  // dit and dah and prev dit
        case 0x06:  // dah and prev dit
        case 0x02:  // dah
          keyerinfo &= ~DAH_REG;
          keyerinfo &= ~WAS_DIT;
          ktimer = millis() + dahtime;
          maddr_cmd(1);
          if( realtime_xmit == 0 )
            Serial.print("-");
          else if( realtime_xmit != 0 && xmit_cnt < sizeof( xmit_buf )-1 )
            xmit_buf[xmit_cnt++] = '-';
          keyerstate = KEY_WAIT;
          break;
        default:
          // case 0x00:  // nothing
          // case 0x04:  // nothing and prev dit
          keyerstate = KEY_IDLE;
      }
      break;
    case KEY_WAIT:
      // wait dit/dah duration
      if (millis() > ktimer) {
        // done sending dit/dah
        noTone(pinBuzz);
        // inter-symbol time is 1 dit
        ktimer = millis() + dittime;
        keyerstate = IDD_WAIT;
      }
      break;
    case IDD_WAIT:
      // wait time between dit/dah
      if (millis() > ktimer) {
        // wait done
        keyerinfo &= ~KEY_REG;
        if (keyermode == IAMBICA) {
          // Iambic A
          // check for letter space
          ktimer = millis() + lettergap1;
          keyerstate = LTR_GAP;
        } else {
          // Iambic B
          read_paddles();
          if (NOKEY && (keyerinfo & BOTH_REG)) {
            // send opposite of last paddle sent
            if (keyerinfo & WAS_DIT) {
              // send a dah
              ktimer = millis() + dahtime;
              maddr_cmd(1);
            }
            else {
              // send a dit
              ktimer = millis() + dittime;
              maddr_cmd(0);
            }
            keyerinfo = 0;
            keyerstate = KEY_WAIT;
          } else {
            // check for letter space
            ktimer = millis() + lettergap1;
            keyerstate = LTR_GAP;
          }
        }
      }
      break;
    case LTR_GAP:
      if (millis() > ktimer) {
        // letter space found so print char
        maddr_cmd(2);
          if( xmit_cnt < sizeof( xmit_buf )-1 )
          {
            if( realtime_xmit == 0 )
              Serial.print(" ");
            else if( realtime_xmit == 1 )
            {
              xmit_buf[xmit_cnt++] = ' ';
            }
            else if( realtime_xmit == 2 )
            {
              xmit_buf[xmit_cnt++] = ' ';
              xmit_buf[xmit_cnt] = '\0';
              Serial.print(xmit_buf);
              memset( xmit_buf, 0, sizeof( xmit_buf ) );
              xmit_cnt = 0;
            }
          }
        // check for word space
        ktimer = millis() + wordgap1;
        keyerstate = WORD_GAP;
      }
      read_paddles();
      if (GOTKEY) {
        keyerstate = CHK_KEY;
      } else {
        keyerinfo = 0;
      }
      break;
    case WORD_GAP:
      if (millis() > ktimer) {
        // word gap found so print a space
        maddr = 1;
        print_cw();
        keyerstate = KEY_IDLE;
        if( realtime_xmit == 1 )
        {
          Serial.print("\r\n");
        }
        else if( realtime_xmit == 2 )
        {
            Serial.print(xmit_buf);
            memset( xmit_buf, 0, sizeof( xmit_buf ) );
            xmit_cnt = 0;
        }
      }
      read_paddles();
      if (GOTKEY) {
        keyerstate = CHK_KEY;
      } else {
        keyerinfo = 0;
      }
      break;
    default:
      break;
  }



      {
          if (Serial.available() > 0) {
            // read the incoming byte:
            char incomingByte = Serial.read();
        
            // say what you got:
            if( incomingByte == '.' )
              {
                tone(pinBuzz, remotetone);
                delay( dittime );
                noTone( pinBuzz );
              }
            else if( incomingByte == '-' )
              {             
                tone(pinBuzz, remotetone);
                delay( dahtime );
                noTone( pinBuzz );
              }
              delay( dittime );
          }
      }
  
}

// handle straight key mode
void straight_key() {
  uint8_t pin = keyswap ? pinDah : pinDit;
  if (!digitalRead(pin)) {
    tone(pinBuzz, keyertone);
    while(!digitalRead(pin)) {
      delay(1);
    }
    noTone(pinBuzz);
  }
}

// convert ascii to morse
void send_cwchr(char ch) {
  uint8_t mcode;
  // remove the ascii code offset (0x20) to
  // create a lookup table address
  uint8_t addr = (uint8_t)ch - 0x20;
  // then lookup the Morse code from the a2m table
  // note: any unknown unknown ascii char is
  // translated to a '?' (mcode 0x4c)
  if (addr < 64) mcode = pgm_read_byte(a2m + addr);
  else mcode = 0x4c;
  // if space (mcode 0x01) is found
  // then wait for one word space
  if (mcode == 0x01) delay(wordgap2);
  else {
    uint8_t mask = 0x80;
    // use a bit mask to find the leftmost 1
    // this marks the start of the Morse code bits
    while (!(mask & mcode)) mask = mask >> 1;
    while (mask != 1) {
      mask = mask >> 1;
      // use the bit mask to select a bit from
      // the Morse code starting from the left
      tone(pinBuzz, keyertone);
      // turn the side-tone on for dit or dah length
      // if the mcode bit is a 1 then send a dah
      // if the mcode bit is a 0 then send a dit
      delay((mcode & mask) ? dahtime : dittime);
      noTone(pinBuzz);
      // turn the side-tone off for a symbol space
      delay(dittime);
    }
    delay(lettergap2);  // add letter space
  }
}

// transmit a CW message
void send_cwmsg(char *str, uint8_t prn) {
  for (uint8_t i=0; str[i]; i++) {
    if (prn) {
      printchar(str[i]);
      read_switch();
      if (sw1Pushed) break;
    }
    send_cwchr(str[i]);
  }
}

// back to run mode
void back2run() {
  menumode = RUN_MODE;
  lcd.clear();
  print_line(0, "READY >>");
  delay(700);
  clear_line(0);
  myrow = 0;
  mycol = 0;
}

uint8_t keyerwpm;

// initial keyer speed
void ditcalc() {
  int farn = keyerwpm - farns;
  if( farn <= 0 ) farn = keyerwpm;

  dittime    = DITCONST/keyerwpm;
  dahtime    = (DITCONST * 3)/keyerwpm;
  lettergap1 = (DITCONST * 2.5)/(keyerwpm);
  lettergap2   = (DITCONST * 3)/(farn);
  wordgap1   = (DITCONST * 5)/(keyerwpm);
  wordgap2   = (DITCONST * 7)/(farn);
}

// change the keyer speed
void change_wpm(uint8_t wpm) {
  keyerwpm = wpm;
  ditcalc();
}

// user interface menu to
// increase or decrease keyer speed
void menu_wpm() {
  uint8_t prev_wpm = keyerwpm;
  lcd.clear();
  print_line(0, "SPEED IS");
  // wait until button is released
  while (sw1Pushed) {
    read_switch();
    delay(10);
  }
  // loop until button is pressed
  while (!sw1Pushed) {
    read_switch();
    keyerinfo = 0;
    read_paddles();
    if (keyerinfo & DAH_REG) {
      keyerwpm++;
    }
    if (keyerinfo & DIT_REG) {
      keyerwpm--;
    }
    // check limits
    if (keyerwpm < MINWPM) keyerwpm = MINWPM;
    if (keyerwpm > MAXWPM) keyerwpm = MAXWPM;
    while (GOTKEY) {
      keyerinfo = 0;
      read_paddles();
      delay(10);
    }
    keyerinfo = 0;
    itoa(keyerwpm,tmpstr,10);
    strcat(tmpstr," WPM");
    print_line(1, tmpstr);
  }
  delay(10); // debounce
  // if wpm changed the recalculate the
  // dit timing and and send an OK message
  if (prev_wpm != keyerwpm) {
    EEPROM[4] = keyerwpm;
    ditcalc();
    send_cwmsg("OK", 0);
    back2run();
  } else menu_tone();
}


// local keyer tone menu to
// increase or decrease keyer tone
void menu_tone() {
  uint16_t prev_tone = keyertone;
  lcd.clear();
  print_line(0, "KEYER TONE IS");
  // wait until button is released
  while (sw1Pushed) {
    read_switch();
    delay(10);
  }
  // loop until button is pressed
  while (!sw1Pushed) {
    read_switch();
    keyerinfo = 0;
    read_paddles();
    if (keyerinfo & DAH_REG) {
      keyertone+=10;
      tone(pinBuzz, keyertone );
      delay( dittime );
      noTone( pinBuzz);
    }
    if (keyerinfo & DIT_REG) {
      keyertone-=10;
      tone(pinBuzz, keyertone );
      delay( dittime );
      noTone( pinBuzz);
    }
    // check limits
    if (keyertone < MINTONE) keyertone = MINTONE;
    if (keyertone > MAXTONE) keyertone = MAXTONE;

    
    while (GOTKEY) {
      keyerinfo = 0;
      read_paddles();
      delay(10);
    }
    keyerinfo = 0;
    itoa(keyertone,tmpstr,10);
    strcat(tmpstr," Hz");
    print_line(1, tmpstr);
  }
  delay(10); // debounce
  // if wpm changed the recalculate the
  // dit timing and and send an OK message
  if (prev_tone != keyertone) {
    ditcalc();
    send_cwmsg("OK", 0);
    back2run();
  } else menu_remotetone();
}

// remote keyer tone menu to
// increase or decrease keyer tone
void menu_trainer_mode() {
  uint16_t prev_lesson_mode = lesson_mode;
  lcd.clear();
  print_line(0, "TRAINER MODE");
  // wait until button is released
  while (sw1Pushed) {
    read_switch();
    delay(10);
  }
  // loop until button is pressed
  while (!sw1Pushed) {
    read_switch();
    keyerinfo = 0;
    read_paddles();
    if (keyerinfo & DAH_REG) {
      lesson_mode+=1;
      tone(pinBuzz, keyertone );
      delay( dittime );
      noTone( pinBuzz);
    }
    if (keyerinfo & DIT_REG) {
      lesson_mode-=1;
      tone(pinBuzz, keyertone );
      delay( dittime );
      noTone( pinBuzz);
    }
    // check limits
    if (lesson_mode < MINLESSONMODE) lesson_mode = MINLESSONMODE;
    if (lesson_mode > MAXLESSONMODE) lesson_mode = MAXLESSONMODE;

    
    while (GOTKEY) {
      keyerinfo = 0;
      read_paddles();
      delay(10);
    }
    keyerinfo = 0;
    itoa(lesson_mode,tmpstr,10);
    //strcat(tmpstr," Hz");
    print_line(1, tmpstr);
  }
  delay(10); // debounce
  // if wpm changed the recalculate the
  // dit timing and and send an OK message
  if (prev_lesson_mode != lesson_mode) {
    EEPROM[2] = (byte)lesson_mode;
    ditcalc();
    send_cwmsg("OK", 0);
    back2run();
  } else menu_trainer_lesson();
}


// remote keyer tone menu to
// increase or decrease keyer tone
void menu_trainer_lesson() {
  uint16_t prev_lesson = lesson;
  lcd.clear();
  print_line(0, "TRAINER LESSON");
  // wait until button is released
  while (sw1Pushed) {
    read_switch();
    delay(10);
  }
  // loop until button is pressed
  while (!sw1Pushed) {
    read_switch();
    keyerinfo = 0;
    read_paddles();
    if (keyerinfo & DAH_REG) {
      lesson+=1;
      tone(pinBuzz, keyertone );
      delay( dittime );
      noTone( pinBuzz);
    }
    if (keyerinfo & DIT_REG) {
      lesson-=1;
      tone(pinBuzz, keyertone );
      delay( dittime );
      noTone( pinBuzz);
    }
    // check limits
    if (lesson < MINLESSON) lesson = MINLESSON;
    if (lesson > MAXLESSON) lesson = MAXLESSON;

    
    while (GOTKEY) {
      keyerinfo = 0;
      read_paddles();
      delay(10);
    }
    keyerinfo = 0;
    itoa(lesson,tmpstr,10);
    //strcat(tmpstr," Hz");
    print_line(1, tmpstr);
  }
  delay(10); // debounce
  // if wpm changed the recalculate the
  // dit timing and and send an OK message
  if (prev_lesson != lesson) {
    EEPROM[1] = (byte)lesson;
    ditcalc();
    send_cwmsg("OK", 0);
    back2run();
  } else menu_trainer_lesson_size();
}


// remote keyer tone menu to
// increase or decrease keyer tone
void menu_trainer_lesson_size() {
  uint16_t prev_lesson_size = lesson_size;
  lcd.clear();
  print_line(0, "TRAINING SIZE");
  // wait until button is released
  while (sw1Pushed) {
    read_switch();
    delay(10);
  }
  // loop until button is pressed
  while (!sw1Pushed) {
    read_switch();
    keyerinfo = 0;
    read_paddles();
    if (keyerinfo & DAH_REG) {
      if( lesson_size < 10 )
        lesson_size+=1;
      else
        lesson_size+=10;

      tone(pinBuzz, keyertone );
      delay( dittime );
      noTone( pinBuzz);
    }
    if (keyerinfo & DIT_REG) {
      if( lesson_size <= 10 )
        lesson_size-=1;
      else
        lesson_size-=10;
      tone(pinBuzz, keyertone );
      delay( dittime );
      noTone( pinBuzz);
    }
    // check limits
    if (lesson_size < MINLESSONCNT) lesson_size = MINLESSONCNT;
    if (lesson_size > MAXLESSONCNT) lesson_size = MAXLESSONCNT;

    
    while (GOTKEY) {
      keyerinfo = 0;
      read_paddles();
      delay(10);
    }
    keyerinfo = 0;
    itoa(lesson_size,tmpstr,10);
    //strcat(tmpstr," Hz");
    print_line(1, tmpstr);
  }
  delay(10); // debounce
  // if wpm changed the recalculate the
  // dit timing and and send an OK message
  if (prev_lesson_size != lesson_size) {
    EEPROM[6] = (byte)lesson_size;
    ditcalc();
    send_cwmsg("OK", 0);
    back2run();
  } else menu_trainer_farns();
}




// remote keyer tone menu to
// increase or decrease keyer tone
void menu_trainer_farns() {
  uint16_t prev_farns = farns;
  lcd.clear();
  print_line(0, "TRAINER FARNSWORTH");
  // wait until button is released
  while (sw1Pushed) {
    read_switch();
    delay(10);
  }
  // loop until button is pressed
  while (!sw1Pushed) {
    read_switch();
    keyerinfo = 0;
    read_paddles();
    if (keyerinfo & DAH_REG) {
      farns+=1;
      tone(pinBuzz, keyertone );
      delay( dittime );
      noTone( pinBuzz);
    }
    if (keyerinfo & DIT_REG) {
      farns-=1;
      tone(pinBuzz, keyertone );
      delay( dittime );
      noTone( pinBuzz);
    }
    // check limits
    if (farns < 0) farns = 0;
    if (farns > 15) farns = 15;

    
    while (GOTKEY) {
      keyerinfo = 0;
      read_paddles();
      delay(10);
    }
    keyerinfo = 0;
    itoa(farns,tmpstr,10);
    //strcat(tmpstr," Hz");
    print_line(1, tmpstr);
  }
  delay(10); // debounce
  // if wpm changed the recalculate the
  // dit timing and and send an OK message
  if (prev_farns != farns) {
    EEPROM[3] = farns;
    ditcalc();
    send_cwmsg("OK", 0);
    back2run();
  } else menu_wpm();
}



// remote keyer tone menu to
// increase or decrease keyer tone
void menu_remotetone() {
  uint16_t prev_tone = remotetone;
  lcd.clear();
  print_line(0, "REMOTE TONE IS");
  // wait until button is released
  while (sw1Pushed) {
    read_switch();
    delay(10);
  }
  // loop until button is pressed
  while (!sw1Pushed) {
    read_switch();
    keyerinfo = 0;
    read_paddles();
    if (keyerinfo & DAH_REG) {
      remotetone+=10;
      tone(pinBuzz, remotetone );
      delay( dittime );
      noTone( pinBuzz);
    }
    if (keyerinfo & DIT_REG) {
      remotetone-=10;
      tone(pinBuzz, remotetone );
      delay( dittime );
      noTone( pinBuzz);
    }
    // check limits
    if (remotetone < MINTONE) remotetone = MINTONE;
    if (remotetone > MAXTONE) remotetone = MAXTONE;

    
    while (GOTKEY) {
      keyerinfo = 0;
      read_paddles();
      delay(10);
    }
    keyerinfo = 0;
    itoa(remotetone,tmpstr,10);
    strcat(tmpstr," Hz");
    print_line(1, tmpstr);
  }
  delay(10); // debounce
  // if wpm changed the recalculate the
  // dit timing and and send an OK message
  if (prev_tone != remotetone) {
    ditcalc();
    send_cwmsg("OK", 0);
    back2run();
  } else menu_xmit_mode();
}



// xmit mode
// increase or decrease keyer tone
void menu_xmit_mode() {
  char prev_mode = realtime_xmit;
  lcd.clear();
  print_line(0, "XMIT MODE");
  // wait until button is released
  while (sw1Pushed) {
    read_switch();
    delay(10);
  }
  // loop until button is pressed
  while (!sw1Pushed) {
    read_switch();
    keyerinfo = 0;
    read_paddles();
    if (keyerinfo & DAH_REG) {
      realtime_xmit++;
      tone(pinBuzz, remotetone );
      delay( dittime );
      noTone( pinBuzz);
    }
    if (keyerinfo & DIT_REG) {
      realtime_xmit--;
      tone(pinBuzz, remotetone );
      delay( dittime );
      noTone( pinBuzz);
    }
    // check limits
    if (realtime_xmit < 0) realtime_xmit = 0;
    if (realtime_xmit > 2) realtime_xmit = 2;

    switch( realtime_xmit )
    {
      case 0:
        print_line(1, "REALTIME");
      break;
      case 1:
        print_line(1, "PER LETTER");
      break;
      case 2:
        print_line(1, "PER WORD");
      break;
      default:
        print_line(1, "ERROR");
      break;
    }
    
    while (GOTKEY) {
      keyerinfo = 0;
      read_paddles();
      delay(10);
    }
    keyerinfo = 0;
  }
  delay(10); // debounce
  // if wpm changed the recalculate the
  // dit timing and and send an OK message
  if (prev_mode != realtime_xmit) {
    ditcalc();
    send_cwmsg("OK", 0);
    back2run();
  } else menu_mode();
}


// select keyer mode
void menu_mode() {
  uint8_t prev_mode = keyermode;
  lcd.clear();
  print_line(0, "KEYER IS");
  // wait until button is released
  while (sw1Pushed) {
    read_switch();
    delay(10);
  }
  // loop until button is pressed
  while (!sw1Pushed) {
    read_switch();
    keyerinfo = 0;
    read_paddles();
    if (keyerinfo & KEY_REG) {
      keyermode++;
    }
    if (keyermode > IAMBICB) {
      keyermode = IAMBICA;
    }
    while (GOTKEY) {
      keyerinfo = 0;
      read_paddles();
      delay(10);
    }
    keyerinfo = 0;
    switch (keyermode) {
      case IAMBICA:
        print_line(1, "IAMBIC A");
        break;
      case IAMBICB:
        print_line(1, "IAMBIC B");
        break;
      default:
        print_line(1, "ERROR");
        break;
    }
  }
  delay(10); // debounce
  if (prev_mode != keyermode) 
  {
    EEPROM[5] = keyermode;
    send_cwmsg("OK", 0);
  }
  back2run();
}

// send a message
void menu_msg() {
test_again:
  if( lesson_mode == 1 )
  {
    char *quiz = "ALL WORK  AND NO PLAY MAKES  JACK A DULL BOY.  A DULL BOY.   A DULL BOY.   DULL";
    myrow = 0;
    mycol = 0;
    lcd.clear();
    // wait until button is released
    while (sw1Pushed) {
      read_switch();
      delay(1);
    }

    lcd.clear();
    print_line(0, "QUIZ MODE");

    delay(2200);

    lcd.clear();

    char* lesson_seq = "KMRSUAPTLOWI.NJEF0Y,VG5/Q9ZH38B?427C1D6X";
    int len = strlen(quiz);

    if( len > lesson_size )
    {
      len = lesson_size;
    }
    
    srandom( millis() );

    for( int i=0; i < len; i++ )
    {
      quiz[i] = (random() % 4) == 0 && quiz[i-1] != ' ' && lesson_size > 6 ? ' ' : lesson_seq[random() % (lesson+1)];
    }
    quiz[len] = 0;
    send_cwmsg(quiz, 1);

    // loop until button is pressed
    while (!sw1Pushed) {

      if( !digitalRead(pinDit) && !digitalRead(pinDah) )
        goto test_again;

      read_switch();
      delay(1);
    }
    back2run();
  }
  else
  {
    char *msg = "ALL WORK  AND NO PLAY MAKES  JACK A DULL BOY. ";
    myrow = 0;
    mycol = 0;
    lcd.clear();
    // wait until button is released
    while (sw1Pushed) {
      read_switch();
      delay(1);
    }
    // loop until button is pressed
    while (!sw1Pushed) {
      send_cwmsg(msg, 1);
    }
    back2run();
  }
}

// reset the Arduino
void doError() {
  resetFunc();
}

// program setup
void setup() {
  // set GPIO
  pinMode(pinDit,  INPUT_PULLUP);
  pinMode(pinDah,  INPUT_PULLUP);
  pinMode(pinSw1,  INPUT_PULLUP);
  pinMode(pinBuzz, OUTPUT);
  // startup init
  Serial.begin(BAUDRATE);     // init serial/debug port
  change_wpm(INITWPM);        // init keyer speed
  lcd.init();                 // init LCD display
  lcd.backlight();
  lcd.clear();
  lcd.noCursor();

  print_line(0, "Decoder");
  print_line(1, "Version");
  print_line(2, VERSION);
  delay(1500);
  //lcd.setRowOffsets( 0, 20, 30 40 );
  lcd.clear();
  //lcd.autoscroll();
  lcd.setCursor(20,0);    

  byte val = EEPROM[0];
  if( val != ver )
  {
    EEPROM[0] = ver;
    EEPROM[1] = lesson = (byte) 0;
    EEPROM[2] = lesson_mode = (byte) 0;
    EEPROM[3] = farns = (byte) 0;
    EEPROM[4] = keyerwpm = (byte) INITWPM;
    EEPROM[5] = keyermode = (byte) IAMBICA;
    EEPROM[6] = lesson_size = (byte) MAXLESSONCNT;


    //EEPROM.write()
  }
  else
  {
    lesson = EEPROM[1];
    lesson_mode = EEPROM[2];
    farns = EEPROM[3];
    keyerwpm = EEPROM[4];
    keyermode = EEPROM[5];
    lesson_size = EEPROM[6];

    //EEPROM.write()
  }

  ditcalc();
}

// main loop
void loop() {
  uint32_t t0;
  read_switch();
  if (sw1Pushed) {
    event = BSC;
    delay(10); // debounce
    t0 = millis();
    // check for long button press
    while (sw1Pushed && (event != BPL)) {
      if (millis() > (t0 + LONGPRESS)) event = BPL;
      read_switch();
      delay(10);  // debounce
    }

    // button single click
    if (event == BSC) {
      switch (menumode) {
        case RUN_MODE:
          menumode = SETUP;
          break;
        default:
          menumode = 0;
      }
      menu_trainer_mode();
      event = NBP;
    }

    // long button press
    else if (event == BPL) {
      menu_msg();
      event = NBP;
    }
  }
  // no buttons pressed
  iambic_keyer();
}


