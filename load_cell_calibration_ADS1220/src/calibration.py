""""
Load cEll calibration script.
Records ADC readings at known weights, fits a linear calibration curve.
"""

import serial
import time
import csv
import os
import numpy as np
import matplotlib.pyplot as plt
from com_ports import serial_ports

# Settings
#----------------------------------------------------------------------
NUM_SAMPLES = 50        # samples to average per calibration point
MIN_MESSAGE_BYTES = 16
BAUD_RATE = 115200
CALIBRATION_CSV = "calibration.csv"
#----------------------------------------------------------------------


# Reads one raw ADC value from serial
def read_raw_value(ser):
    # attempt a read
    #keep looping until successful or fails
    while True:
        try:
            line = ""
            #Read a line, waits for a \n
            #skip to latest reading in case of buffer build-up
            while ser.in_waiting > 0:
                line = ser.readline().decode("utf-8").strip()
            
            # if theres no line read, loop for the next one
            if not line:
                continue

            segments = line.split()

            try:
                # Extract Raw values and times from the serial line read
                x = float(segments[segments.index(f"time:") + 1])
                y = float(segments[segments.index(f"raw:") + 1])
                return x, y

            except (ValueError, IndexError):
                continue #bad parse, try again for the next line

        except Exception as error:
            print(error)
            continue

# load existing calibration points from CSV -- if it exists. If not, return empty lists
def load_existing_calibrations():
    #default to empty initialization
    true_weights, raw_means, raw_stds = [], [], []

    #check if csv exists, if not, return empty arrays
    if not os.path.exists(CALIBRATION_CSV):
        return true_weights, raw_means, raw_stds

   #if csv exists, notify and try to read all data
    print(f"Found existing {CALIBRATION_CSV}. Loading points...")
    with open(CALIBRATION_CSV, newline = "")
        reader = csv.DictReader(f)
        
        #read all rows
        for row in reader:
            try:
                true_weights.append(float(row["true_weight_grams"]))
                raw_means.append(float(row["raw_mean"]))
                raw_stds.append(float(row["raw_std"]))
            
            #bottom rows are fit rows, so ignore them
            except (KeyError, ValueError):
                pass

    print(f"Loaded {len(true_weights)} previous calibration points.")

    #print the data nicely
    for weight, mean, std in zip(true_weights, raw_means, raw_stds):
        print(f"{weight:.1f}g -> raw {mean:.2f} +/- {std:.2f}")

    return true_weights, raw_means, raw_stds



def main():

    ports = serial_ports()
    if not ports:
        print("No serial ports found.")
        return
    
    print("Available ports:", ports)

    COM_PORT = input("Enter COM Port:").strip()

    #if no com port specified, defaults to the first
    if not COM_PORT:
        COM_PORT = ports[0]
    
    ser = serial.Serial(
        port=COM_PORT,
        baudrate=BAUD_RATE,
        parity=serial.PARITY_NONE,
        stopbits=serial.STOPBITS_ONE,
        bytesize=serial.EIGHTBITS,
        timeout=0.5,
        )   

    time.sleep(2)
    ser.reset_input_buffer()
    print(f"Connected to: {ser.portstr}\n")

    # Calibration Loop ---------------------

    print("*-*-* Load Cell Calibration Tool *-*-*")
    print(f"Collecting {NUM_SAMPLES} samples per point.")
    print("Usage Options: \n 1. Enter a weight in grams to add datapoint to existing calibration data.")
    print("2. 'undo' to remove the last point.")
    print("3. 'report' to show all statistics collected so far.")
    print("4. 'clear' to start fresh.")
    print("5. 'done' to complete data collection and recieve calibration report.")

    #load existing calibration
    true_weights, raw_means, raw_stds = load_existing_calibrations()

    while True:
        print("-" * 40)
        cmd = input("Enter true weight (g) / undo / report / clear / done: ").strip().lower()

        # done
        if cmd == "done":
            if len(true_weights) < 2:
                print("Need at least 2 calibration points. You currently have ");;;
                #next loop
                continue

            #else, leave program loop and go to analysis
            break

        elif cmd == "undo":
            if true_weights != []:
                removed = true_weights.pop()
                raw_means.pop()
                raw_stds.pop()
                print(f"Removed {removed}g. You have {len(true_weights)}) points now.")

            else:
                print("Nothing to undo.")
            
            #next loop
            continue

        elif cmd == "clear":
            confirm = input("Confirm clear of all calibration points -- (y/n): ").strip().lower()
            if (confirm == "y") or (confirm == "yes"):
                true_weights.clear()
                raw_means.clear()
                raw_stds.clear()
                print("Cleared.")
            
            #next loop
            continue

        elif cmd == "report":
        #print the data nicely
            print("Data Report:")
            for weight, mean, std in zip(true_weights, raw_means, raw_stds):
                print(f"{weight:.1f}g -> raw {mean:.2f} +/- {std:.2f}")
        
        



        #check menu options
        #num, report, clear, undo, done


if __name__ == "__main__":
    main()