"""
Reads data from a serial port and displays it in real-time using matplotlib.
Make sure to set COM_PORT to the correct value before running.
"""

# Changed to reflect main.cpp for live reading of ADC raw values of load cell
import serial
from rendering import PointsInSpace
import time
import csv
import sys


def main():
    
    # change to your COM port
    COM_PORT = 'COM3'# COM port for Yuchan's computer
    TRAILING_POINTS = 100
    MIN_MESSAGE_BYTES = 16

    
    ser = serial.Serial(
        port=COM_PORT,
        baudrate=115200,
        parity=serial.PARITY_NONE,
        stopbits=serial.STOPBITS_ONE,
        bytesize=serial.EIGHTBITS,
        timeout=0.5,
        )   
    
    time.sleep(2)
    ser.reset_input_buffer()
   
    csv_file = open("data.csv", "w", newline="")
    writer = csv.writer(csv_file)
    # writer.writerow(["time", "raw"]) # <<< OLD: write raw value to csv
    writer.writerow(["time", "modified"]) # <<< NEW: write modified weight to csv instead of raw value
    print("Connected to: " + ser.portstr)

    xs = []
    ys = []

    
    live_plotter = PointsInSpace(
                    "Raw Data vs. Time",
                    "Time (s)",
                    "Value (g)",
                    xlim=[0, 60],
                    ylim=[-100, 100],
                    enable_grid=True,
                    enable_legend=True,
                )

    live_plotter.register_plot("Position Tracking", alpha=0.5)

    while True:
        try:
            # Read bytes goofery
            bytes_to_read = ser.in_waiting
            if bytes_to_read < MIN_MESSAGE_BYTES:
                continue
            line = ser.read(bytes_to_read).decode("utf-8")
            segments = line.split()

            # Parse the message by reading the value after each label
            try:
                
                # Do this in two steps so that values are not changed if not all values exist
                def value_by_label(label):
                    res = float(segments[segments.index(f"{label}:") + 1])
                    return res

                x = value_by_label("time")
                #y = value_by_label("raw") # <<< OLD: read raw value
                y = value_by_label("modified_weight")  # <<< NEW: read modified weight instead of raw value

                print("time: ", x)
                #print("raw: ", y) <<< OLD: print raw value
                print("modified_weight: ", y)  #<< NEW: print modified weight instead of raw value

                xs.append(x)
                ys.append(y)

                writer.writerow([x, y])
                csv_file.flush()

            except Exception as e:
                print(e)
                continue

            if len(xs) > TRAILING_POINTS:
                xs.pop(0)
                ys.pop(0)
            # Display results
            live_plotter.start_drawing()
            live_plotter.draw_points("Position Tracking", xs, ys)
            live_plotter.end_drawing()

        except Exception as e:
            print(e)
            ser.close()
            csv_file.close()
            print("Closed connection")
            quit()

    ser.close()
    print("Closed connection")

if __name__ == "__main__":
    main()