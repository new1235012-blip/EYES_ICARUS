import os
from src34.read_layers import extract_layers

base_dir = os.path.dirname(os.path.abspath(__file__))
file_path = os.path.join(base_dir, "data", "test.txt")

if __name__ == "__main__":
    extract_layers(file_path)