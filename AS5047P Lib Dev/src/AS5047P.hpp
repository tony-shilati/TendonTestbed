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
#define AS5047P_WRITE_BIT   (0 << 14)   // 1 = read, 0 = write
#define AS5047P_PARITY_BIT  (1 << 15)   // even parity overbits 14:0
#define AS5047P_ERROR_BIT   (1 << 14)   // error flag in response



// Header Functions

// Calculate parity over 14:0 bits
static inline uint16_t as5047p_parity(uint16_t val){
    val &= 0x7FFF;      // keeps only bits 14:0
    uint16_t parity = 0;     // parity flag
    while (val){        // while val not equal to zero, which keeps us 
                        // going until all 1's have been accoutned for.

        parity ^= (val & 1); // isolate lowest bit of val, then XOR into p
        val >>= 1;      // shift all bits of val
    }
    return parity;
}

// Build a read command frame given a register (bit 14 = 1)
static inline uint16_t as5047p_read_frame(uint16_t reg){
    uint16_t frame = AS5047P_READ_BIT | (reg & 0x3FFF);     // combine read 
                                                            // bit and address
                                                            // at 14 bits
    if (as5047p_parity(frame)){     // if parity must be added
        frame |= AS5047P_PARITY_BIT;        // add parity          
    }
    return frame;
}

// Build a write command frame given a register (bit 14 = 0)
static inline uint16_t as5047p_write_frame(uint16_t reg){
    uint16_t frame = AS5047P_READ_BIT | (reg & 0x3FFF);     // combine read 
                                                            // bit and address
                                                            // at 14 bits
    if (as5047p_parity(frame)){     // if parity must be added
        frame |= AS5047P_PARITY_BIT;        // add parity          
    }
    return frame;
}

static inline int16_t as5047p_read_reg(uint8_t cs_pin, uint16_t reg){
    uint16_t command = as5047p_read_frame(reg);     // build the read command to transfer

    // send read command to ask for data
    digitalWrite(cs_pin, LOW);      // spi mode 1, low = selected
    SPI.transfer16(command);        // send our command to the chip
    digitalWrite(cs_pin, HIGH);     // release chip

    // unfinished

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