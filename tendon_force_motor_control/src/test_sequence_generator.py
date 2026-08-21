"""
Test Sequence Generator.
Generates an intended force sequence and exports it to a CSV.
Written by Emerson Tiller
"""

import csv
import math
import matplotlib.pyplot as plt

# Settings
#----------------------------------------------------------------------
TEST_SEQ_CSV = "test_seq.csv"
MIN_FORCE = 2.5         # [N], usually determined by setting of kill timer in main.cpp
MAX_FORCE = 2000        # [N]
MIN_AMPLITUDE = 1       # [N]
MIN_PERIOD = 0.2        # [s]
MAX_TIME = 25           # [s], maximum total waveform duration. Currently takes ~19% of Teensy RAM.
#----------------------------------------------------------------------

test_sequence_array = []

def sine_wave():
    global test_sequence_array
    test_sequence_array = []

    # menu functions
    while(True):
        print(f"Base Level [N] (midpoint of sine, must be > {MIN_FORCE + MIN_AMPLITUDE}N): ")
        base_level = float(input())

        if base_level <= MIN_FORCE:
            print(f"Invalid — base level must be above MIN_FORCE ({MIN_FORCE}N)")
        elif (base_level - MIN_FORCE) < MIN_AMPLITUDE:
            print(f"Invalid — base level only leaves {base_level - MIN_FORCE:.2f}N of headroom above MIN_FORCE, minimum amplitude is {MIN_AMPLITUDE}N")
        else:
            break

    while(True):
        print(f"Period [s] (minimum of {MIN_PERIOD}s): ")
        period = float(input())

        if period < MIN_PERIOD:
            print("Invalid Period")

        else:
            break

    while(True):
        max_amplitude = min(base_level - MIN_FORCE, MAX_FORCE - base_level)
        print(f"Amplitude [N] (min = {MIN_AMPLITUDE}N, max = {max_amplitude:.2f}N): ")
        amplitude = float(input())

        sine_min = base_level - amplitude
        sine_max = base_level + amplitude

        if amplitude < MIN_AMPLITUDE:
            print(f"Invalid — minimum amplitude is {MIN_AMPLITUDE}N")
        elif sine_min < MIN_FORCE:
            print(f"Invalid — sine min would be {sine_min:.2f}N, below MIN_FORCE ({MIN_FORCE}N)")
        elif sine_max > MAX_FORCE:
            print(f"Invalid — sine max would be {sine_max:.2f}N, above MAX_FORCE ({MAX_FORCE}N)")
        else:
            break

    while(True):
        print(f"Ending Tail Length [s] (btwn 0 and {MAX_TIME - period}s): ")
        tail_len = float(input())

        if (tail_len > (MAX_TIME - period)) or (tail_len < 0):
            print(f"Invalid Tail Length — must be between 0 and {MAX_TIME - period:.2f}s")

        else:
            break

    # create the waveform
    # it's at 1khz
    # one full period of sine
    num_samples = int(period * 1000)
    for i in range(num_samples):
        t = i / 1000.0
        val = base_level + amplitude * math.sin((2 * math.pi / period) * t)
        test_sequence_array.append(round(val, 4))

    #add the trailing tail -- it rests at the last value of the sine wave (middle)
    for i in range(int(tail_len * 1000)):
        test_sequence_array.append(base_level)

    print(f"Generated {len(test_sequence_array)} samples — range: {base_level-amplitude:.2f}N to {base_level+amplitude:.2f}N")

def step_func():
    global test_sequence_array
    test_sequence_array = []

    # menu functions
    while(True):
        print(f"Base Level [N] (must be >= {MIN_FORCE}N): ")
        base_level = float(input())

        if base_level < MIN_FORCE:
            print(f"Invalid — base level must be >= MIN_FORCE ({MIN_FORCE}N), got {base_level:.2f}N")

        else:
            break

    while(True):
        print(f"Final Level [N] (must be <{MAX_FORCE}): ")
        final_level = float(input())

        if final_level <= base_level:
            print(f"Invalid — final level ({final_level:.2f}N) must be greater than base level ({base_level:.2f}N)")

        elif final_level > MAX_FORCE:
            print(f"Invalid — final level ({final_level:.2f}N) exceeds MAX_FORCE ({MAX_FORCE}N)")

        else:
            break

    while(True):
        print(f"Tail Times on either side [s] (0 to {MAX_TIME}s): ")
        tail_times = float(input())

        if tail_times < 0:
            print(f"Invalid — tail time must be >= 0s, got {tail_times:.3f}s")
        elif tail_times > MAX_TIME:
            print(f"Invalid — tail time ({tail_times:.2f}s) exceeds maximum of {MAX_TIME}s")
        else:
            break

    # create the waveform
    # it's at 1khz
    for i in range(int(tail_times * 1000)):
        test_sequence_array.append(base_level)

    for j in range(int(tail_times * 1000)):
        test_sequence_array.append(final_level)

    print(f"Generated {len(test_sequence_array)} samples — step from {base_level:.2f}N to {final_level:.2f}N, {tail_times}s tails each side")

def triangle_wave():
    global test_sequence_array
    test_sequence_array = []

    while(True):
        print(f"Base Level [N] (midpoint of triangle, must be > {MIN_FORCE + MIN_AMPLITUDE}N): ")
        base_level = float(input())

        if base_level <= MIN_FORCE:
            print(f"Invalid — base level must be above MIN_FORCE ({MIN_FORCE}N)")
        elif (base_level - MIN_FORCE) < MIN_AMPLITUDE:
            print(f"Invalid — base level only leaves {base_level - MIN_FORCE:.2f}N of headroom above MIN_FORCE, minimum amplitude is {MIN_AMPLITUDE}N")
        else:
            break

    while(True):
        print(f"Period [s] (minimum of {MIN_PERIOD}s): ")
        period = float(input())

        if period < MIN_PERIOD:
            print("Invalid Period")
        else:
            break

    while(True):
        max_amplitude = min(base_level - MIN_FORCE, MAX_FORCE - base_level)
        print(f"Amplitude [N] (min = {MIN_AMPLITUDE}N, max = {max_amplitude:.2f}N): ")
        amplitude = float(input())

        tri_min = base_level - amplitude
        tri_max = base_level + amplitude

        if amplitude < MIN_AMPLITUDE:
            print(f"Invalid — minimum amplitude is {MIN_AMPLITUDE}N")
        elif tri_min < MIN_FORCE:
            print(f"Invalid — triangle min would be {tri_min:.2f}N, below MIN_FORCE ({MIN_FORCE}N)")
        elif tri_max > MAX_FORCE:
            print(f"Invalid — triangle max would be {tri_max:.2f}N, above MAX_FORCE ({MAX_FORCE}N)")
        else:
            break

    while(True):
        print(f"Ending Tail Length [s] (btwn 0 and {MAX_TIME - period:.2f}s): ")
        tail_len = float(input())

        if (tail_len > (MAX_TIME - period)) or (tail_len < 0):
            print(f"Invalid Tail Length — must be between 0 and {MAX_TIME - period:.2f}s")
        else:
            break

    # create the waveform at 1kHz
    # triangle wave: rises from base to peak in first half, falls back to base in second half
    num_samples = int(period * 1000)
    half = num_samples // 2
    for i in range(num_samples):
        if i < half:
            val = base_level + amplitude * (i / half)           # rising
        else:
            val = base_level + amplitude * (2 - i / half)       # falling
        test_sequence_array.append(round(val, 4))

    # trailing tail at base level
    for i in range(int(tail_len * 1000)):
        test_sequence_array.append(base_level)

    print(f"Generated {len(test_sequence_array)} samples — range: {base_level-amplitude:.2f}N to {base_level+amplitude:.2f}N")

def save_test_array():
    "Write the test array to CSV."
    
    with open(TEST_SEQ_CSV, "w", newline="") as f:
        writer = csv.writer(f)

        for force in test_sequence_array:
            writer.writerow([force])

    print(f"Saved {len(test_sequence_array)} points to {TEST_SEQ_CSV}")

def plot_saved():
    "Plots the currently saved test sequence CSV."

    try:
        forces = []
        with open(TEST_SEQ_CSV, "r") as f:
            reader = csv.reader(f)
            for row in reader:
                if row:
                    forces.append(float(row[0]))

        if not forces:
            print("CSV is empty.")
            return

        # 1kHz - each sample is 1ms
        times = [i / 1000.0 for i in range(len(forces))]

        plt.figure(figsize=(10, 4))
        plt.plot(times, forces, color='blue', marker='.', linestyle='none', markersize=2)
        plt.xlabel("Time (s)")
        plt.ylabel("Force (N)")
        plt.title(f"Saved Test Sequence — {len(forces)} samples ({times[-1]:.2f}s)")
        plt.axhline(y=MIN_FORCE, color='red', linestyle='--', linewidth=1, label=f"MIN_FORCE ({MIN_FORCE}N)")
        plt.axhline(y=MAX_FORCE, color='orange', linestyle='--', linewidth=1, label=f"MAX_FORCE ({MAX_FORCE}N)")
        plt.legend()
        plt.grid(True)
        plt.tight_layout()
        plt.show()

    except FileNotFoundError:
        print(f"No saved CSV found at {TEST_SEQ_CSV}")

def main():
    print("*-*-* Test Sequence Generator *-*-*")
    print(f"Generates a waveform test sequence to be run on the testbed. Saves the waveform to {TEST_SEQ_CSV}")
    print("Warning: Clears previous test sequences when run.")

    while(True):
        print("\nOptions:")
        print("1. Step")               
        print("2. Sine Wave")
        print("3. Triangle Wave")
        print("4. Plot Saved CSV")
        print("5. Exit")
        mode = input("Make selection (1/2/3/4/5): ").strip()

        if (mode not in ["1", "2", "3", "4", "5"]):
            print("Invalid Input")
            continue

        if mode == "5":
            print("Exiting.")
            return

        if mode == "4":
            plot_saved()
            continue

        print("Enter Wave Parameters...")
        if mode == "1":
            step_func()
        elif mode == "2":
            sine_wave()
        elif mode == "3":
            triangle_wave()

        break

    save_test_array()


if __name__ == "__main__":
    main()