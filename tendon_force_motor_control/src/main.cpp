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
 * Control Type
 * ------------------------------------- */

 //#define ADMITTANCE_CONTROL
 #define PID_CONTROL

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
const float LOAD_CELL_SCALE  = 0.07007488819976268;

#define MA_WINDOW_SIZE 6   // Moving average filter

float weight_buffer[MA_WINDOW_SIZE] = {0};
int weight_buffer_index = 0;
float weight_sum = 0.0f;
bool weight_buffer_filled = false;

float weight_filtered = 0.0f;

float updateMovingAverage(float new_sample);

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
void flip_control_ready();
void flip_encoder_ready();

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
 * Control Loop Timers & Flags
 * ------------------------------------- */
// Flags
volatile bool adcDataReady = false;
volatile bool encoder_ready = false;
volatile bool control_loop = false;
int kill_counter = 0;

// Timers
IntervalTimer control_timer, encoder_timer;


/* -------------------------------------
 * Global Vars
 * ------------------------------------- */
#ifdef MOTORS_ON
ODriveUserData odrv0_user_data;   // Keep some application-specific user data for every ODrive.
#endif

// Encoder Related
float center = 0.0f;

// Controller Parameters
const float m_v = 5000.0f;     // Formerly 1000000.0f
const float b = 20000.0f;       // Formerly 36750.0f and 100000.0f
const float PULLEY_RADIUS = 0.0089f;    // in meters
const float GEAR_RATIO = 146.0f;

const float kp = 56000.0f;   // N/m stiffness coefficient (thinner -- 33000)
const float kd = 0.0000f;
const float ki = 0.0003f;   // 3800.0f  -- 0.0000125f --> (thinner -- 0.0005f)

float last_motor_turns = 0.0f;

// Control Vars -- Pre-defined to minimize loop time
float xdot_ref = 0.0f;
float F_ref = 0.0f;
float xddot = 0.0f;
float xdot_cmd = 0.0f;
float x_cmd = 0.0f;
float delta_F = 0.0f;
float ddelta_F = 0.0f;
float prev_delta_F = 0.0f;
float integral_F = 0.0f;
float MAX_INTEGRAL = 200;

// Time Control vars
unsigned long time_zero = 0;
unsigned long period_begin = 0;
bool sine_started = false;
unsigned long last_loop_us = 0;
unsigned long now_us = 0;
unsigned long dt_us = 0;
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

  Serial.println("Press Enter to begin...");
  while (!Serial.available()) {}   // wait for any input
  Serial.read();                   // consume the byte
  Serial.println("Starting...");

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

  // Start Timers
  control_timer.begin(flip_control_ready, 1000);    // ODrive Controls at 1kHz (periods in nanoseconds)
  encoder_timer.begin(flip_encoder_ready, 1000);    // Encoder reads at 

  Serial.println("Entering main loop");
}


// Loop runs at the maximum load cell's rate. This may be changed later.
void loop() {

  // Establish first loop time
  if (first_loop){
    time_zero = micros();   // for tracking the sine wave
    first_loop = false;
  }

  // Always grab data if available
  if (adcDataReady){
    float raw_weight = adc.readDataCalibrated(LOAD_CELL_SCALE);
    weight_filtered = updateMovingAverage(raw_weight);
    loadcell_read_time_us = micros();
    adcDataReady = false;
  }

  if (encoder_ready){
    // Read encoder angle
    uint16_t raw = AS5047P->read_raw();

    if (raw != 0 && raw != 16383) {
      float radians = (raw / 16383.0f) * TWO_PI;
      encoder_angle.update_angle(radians);
    }
    encoder_ready = false;
  }

  #ifdef MOTORS_ON
  pumpEvents(can_intf);
  #endif

  now_us = micros();

  if ((now_us - time_zero)/1000000.0f >= 5.0f){
    
    if (!sine_started) {
      period_begin = now_us;    // record once when sine starts
      sine_started = true;
    }
    float t = (now_us - period_begin) / 1000000.0f;   // seconds since sine started
    F_ref = (50.0f * sinf((PI / 0.1f) * t)) + 60.0f;
    
    //F_ref = 110.0f;
  } else {
    F_ref = 60.0f;
  }

  if (control_loop){
    /* ---------
    * Admittance Control
    */
    #ifdef ADMITTANCE_CONTROL

    dt = 0.001f;

    delta_F = F_ref - b*xdot_cmd - (weight_filtered/1000 * 9.80665f); 
    xddot += delta_F/m_v;
    xdot_cmd += xddot * dt;
    x_cmd += xdot_cmd * dt;

    // translate into motor controlling parameters
    motor_turns = (x_cmd / PULLEY_RADIUS) * GEAR_RATIO / TWO_PI;
    // velocity_feedforward = (xdot_ref / PULLEY_RADIUS) * GEAR_RATIO / TWO_PI;
    #ifdef MOTORS_ON
    //Command ODrive to move motor
    odrv0.setPosition(-motor_turns);   // winding backwards
    #endif    
    #endif

    /* ---------
    * PID Control
    */
    #ifdef PID_CONTROL
    delta_F = F_ref - (weight_filtered/1000 * 9.80665f);
    ddelta_F = (delta_F - prev_delta_F)/0.001;
    integral_F += delta_F * 0.001;
    float unclamped_F = integral_F;
    integral_F = constrain(integral_F, -MAX_INTEGRAL, MAX_INTEGRAL);


    x_cmd = ((1.0f/kp * delta_F) - (kd * ddelta_F) + (ki * integral_F));

    motor_turns = (x_cmd / PULLEY_RADIUS) * GEAR_RATIO / TWO_PI;
    float delta_turns = motor_turns - last_motor_turns;
    float intended_response = motor_turns;
    // delta_turns = constrain(delta_turns, -0.3f, 0.3f);   //Clamping for maximum motor speed
    motor_turns = last_motor_turns + delta_turns;

    last_motor_turns = motor_turns;

    #ifdef MOTORS_ON
    odrv0.setPosition(-motor_turns);
    #endif

    prev_delta_F = delta_F;

    //Serial.print("odrv0-pos:");
    //Serial.print(odrv0_user_data.last_feedback.Pos_Estimate);
        
    /*
    //encoder prints
    Serial.print("encoder-time: ");
    Serial.print(millis() / 1000.0, 3);
    Serial.print("  encoder-angle:");
    Serial.println(encoder_angle.get_full_angle());

    //load cell prints
    Serial.print("loadcell-time: ");
    Serial.print(loadcell_read_time_us / 1000.0, 3);
    Serial.print("  weight filtered: ");
    Serial.println(weight_filtered, 4);

    Serial.print("f_ref: ");
    Serial.println(F_ref);

    Serial.print("loadcell force filtered (N): ");
    Serial.println(weight_filtered/1000 * 9.80665f);

    Serial.print("delta_f: ");
    Serial.println(delta_F);

    Serial.print("ddelta_f: ");
    Serial.println(ddelta_F);

    Serial.print("motor_turns (clamped): ");
    Serial.println(motor_turns);

    Serial.print("xcmd (unclamped): ");
    Serial.println(x_cmd, 5);

    Serial.print("Intended Delta: ");
    Serial.println(intended_response-last_motor_turns);

    Serial.print("Unclamped Integral Error: ");
    Serial.println(unclamped_F);

    Serial.print("Clamped Integral Error: ");
    Serial.println(integral_F);

    Serial.println("-----------------------");
    */

    #endif


    //Print on every control loop
    Serial.print("real time: ");
    Serial.print(millis() / 1000.0, 3);
    Serial.print("  Force (N) Averaged: ");
    Serial.print(weight_filtered/1000 * 9.80665f, 4);
    Serial.print("  Intended Force (N): ");
    Serial.print(F_ref);
    Serial.print("  Broken Tendon: ");

    // Kill Program and Print True if tendon is broken.
    if (kill_counter >= 1000){   // 2.5N for ~1 second
      Serial.println("True");

      #ifdef MOTORS_ON
      odrv0.setState(ODriveAxisState::AXIS_STATE_IDLE);   // release motor
      #endif

      control_timer.end();    // stop control loop timer
      encoder_timer.end();    // stop encoder timer

      while(true);            // halt
    }
    else{
      Serial.println("False");    // Otherwise, we just go with false and keep program alive
    }

    // Increment kill sequence -- current force is less than 2.5. 
    // It backs off by 0.2 seconds if it reads force over 2.5 N
    // If the force is less than 2.5 N, then we add to kill count.
    // If force is below 2.5N for 2.5 sec, then we kill the program.
    if ((weight_filtered/1000 * 9.80665f) < 2.5){
      kill_counter += 1;
    }
    else{
      if (kill_counter >= 200){
        kill_counter = kill_counter - 200;
      }
      else{
        kill_counter = 0;
      }
    }

    control_loop = false;
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

#ifdef MOTORS_ON
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
#endif

/* -------------------------------------
 * Data Helper Functions
 * ------------------------------------- */
void onDRDY() {
  adcDataReady = true;
}

void flip_encoder_ready(){
  encoder_ready = true;
}

void flip_control_ready(){
  control_loop = true;
}

float updateMovingAverage(float new_sample) {
  // Remove the oldest sample from the running sum
  weight_sum -= weight_buffer[weight_buffer_index];

  // Add the new sample to the buffer and the running sum
  weight_buffer[weight_buffer_index] = new_sample;
  weight_sum += new_sample;

  // Advance the circular index
  weight_buffer_index = (weight_buffer_index + 1) % MA_WINDOW_SIZE;

  if (weight_buffer_index == 0){
    weight_buffer_filled = true;
  }

  // Average over the full window once filled, otherwise average over samples seen so far
  int count = weight_buffer_filled ? MA_WINDOW_SIZE : weight_buffer_index;
  return weight_sum / count;
}