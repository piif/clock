// TODO :
// - revoir les registre counters pour Uno et tiny
// - voir si on arrive à compter précisement des secondes (avec une précision suffisante pour une heure ? une journée ? )
//   nécessite une fonction "+1" sur un struct time ou "+n", ou passer en epoch+x ?
// - modifier le code pour n'interroger le ds3231 qu'une fois par heure/jour
// - en // chercher "filtre passe bande" pour calibrer les condo + mettre une diode en sortie de tout sauf la matrice
// - en // , il faudra un radiateur au 7508, à moins qu'une autre alim suffise

// quand on modifie l'heure, on doit repasser en UTC sinon, au rattrapage ou reset suivant, on fait +1 ou +2
// + il faudrait caler les rattrapage sur les heures (minute==0) sinon, lors du changement d'heure ça fera n'importe quoi

// voir si c'est mieux avec 2 7805 différents ? (avec la diode entre les 2 et amener 2 +5 séparés dans le montage)

#if defined(__AVR_ATtiny85__) || defined(__AVR_ATtiny45__) || defined(__AVR_ATtiny25__)
	#define ARDUINO_TINY
#elif defined(__AVR_MEGA__)
	#define ARDUINO_UNO
    #define HAVE_SERIAL
#endif

#define USE_BUTTONS
// #define DEBUG_BUTTONS

#include <Arduino.h>
#include "ds3231.h"
#include "ledMatrix.h"
#ifdef USE_BUTTONS
    #define NO_DEBOUNCE
    #include "buttons.h"
#endif

#ifdef ARDUINO_UNO
    #define DS_SDA A4
    #define DS_SCL A5
    // #define DS_SQW A3

    #ifdef USE_BUTTONS
        #define BUTTONS_PIN  A0
    #endif

    #define MATRIX_CLK A2
    #define MATRIX_CS  A1
    #define MATRIX_DIN A4
    // #define MATRIX_DIN 7
#else
    #define DS_SDA 0 // pin 5
    #define DS_SCL 2 // pin 7
    // #define DS_SQW A3 TODO

    #ifdef USE_BUTTONS
        #define BUTTONS_PIN 3 // pin 2
    #endif

    #define MATRIX_CLK 1 // pin 6
    #define MATRIX_CS  4 // pin 3
    // #define MATRIX_DIN 3 // pin 2
    #define MATRIX_DIN 0 // pin 5
#endif

DS3231 rtc;

byte intensity = 0;
LedMatrix ledMatrix = LedMatrix(12, MATRIX_CLK, MATRIX_CS, MATRIX_DIN, intensity);

// int buttonThreshold[] = { 600, 450, 300 };  // value with 22k / 10k / 10k
#ifdef ARDUINO_UNO
int buttonThreshold[] = { 700, 600, 500 };  // value with 56k / 10k / 10k
#else
int buttonThreshold[] = { 900, 800, 700 };  // value with 56k / 10k / 10k
#endif
#ifdef USE_BUTTONS
Buttons buttons = Buttons(BUTTONS_PIN, 3, buttonThreshold);
#endif

void setup() {
#ifdef HAVE_SERIAL
    Serial.begin(115200);
#endif

    ledMatrix.inverted = true;

    /* DS3231 outputs nothing but continues counting when on battery */
    rtc.setControl(0b00000000 , 0b00000000);
#ifdef ARDUINO_UNO
    // initialize external interrupt on button pin A0 (PCINT8)
#ifdef USE_BUTTONS
    PCMSK1 |= (1 << PCINT8);
    PCICR  |= 1 << PCIE1;
#endif

    // timer 1 : WGM 4 = CTC , prescale 3 = /1024
    // 16MHz / 1024 = 15625Hz
    TCCR1A = 0;
    TCCR1B = (1 << WGM12) | (1 << CS12) |(1 << CS10);
    TCCR1C = 0;
    TIMSK1 = 1 << OCIE1A;
    OCR1A = 15625; // = 1s
#else // ARDUINO_TINY
#ifdef USE_BUTTONS
    PCMSK |= (1 << PCINT3);
    GIMSK |= 1 << PCIE;
#endif

	// set Clock to 1MHz instead of 8
	cli(); CLKPR=0x80 ; CLKPR=0x03; sei();

    TCCR1 = (1 << CTC1) | 0x7; // reset on OCR1A match | prescale 64
    // to debug, output on OC1A = out 1 : | (1 << PWM1A) | (1 << COM1A0)
    OCR1A = 30; // interrupt before counter reset
    OCR1C = 128; // 125ms should be 125 but ATtiny internal clock doesn't seem to be exact
    GTCCR = 0;
    TIMSK = 1 << OCIE1A;
#endif

    updateTimeFromRtc();

#ifdef HAVE_SERIAL
    Serial.println(F("Setup OK"));
#endif
}

void displayTime(TimeStruct *time, byte offset) {
    byte X = offset;
    byte h12 = time->hours;
    if (h12 > 12) {
        h12 -= 12;
    }
    if (h12 >= 10) {
        X = ledMatrix.drawChar(X, '1');
    }
    X = ledMatrix.drawChar(X, '0' + h12 % 10);
    X = ledMatrix.drawChar(X, ':');
    X = ledMatrix.drawChar(X, '0' + time->minutes / 10);
    X = ledMatrix.drawChar(X, '0' + time->minutes % 10);
    X = ledMatrix.drawChar(X, ':');
    X = ledMatrix.drawChar(X, '0' + time->seconds / 10);
    X = ledMatrix.drawChar(X, '0' + time->seconds % 10);

// #ifdef HAVE_SERIAL
//     Serial.print(F("X = 1 -> ")); Serial.println(X);
//     Serial.print(time->hours); Serial.print(':');
//     Serial.print(time->minutes); Serial.print(':');
//     Serial.print(time->seconds); Serial.println();
// #endif
}

void displayDate(TimeStruct *time, byte offset) {
    byte X = offset;
    X = ledMatrix.drawString_P(X, (char *)pgm_read_word(&(shortDays[time->dayOfWeek])));
    X = ledMatrix.drawChar(X, ' ');
    if (time->dayOfMonth >= 10) {
        X = ledMatrix.drawChar(X, '0' + time->dayOfMonth / 10);
    }
    X = ledMatrix.drawChar(X, '0' + time->dayOfMonth % 10);
    X = ledMatrix.drawChar(X, ' ');
    X = ledMatrix.drawString_P(X, (char *)pgm_read_word(&(shortMonthes[time->month])));
    X = ledMatrix.drawString(X, " 20");
    X = ledMatrix.drawChar(X, '0' + time->year / 10);
    X = ledMatrix.drawChar(X, '0' + time->year % 10);
    // byte len = ledMatrix.stringWidth(str) - 1;
    // ledMatrix.drawString(offset + (maxLen-len)/2, str);

// #ifdef HAVE_SERIAL
//     Serial.print(time->dayOfWeek); Serial.print(',');
//     Serial.print(time->dayOfMonth); Serial.print('/');
//     Serial.print(time->month); Serial.print('/');
//     Serial.println(time->year);
// #endif
}

#define ST_DISPLAY         0
#define ST_SET_OFFSET      1
#define ST_SET_YEAR        2
#define ST_SET_MONTH       3
#define ST_SET_DAY_OF_WEEK 4
#define ST_SET_DAY         5
#define ST_SET_HOUR        6
#define ST_SET_MINUTES     7
#define ST_SET_SECONDS     8

#define ST_LAST            8

byte state = ST_DISPLAY;

const char prompt1[] PROGMEM = "offset";
const char prompt2[] PROGMEM = "annee";
const char prompt3[] PROGMEM = "mois";
const char prompt4[] PROGMEM = "jour semaine";
const char prompt5[] PROGMEM = "jour";
const char prompt6[] PROGMEM = "heure";
const char prompt7[] PROGMEM = "minute";
const char prompt8[] PROGMEM = "seconde";

const char * const prompts[] PROGMEM = {
    prompt1,
    prompt2,
    prompt3,
    prompt4,
    prompt5,
    prompt6,
    prompt7,
    prompt8
};

byte button = 0;

TimeStruct time; // put static to avoid dynamic allocation of structure

void updateTimeFromRtc() {
    rtc.getTime(&time);
    toLocal(&time);
}

void updateDisplay() {
    ledMatrix.clear();

    if (state == ST_DISPLAY) {
        displayTime(&time, 1);
        displayDate(&time, 32);
    } else {
        const char * const prompt = (char *)pgm_read_word(&(prompts[state-1]));
        byte X = 33;
        X = ledMatrix.drawString_P(X, prompt);
        X = ledMatrix.drawChar(X, ' ');
        X = ledMatrix.drawChar(X, ':');
        X = ledMatrix.drawChar(X, ' ');

        X = 1;
        if (state == ST_SET_OFFSET) {
            byte offset = rtc.registerRead(DS3231_REG_Offset);
            // registerRead returns a byte, we must convert it to signed value
            if (offset >= 128) {
                X = ledMatrix.drawChar(X, '~'); // '-' is not in reduced charmap, but '~' yes
                offset = ~offset + 1;
            }
            X = ledMatrix.drawChar(X, '0' + offset / 100);
            offset = offset % 100;
            X = ledMatrix.drawChar(X, '0' + offset / 10);
            X = ledMatrix.drawChar(X, '0' + offset % 10);
        } else if (state == ST_SET_YEAR) {
            X = ledMatrix.drawChar(X, '2');
            X = ledMatrix.drawChar(X, '0');
            X = ledMatrix.drawChar(X, '0' + time.year / 10);
            X = ledMatrix.drawChar(X, '0' + time.year % 10);
        } else if (state == ST_SET_MONTH) {
            X = ledMatrix.drawString_P(X, (char *)pgm_read_word(&(shortMonthes[time.month])));
        } else if (state == ST_SET_DAY_OF_WEEK) {
            X = ledMatrix.drawString_P(X, (char *)pgm_read_word(&(shortDays[time.dayOfWeek])));
        } else if (state == ST_SET_DAY) {
            if (time.dayOfMonth >= 10) {
                X = ledMatrix.drawChar(X, '0' + time.dayOfMonth / 10);
            }
            X = ledMatrix.drawChar(X, '0' + time.dayOfMonth % 10);
        } else if (state == ST_SET_HOUR) {
            byte h12 = time.hours;
            char suffix = 'a';
            if (h12 > 12) {
                h12 -= 12;
                suffix = 'p';
            }
            if (h12 >= 10) {
                X = ledMatrix.drawChar(X, '0' + h12 / 10);
            }
            X = ledMatrix.drawChar(X, '0' + h12 % 10);
            X = ledMatrix.drawChar(X, suffix);
            X = ledMatrix.drawChar(X, 'm');
        } else if (state == ST_SET_MINUTES) {
            X = ledMatrix.drawChar(X, '0' + time.minutes / 10);
            X = ledMatrix.drawChar(X, '0' + time.minutes % 10);
        } else if (state == ST_SET_SECONDS) {
            X = ledMatrix.drawChar(X, '0' + time.seconds / 10);
            X = ledMatrix.drawChar(X, '0' + time.seconds % 10);
        }
    }

#ifdef USE_BUTTONS
    // debug buttons by displaying a pixel in the corner
    switch (button) {
        case 1:
            ledMatrix.drawPixel(0, 7, 1);
        break;
        case 2:
            ledMatrix.drawPixel(2, 7, 1);
        break;
        case 3:
            ledMatrix.drawPixel(4, 7, 1);
        break;
    }
#endif
    ledMatrix.flush();
#ifdef USE_BUTTONS
    pinMode(BUTTONS_PIN, INPUT);
#endif
}

bool changeValue(byte *value, short delta, short min, short max, bool cycle = 1) {
    if (delta == 1) {
        if (*value == max) {
            if (!cycle) {
                return 0;
            } else {
                *value = min;
            }
        } else {
            (*value)++;
        }
        return 1;
    } else {// delta == -1
        if (*value == min) {
            if (!cycle) {
                return 0;
            } else {
                *value = max;
            }
        } else {
            (*value)--;
        }
        return 1;
    }
}

void handleState(short delta) {
    byte value;
    if (state == ST_DISPLAY) {
        // change intensity
        if (changeValue(&intensity, delta, 0, 7, 0)) {
            ledMatrix.setIntensity(intensity);
        }
    } else if (state == ST_SET_OFFSET) {
        value = rtc.registerRead(DS3231_REG_Offset);
        if (changeValue(&value, delta, 0, 255)) {
            rtc.registerWrite(DS3231_REG_Offset, value);
        }
    } else if (state == ST_SET_YEAR) {
        if (changeValue(&(time.year), delta, 0, 99)) {
            rtc.registerWrite(DS3231_REG_Year, rtc.decToBcd(time.year));
        }
    } else if (state == ST_SET_MONTH) {
        if (changeValue(&(time.month), delta, 1, 12)) {
            rtc.registerWrite(DS3231_REG_Month, rtc.decToBcd(time.month));
        }
    } else if (state == ST_SET_DAY_OF_WEEK) {
        if (changeValue(&(time.dayOfWeek), delta, 1, 7)) {
            rtc.registerWrite(DS3231_REG_Day, time.dayOfWeek);
        }
    } else if (state == ST_SET_DAY) {
        byte lastDay = lastDayOfMonth(time.month, time.year);
        if (changeValue(&(time.dayOfMonth), delta, 1, lastDay)) {
            rtc.registerWrite(DS3231_REG_Date, rtc.decToBcd(time.dayOfMonth));
        }
    } else if (state == ST_SET_HOUR) {
        if (changeValue(&(time.hours), delta, 0, 23)) {
            TimeStruct utcTime = time;
            byte changes = fromLocal(&utcTime);
            switch (changes) {
                case 6:
                    rtc.registerWrite(DS3231_REG_Year, rtc.decToBcd(utcTime.year));
                    [[fallthrough]];
                case 5:
                    rtc.registerWrite(DS3231_REG_Month, rtc.decToBcd(utcTime.month));
                    [[fallthrough]];
                case 4:
                    rtc.registerWrite(DS3231_REG_Date, rtc.decToBcd(utcTime.dayOfMonth));
                    rtc.registerWrite(DS3231_REG_Day, utcTime.dayOfWeek);
                    [[fallthrough]];
                default:
                    rtc.registerWrite(DS3231_REG_Hours, rtc.decToBcd(utcTime.hours));
                    [[fallthrough]];
            }
        }
    } else if (state == ST_SET_MINUTES) {
        if (changeValue(&(time.minutes), delta, 0, 59)) {
            rtc.registerWrite(DS3231_REG_Minutes, rtc.decToBcd(time.minutes));
        }
    } else if (state == ST_SET_SECONDS) {
        if (changeValue(&(time.seconds), delta, 0, 59)) {
            rtc.registerWrite(DS3231_REG_Seconds, rtc.decToBcd(time.seconds));
        }
    }
}

void handleButton(byte newButton) {
    button = newButton;

    switch (newButton) {
        case 1: // change state
#ifdef HAVE_SERIAL
             Serial.println(F("next state"));
#endif
            if (state == ST_LAST) {
                state = ST_DISPLAY;
            } else {
                state++;
            }
        break;
        case 2:
#ifdef HAVE_SERIAL
             Serial.println(F("plus"));
#endif
            handleState(1);
        break;
        case 3:
#ifdef HAVE_SERIAL
             Serial.println(F("minus"));
#endif
            handleState(-1);
        break;
    }
}

volatile bool clockTick = 0;
#ifdef ARDUINO_TINY
volatile unsigned int subTick=0;
#endif

volatile bool buttonChange = 0;

#ifdef USE_BUTTONS
#ifdef ARDUINO_UNO
ISR(PCINT1_vect) {
#else
ISR(PCINT0_vect) {
#endif
    buttonChange = 1;
}
#endif

ISR(TIMER1_COMPA_vect) {
#ifdef ARDUINO_TINY
    if (subTick==125-1) {
        subTick=0;
        clockTick = 1;
    } else {
        subTick++;
    }
#endif
}

#ifdef HAVE_SERIAL
void printTime(TimeStruct *time) {
    Serial.print(time->year);       Serial.print('/');
    Serial.print(time->month);      Serial.print('/');
    Serial.print(time->dayOfMonth); Serial.print(' ');
    Serial.print(time->hours);      Serial.print(':');
    Serial.print(time->minutes);    Serial.print(':');
    Serial.println(time->seconds);
}
#endif

void loop() {
#ifdef DEBUG_BUTTONS
    if (clockTick) {
        clockTick = 0;
        int v = analogRead(BUTTONS_PIN);
        byte X = 33;
        ledMatrix.clear();
        X = ledMatrix.drawChar(X, '0' + (v / 1000));
        X = ledMatrix.drawChar(X, '0' + ((v / 100) % 10));
        X = ledMatrix.drawChar(X, '0' + ((v / 10) % 10));
        X = ledMatrix.drawChar(X, '0' + (v % 10));
        ledMatrix.flush();
    }
#else
    bool needUpdate = 0;

    if (buttonChange) {
        buttonChange = 0;
#ifdef ARDUINO_UNO
        byte timer1 = TCNT1;
        if (timer1 > 15625-1562) {
             byte timerLoop = timer1 - (15625-1562);
            while(TCNT1 >= timer1 || TCNT1 < timerLoop); // wait for counter loop
        } else {
            timer1 += 1562;
        } 
        while(TCNT1 < timer1); // wait 1562 ticks = 100ms
#else // ARDUINO_TINY
        byte timer1 = TCNT1;
        if(timer1 > 125-12) {
            byte timerLoop = timer1 - (125-12);
            while(TCNT1 >= timer1 || TCNT1 < timerLoop); // wait for counter loop
        } else {
            timer1 += 12;
        } 
        while(TCNT1 < timer1); // wait 12 ticks = ~100ms
#endif
#ifdef USE_BUTTONS
        byte newButton = buttons.read();
        if (newButton != NO_BUTTON_CHANGE) {
            handleButton(newButton);
            needUpdate = 1;
        }
#endif
    }
    if (clockTick) {
        clockTick = 0;
        if (incrementSecond(&time) >= 2) {
            // on minute change, update to fix internal clock drift
#ifdef HAVE_SERIAL
            Serial.print("BEFORE : ");
            printTime(&time);
#endif
            updateTimeFromRtc();
#ifdef HAVE_SERIAL
            Serial.print("AFTER : ");
            printTime(&time);
#endif
        }
#ifdef HAVE_SERIAL
        // printTime(&time);
#endif
        needUpdate = 1;
    }
    if (needUpdate) {
        updateDisplay();
    }
#endif
}
