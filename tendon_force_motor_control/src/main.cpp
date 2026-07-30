/* -------------------------------------
 * Includes
 * ------------------------------------- */

#include <Arduino.h>
#include <SPI.h>

// ODrive
#include "ODriveCAN.h"
#include <FlexCAN_T4.h>
#include "ODriveFlexCAN.hpp"

// Encoder
#include "encoder.hpp"
#include "spi_encoder.hpp"

// Load Cell
#include "ADS1220.h"

/* -------------------------------------
 * Motors On/Off & Nodes
 * ------------------------------------- */

#define MOTORS_ON
#define ODRV0_NODE_ID 0

/* -------------------------------------
 * Communication Baudrates
 * ------------------------------------- */

// CAN bus baudrate. Make sure this matches for every device on the bus
#define CAN_BAUDRATE 250000
// #define SPI_BAUDRATE 1000000   -- Manually set inside libraries
#define SERIAL_BAUDRATE 115200


/* -------------------------------------
 * Pin Defines
 * ------------------------------------- */

// Loadcell SPI Pin Defines
#define LOADCELL_CS_PIN 21
#define DRDY_PIN 9

// Encoder SPI Pin Defines
#define ENCODER_CS_PIN 10

/* -------------------------------------
 * Loadcell Defines
 * ------------------------------------- */

// Num sample for determining the zero offset of the load cell
#define OFFSET_SAMPLES 2000

// Load Cell calibration factor: (Vref / gain) / (2^23) — tune to your load cell (N/tick)
const float LOAD_CELL_SCALE  = 0.07007488819976268f;
const float LOAD_CELL_OFFSET = 49.394776217565585f;


/* -------------------------------------
 * ODRIVE Setup
 * ------------------------------------- */

#ifdef MOTORS_ON
void onCanMessage(const CanMsg& msg);
bool setupCan();
void onHeartbeat(Heartbeat_msg_t& msg, void* user_data);
void onFeedback(Get_Encoder_Estimates_msg_t& msg, void* user_data);
void onCanMessage(const CanMsg& msg);
#endif

void onDRDY();

/* -------------------------------------
 * ODRIVE Structures
 * ------------------------------------- */
#ifdef MOTORS_ON
struct ODriveUserData {
  Heartbeat_msg_t last_heartbeat;
  bool received_heartbeat = false;
  Get_Encoder_Estimates_msg_t last_feedback;
  bool received_feedback = false;
};

struct ODriveStatus; // hack to prevent teensy compile error
#endif

/* -------------------------------------
 * Object Instantiation
 * ------------------------------------- */
#ifdef MOTORS_ON
FlexCAN_T4<CAN3, RX_SIZE_256, TX_SIZE_16> can_intf;   // Using CAN3 on Teensy
ODriveCAN odrv0(wrap_can_intf(can_intf), ODRV0_NODE_ID); // Standard CAN message ID
ODriveCAN* odrives[] = {&odrv0}; // Make sure all ODriveCAN instances are accounted for here
#endif

// Load Cell Related
ADS1220 adc(LOADCELL_CS_PIN, DRDY_PIN);

/* -------------------------------------
 * Object Instantiation
 * ------------------------------------- */
SPIEncoder* AS5047P = nullptr;
Angle encoder_angle;  // rotations=0, radians=0, direction=1 by default

/* -------------------------------------
 * Global Vars
 * ------------------------------------- */
#ifdef MOTORS_ON
ODriveUserData odrv0_user_data;   // Keep some application-specific user data for every ODrive.
#endif

// Load Cell Related
volatile bool adcDataReady = false;

// Encoder Related
float center = 0.0f;

// Controller Parameters
const float m_v = 1378.125f;
const float b = 36750.0f;
const float PULLEY_RADIUS = 0.0089f;    // in meters
const float GEAR_RATIO = 146.0f;

// Control Vars -- Pre-defined to minimize loop time
float x_ref = 0.0f;
float xdot_ref = 0.0f;
float F_ref = 0.0f;
float xddot = 0.0f;
float xdot_cmd = 0.0f;
float x_cmd = 0.0f;

// Time Control vars
uint32_t time_zero = 0;
uint32_t last_loop_us = 0;
uint32_t now_us = 0;
uint32_t dt_us = 0;
float dt = 0;
bool first_loop = true;

// Motor Control Vars
float motor_turns = 0;
float velocity_feedforward = 0;
float torque_feedforward = 0;

//Print Vars
float loadcell_read_time_us = -1.0f;
float weight = 0.0f;



/* -------------------------------------
 * Setup
 * ------------------------------------- */
void setup() {  
  Serial.begin(115200);
  // Wait for serial port to open
  while(!Serial){
    delay(10);
  }

  /* ---------
   * Configure ODrive
   */

  #ifdef MOTORS_ON
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
    delay(1);
  }

  Serial.println("Found ODrive");

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

  while (odrv0_user_data.last_heartbeat.Axis_State != ODriveAxisState::AXIS_STATE_CLOSED_LOOP_CONTROL) {
    odrv0.clearErrors();
    delay(1);
    odrv0.setAbsolutePosition(0.0f);
    odrv0.setControllerMode(CONTROL_MODE_POSITION_CONTROL, INPUT_MODE_PASSTHROUGH);
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

  // wait for the first ODRIVE encoder feedback
  while (!odrv0_user_data.received_feedback) {
    pumpEvents(can_intf);
  }

  //define starting point for sin wave around initialized position
  center = odrv0_user_data.last_feedback.Pos_Estimate;

  //print the centerpoint
  Serial.print("Center position: ");
  Serial.println(center);

  Serial.println("Starting Control Loop...");

  #endif

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

  adc.writeRegister(0x00, 0x2E);
  adc.writeRegister(0x01, 0xD4);    // 2kHz
  adc.startConversion();
  
  adc.findADCOffset(OFFSET_SAMPLES);

  Serial.println("Entering main loop");
}

// Loop runs at the maximum load cell's rate. This may be changed later.
void loop() {
  pumpEvents(can_intf); // This is required on some platforms to handle incoming feedback CAN messages
                        // Note that on MCP2515-based platforms, this will delay for a fixed 10ms.
                        //
                        // This has been found to reduce the number of dropped messages, however it can be removed
                        // for applications requiring loop times over 100Hz.
                        // Enabled because of Teensy + FlexCAN Implementation

  static uint32_t last_print = 0;

  if (first_loop){
    time_zero = micros();   // for tracking the sine wave
    last_loop_us = now_us;
    first_loop = false;
  }

  while (digitalRead(DRDY_PIN)) {}    //wait for drdy to trip before proeceding

  weight = adc.readDataCalibrated(LOAD_CELL_SCALE) + LOAD_CELL_OFFSET;
  loadcell_read_time_us = micros();
  now_us = micros();
  
  F_ref = (100.0f * sin((PI/10) * ((now_us/1000000.0f) - (time_zero/1000000.0f)))) + 100.0f;


  dt_us = now_us - last_loop_us;
  dt = dt_us/1e6f;

  float delta_F = F_ref - b*xdot_cmd - (weight * 9.81f/1000.0f); 
  float delta_xddot = delta_F/m_v;
  float delta_xdot = delta_xddot * dt;
  x_cmd = delta_xdot * dt + x_cmd;
  xdot_cmd = delta_xddot * dt + xdot_cmd;

  // translate into motor controlling parameters
  motor_turns = (x_ref / PULLEY_RADIUS) * GEAR_RATIO / TWO_PI;
  velocity_feedforward = (xdot_ref / PULLEY_RADIUS) * GEAR_RATIO / TWO_PI;

  //Command ODrive to move motor
  odrv0.setPosition(-motor_turns, -velocity_feedforward);   // winding backwards


  //Serial.print("time: ");
  //Serial.print(millis() / 1000.0, 3);
  //Serial.print(" weight: ");
  //Serial.println(weight, 4);
  

  // Time control

  

  // do a zero-time dt to start accumulation properly

  


  // Control Calulations

  // F_ref/m_v = xddot_ref (must subtract other forces to get true xddot)
  // integral(xddot) = x_dot
  // Feed ODrive: x_ref and xdot (and also a caluculated torque feed forward)
  // x_ref isn't something we care about much -- it's not going to matter much
  

    // Read encoder angle
    uint16_t raw = AS5047P->read_raw();

    if (raw != 0 && raw != 16383) {
      float radians = (raw / 16383.0f) * TWO_PI;
      encoder_angle.update_angle(radians);
    }


    // print data every 100(ish) ms
    if (millis() - last_print >= 100) {
      last_print = millis();
      //Serial.print("odrv0-pos:");
      //Serial.print(odrv0_user_data.last_feedback.Pos_Estimate);
      
      //encoder prints
      Serial.print("encoder-time:");
      Serial.print(millis() / 1000.0, 3);
      Serial.print("  encoder-angle:");
      Serial.println(encoder_angle.get_full_angle());

      //load cell prints
      Serial.print("loadcell-time");
      Serial.print(loadcell_read_time_us / 1000.0, 3);
      Serial.print("  weight:");
      Serial.println(weight, 4);

      Serial.print("f_ref: ");
      Serial.println(F_ref);

      Serial.print("delta_f: ");
      Serial.println(delta_F);

      Serial.print("delta_xddot: ");
      Serial.println(delta_xddot);
    }

    // Updates for next loop
    last_loop_us = micros();

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


/* -------------------------------------
 * ODrive Helper Functions
 * ------------------------------------- */
bool setupCan() {
  can_intf.begin();
  can_intf.setBaudRate(CAN_BAUDRATE);
  can_intf.setMaxMB(16);
  can_intf.enableFIFO();
  can_intf.enableFIFOInterrupt();
  can_intf.onReceive(onCanMessage);
  return true;
}

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

/* -------------------------------------
 * Loadcell Helper Functions
 * ------------------------------------- */
void onDRDY() {
  adcDataReady = true;
}