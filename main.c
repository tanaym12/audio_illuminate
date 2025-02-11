#include <msp430.h>

// Define states for program selection
#define PROGRAM_IDLE 0
#define PROGRAM_1_ACTIVATED 1
#define PROGRAM_2_ACTIVATED 2
#define RESET_ACTIVATED 3

// Function prototypes
void initialize();
void activate_program_1();
void activate_program_2();
void reset();

// Global variables
volatile unsigned int program_state = PROGRAM_IDLE;

void main() {
    initialize();
}

void initialize() {
    // Stop watchdog timer
    WDTCTL = WDTPW + WDTHOLD;

    // Set up GPIO for program selection and interrupts
    // P1.0 and P1.1 are configured for program selection
    P1DIR |= 0b00000001;             // Set P1.0 pin for output, including P1.1 are outputs
    P1OUT |= 0b00000011;             // Set Pin P1.0 to high and P1.1 to pullup
    P1REN |= 0b00000010;             // Enable pull-up/down resistor on P1.1
    P1IE |=  0b00000010;             // Enable input at P1.1 as an interrupt

    // Similar setup for P2.1
    P2DIR |= 0b00000000;
    P2OUT |= 0b00000010;
    P2REN |= 0b00000010;
    P2IE |=  0b00000010;

    // Enable interrupts and go into the lowest power mode
    _BIS_SR (LPM4_bits + GIE);

    // Additional initialization for your programs if needed
}

void activate_program_2() {
    // Code to activate program 2
    program_state = PROGRAM_2_ACTIVATED;

    // Define constants for peak detection
    #define PEAK_THRESHOLD 2800 // Peak threshold for spike detection
    #define LOW 400
    #define PEAK_HOLD_TIME 100  // Time to hold the peak value before resetting (in cycles)

    // Initialize ADC and peripherals
    // Stop the watchdog timer
    WDTCTL = WDTPW + WDTHOLD;
    // Configure ADC
    ADC12CTL0 = ADC12SHT02 + ADC12ON;   // Sampling time, ADC12 on
    ADC12CTL1 = ADC12SHP;               // Sampling timer
    ADC12MCTL0 = ADC12INCH_1;
    ADC12CTL0 |= ADC12ENC;                    // ADC enable

    // Set GPIO for LEDs
    P1DIR |= BIT0 + BIT2 + BIT4 + BIT5;
    P1OUT &= ~(BIT0 + BIT2 + BIT4 + BIT5);
    P4DIR |= BIT7;
    P4OUT &= ~BIT7;

    // Set P6.1 as input for analog signal from amplifier
    P6DIR &= ~BIT1;
    // Set P6.1 as analog input (ADC)
    P6SEL |= BIT1;

    // Variables for peak detection
    int peakValue = 0;
    int peakHoldCounter = 0;
    int snapCounter = 0;
    int patternCounter = 0;

    while(1) {
        // Read analog input voltage
        ADC12CTL0 |= ADC12SC;                   // Start sampling
        while (ADC12CTL1 & ADC12BUSY);          // Wait for conversion to complete
        int micVoltage = ADC12MEM0;

        // Update peak value if the current voltage is greater than the peak
        if (micVoltage > peakValue) {
            peakValue = micVoltage;
            peakHoldCounter = PEAK_HOLD_TIME;
        }

        // Decrement peak hold counter
        if (peakHoldCounter > 0) {
            peakHoldCounter--;
        } else {
            // Check if peak value exceeds threshold
            if (peakValue > PEAK_THRESHOLD || peakValue < LOW) {
                // Increment snap counter and handle snaps
                snapCounter++;
                // Handle different snap counts
                if (snapCounter % 6 == 5) { // 4th snap
                    // Turn off LEDs and start the pattern
                    P1OUT &= ~BIT2;
                    patternCounter = 0;
                    // Blink LEDs based on pattern
                    while (snapCounter % 6 == 5) {
                        // Read analog input voltage to check for snap
                        ADC12CTL0 |= ADC12SC;                   // Start sampling
                        while (ADC12CTL1 & ADC12BUSY);          // Wait for conversion to complete
                        int micVoltage = ADC12MEM0;
                        if (micVoltage > PEAK_THRESHOLD || micVoltage < LOW) {
                            P1OUT &= ~(BIT2 + BIT4 + BIT5); // Turn off all LEDs
                            break; // Exit the blinking pattern loop
                        }
                        // Turn on LEDs based on the blinking pattern
                        if (!(patternCounter % 3)) { // Turn on 1.5
                            P1OUT |= BIT5;
                        } else if (patternCounter % 3 == 1) { // Turn on 1.4
                            P1OUT |= BIT4;
                            P1OUT &= ~BIT5;
                        } else if (patternCounter % 3 == 2) { // Turn on 1.2
                            P1OUT |= BIT2;
                            P1OUT &= ~(BIT4 + BIT5);
                        }
                        // Delay for LED blink
                        __delay_cycles(25000);
                        // Turn off LEDs
                        P1OUT &= ~(BIT2 + BIT4 + BIT5);
                        patternCounter++;
                    }
                } else if (snapCounter % 6 == 0) { // 5th snap
                    // Turn off all LEDs
                    P1OUT &= ~(BIT2 + BIT4 + BIT5);
                    // Turn on P1.0 to indicate program idle
                    P1OUT |= BIT0;
                    program_state = PROGRAM_IDLE;
                    __delay_cycles(1000); // Delay for stability
                    break;
                } else if (snapCounter % 6 == 4) { // 5th snap
                    // Turn off all LEDs
                    P1OUT |= (BIT2 + BIT4 + BIT5);
                }  else if ((snapCounter % 6 == 1) || (snapCounter % 6 == 2) || (snapCounter % 6 == 3)) { // First three snaps
                    // Implement the desired functionality based on current LED state
                    if (!(P1OUT & BIT2) && !(P1OUT & BIT4) && !(P1OUT & BIT5)) { // If no LED is on
                        P1OUT |= BIT5;  // Turn on 1.5
                    } else if (P1OUT & BIT5) { // If 1.5 is on
                        P1OUT &= ~BIT5;     // Turn off 1.5
                        P1OUT |= BIT4;      // Turn on 1.4
                    } else if (P1OUT & BIT4) { // If 1.4 is on
                        P1OUT &= ~BIT4;     // Turn off 1.4
                        P1OUT |= BIT2;      // Turn on 1.2
                    }
                }
            }
            // Reset peak value
            peakValue = 0;
        }

        // Add a small delay to avoid rapid toggling
        __delay_cycles(1000); // Adjust delay based on requirement
    }
}

void activate_program_1() {
    // Code to activate program 1
    program_state = PROGRAM_1_ACTIVATED;

    // Define threshold difference for LED indication
    #define THRESHOLD_DIFFERENCE 250

    // Set GPIO for LEDs
    P1DIR |= BIT0 + BIT2 + BIT4 + BIT5;
    P1OUT &= ~(BIT0 + BIT2 + BIT4 + BIT5);
    P4DIR |= BIT7;
    P4OUT &= ~BIT7;

    // Delay before starting program to ensure stability
    __delay_cycles(800000); // Delay for 0.5 seconds
    P4OUT |= BIT7;

    // ADC configuration
    ADC12CTL0 = ADC12SHT02 + ADC12ON;   // 12-bit conversion results, Sampling time, ADC12 on
    ADC12CTL1 = ADC12SHP;               // Use sampling timer
    ADC12CTL0 |= ADC12ENC;              // Enable ADC12
    P6SEL |= BIT0;                      // Enable A/D channel A0

    // Variables for voltage comparison
    int initial_voltage = 1860;
    int max_voltage = 0;

    // Read initial voltage
    ADC12CTL0 |= ADC12SC;               // Start sampling
    while (ADC12CTL1 & ADC12BUSY);      // Wait for conversion to complete
    initial_voltage = ADC12MEM0;        // Store initial voltage value

    // Delay loop for 5 seconds to capture maximum voltage
    int i;
    for (i = 0; i < 1800; ++i) {
        // Read ADC value
        ADC12CTL0 |= ADC12SC;           // Start sampling
        while (ADC12CTL1 & ADC12BUSY);  // Wait for conversion to complete
        int adc_value = ADC12MEM0;

        // Update max voltage if necessary
        if (adc_value > max_voltage)
            max_voltage = adc_value;

        // Delay approximately 1 millisecond
        __delay_cycles(1000);
    }

    // Calculate difference compared to the initial value
    int diff = max_voltage - initial_voltage;

    // Turn on appropriate LED based on difference
    if ((diff > THRESHOLD_DIFFERENCE && diff < 2 * THRESHOLD_DIFFERENCE) ||
       (diff < -THRESHOLD_DIFFERENCE && diff > -(2 * THRESHOLD_DIFFERENCE)))
    {
        // If difference exceeds threshold but less than 1.5 times the threshold, turn on green LED
        P1OUT &= ~BIT4;         // Turn off P1.0 LED (Red LED)
        P1OUT |= BIT5;
        P1OUT |= BIT2;          // Turn on P4.7 LED (Green LED)
        P4OUT &= ~BIT7;

        __delay_cycles(3000000);

        P1OUT &= ~(BIT2 + BIT4 + BIT5);
        __delay_cycles(50000);
        P1OUT |= BIT0;
        program_state = PROGRAM_IDLE;
    }
    else if ((diff > 2 * THRESHOLD_DIFFERENCE) || (diff < -(2 * THRESHOLD_DIFFERENCE)))
    {
        // If difference exceeds 1.5 times the threshold, turn on red LED
        P1OUT &= ~BIT2;         // Turn off P1.0 LED (Red LED)
        P1OUT &= ~BIT5;
        P1OUT |= BIT4;          // Turn on P4.7 LED (Green LED)
        P4OUT &= ~BIT7;

        __delay_cycles(3000000);

        P1OUT &= ~(BIT2 + BIT4 + BIT5);
        __delay_cycles(50000);
        P1OUT |= BIT0;
        program_state = PROGRAM_IDLE;

    }
    else
    {
        // If difference is within threshold, blink both LEDs alternatively
        P4OUT &= ~BIT7;
        for (i = 0; i < 30; ++i) { // Blink 5 times
            P1OUT ^= BIT5; // Toggle P4.7 LED (Red LED)
            P1OUT ^= BIT4; // Toggle P1.0 LED (Green LED)
            P1OUT ^= BIT2;

            __delay_cycles(90000); // Delay for 0.5 seconds
        }
    }
    P1OUT &= ~(BIT2 + BIT4 + BIT5);
    __delay_cycles(50000);
    P1OUT |= BIT0;
    program_state = PROGRAM_IDLE;
}

// Interrupt service routine for Port 1
void __attribute__ ((interrupt(PORT1_VECTOR))) PORT1_ISR(void) {
    // Check program state and activate program 1 if idle
    if (program_state == PROGRAM_IDLE) {
        _delay_cycles (250000);
        activate_program_1();
        P1IFG &= ~0b00000010; // Clear P1.1 IFG to avoid re-triggering
    }
}

// Interrupt service routine for Port 2
void __attribute__ ((interrupt(PORT2_VECTOR))) PORT2_ISR(void) {
    // Check program state and activate program 2 if idle
    if (program_state == PROGRAM_IDLE) {
        _delay_cycles (250000);
        activate_program_2();
        P2IFG &= ~0b00000010; // Clear P2.1 IFG to avoid re-triggering
    }
}
