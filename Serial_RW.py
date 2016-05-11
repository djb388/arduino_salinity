# Script for running salinity experiments
# Uses serial port to interface with Arduino microcontroller

import time
from time import sleep
import serial

# Configure the serial connection
ser = serial.Serial(
    port='\\.\COM3',
    baudrate=9600,
    parity=serial.PARITY_NONE,
    stopbits=serial.STOPBITS_ONE,
    bytesize=serial.EIGHTBITS
)

ser.close() # Close a possible preexisting connection
ser.open()  # Open the serial connection
ser.isOpen()

# Get the experiment variables
print 'Please specify the following variables.\n'
mL_beads = int(raw_input("volume of beads: "))
mL_water = int(raw_input("volume of water: "))
target_salinity = float(raw_input("target salinity: "))
temperature = int(raw_input("    temperature: "))

# Calculate the volume of saline solution to pump
mL_saline = (mL_beads / 5.0 + mL_water) * (target_salinity/100.0) / (0.26 - (target_salinity/100.0))
print str(mL_saline) + 'mL saline will be pumped.'

# Send all of the experiment variables to the Arduino
ser.write((str(mL_beads).strip()+'\r\n'))
sleep(1)
response = ser.readline()
print response + 'mL beads ACK'
ser.write((str(mL_saline).strip()+'\r\n'))
sleep(1)
response = ser.readline()
print response + 'mL saline ACK'

out = ''    # Arduino output
t_start = int(round(time.time() * 1000))    # Experiment start time

# Open the output file, marking a new timestamp
with open('C:/Users/davej/Desktop/salinity_data.txt', "a") as myfile:
    myfile.write(str(time.time())+'\n')

# Loop until the Arduino indicates that it is done with its experiment
while out != 'END':
    out = str(ser.readline()).strip()   # Wait for the Arduino to send a message, then set out = the message
    if out != '' and out != 'END':      # If out is not empty and is not equal to END, print and save the data
        millis = int(round(time.time() * 1000)) - t_start   # Time in milliseconds since the start of the experiment
        salinity = 0.0000002074 * pow(2.71828,float(out)*0.056) # Calculate the salinity
        data = str(out)+','+str(salinity)+','+str(millis)   # String with the timestamp millis, raw output, and salinity
        print data  # Print the data
        data += '\n'
        # Save the data to the file
        with open('C:/Users/davej/Desktop/salinity_data.txt', "a") as myfile:
            myfile.write(data)

ser.close() # Close the serial connection
print 'End experiment'
exit()
