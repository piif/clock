#include <Arduino.h>

void setup() {
	// set Clock to 1MHz instead of 8
	cli(); CLKPR=0x80 ; CLKPR=0x03; sei();

    TCCR1 = (1 << CTC1) | (1 << COM1A0) | 0x7; // reset on OCR1A match | prescale 64 = 15625Hz
    // to debug, output on OC1A = out 1 : | (1 << PWM1A) | (1 << COM1A0)
    OCR1B = 30;  // interrupt before counter reset
    OCR1C = 125; // 125/15625 = 8ms = 1/125s
    GTCCR = (1 << COM1B0); // toggle OCB1 on match
    TIMSK = 1 << OCIE1B;

    pinMode(0, OUTPUT);
    pinMode(1, OUTPUT);
}

volatile byte subTick=0;
volatile byte clockTick=0;

ISR(TIMER1_COMPB_vect) {
    if (subTick==125-1) {
        subTick=0;
        clockTick = 1;
    } else {
        subTick++;
    }
}

byte out = 0;

void loop() {
    if (clockTick) {
        clockTick = 0;
        digitalWrite(0, out);
        out = ~out;
    }
}