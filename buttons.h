#ifndef BUTTONS_H
#define BUTTONS_H

#include <Arduino.h>

#define DEBOUNCE_DELAY 100
#define NO_BUTTON_CHANGE 0xff

class Buttons {
public:
    Buttons(byte _input, byte _buttonsNumber, int *_thresholds, int _debounceDelay = DEBOUNCE_DELAY) {
        input = _input;
        buttonsNumber = _buttonsNumber;
        thresholds = _thresholds;
        debounceDelay = _debounceDelay;
        pinMode(input, INPUT);
    }

    byte read() {
        int v =  analogRead(input);
        byte newButton = decode(v);
        if (newButton != button) {
            if (debounceDelay) {
                delay(debounceDelay);
                v =  analogRead(input);
                newButton = decode(v);
                if (newButton != button) {
                    button = newButton;
                    return button;
                }
            } else {
                button = newButton;
                return button;
            }
        }
        return NO_BUTTON_CHANGE;
    }

private:
    byte input;
    byte buttonsNumber;
    int *thresholds;
    int debounceDelay;

    byte button = 0;

    byte decode(int v) {
        for (byte b = 0; b < buttonsNumber; b++) {
            if (v > thresholds[b]) {
                return b+1;
            }
        }
        return 0;
    }
};

#endif