#include <Arduino.h>
#include "ODriveCAN.h"
#include "encoder.hpp"
#include "spi_encoder.hpp"
#include <SPI.h>
#include "ADS1220.h"

// Pin Defines
#define LOADCELL_CS_PIN 21
#define ENCODER_CS_PIN 10
#define DRDY_PIN 9

//Start ADC
ADS1220 adc(LOADCELL_CS_PIN, DRDY_PIN);

// Load Cell data flag setting
volatile bool adcDataReady = false;

void onDRDY() {
  adcDataReady = true;
}

// Load Cell calibration factor: (Vref / gain) / (2^23) — tune to your load cell
const float LOAD_CELL_SCALE  = 0.07007488819976268f;
const float LOAD_CELL_OFFSET = 49.394776217565585f;

// Modified Example Code derived from ODrive's Website

// Documentation for the example can be found here:
// https://docs.odriverobotics.com/v/latest/guides/arduino-can-guide.html

// CAN bus baudrate. Make sure this matches for every device on the bus
#define CAN_BAUDRATE 250000

// ODrive node_id for odrv0
#define ODRV0_NODE_ID 0

// Uncomment below the line that corresponds to your hardware.
// See also "Board-specific settings" to adapt the details for your hardware setup.

#define IS_TEENSY_BUILTIN // Teensy boards with built-in CAN interface (e.g. Teensy 4.1). See below to select which interface to use.

/* Board-specific includes ---------------------------------------------------*/

#if defined(IS_TEENSY_BUILTIN) + defined(IS_ARDUINO_BUILTIN) + defined(IS_MCP2515) + defined(IS_STM32_BUILTIN) + defined(IS_ESP32_TWAI) != 1
#warning "Select exactly one hardware option at the top of this file."

#if CAN_HOWMANY > 0 || CANFD_HOWMANY > 0
#define IS_ARDUINO_BUILTIN
#warning "guessing that this uses HardwareCAN"
#else
#error "cannot guess hardware version"
#endif

#endif

#ifdef IS_TEENSY_BUILTIN
// See https://github.com/tonton81/FlexCAN_T4
// clone https://github.com/tonton81/FlexCAN_T4.git into /src
#include <FlexCAN_T4.h>
#include "ODriveFlexCAN.hpp"
struct ODriveStatus; // hack to prevent teensy compile error
#endif // IS_TEENSY_BUILTIN

/* Board-specific settings ---------------------------------------------------*/

void onCanMessage(const CanMsg& msg);

/* Teensy */

#ifdef IS_TEENSY_BUILTIN

FlexCAN_T4<CAN3, RX_SIZE_256, TX_SIZE_16> can_intf;   // Using CAN3 on Teensy

bool setupCan() {
  can_intf.begin();
  can_intf.setBaudRate(CAN_BAUDRATE);
  can_intf.setMaxMB(16);
  can_intf.enableFIFO();
  can_intf.enableFIFOInterrupt();
  can_intf.onReceive(onCanMessage);
  return true;
}

#endif // IS_TEENSY_BUILTIN

// Instantiate ODrive objects
ODriveCAN odrv0(wrap_can_intf(can_intf), ODRV0_NODE_ID); // Standard CAN message ID
ODriveCAN* odrives[] = {&odrv0}; // Make sure all ODriveCAN instances are accounted for here

struct ODriveUserData {
  Heartbeat_msg_t last_heartbeat;
  bool received_heartbeat = false;
  Get_Encoder_Estimates_msg_t last_feedback;
  bool received_feedback = false;
};

// Keep some application-specific user data for every ODrive.
ODriveUserData odrv0_user_data;

// Encoder Defines
float center = 0.0f;
SPIEncoder* AS5047P = nullptr;
Angle encoder_angle;  // rotations=0, radians=0, direction=1 by default

// Called every time a Heartbeat message arrives from the ODrive
void onHeartbeat(Heartbeat_msg_t& msg, void* user_data) {
  ODriveUserData* odrv_user_data = static_cast<ODriveUserData*>(user_data);
  odrv_user_data->last_heartbeat = msg;
  odrv_user_data->received_heartbeat = true;
}

// Called every time a feedback message arrives from the ODrive
void onFeedback(Get_Encoder_Estimates_msg_t& msg, void* user_data) {
  ODriveUserData* odrv_user_data = static_cast<ODriveUserData*>(user_data);
  odrv_user_data->last_feedback = msg;
  odrv_user_data->received_feedback = true;
}

// Called for every message that arrives on the CAN bus
void onCanMessage(const CanMsg& msg) {
  //CAN debug messages
  //Serial.print("CAN msg received, id: 0x");
  //Serial.println(msg.id, HEX);
  
  for (auto odrive: odrives) {
    onReceive(msg, *odrive);
  }
}

void setup() {
  Serial.begin(115200);

  // Wait for up to 6 seconds for the serial port to be opened on the PC side.
  // If no PC connects, continue anyway.
  //for (int i = 0; i < 60 && !Serial; ++i) {
  //  delay(100);
  //}
  delay(200);


  Serial.println("Starting ODriveCAN...");

  // Register callbacks for the heartbeat and encoder feedback messages
  odrv0.onFeedback(onFeedback, &odrv0_user_data);
  odrv0.onStatus(onHeartbeat, &odrv0_user_data);

  // Configure and initialize the CAN bus interface. This function depends on
  // your hardware and the CAN stack that you're using.
  if (!setupCan()) {
    Serial.println("CAN failed to initialize: reset required");
    while (true); // spin indefinitely
  }

  Serial.println("Waiting for ODrive...");
  while (!odrv0_user_data.received_heartbeat) {
    pumpEvents(can_intf);
  }

  Serial.println("found ODrive");

  // request bus voltage and current (1sec timeout)
  Serial.println("attempting to read bus voltage and current");
  Get_Bus_Voltage_Current_msg_t vbus;
  if (!odrv0.request(vbus, 1000)) {
    Serial.println("vbus request failed!");
    while (true); // spin indefinitely
  }

  Serial.print("DC voltage [V]: ");
  Serial.println(vbus.Bus_Voltage);
  Serial.print("DC current [A]: ");
  Serial.println(vbus.Bus_Current);

  Serial.println("Enabling closed loop control...");

  Serial.println("Setting up AS5047P Encoder...");
  pinMode(ENCODER_CS_PIN, OUTPUT);
  digitalWrite(ENCODER_CS_PIN, HIGH);
  AS5047P = new SPIEncoder(EncoderReadCmd, SPI, ENCODER_CS_PIN);

  Serial.println("Initializing ADS1220...");

  adc.begin();
  pinMode(DRDY_PIN, INPUT_PULLDOWN);
  attachInterrupt(digitalPinToInterrupt(DRDY_PIN), onDRDY, FALLING);  // ADS1220 pulls DRDY low when data's ready
  adc.reset();
  delay(100);

  Serial.println("ADS1220 reset done");

  adc.writeRegister(0x00, 0x2E);
  adc.writeRegister(0x01, 0xD4);
  delay(10);

    /*
    Serial.println("Registers written");
    Serial.print("Reg0 readback (should be 2E): 0x");
    Serial.println(adc.readRegister(0x00), HEX);
    Serial.print("Reg1 readback (should be D4): 0x");
    Serial.println(adc.readRegister(0x01), HEX);
    */


    // Wait for up to 6 seconds for the serial port to be opened on the PC side.
    // If no PC connects, continue anyway.
    // for (int i = 0; i < 60; ++i) {
    //    delay(100);
    //}
  
  while (odrv0_user_data.last_heartbeat.Axis_State != ODriveAxisState::AXIS_STATE_CLOSED_LOOP_CONTROL) {
    odrv0.clearErrors();
    delay(1);
    odrv0.setState(ODriveAxisState::AXIS_STATE_CLOSED_LOOP_CONTROL);

    // Pump events for 150ms. This delay is needed for two reasons;
    // 1. If there is an error condition, such as missing DC power, the ODrive might
    //    briefly attempt to enter CLOSED_LOOP_CONTROL state, so we can't rely
    //    on the first heartbeat response, so we want to receive at least two
    //    heartbeats (100ms default interval).
    // 2. If the bus is congested, the setState command won't get through
    //    immediately but can be delayed.
    for (int i = 0; i < 15; ++i) {
      delay(10);
      pumpEvents(can_intf);
    }
  }

  Serial.println("ODrive running!");

  adc.startConversion();
  delay(100);
  // Serial.println("Conversion started, waiting for DRDY...");

  // Zero the offset with 100 samples --> sets the initial position as zero.
  adc.findADCOffset(2000);

  Serial.println("ADS1220 Setup Complete");

  // wait for the first ODRIVE encoder feedback
  while (!odrv0_user_data.received_feedback) {
    pumpEvents(can_intf);
  }

  //define starting point for sin wave around initialized position
  center = odrv0_user_data.last_feedback.Pos_Estimate;

  //print the centerpoint
  Serial.print("Center position: ");
  Serial.println(center);
}

void loop() {
  pumpEvents(can_intf); // This is required on some platforms to handle incoming feedback CAN messages
                        // Note that on MCP2515-based platforms, this will delay for a fixed 10ms.
                        //
                        // This has been found to reduce the number of dropped messages, however it can be removed
                        // for applications requiring loop times over 100Hz.
                        // Enabled because of Teensy + FlexCAN Implementation

  static uint32_t last_print = 0;

  float SINE_PERIOD = 10.0f; // Period of the position command sine wave in seconds
  float amplitude = 10.0f;

  float t = 0.001 * millis();
  
  float phase = t * (TWO_PI / SINE_PERIOD);

  odrv0.setPosition(
    center + amplitude * sin(phase), // position
    amplitude * cos(phase) * (TWO_PI / SINE_PERIOD) // velocity feedforward (optional)
  );

    // Read encoder angle
    uint16_t raw = AS5047P->read_raw();

    if (raw != 0 && raw != 16383) {
      float radians = (raw / 16383.0f) * TWO_PI;
      encoder_angle.update_angle(radians);
    }

    // Read loadcell if ready
    if (adcDataReady) {
      adcDataReady = false;
      float raw_lc = adc.readDataCalibrated(1.0f);
      Serial.print("time: ");
      Serial.print(millis() / 1000.0, 3);
      Serial.print(" raw: ");
      Serial.println(raw_lc);
    }

    // print position every 100(ish) ms
    if (millis() - last_print >= 100) {
      last_print = millis();
      Serial.print("odrv0-pos:");
      Serial.print(odrv0_user_data.last_feedback.Pos_Estimate);
      Serial.print(",encoder-angle:");
      Serial.println(encoder_angle.get_full_angle());
    }

  // print position and velocity for Serial Plotter
  /*
  if (odrv0_user_data.received_feedback) {
    Get_Encoder_Estimates_msg_t feedback = odrv0_user_data.last_feedback;
    odrv0_user_data.received_feedback = false;
    Serial.print("odrv0-pos:");
    Serial.print(feedback.Pos_Estimate);
    Serial.print(",");
    Serial.print("odrv0-vel:");
    Serial.println(feedback.Vel_Estimate);
  }
    */
}