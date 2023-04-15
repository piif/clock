#ifndef BUTTONS_H
#define BUTTONS_H

#include <Arduino.h>

#define DEFAULT_DEBOUNCE_DELAY 100
#define NO_BUTTON_CHANGE 0xff

class Buttons {
public:
    Buttons(byte _input, byte _buttonsNumber, int *_thresholds
#ifndef NO_DEBOUNCE
        , int _debounceDelay = DEFAULT_DEBOUNCE_DELAY
#endif
    ) {
        input = _input;
        buttonsNumber = _buttonsNumber;
        thresholds = _thresholds;
#ifndef NO_DEBOUNCE
        debounceDelay = _debounceDelay;
#endif
        pinMode(input, INPUT);
    }

    byte read() {
        int v =  analogRead(input);
        byte newButton = decode(v);
        if (newButton != button) {
#ifndef NO_DEBOUNCE
            if (debounceDelay) {
                delay(debounceDelay);
                v =  analogRead(input);
                newButton = decode(v);
                if (newButton != button) {
                    button = newButton;
                    return button;
                }
            } else {
#endif
                button = newButton;
                return button;
#ifndef NO_DEBOUNCE
            }
#endif
        }
        return NO_BUTTON_CHANGE;
    }

private:
    byte input;
    byte buttonsNumber;
    int *thresholds;
#ifndef NO_DEBOUNCE
    int debounceDelay;
#endif

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