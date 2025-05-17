#include "mbed.h"

// Define shield connections (NUCLEO-F401RE pins)
DigitalOut latch(PB_5);    // D4 -> LCHCLK (Latch)
DigitalOut clk(PA_8);      // D7 -> SFTCLK (Clock)
DigitalOut data(PA_9);     // D8 -> SDI (Data)
DigitalIn  button1(PA_1);  // S1 -> A1 (Reset button, active low)
DigitalIn  button3(PB_0);  // S3 -> A3 (Mode button, active low)
AnalogIn   pot(PA_0);      // Potentiometer -> A0

// 7-segment encoding (Common Anode – active low)
const uint8_t Screen[10] = {
    0xC0, 0xF9, 0xA4, 0xB0, 0x99,
    0x92, 0x82, 0xF8, 0x80, 0x90
};

// Digit selector (active low, 4-digit)
const uint8_t Selector[4] = { 0xF1, 0xF2, 0xF4, 0xF8 };

// Shared state
volatile int Sec = 0;
volatile bool Displayer = true; 
Ticker tick_2;
Ticker refresh;
volatile int currentDigit = 0;

// ISR: Increment clock
void tick() {
    Sec++;
    if (Sec >= 6000) Sec = 0;  // Wrap around (max 99:59)
}

// ISR: Trigger display refresh
void refreshISR() {
    Displayer = true;
}

// Send data to 74HC595 shift registers
void outputToDisplay(uint8_t segments, uint8_t digitSelect) {
    latch = 0;
    // Shift segments (MSB first)
    for (int i = 7; i >= 0; --i) {
        data = (segments >> i) & 0x1;
        clk = 0; clk = 1;
    }
    // Shift digit selection
    for (int i = 7; i >= 0; --i) {
        data = (digitSelect >> i) & 0x1;
        clk = 0; clk = 1;
    }
    latch = 1;
}

int main() {
    // Set button modes
    button1.mode(PullUp);
    button3.mode(PullUp);

    // Start timers
    tick_2.attach(&tick, 1.0);           // Every 1 sec
    refresh.attach(&refreshISR, 0.002);  // Every 2 ms (refresh display)

    // Initial mode
    bool modeVoltage = false;
    int prev_b1 = 1, prev_b3 = 1;

    while (true) {
        // --- Button S1: Reset time ---
        int b1 = button1.read();
        if (b1 == 0 && prev_b1 == 1) {
            Sec = 0;
        }
        prev_b1 = b1;

        // --- Button S3: Switch mode ---
        int b3 = button3.read();
        modeVoltage = (b3 == 0);  // Hold to show voltage
        prev_b3 = b3;

        // --- Refresh display ---
        if (Displayer) {
            Displayer = false;

            uint8_t segByte = 0xFF, selByte = 0xFF;

            if (!modeVoltage) {
                // Display MM:SS
                int seconds = Sec % 60;
                int minutes = Sec / 60;

                switch (currentDigit) {
                    case 0: segByte = Screen[minutes / 10]; selByte = Selector[0]; break;
                    case 1: segByte = Screen[minutes % 10] & 0x7F; selByte = Selector[1]; break; // add colon
                    case 2: segByte = Screen[seconds / 10]; selByte = Selector[2]; break;
                    case 3: segByte = Screen[seconds % 10]; selByte = Selector[3]; break;
                }
            } else {
                // Display voltage in X.XXX format
                float volts = pot.read() * 3.3f;
                int millivolts = (int)(volts * 1000.0f);
                if (millivolts > 9999) millivolts = 9999;

                int intPart = millivolts / 1000;
                int fracPart = millivolts % 1000;

                switch (currentDigit) {
                    case 0: segByte = Screen[intPart] & 0x7F; selByte = Selector[0]; break; // Decimal point
                    case 1: segByte = Screen[fracPart / 100]; selByte = Selector[1]; break;
                    case 2: segByte = Screen[(fracPart % 100) / 10]; selByte = Selector[2]; break;
                    case 3: segByte = Screen[fracPart % 10]; selByte = Selector[3]; break;
                }
            }

            outputToDisplay(segByte, selByte);
            currentDigit = (currentDigit + 1) % 4;
        }

        // Optionally sleep here in real projects
        // ThisThread::sleep_for(1ms);
    }
}