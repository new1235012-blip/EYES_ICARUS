import re
import numpy as np

#SEGMENT's size 90*90mm

def extract_coordinates(line):
    parts = line.split()
    new_X = new_Y = new_E = None

    for p in parts:
        if p[0] == 'X':
            new_X = float(p[1:])
        elif p[0] == 'Y':
            new_Y = float(p[1:])
        elif p[0] == 'E':
            new_E = float(p[1:])
    return new_X, new_Y, new_E
    

def extract_layers(gcode_file):
    print("Đang đọc file G-code: ", gcode_file)

    heatbed = [
        [0, 0, 0, 0],
        [0, 0, 0, 0],  
        [0, 0, 0, 0],
        [0, 0, 0, 0]
    ]
    layer_name = ""
    cur_X = cur_Y = cur_E = 0.0
    processed_layer = False

    with open(gcode_file, 'r') as file:
        for line in file:

            if line.startswith("; layer"):

                if processed_layer:
                    print(layer_name)
                    for row in heatbed[::-1]:
                        print(row)

                layer_name = line
                print("Đang xử lý: ", layer_name)
                heatbed = np.zeros((4, 4))
                processed_layer = True
            
            elif line.startswith("G92") and "E" in line:
                parts = line.split()
                for p in parts:
                    if p[0] == 'E':
                        cur_E = float(p[1:])

            elif line.startswith("G1") or line.startswith("G0"):
                new_X, new_Y, new_E = extract_coordinates(line)
                
                if new_X is not None:
                    cur_X = new_X
                if new_Y is not None:
                    cur_Y = new_Y
                
                if new_E is not None:
                    if new_E > cur_E: # make sure extruding not just moving
                        col = int(cur_X // 90)
                        row = int(cur_Y // 90)

                        if 0 <= row < 4 and 0 <= col < 4:
                            heatbed[row][col] = 1
                    cur_E = new_E
            
    if processed_layer:
        print(layer_name)
        for row in heatbed[::-1]:                        
            print(row)



extract_layers("test.txt")
            