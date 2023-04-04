#include <Arduino.h>
#include "ds3231.h"
#include "ledMatrix.h"
#include "buttons.h"

#define HAVE_SERIAL

#define DS_SDA A4
#define DS_SCL A5
#define DS_SQW A3

#define BUTTONS_PIN  A0

#define MATRIX_CLK A2
#define MATRIX_CS  A1
// #define MATRIX_DIN A4
// #define MATRIX_DIN A5
#define MATRIX_DIN 7

DS3231 rtc;

byte intensity = 3;
LedMatrix ledMatrix = LedMatrix(12, MATRIX_CLK, MATRIX_CS, MATRIX_DIN, intensity);

// int buttonThreshold[] = { 600, 450, 300 };  // value wuth 22k / 10k / 10k
int buttonThreshold[] = { 900, 800, 700 };  // value wuth 56k / 10k / 10k
Buttons buttons = Buttons(BUTTONS_PIN, 3, buttonThreshold, 0);

void setup() {
#ifdef HAVE_SERIAL
    Serial.begin(115200);
#endif

    // TODO : use internal counter instead of external tick
    /* DS3231 outputs nothing but continues counting when on battery */
    rtc.setControl(0b00000000 , 0b00000000);

    // initialize external interrupt on button pin A0 (PCINT8) + clock pin A3 (PCINT11)
    PCMSK1 |= (1 << PCINT8) ;//| (1 << PCINT11);
    PCICR  |= 1 << PCIE1;

    pinMode(13, OUTPUT);
    // ledMatrix.setup();
    pinMode(BUTTONS_PIN, INPUT);

#ifdef HAVE_SERIAL
    Serial.println("Setup OK");
#endif
}

char *appendStr(char* dst, char* src) {
    while (*src) {
        *dst++ = *src++;
    }
    return dst;
}
char *appendChar(char* dst, char src) {
    *dst++ = src;
    return dst;
}

void displayTime(TimeStruct *time, byte offset, byte maxLen) {
    char str[9], *ptr = str;
    if (time->hours > 10) {
        ptr = appendChar(ptr, '0' + time->hours / 10);
    }
    ptr = appendChar(ptr, '0' + time->hours % 10);
    ptr = appendChar(ptr, ':');
    ptr = appendChar(ptr, '0' + time->minutes / 10);
    ptr = appendChar(ptr, '0' + time->minutes % 10);
    ptr = appendChar(ptr, ':');
    ptr = appendChar(ptr, '0' + time->seconds / 10);
    ptr = appendChar(ptr, '0' + time->seconds % 10);
    ptr = appendChar(ptr, 0);
    byte len = ledMatrix.stringWidth(str) - 1;
    ledMatrix.drawString(offset + (maxLen-len)/2, str);

// #ifdef HAVE_SERIAL
//     Serial.print(time->hours); Serial.print(':');
//     Serial.print(time->minutes); Serial.print(':');
//     Serial.print(time->seconds); Serial.println();
// #endif
}

void displayDate(TimeStruct *time, byte offset, byte maxLen) {
    char str[16], *ptr = str;
    ptr = appendStr(ptr, shortDays[time->dayOfWeek]);
    ptr = appendChar(ptr, ' ');
    if (time->dayOfMonth > 10) {
        ptr = appendChar(ptr, '0' + time->dayOfMonth / 10);
    }
    ptr = appendChar(ptr, '0' + time->dayOfMonth % 10);
    ptr = appendChar(ptr, ' ');
    ptr = appendStr(ptr, shortMonthes[time->month]);
    ptr = appendStr(ptr, " 20");
    ptr = appendChar(ptr, '0' + time->year / 10);
    ptr = appendChar(ptr, '0' + time->year % 10);
    ptr = appendChar(ptr, 0);
    byte len = ledMatrix.stringWidth(str) - 1;
    ledMatrix.drawString(offset + (maxLen-len)/2, str);

// #ifdef HAVE_SERIAL
//     Serial.print(shortDays[time->dayOfWeek]); Serial.print(' ');
//     Serial.print(time->dayOfMonth); Serial.print(' ');
//     Serial.print(shortMonthes[time->month]); Serial.print(' ');
//     Serial.print(time->year); Serial.print(' ');
// #endif
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

char *prompts[] = {
    " : ",
    "annee",
    "mois",
    "jour semaine",
    "jour",
    "heure",
    "minute",
    "seconde"
};

byte button = 0;

void updateDisplay() {
    TimeStruct time;
    rtc.getTime(&time);
    toLocal(&time);

    ledMatrix.clear();

    if (state == ST_DISPLAY) {
        displayTime(&time, 0, 32);
        displayDate(&time, 32, 64);
    } else {
        char str[16], *ptr = str;
        ptr = appendStr(ptr, prompts[state]);
        ptr = appendStr(ptr, prompts[0]);
        ptr = appendChar(ptr, 0);
        ledMatrix.drawString(33, str);

        ptr = str;
        switch(state) {
            case ST_SET_YEAR:
                ptr = appendStr(ptr, "20");
                ptr = appendChar(ptr, '0' + time.year / 10);
                ptr = appendChar(ptr, '0' + time.year % 10);
            break;
            case ST_SET_MONTH:
                ptr = appendStr(ptr, shortMonthes[time.month]);
            break;
            case ST_SET_DAY_OF_WEEK:
                ptr = appendStr(ptr, shortDays[time.dayOfWeek]);
            break;
            case ST_SET_DAY:
                if (time.dayOfMonth > 10) {
                    ptr = appendChar(ptr, '0' + time.dayOfMonth / 10);
                }
                ptr = appendChar(ptr, '0' + time.dayOfMonth % 10);
            break;
            case ST_SET_HOUR:
                if (time.hours > 10) {
                    ptr = appendChar(ptr, '0' + time.hours / 10);
                }
                ptr = appendChar(ptr, '0' + time.hours % 10);
            break;
            case ST_SET_MINUTES:
                ptr = appendChar(ptr, '0' + time.minutes / 10);
                ptr = appendChar(ptr, '0' + time.minutes % 10);
            break;
            case ST_SET_SECONDS:
                ptr = appendChar(ptr, '0' + time.seconds / 10);
                ptr = appendChar(ptr, '0' + time.seconds % 10);
            break;
        }
        ptr = appendChar(ptr, 0);
        ledMatrix.drawString(0, str);
    }

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
    ledMatrix.flush();
    pinMode(BUTTONS_PIN, INPUT);
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
    switch (state) {
        case ST_DISPLAY:
            // change intensity
            if (changeValue(&intensity, delta, 0, 7, 0)) {
                ledMatrix.setIntensity(intensity);
            }
        break;
        TODO : BCD conversions ! ! !
        case ST_SET_YEAR:
            value = rtc.registerRead(DS3231_REG_Year);
            if (changeValue(&value, delta, 0, 99)) {
                rtc.registerWrite(DS3231_REG_Year, value);
            }
        break;
        case ST_SET_MONTH:
            value = rtc.registerRead(DS3231_REG_Month) && 0x7F;
            if (changeValue(&value, delta, 1, 12)) {
                rtc.registerWrite(DS3231_REG_Month, value);
            }
        break;
        case ST_SET_DAY_OF_WEEK:
            value = rtc.registerRead(DS3231_REG_Day);
            if (changeValue(&value, delta, 1, 7)) {
                rtc.registerWrite(DS3231_REG_Day, value);
            }
        break;
        case ST_SET_DAY:
            // TODO
            byte month = rtc.registerRead(DS3231_REG_Month) && 0x7F;
            byte year  = rtc.registerRead(DS3231_REG_Year);
            byte lastDay = lastDayOfMonth(month, year);
            value = rtc.registerRead(DS3231_REG_Date);
            if (changeValue(&value, delta, 1, lastDay)) {
                rtc.registerWrite(DS3231_REG_Date, value);
            }
        break;
        case ST_SET_HOUR:
            value = rtc.registerRead(DS3231_REG_Hours);
            if (changeValue(&value, delta, 0, 60)) {
                rtc.registerWrite(DS3231_REG_Hours, value);
            }
        break;
        case ST_SET_MINUTES:
            value = rtc.registerRead(DS3231_REG_Minutes);
            if (changeValue(&value, delta, 0, 60)) {
                rtc.registerWrite(DS3231_REG_Minutes, value);
            }
        break;
        case ST_SET_SECONDS:
            value = rtc.registerRead(DS3231_REG_Seconds);
            if (changeValue(&value, delta, 0, 60)) {
                rtc.registerWrite(DS3231_REG_Seconds, value);
            }
        break;
    }
}

void handleButton(byte newButton) {
    button = newButton;

    switch (newButton) {
        case 1: // change state
#ifdef HAVE_SERIAL
             Serial.println("next state");
#endif
            if (state == ST_LAST) {
                state = ST_DISPLAY;
            } else {
                state++;
            }
        break;
        case 2:
#ifdef HAVE_SERIAL
             Serial.println("plus");
#endif
            handleState(1);
        break;
        case 3:
#ifdef HAVE_SERIAL
             Serial.println("minus");
#endif
            handleState(-1);
        break;
    }
}

volatile bool clockTick = 0;
volatile bool buttonChange = 0;

ISR(PCINT1_vect) {
    clockTick = !digitalRead(DS_SQW);
    buttonChange = 1;
    digitalWrite(13, 1);
}

unsigned long counter = 0;

void loop() {
    counter++;
    bool needUpdate = 0;

    if (buttonChange) {
        buttonChange = 0;
        delay(100);
        byte newButton = buttons.read();
        if (newButton == NO_BUTTON_CHANGE) {
            digitalWrite(13, 0);
        } else {
            handleButton(newButton);
            needUpdate = 1;
        }
    }
    if (clockTick) {
        clockTick = 0;
        needUpdate = 1;
    }
    if (needUpdate) {
// #ifdef HAVE_SERIAL
//         Serial.print(counter);
//         Serial.print(" t "); Serial.print(clockTick); Serial.print(buttonChange); Serial.println(button);
// #endif
        updateDisplay();
        digitalWrite(13, 0);
    }
}
