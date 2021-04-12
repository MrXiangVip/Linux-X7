#! /usr/bin/python
#============================================
#  Date: 7/1/2019
#  Author: Terry Lv
#  Version: 0.01
#============================================

import subprocess
import os
import time
import struct
import sys

def print_menu():
    print "Waiting for key:"
    print "VOL+ -- Face recognize"
    print "VOL- -- Face register"
    print "POWER -- Exit"

def main():
    rtn_val = 1

    script_path = sys.path[0]
    recognize_app_cmd = script_path + "/face_recognize -l -p"
    register_app_cmd = script_path + "/face_register -l -p"
    infile_path = "/dev/input/event" + (sys.argv[1] if len(sys.argv) > 1 else "0")

    script_run = 1

    #long int, long int, unsigned short, unsigned short, unsigned int
    FORMAT = 'llHHI'
    EVENT_SIZE = struct.calcsize(FORMAT)

    #open file in binary mode
    in_file = open(infile_path, "rb")
    key_type = 1
    key_value = 0

    while (script_run == 1):
        if (key_value == 0 and key_type == 1):
            print_menu()
        key_event = in_file.read(EVENT_SIZE)
        (tv_sec, tv_usec, key_type, key_code, key_value) = struct.unpack(FORMAT, key_event)
        if (key_type == 1 and key_value == 0): # EV_KEY
            if (key_code == 115): # KEY_VOLUMEUP
                # Start face recognize
                print "Start face recognize... "
                recognize_child = subprocess.Popen(recognize_app_cmd, shell=True)
                recognize_child.wait()
            elif (key_code == 114): # KEY_VOLUMDOWN
                # Start face register
                print "Start face register... "
                register_child = subprocess.Popen(register_app_cmd, shell=True)
                register_child.wait()
            elif (key_code == 116): # KEY_POWONOFF
                # Exit
                script_run = 0
                print "Exit script"

    in_file.close()

if __name__ == '__main__':
    sys.exit(main())
