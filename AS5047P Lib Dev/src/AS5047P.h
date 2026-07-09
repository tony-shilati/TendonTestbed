// general purpose tools for pico coding -- includes i2c reading & writing.

#ifndef AS5047P_H
#define AS5047P_H

#include <SPI.h>

// Register Addresses
#define AS5047P_ANGLECOM    0x3FFE      // Corrected angle (using DAEC)
#define AS5047P_ANGLEUNC    0x3FFF      // Raw angle (uncorrected)
#define AS5047P_NOP         0x0000      // No operation

// Frame bitmasks
#define AS5047P_READ_BIT    (1 << 14)   // 1 = read, 0 = write
#define AS5047P_PARITY_BIT  (1 << 15)   // even parity overbits 14:0
#define AS5047P_ERROR_BIT   (1 << 14)   // error flag in response

// Calculate parity over 14:0 bits
static inline uint16_t as5047p_parity(uint16_t val){
    val &= 0x7FFF;      // keeps only bits 14:0
    uint16_t parity = 0;     // parity flag
    while (val){        // while val not equal to zero, which keeps us 
                        // going until all 1's have been accoutned for.

        p ^= (val & 1); // isolate lowest bit of val, then XOR into p
        val >>= 1;      // shift all bits of val
    }
    return parity;
}


/* Unused boilerplate
TODO: implement simple SPI Read/Writes for as5047p
// proof of life functions
void init_heartbeat(int heartbeat_pin, int blink_type, int timing);
bool led_beat(repeating_timer_t *repeatingtimer);
*/

/* Pulls the desired CS Pin low to tell the SPI device that we are speaking to it.
 *
 * 
 * @param cs_pin The CS Pin connected to desired SPI Device
 * 
 * @return void
*/
/*
static inline void cs_select(uint cs_pin) {
    asm volatile("nop \n nop \n nop"); // FIXME
    gpio_put(cs_pin, 0);
    asm volatile("nop \n nop \n nop"); // FIXME
}
*/


/* Pulls the desired CS Pin high to tell the SPI device that we are *not* speaking to it.
 *
 * 
 * @param cs_pin The CS Pin connected to desired SPI Device
 * 
 * @return void
*/
/*
static inline void cs_deselect(uint cs_pin) {
    asm volatile("nop \n nop \n nop"); // FIXME
    gpio_put(cs_pin, 1);
    asm volatile("nop \n nop \n nop"); // FIXME
}
*/

#endif