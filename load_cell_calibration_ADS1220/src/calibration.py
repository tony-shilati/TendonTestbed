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
    tight_loop_contents;

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

    # change to your COM port
    COM_PORT = 'COM3'
    TRAILING_POINTS = 100
    MIN_MESSAGE_BYTES = 16
    
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

    
