# Loadcell Calibration Tool
A tool for calibrating loadcells using the ADS1220 Load Cell Amplifier.

## Library Dependencies:
- Matplotlib
- Numpy
- com_ports.py

## Settings

The following settings exist as global variables in calibration.py:
- NUM_SAMPLES    (Samples to average per calibration point)
- MIN_MESSAGE_BYTES = 16    (unused)
- BAUD_RATE = 115200    (Serial Baud)    
- CALIBRATION_CSV = "calibration.csv"   (CSV datafile name)
- GARBAGE_BUFFER = 1000    (How many initial points to discard -- for increased measurement reliability)

## Usage

### Parsing
calibration.py expects and parses serial prints as follows:
"time: <float> raw: <int>\n"

These should be the only serial prints that your code completes.

### CSV Data Storage
calibration.py outputs its recorded data to ./calibration.csv by default. This path may be changed. If the CSV data file already exists, calibration.py collects previous datapoints from the CSV file and includes them in its analysis. Future points are added to the existing CSV data. The program only clears the CSV file when commanded via the menu.

### Running
Run calibration.py with com_ports.py in the same directory, and follow the program menus to complete calibration. 

### Commands

- Enter a weight in grams to add datapoint to existing calibration data.
- 'undo' to remove the last point.
- 'report' to show all datapoint statistics collected so far.
- 'clear' to delete all points from data CSV.
- 'done' to complete data collection and recieve calibration report.

## Output
calibration.py stores its fit inside the corresponding CSV datafile, and outputs a plot of the callibration data to calibration_curve.png (in the same directory).

Written by Emerson Tiller
