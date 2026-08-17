"""
Test Sequence Generator.
Generates an intended force sequence and exports it to a CSV.
Written by Emerson Tiller
"""

import csv
import math

# Settings
#----------------------------------------------------------------------
TEST_SEQ_CSV = "test_seq.csv"
#----------------------------------------------------------------------

test_sequence_array = []

def sine_wave():
    global test_sequence_array 

    # menu functions
    while(True):
        print("Base Level [N] (must be 0N or greater): ")
        base_level = float(input())

        if base_level < 0:
            print("Invalid Base Level")

        else:
            break
    
    while(True):
        print("Period [s] (minimum of 0.2s): ")
        period = float(input())

        if period < 0.2:
            print("Invalid Period")

        else:
            break

    while(True):
        print("Amplitude [N] (max of 1000 - base level): ")
        amplitude = float(input())

        if (amplitude >= 1000) or (amplitude > (1000 - base_level)) or (amplitude <= 0):
            print("Invalid Amplitude")

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

def step_func():
    global test_sequence_array

    # menu functions
    while(True):
        print("Base Level [N] (must be >5N): ")
        base_level = float(input())

        if base_level < 5:
            print("Invalid Base Level")

        else:
            break

    while(True):
        print("Final Level [N] (must be <2000 N): ")
        final_level = float(input())

        if (final_level >= 2000) or (final_level < base_level) or (final_level <= 5):
            print("Invalid Final Level")

        else:
            break

    while(True):
        print("Tail Times on either side [s]: ")
        tail_times = float(input())

        if (tail_times > 25) or (tail_times <= 0.001):
            print("Invalid Time")

        else:
            break

    # create the waveform
    # it's at 1khz
    for i in range(int(tail_times * 1000)):
        test_sequence_array.append(base_level)

    for j in range(int(tail_times * 1000)):
        test_sequence_array.append(final_level)



def save_test_array():
    "Write the test array to CSV."
    
    with open(TEST_SEQ_CSV, "w", newline="") as f:
        writer = csv.writer(f)

        for force in test_sequence_array:
            writer.writerow([force])

    print(f"Saved {len(test_sequence_array)} points to {TEST_SEQ_CSV}")



def main():
    print("*-*-* Test Sequence Generator *-*-*")
    print(f"Generates a waveform test sequence to be run on the testbed. Saves the waveform to {TEST_SEQ_CSV}")
    print("Warning: Clears previous rest sequences when run.")

    while(True):
        print("\nOptions:")
        print("1. Step")               
        print("2. Sine Wave")
        print("3. Exit")
        mode = input("Make selection (1/2/3): ").strip()

        if (mode not in ["1", "2", "3"]):
            print("Invalid Input")
            continue
        else:
            
            if mode == "3":
                print("Exiting.")
                return

            print("Enter Wave Parameters...")
            if mode == "1":
                step_func()
            elif mode == "2":
                sine_wave()

            break
    
    save_test_array()
        

if __name__ == "__main__":
    main()