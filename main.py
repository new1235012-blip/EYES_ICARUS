import os
import time
from src34.read_layers import extract_layers

base_dir = os.path.dirname(os.path.abspath(__file__))
file_path = os.path.join(base_dir, "data", "Albar.gcode")

if __name__ == "__main__":
    start_time = time.time()
    extract_layers(file_path)
    print("--- %s seconds ---" % (time.time() - start_time))