#include <Arduino.h>
#include <SPI.h>
#include "ADS1220.h"

#define CS_PIN    21
#define DRDY_PIN  9

ADS1220 adc(CS_PIN, DRDY_PIN);

// Calibration factor: (Vref / gain) / (2^23) — tune to your load cell
const float LOAD_CELL_CAL = 1.0f;

void setup() {
    Serial.begin(115200);
    while (!Serial) {}
    Serial.println("Teensy booted");

    SPI.begin();
    SPI.beginTransaction(SPISettings(1000000, MSBFIRST, SPI_MODE1));

    Serial.println("Initializing ADS1220...");

    adc.begin();
    pinMode(DRDY_PIN, INPUT_PULLDOWN);
    adc.reset();
    delay(100);

    Serial.println("ADS1220 reset done");

    adc.writeRegister(0x00, 0x2E);
    adc.writeRegister(0x01, 0xD4);
    delay(10);


    Serial.println("Registers written");
    Serial.print("Reg0 readback (should be 2E): 0x");
    Serial.println(adc.readRegister(0x00), HEX);
    Serial.print("Reg1 readback (should be D4): 0x");
    Serial.println(adc.readRegister(0x01), HEX);

    // Wait for up to 6 seconds for the serial port to be opened on the PC side.
    // If no PC connects, continue anyway.
    for (int i = 0; i < 60; ++i) {
        delay(100);
    }

    adc.startConversion();
    delay(100);
    Serial.println("Conversion started, waiting for DRDY...");

    // Zero the offset with 100 samples --> sets the initial position as zero.
    adc.findADCOffset(2000);

    Serial.println("SETUP COMPLETE");
}

void loop() {

    //    Wait for DRDY to go LOW (conversion ready)
    while (digitalRead(DRDY_PIN)) {}

    // Calibrated Values (?) not sure what they are calibrated to.
    int32_t raw = adc.readData();
    float value = (raw - 0) * LOAD_CELL_CAL;
    // Serial.println(value);
    Serial.print("time: "); // printing values to plot on loadcell_liveplot.py
    Serial.print(millis() / 1000.0, 3);
    Serial.print(" modified_weight: ");
    Serial.println(value, 6);

    //Raw Values
    Serial.println(raw);

}






























// #include <Arduino.h>
// #include <SPI.h>

 
// #define SPI_BAUDRATE 4096000/4
// #define SERIAL_BAUDRATE 115200
 
// #define CS_PIN 10 // LC0
// #define DRDY_PIN 9  //LC0
 
// //#define CS_PIN 8 // LC1
// //#define DRDY_PIN 7  //LC1
 
// #define RDATA 0x10
 
// #define LC_CAL 0.0000165527f // Load cell callibration factor (N/tick)
 
// // Global vars
// int32_t lc_offset = 0;
// uint8_t ch0_drdy = 0;
 
// // Function prototypes for reading and writing to the ADC
// int32_t readADC();
// void printADCData();
// void writeADCData(uint8_t* write_bufer);
// int32_t findADCOffset();
 
// volatile long readingSum = 0;
// volatile int samples = 0;
 
// IntervalTimer adc_read;
 
// void setup() {
//   // Create PWM to act as a clock for the ADS1220
//   pinMode(6, OUTPUT);
//   //digitalWrite(6, LOW);
//   analogWriteFrequency(6, 4096000);
//   analogWrite(6, 127);
 
//   // Initialize SPI
//   SPI.begin();
//   pinMode(CS_PIN, OUTPUT);
//   digitalWrite(CS_PIN, HIGH);
//   SPI.beginTransaction(SPISettings(SPI_BAUDRATE, MSBFIRST, SPI_MODE1));
 
//   // Initialize Serial and wait till connection is made
//   Serial.begin(SERIAL_BAUDRATE);
//   while (!Serial) {
//     delay(10);
//   }
//   Serial.println("Connected");
 
//   // Format the ADC configuration data
 
//   /*
//      Channel 1
//   */
//   //uint8_t config_msg[] = {0b01000001, 0b01101110, 0b11000100};//1000 sps
//   //uint8_t config_msg[] = {0b01000001, 0b01101110, 0b00000100};  //20sps
//   //uint8_t config_msg[] = {0b01000001, 0b01100001, 0b00000100};  //20sps and gain 1
 
//   /*
//      Channel 2
//   */
//   uint8_t config_msg[] = {0b01000001, 0b01011110, 0b00000100};  //20sps
//   //uint8_t config_msg[] = {0b01000001, 0b01011110, 0b11000100};//1000 sps
 
//   uint8_t sync_byte = 0b00001000;
 
//   // Write the configuration to the ADC
//   digitalWrite(CS_PIN, LOW);
//   SPI.transfer(config_msg, 3);
//   digitalWrite(CS_PIN, HIGH);
 
//   // Read the configuration and print to serial
 
//   // Sync the timer of the ADC and wait a specified time
//   digitalWrite(CS_PIN, LOW);
//   SPI.transfer(sync_byte);
//   delayMicroseconds(200);
//   digitalWrite(CS_PIN, HIGH);
 
//   // Callibrate the offset of the adc
//   lc_offset = findADCOffset();
//   Serial.println(lc_offset);
//   delay(2000);
 
//   // Start capturing the ADC data via interrupt
//   attachInterrupt(DRDY_PIN, printADCData, FALLING);
//   //adc_read.begin(printADCData, 1000);
 
// }
 
// void loop() {
 
 
// }
 
 
// void printADCData() {
//   if (!digitalRead(DRDY_PIN)) {
//     Serial.print("F: ");
//     //Serial.println((readADC() - lc_offset) * LC_CAL, 10);
//     Serial.println(readADC() - lc_offset);
//   }
// }
 
// int32_t readADC() {
//   uint8_t b0, b1, b2;
 
//   digitalWrite(CS_PIN, LOW);
//   SPI.transfer(RDATA);
//   b0 = SPI.transfer(0x00);
//   b1 = SPI.transfer(0x00);
//   b2 = SPI.transfer(0x00);
//   digitalWrite(CS_PIN, HIGH);
 
//   int32_t adc = (int32_t)b0 << 16 | (int32_t)b1 << 8 | (int32_t)b2;
 
//   // Sign-extend 24-bit to 32-bit
//   if (adc & 0x800000) {
//     adc |= 0xFF000000;
//   }
 
//   return adc;
// }
 
// int32_t findADCOffset() {
//   int32_t cal_size = 50;
//   int32_t cal_sum = 0;
 
//   for (int i = 0; i < cal_size; i++) {
//     while (digitalRead(DRDY_PIN)) {
//       delayMicroseconds(1);
//     }
//     cal_sum += readADC();
//   }
//   return  (int32_t) (1.0f * cal_sum / cal_size);
// }

































// ---------------- PREVIOUS CODE FOR NAU7802 CHIP WE ARE NOT USING, WE ARE USING ADS1220 NOW ----------------

// #include <Arduino.h>
// #include <Wire.h>
// #include <Adafruit_NAU7802.h>

// Adafruit_NAU7802 nau;

// // ---------------- USER-DEFINED CONSTANTS ----------------
// uint32_t USER_DURATION_S = 60;        // seconds (set once in setup)
// float    USER_CAL_WEIGHT = 0.0f;     // known calibration weight (units in gram)

// // ---------------- CONFIG ----------------
// const uint32_t SAMPLE_RATE_HZ = 320; // Fastest NAU7802 sample rate
// const uint32_t TOTAL_DURATION_S = 60; // seconds
// const uint32_t TOTAL_DURATION_MS = TOTAL_DURATION_S * 1000UL; // milliseconds
// const uint32_t TOTAL_SAMPLES = SAMPLE_RATE_HZ * TOTAL_DURATION_S; // total samples collected
// const uint32_t MAX_DURATION_S = 60; // Maximum allowed duration (seconds)

// // ---------------- NEW: dynamic offset from first 80 readings (1/4 seconds) ----------------
// const uint16_t OFFSET_CAL_SAMPLES = 80;      // Number of initial samples to use for dynamic offset calibration (at 320 SPS, 80 samples = 0.25 seconds)
// uint16_t offsetCount = 0;                   // Count of samples included in offset sum, up to OFFSET_CAL_SAMPLES
// int64_t  offsetSum = 0;                     // Accumulator for sum of raw values used to calculate dynamic offset (use int64_t to avoid overflow) (units in raw NAU7802 counts)
// bool     offsetLocked = false;              // Flag to indicate when dynamic offset is locked in and ready to use

// // ---------------- STORAGE ----------------
// uint32_t time_ms[TOTAL_SAMPLES];
// int32_t  raw[TOTAL_SAMPLES];

// uint32_t sampleIndex = 0;

// bool started = false;
// uint32_t runStart = 0;

// bool finished = false;

// // ---------------- CALIBRATION MATH ---------------- (y = mx + b) to convert raw NAU7802 counts to grams
// float strain_offset = 415043.1887; // NEEDS TO CHANGE BASED ON YOUR LOAD CELL AND CALIBRATION WEIGHT! (units in raw NAU7802 counts)
// float strain_slope_pos = 208.5548782;// NEEDS TO CHANGE BASED ON YOUR LOAD CELL AND CALIBRATION WEIGHT! (units in raw NAU7802 counts per gram)
// float strain_slope_neg = 209.6062518; // NEEDS TO CHANGE BASED ON YOUR LOAD CELL AND CALIBRATION WEIGHT! (units in raw NAU7802 counts per gram)
// uint32_t n = 0; // number of samples collected (for calibration math)
// float rtotal = 0;
// float mtotal = 0; // total modified weight (for calibration math)


// // helper to wait for a line of input (non-deadlocking)
// static void waitForUserInputLine() {   
//   while (!Serial.available()) {        
//     delay(10);                         // yields time to USB/Serial)
//   }
// }

// // helper to prompt user for duration + cal weight (re-used every run)
// static void promptUserConfig() {                    
//   Serial.println("Enter duration (seconds): ");     
//   waitForUserInputLine();                           
//   USER_DURATION_S = Serial.parseInt();              
//   Serial.readStringUntil('\n');                     // (clear rest of line)

//   if (USER_DURATION_S == 0) USER_DURATION_S = 1; 
//   if (USER_DURATION_S > MAX_DURATION_S) USER_DURATION_S = MAX_DURATION_S;

//   delay(200);                                      // (small buffer clear delay)

//   Serial.println("Enter calibration weight (in grams): ");
//   waitForUserInputLine();                           
//   USER_CAL_WEIGHT = Serial.parseFloat();            
//   Serial.readStringUntil('\n');                     // (clear rest of line)

//   Serial.println("CONFIG RECEIVED");                
//   Serial.print("Duration (s): ");                   
//   Serial.println(USER_DURATION_S);                  
//   Serial.print("Calibration weight: ");             
//   Serial.println(USER_CAL_WEIGHT);                  
// }

// // helper to ask user if they want to run again
// static bool askRunAgain() {                         
//   Serial.println("Run again? (y/n): ");             
//   waitForUserInputLine();                           
//   String ans = Serial.readStringUntil('\n');        
//   ans.trim();                                       
//   ans.toLowerCase();                                
//   return (ans.startsWith("y"));                     
// }

// // ---------------- SETUP ----------------
// void setup() {
//   Serial.begin(115200);

//   // Wait for Serial to be ready
//   while (!Serial && millis() < 3000) {}


//   // IMPORTANT NOTE: The following block of code is meant to be commented out if you want to use the live plot Python script
//   //-----------------------------------------------------------------------------------------------------------------------
//   // --- Comment out the following if you wanna use live plot

//   // delay(500); // Small delay to ensure Serial is ready before prompting user

//   // Serial.println("Enter duration (seconds): ");
//   // waitForUserInputLine(); 
//   // USER_DURATION_S = Serial.parseInt();
//   // Serial.readStringUntil('\n'); // Clear the rest of the line


//   // if (USER_DURATION_S == 0) USER_DURATION_S = 1;
//   // if (USER_DURATION_S > MAX_DURATION_S) USER_DURATION_S = MAX_DURATION_S;

//   // delay(500); // Small delay to ensure Serial buffer is clear before next prompt
  
  
//   // Serial.println("Enter calibration weight (in grams): ");
//   // waitForUserInputLine();
//   // USER_CAL_WEIGHT = Serial.parseFloat();
//   // Serial.readStringUntil('\n'); // Clear the rest of the line

//   // Serial.println("CONFIG RECEIVED");
//   // Serial.print("Duration (s): ");
//   // Serial.println(USER_DURATION_S);
//   // Serial.print("Calibration weight: ");
//   // Serial.println(USER_CAL_WEIGHT);

//   // End of comment out block for Python live plot (loadcell.liveplot.py)
//   //-----------------------------------------------------------------------------------------------------------------------


//   // Initialize and begin I2C on NAU7802
//   Wire.begin();
//   if (!nau.begin()) {
//     Serial.println("ERROR: NAU7802 not detected");
//     while (1) {}
//   }
  
//   // Setting NAU7802 parameters
//   nau.setGain(NAU7802_GAIN_128);
//   nau.setRate(NAU7802_RATE_320SPS);
//   //nau.calibrate(NAU7802_CALMOD_INTERNAL);

//   Serial.println("READY");
//   Serial.println("[Starting in 1 second...]");
//   delay(1000); // Give user a moment to see "READY" before data starts flowing
//   Serial.println("START");
// }

// // ------------- LOOP ----------------
// void loop() {

//   if (finished) {                                   
//     bool again = askRunAgain();                     
//     if (!again) {                                   
//       Serial.println("Stopping. (Reset board to start again)");
//       while (1) {        // (idle forever)
//         delay(1000); 
//       }
//     }

//     // user chose to run again -> re-prompt config + reset run state
//     promptUserConfig();                              
//     finished = false;                                
//     started  = false;                                
//     n = 0;                                        
//     rtotal = 0;
//     mtotal = 0;
    
//     // ---------------- NEW: reset dynamic offset accumulator for the new run ----------------
//     offsetCount = 0;          // NEW
//     offsetSum = 0;            // NEW
//     offsetLocked = false;     // NEW

//     Serial.println("[Starting in 1 second...]");     
//     delay(1000);                                     
//     Serial.println("START"); 
//   }                        

//   if(finished) {
//     return;
//   }

//   if (!started) {
//     started = true;
//     runStart = millis();
//   }

//   if (nau.available()) {
//     uint32_t t_ms = millis() - runStart;     // time since start of run
//     float t_s = t_ms / 1000.0f;              // seconds for nicer plotting
//     int32_t v = nau.read();
//     float modified_weight = 0;


//     // ---------------- NEW: build dynamic offset from first 50 readings ----------------
//     if (!offsetLocked) {                              // NEW
//       offsetSum += (int64_t)v;                        // NEW
//       offsetCount++;                                  // NEW
//       if (offsetCount >= OFFSET_CAL_SAMPLES) {        // NEW
//         strain_offset = (float)(offsetSum / (int64_t)OFFSET_CAL_SAMPLES); // NEW
//         offsetLocked = true;                          // NEW
//       }
//     }

//     // ---- THIS LINE FORMAT MATCHES PYTHON PARSER ----
//     // Must be whitespace-separated tokens: "X:" <num> "Y:" <num>
//     Serial.print("time: ");
//     Serial.print(t_s, 6);
//     Serial.print(" raw: ");
//     Serial.print((float)v);  // print as float so Python float() always works
//     Serial.print(" modified_weight: ");

//     // modified weight calculator
//     float delta = (float)v - strain_offset;          // use delta from offset (not sign of v)

//     if (delta >= 0) {                                // branch on delta sign
//       modified_weight = delta / strain_slope_pos;    // (v - offset)/slope_pos
//     } else {
//       modified_weight = delta / strain_slope_neg;    // still (v - offset), keeps negative values negative
//     }

//     Serial.println(modified_weight, 6);  // print as float so Python float() always works
    
//     n++;
//     rtotal += v;
//     mtotal += modified_weight;
    
    
//     if(t_s >= (float)USER_DURATION_S) {
//       finished = true;

//       // printing test results
//       float average = rtotal / n;
//       Serial.println("\n ----- TEST RESULTS ----- \n");
//       Serial.print("Average raw value: ");
//       Serial.println(average, 6);
//       float average_modified = mtotal / n;
//       Serial.print("Average modified weight: ");
//       Serial.println(average_modified, 6);
//       Serial.print("% error (modified weight vs cal weight): ");
//       float error_percent = 100.0f * (fabs(average_modified) - fabs(USER_CAL_WEIGHT)) / USER_CAL_WEIGHT;
//       Serial.println(error_percent, 2);
//       Serial.println("\n ----- CALIBRATION INFO ----- \n");

//       // printing test parameters info for reference
//       Serial.print("Calibration weight (g): ");
//       Serial.println(USER_CAL_WEIGHT, 6);
//       Serial.print("Calculated strain offset: ");
//       Serial.println(strain_offset, 6);
//       Serial.print("Calculated strain slope (pos): ");
//       Serial.println(strain_slope_pos, 6);
//       Serial.print("Calculated strain slope (neg): ");
//       Serial.println(strain_slope_neg, 6);
      
//       Serial.println("\nTest Done \n");
//     }
//   }
// }
