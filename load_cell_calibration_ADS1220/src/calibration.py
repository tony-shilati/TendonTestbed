"""
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
from rendering import PointsInSpace

# Settings
#----------------------------------------------------------------------
NUM_SAMPLES = 50        # samples to average per calibration point
MIN_MESSAGE_BYTES = 16
BAUD_RATE = 115200
CALIBRATION_CSV = "calibration.csv"
#----------------------------------------------------------------------



def read_raw_value(ser):
    "Reads and returns one raw ADC value from serial. Blocks until then."

    #attempt a read
    #keep looping until successful or fails
    while True:
        try:
            #Read a line, waits for a \n
            #skip to latest reading in case of buffer build-up
            # block until a complete \n terminated line arrives
            line = ser.readline().decode("utf-8").strip()
            
            if not line:
                continue  # timeout with no data, try again

            segments = line.split()

            try:
                x = float(segments[segments.index("time:") + 1])
                y = float(segments[segments.index("raw:") + 1])
                return x, y

            except (ValueError, IndexError):
                continue  # bad parse, try next line

        except Exception as error:
            print(error)
            continue


def load_existing_calibrations():
    "Load existing calibration points from CSV -- if it exists. If not, return empty lists to start fresh."
    #default to empty initialization
    true_weights, raw_means, raw_stds = [], [], []

    #check if csv exists, if not, return empty arrays
    if not os.path.exists(CALIBRATION_CSV):
        return true_weights, raw_means, raw_stds

   #if csv exists, notify and try to read all data
    print(f"Found existing {CALIBRATION_CSV}. Loading points...")
    with open(CALIBRATION_CSV, newline = "") as f:
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

# collect a certain number of samples, in accordance with NUM_SAMPLES. Returns a mean and std.
def collect_samples(ser, sample_num):
    "Collect NUM_SAMPLES readings. Returns (mean, std)."

    raw_vals = []
    print(f"Collecting {sample_num} samples", end="", flush=True)

    for sample in range(sample_num):
        _, y = read_raw_value(ser)      #ignoring time, for now.
        raw_vals.append(y)

        #fun reporting every 10 samples
        if len(raw_vals) % 10 == 0:
            print(".", end="", flush=True)

    print()     # new line
    mean = np.mean(raw_vals)
    std = np.std(raw_vals)
    print(f"Mean: {mean:.2f}    STD: {std:.2f}")

    # show static plot of this collection after done
    plt.figure(figsize=(6, 3))
    plt.plot(raw_vals, 'o', alpha=0.5, markersize=3)
    plt.axhline(mean, color='r', label=f"Mean: {mean:.2f}")
    plt.axhline(mean + std, color='orange', linestyle='--', label=f"±STD: {std:.2f}")
    plt.axhline(mean - std, color='orange', linestyle='--')
    plt.xlabel("Sample #")
    plt.ylabel("Raw ADC")
    plt.title("Collected Samples")
    plt.legend()
    plt.grid(True)
    plt.tight_layout()
    plt.show(block=False)   # non-blocking — continues to input prompt
    plt.pause(0.5)
    return mean, std


def save_calibration(true_weights, raw_means, raw_stds, scale, offset, r_squared = None):
    "Write all calibration data & fit results to CSV."
    with open(CALIBRATION_CSV, "w", newline="") as f:
        writer = csv.writer(f)

        #header + data points
        writer.writerow(["true_weight_grams", "raw_mean", "raw_std"])
        for w, m, s in zip(true_weights, raw_means, raw_stds):
            writer.writerow([w, m, s])

        #give a seperation before storing results
        writer.writerow([])
        writer.writerow(["scale", scale])
        writer.writerow(["offset", offset])
        if r_squared is not None:
            writer.writerow(["r_squared", r_squared])
    print(f"Saved {len(true_weights)} points & fit to {CALIBRATION_CSV}")

def fit_and_plot(true_weights, raw_means, raw_stds):
    "Fit a line and plot the calibration curve."
    true_weights = np.array(true_weights)
    raw_means    = np.array(raw_means)
    raw_stds     = np.array(raw_stds)

    coeffs = np.polyfit(raw_means, true_weights, 1)
    scale, offset = coeffs

    print("\n*-*-* Calibration Results *-*-*")
    print(f"  scale  = {scale:.8f}")
    print(f"  offset = {offset:.4f}")
    print(f"  weight = {scale:.8f} * raw + ({offset:.4f})")

    # Residuals
    predicted = np.polyval(coeffs, raw_means)
    residuals = true_weights - predicted

    #r^2 reporting
    ss_res = np.sum(residuals ** 2)     # sum of squared residuals
    ss_tot = np.sum((true_weights - np.mean(true_weights)) ** 2)     #total variance
    r_squared = 1 - (ss_res / ss_tot)

    print(f"Max residual: {np.max(np.abs(residuals)):.4f}g")
    print(f"R^2: {r_squared:.6f}")

    # Plot
    fit_x = np.linspace(raw_means.min(), raw_means.max(), 200)
    fit_y = np.polyval(coeffs, fit_x)

    plt.figure(figsize=(8, 5))
    plt.errorbar(raw_means, true_weights, xerr=raw_stds,
                 fmt='o', capsize=5, label="Calibration points")
    plt.plot(fit_x, fit_y, '-',
             label=f"Fit: y = {scale:.4f}x + {offset:.2f}  |  R²={r_squared:.4f}")
    plt.xlabel("Raw ADC Reading")
    plt.ylabel("True Weight (g)")
    plt.title("Load Cell Calibration Curve")
    plt.legend()
    plt.grid(True)
    plt.tight_layout()
    plt.savefig("calibration_curve.png")   # ← add this before show()
    print("Saved plot to calibration_curve.png")
    plt.show()

    return scale, offset, r_squared


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
    print("Usage Options: \n1. Enter a weight in grams to add datapoint to existing calibration data.")
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
                print(f"Need at least 2 calibration points. You currently have {len(true_weights)}.")
                #next loop
                continue

            #else, leave program loop and go to analysis
            break

        elif cmd == "undo":
            if true_weights != []:
                removed = true_weights.pop()
                raw_means.pop()
                raw_stds.pop()
                print(f"Removed {removed}g. You have {len(true_weights)} points now.")

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

            if not true_weights:
                print("No data collected yet.")
                continue

            #print the data nicely
            print("Data Report:")
            for weight, mean, std in zip(true_weights, raw_means, raw_stds):
                print(f"{weight:.1f}g -> raw {mean:.2f} +/- {std:.2f}")

            #next loop
            continue
        

        #at this point, it's invalid or it's a number
        else:
            #try to make it a float
            try:
                new_true_weight = float(cmd)

            #if it's not a number, it's invalid
            except ValueError:
                print("Invalid input.")
                continue

            #tell user to do calibration and block until ready
            input(f"Place {new_true_weight}g on calibrator, then press Enter.")
            ser.reset_input_buffer()

            #take and store samples
            mean, std = collect_samples(ser, NUM_SAMPLES)
            true_weights.append(new_true_weight)
            raw_means.append(mean)
            raw_stds.append(std)

            #Update CSV after each point

            #temp fit if possible
            if len(true_weights) >= 2:
                temp_coefficients = np.polyfit(raw_means, true_weights, 1)
                # compute r^2 for temp save
                predicted = np.polyval(temp_coefficients, raw_means)
                residuals = np.array(true_weights) - predicted
                ss_res = np.sum(residuals ** 2)
                ss_tot = np.sum((np.array(true_weights) - np.mean(true_weights)) ** 2)
                temp_r2 = 1 - (ss_res / ss_tot)
                save_calibration(true_weights, raw_means, raw_stds,
                                 temp_coefficients[0], temp_coefficients[1], temp_r2)

            #save without a fit
            else:
                with open(CALIBRATION_CSV, "w", newline="") as f:
                    writer = csv.writer(f)
                    writer.writerow(["true_weight_grams", "raw_mean", "raw_std"])
                    for w, m, s in zip(true_weights, raw_means, raw_stds):
                        writer.writerow([w, m, s])
                print(f"  Saved to {CALIBRATION_CSV} (need 1 more point to fit)")

    ser.close()

    #Final fit & plot
    scale, offset, r_squared = fit_and_plot(true_weights, raw_means, raw_stds)
    save_calibration(true_weights, raw_means, raw_stds, scale, offset, r_squared)

if __name__ == "__main__":
    main()