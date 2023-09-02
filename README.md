# Fips
Fips, mobile communication device for deafblind people

![Offen](https://github.com/HWuest/Fips/assets/101700554/03f06f5f-590f-4728-87d6-e03cab2cd4a7)

## Motivation
The biggest problem for deafblind people is social isolation.
Ofcourse they can read books printed in Braille or write a letter using a Braille keyboard, but this is not the same as talking with somebody.
## The aim of the project
is to provide a technical solution which allows deafblind people to talk with somebody as similar to a normal talk as possible. 
## System architecture - Top Level
![JPG image](assets/systemarchitecture.jpg)

## How to build

1. Get all hardware components from the partlist (see hardware folder)
2. Generate the 3D printing files for an available 3D printer from the .stl files in the 3D folder
3. Print the components (3 parts of the aktuators are needed 5 times)
4. Clue the magnets on the magnet holder and place them in the coil holder so that the magnet holder can move freely up and down in the coil holder
5. Clip the bootom of the coil holder on to the coil holder
6. Wind 500 windings of the cooper wire on each coil holder leaving two 5cm long wires outside for electrically connection
7. Remove the isolation half a centimeter from the ends of the wires (with sandpaper)
8. Test the functionality of the actuator with the battery and mark the polarity where the magnet moves upwards
9. Mount the charging electronics, the battery, the actuators, the switches and the microcontroller in the housing
10. Connect the electronic components and the driver IC acording to the schematic (see hardware folder)
11. Download the SW on the microcontroller -> [Link to description](bin/Readme.md)
12. Test the functionality: inputs done with the switches on the input side should imediatly be mirrored on the actuators on the output side
13. Fix all parts with screws and put the housing together, finaly fix it with screws


    
