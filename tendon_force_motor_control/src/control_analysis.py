"""
Controller Response Analysis Script.
Plots the force response of the tendon testbed system against the intended response.
Written by Emerson Tiller
"""

import serial
import time
import csv
import os
import numpy as np
import matplotlib.pyplot as plt
from com_ports import serial_ports
import threading
import requests
from dotenv import load_dotenv

# load environment variables
load_dotenv()

# Settings
#----------------------------------------------------------------------
MIN_MESSAGE_BYTES = 16
BAUD_RATE = 115200
DATA_CSV = "data.csv"
DRAIN_WINDOW = 0.5      #extra seconds to drian after time stop
BW_LOW  = 20.0          # [N] — 10% of 10 --> 110 step
BW_HIGH = 100.0         # [N] — 90% of 10 --> 110 step
#----------------------------------------------------------------------

# Threading stop condition
stop_event = threading.Event()
stop_time = float('inf')
time_zero = None

# First, read the ports
# Then, keep reading the port until user says we're done
# When user says we're done, plot everything we've recieved vs intended
# Save data to CSV
# we can add analysis later
#

def collect_until_stop(ser, data_times, data_forces, data_intended):
    "Reads serial in background until stop_event is set or the tendon breaks. Sends notification upon stop."
    while True:
        try:
            t, y, i, kill = read_raw_value(ser)   # unpack 3 values now
            data_times.append(t)
            data_forces.append(y)
            data_intended.append(i)
            
            # kill the collection if the tendon broke
            if kill == "True":
                stop_event.set()
                try:
                    notify("Tendon Testbed", "Tendon Broken -- Test Completed", 0)
                except Exception as e:
                    print(f"Notify failed: {e}")   # ← don't let notify kill the thread
                break

            # if Enter was hit AND the serial timestamp is past stop_time, finish
            # catches everything in the case of serial flooding
            if stop_event.is_set() and (data_times[-1] - data_times[0]) >= stop_time:
                try:
                    notify("Tendon Testbed", "Test Manually Completed", 0)
                except Exception as e:
                    print(f"Notify failed: {e}")
                break
            
        except Exception as e:
            if stop_event.is_set():
                break
            continue

def calc_bandwidth(data_times, data_forces):
    "Calculates bandwidth from a 10 --> 110N step. Bw = 0.35/tr"

    # find first time force crosses BW_LOW (20N) on the way up
    t_low = None
    t_high = None

    for t, f in zip(data_times, data_forces):
        if t_low is None and f >= BW_LOW:
            t_low = t
        if t_high is None and f >= BW_HIGH:
            t_high = t
            break

    if t_low is None or t_high is None:
        print("Could not find 20N or 100N crossing — check data.")
        return

    tr = t_high - t_low
    bw = 0.35 / tr

    print(f"t_20N  = {t_low:.4f}s")
    print(f"t_100N = {t_high:.4f}s")
    print(f"Rise time tr = {tr*1000:.2f}ms")
    print(f"Bandwidth Bw = {bw:.2f} Hz")

def read_raw_value(ser):
    "Reads and returns one raw ADC value from serial. Blocks until then."
    # attempt a read
    # keep looping until successful or fails
    # serial may be flooded at 1khz, but we accept this in exchange for data accuracy.
    # collects all data in serial buffer, but may take longer to run.
    while True:
        try:
            # Read a line, waits for a \n
            # skip to latest reading in case of buffer build-up
            # block until a complete \n terminated line arrives
            line = ser.readline().decode("utf-8").strip()
            
            if not line:
                continue  # timeout with no data, try again

            segments = line.split()

            try:
                # "real time: 1.234  Force (N) Averaged: 9.8765  Intended Force (N): 10.0  Broken Tendon: True/False"
                t = float(segments[segments.index("time:") + 1])
                y = float(segments[segments.index("Averaged:") + 1])
                i = float(segments[segments.index("Intended") + 3])
                kill = str(segments[segments.index("Tendon:") + 1])
                return t, y, i, kill

            except (ValueError, IndexError):
                continue  # bad parse, try next line

        except Exception as error:
            print(error)
            continue

def save_data(data_times, data_forces, data_intended):
    "Saves collected run data to CSV."
    with open(DATA_CSV, 'w', newline='') as f:
        writer = csv.writer(f)
        writer.writerow(["time_s", "force", "intended_n"])
        for t, y, i in zip(data_times, data_forces, data_intended):
            writer.writerow([t, y, i])
    print(f"Data saved to {DATA_CSV}")

def notify(title, message, priority=0):
    """
    Send a push notification via Pushover.
    priority: -2 (silent) to 2 (emergency, requires ack)
    """
    response = requests.post(
        "https://api.pushover.net/1/messages.json",
        data={
            "token": os.environ["PUSHOVER_TOKEN"],
            "user": os.environ["PUSHOVER_USER"],
            "title": title,
            "message": message,
            "priority": priority,
        }
    )
    response.raise_for_status()  # raises an error if something went wrong
    return response.json()

def main():
    global stop_time

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


    print("*-*-* Simple Controller Analysis Tool *-*-*")
    print(f"Plots collected data vs. intended response and saves the run to {DATA_CSV}")
    print("Warning: Clears previous data after each run.")
    print("\nOptions:")
    print("1. Record and plot")
    print("2. Record, plot, and calculate bandwidth (assumes 10 -> 110N step)")
    mode = input("Select mode (1/2): ").strip()
    calc_bw = (mode == "2")

    # teensy waits for an enter to start up.
    ser.write(b"\n")

    # empty data arrays
    data_times = []
    data_forces = []
    data_intended = []

    time_zero = time.time()
    # start threading to collect data until user defines stop point
    thread = threading.Thread(target=collect_until_stop, args=(ser, data_times, data_forces, data_intended), daemon=True)
    thread.start()

    def wait_for_enter():
        input("Recording... press Enter to stop.")
        stop_event.set()

    input_thread = threading.Thread(target=wait_for_enter, daemon=True)
    input_thread.start()

    # block main until either Enter is hit or tendon breaks
    stop_event.wait()

    end_time = time.time()
    stop_time = (end_time - time_zero) + DRAIN_WINDOW

    stop_event.set()   # signals background thread to stop
    print("Draining remaining serial buffer...")

    thread.join()      # wait for it to finish

    print(f"Collected {len(data_times)} samples.")
    ser.close()

    # normalize time to start at 0
    if data_times:
        t_start = data_times[0]
        data_times = [t - t_start for t in data_times]

    save_data(data_times, data_forces, data_intended)

    if calc_bw:
        calc_bandwidth(data_times, data_forces)

    plt.figure(figsize=(10, 4))
    plt.plot(data_times, data_forces, color='red', label="Measured Force")    
    plt.plot(data_times, data_intended, color='blue', linestyle='--', label="Intended Force")

    if calc_bw:
        plt.axhline(y=BW_LOW,  color='gray', linestyle=':', linewidth=1, label=f"{BW_LOW}N (10%)")
        plt.axhline(y=BW_HIGH, color='gray', linestyle=':', linewidth=1, label=f"{BW_HIGH}N (90%)")

    plt.xlabel("Time (s)")
    plt.ylabel("Force (N)")
    plt.title("Controller Response vs Intended")
    plt.legend()
    plt.grid(True)
    plt.tight_layout()
    plt.show()

if __name__ == "__main__":
    main()