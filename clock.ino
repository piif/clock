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
        #define BUTTONS_PIN 3 // pin 1
    #endif

    #define MATRIX_CLK 1 // pin 6
    #define MATRIX_CS  4 // pin 3
    // #define MATRIX_DIN 3 // pin 2
    #define MATRIX_DIN 0 // pin 5
#endif

DS3231 rtc;

byte intensity = 3;
LedMatrix ledMatrix = LedMatrix(12, MATRIX_CLK, MATRIX_CS, MATRIX_DIN, intensity);

// int buttonThreshold[] = { 600, 450, 300 };  // value with 22k / 10k / 10k
int buttonThreshold[] = { 900, 800, 700 };  // value with 56k / 10k / 10k
#ifdef USE_BUTTONS
Buttons buttons = Buttons(BUTTONS_PIN, 3, buttonThreshold);
#endif

void setup() {
#ifdef HAVE_SERIAL
    Serial.begin(115200);
#endif

    // TODO : use internal counter instead of external tick
    /* DS3231 outputs nothing but continues counting when on battery */
    rtc.setControl(0b00000000 , 0b00000000);

#ifdef ARDUINO_UNO
    // initialize external interrupt on button pin A0 (PCINT8)
#ifdef USE_BUTTONS
    PCMSK1 |= (1 << PCINT8);
    PCICR  |= 1 << PCIE1;
#endif

    // timer 1 : WGM 4 = CTC , prescale 3 = /64
    TCCR1A = 0;
    TCCR1B = (1 << WGM12) | (1 << CS12) |(1 << CS10);
    TCCR1C = 0;
    TIMSK1 = 1 << OCIE1A;
    OCR1A = 15625;
#else // ARDUINO_TINY
#ifdef USE_BUTTONS
    PCMSK |= (1 << PCINT3);
    GIMSK |= 1 << PCIE;
#endif

	// set Clock to 1MHz instead of 8
	cli(); CLKPR=0x80 ; CLKPR=0x03; sei();

    TCCR1 = (1 << CTC1) | 0xF; // reset on OCR1A match | prescale 16384
    // to debug, output on OC1A = out 1 : | (1 << PWM1A) | (1 << COM1A0)
    OCR1A = 30; // interrupt before counter reset
    OCR1C = 61; // -> ~1s
    GTCCR = 0;
    TIMSK = 1 << OCIE1A;
#endif

#ifdef HAVE_SERIAL
    Serial.println(F("Setup OK"));
#endif
}

void displayTime(TimeStruct *time, byte offset, byte maxLen) {
    byte X = 1;
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

#ifdef HAVE_SERIAL
    Serial.print(F("X = 1 -> ")); Serial.println(X);
    Serial.print(time->hours); Serial.print(':');
    Serial.print(time->minutes); Serial.print(':');
    Serial.print(time->seconds); Serial.println();
#endif
}

void displayDate(TimeStruct *time, byte offset, byte maxLen) {
    byte X = 33;
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

#ifdef HAVE_SERIAL
    Serial.print(time->dayOfWeek); Serial.print(',');
    Serial.print(time->dayOfMonth); Serial.print('/');
    Serial.print(time->month); Serial.print('/');
    Serial.println(time->year);
#endif
}

#define ST_DISPLAY         0
#define ST_SET_YEAR        1
#define ST_SET_MONTH       2
#define ST_SET_DAY_OF_WEEK 3
#define ST_SET_DAY         4
#define ST_SET_HOUR        5
#define ST_SET_MINUTES     6
#define ST_SET_SECONDS     7

#define ST_LAST            7

byte state = ST_DISPLAY;

const char prompt1[] PROGMEM = "annee";
const char prompt2[] PROGMEM = "mois";
const char prompt3[] PROGMEM = "jour semaine";
const char prompt4[] PROGMEM = "jour";
const char prompt5[] PROGMEM = "heure";
const char prompt6[] PROGMEM = "minute";
const char prompt7[] PROGMEM = "seconde";

const char * const prompts[] PROGMEM = {
    prompt1,
    prompt2,
    prompt3,
    prompt4,
    prompt5,
    prompt6,
    prompt7
};

byte button = 0;

void updateDisplay() {
    static TimeStruct time; // put static to avoid dynamic allocation of structure
    rtc.getTime(&time);
    toLocal(&time);

    ledMatrix.clear();

    if (state == ST_DISPLAY) {
        displayTime(&time, 0, 32);
        displayDate(&time, 32, 64);
    } else {
        const char * const prompt = (char *)pgm_read_word(&(prompts[state-1]));
        byte X = 33;
        X = ledMatrix.drawString_P(X, prompt);
        X = ledMatrix.drawChar(X, ' ');
        X = ledMatrix.drawChar(X, ':');
        X = ledMatrix.drawChar(X, ' ');

        X = 1;
        if (state == ST_SET_YEAR) {
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
    }
    if (delta == -1) {
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
    } else if (state == ST_SET_YEAR) {
        value = rtc.bcdToDec(rtc.registerRead(DS3231_REG_Year));
        if (changeValue(&value, delta, 0, 99)) {
            rtc.registerWrite(DS3231_REG_Year, rtc.decToBcd(value));
        }
    } else if (state == ST_SET_MONTH) {
        value = rtc.bcdToDec(rtc.registerRead(DS3231_REG_Month) & 0x7F);
        if (changeValue(&value, delta, 1, 12)) {
            rtc.registerWrite(DS3231_REG_Month, rtc.decToBcd(value));
        }
    } else if (state == ST_SET_DAY_OF_WEEK) {
        value = rtc.registerRead(DS3231_REG_Day);
        if (changeValue(&value, delta, 1, 7)) {
            rtc.registerWrite(DS3231_REG_Day, value);
        }
    } else if (state == ST_SET_DAY) {
        byte month = rtc.bcdToDec(rtc.registerRead(DS3231_REG_Month) & 0x1F);
        byte year  = rtc.bcdToDec(rtc.registerRead(DS3231_REG_Year));
        byte lastDay = lastDayOfMonth(month, year);
        value = rtc.bcdToDec(rtc.registerRead(DS3231_REG_Date));
        if (changeValue(&value, delta, 1, lastDay)) {
            rtc.registerWrite(DS3231_REG_Date, rtc.decToBcd(value));
        }
    } else if (state == ST_SET_HOUR) {
        value = rtc.bcdToDec(rtc.registerRead(DS3231_REG_Hours) & 0x3F);
        if (changeValue(&value, delta, 0, 23)) {
            rtc.registerWrite(DS3231_REG_Hours, rtc.decToBcd(value));
        }
    } else if (state == ST_SET_MINUTES) {
        value = rtc.bcdToDec(rtc.registerRead(DS3231_REG_Minutes));
        if (changeValue(&value, delta, 0, 59)) {
            rtc.registerWrite(DS3231_REG_Minutes, rtc.decToBcd(value));
        }
    } else if (state == ST_SET_SECONDS) {
        value = rtc.bcdToDec(rtc.registerRead(DS3231_REG_Seconds));
        if (changeValue(&value, delta, 0, 59)) {
            rtc.registerWrite(DS3231_REG_Seconds, rtc.decToBcd(value));
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
    clockTick = 1;
}

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
        if(timer1 > 61-6) {
            byte timerLoop = timer1 - (61-6);
            while(TCNT1 >= timer1 || TCNT1 < timerLoop); // wait for counter loop
        } else {
            timer1 += 6;
        } 
        while(TCNT1 < timer1); // wait 6 ticks = ~100ms
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
        needUpdate = 1;
    }
    if (needUpdate) {
        updateDisplay();
    }
#endif
}
