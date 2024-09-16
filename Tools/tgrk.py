import serial
from time import sleep
from os import system

keywords = {
    'EOP': -128,
    'SEP': -1,
    'FUNC': -2,
    'ADD': -3,
    'SUB': -4,
    'MUL': -5,
    'DIV': -6,
    'AND': -7,
    'OR': -8,
    'RND': -9,
    'CG': -11,
    'CL': -12,
    'CE': -13,
    'CNE': -14,
    'CGE': -15,
    'CLE': -16,
    'LP': -96,
    'LR1': -95,
    'LR2': -94,
    'LR3': -93,
    'LR4': -92,
    'LR5': -91,
    'LR6': -90,
    'LR7': -89,
    'LR8': -88,
    'LPF': -87,
    'R1': -127,
    'R2': -126,
    'R3': -125,
    'R4': -124,
    'R5': -123,
    'R6': -122,
    'R7': -121,
    'R8': -120,
    'ER': -119,
    'EW': -118,
    'F1': -63,
    'F2': -62,
    'F3': -61,
    'F4': -60,
    'F5': -59,
    'F6': -58,
    'F7': -57,
    'F8': -56,
    'PDOH': -25,
    'PDOL': -26,
    'PDIU': -27,
    'PDID': -28,
    'PAIX': -29,
    'PAXX': -30,
    'L0L': -48,
    'L0H': -47,
    'L1L': -46,
    'L1H': -45,
    'S1': -111,
    'S2': -110,
    'S3': -109,
    'S4': -108,
    'S5': -107,
    'S6': -106,
    'S7': -105,
    'S8': -104,
    'D0': -80,
    'D1': -79,
    'D2': -78,
    'D3': -77,
    'D4': -76,
    'D5': -75,
    'D6': -74,
    'D7': -73,
    'D8': -72,
    'BG': -44,
    'BV': -43,
    'BB': -33
    }

prog = []

byte_count = 0
last_mode = 0
mode = 0
connected = False
menu = 0
read = 1
write = 2
get_bytes = 3
convert = 4

def print_dots():
    for t in range(26):
        print("=", end="")
    print("=")
    sleep(0.4)
    
def read_program_file():
    global byte_count
    byte_count = 0;
    with open('prog.grk', 'r') as f:
       text_prog = f.read().split()
    
    in_comment = False
    for word in text_prog:
        
        if word in keywords and in_comment == False:
            prog.append(keywords[word])
            byte_count += 1
            print(f'{byte_count}: {word}')
        else:
            if(in_comment == False):
                if(word == '*' or word[0] == '*'):
                    print(f'--------{word} ',end='')
                    in_comment = True
                elif(word.isalnum()):
                    prog.append(int(word))
                    byte_count += 1
                    print(f'{byte_count}: {word}')
                    
            elif(in_comment == True):
                if(word == '*' or word[-1] == '*'):
                    in_comment = False
                    print(f'{word}--------')
                else:
                    print(f'{word} ',end='')
                    
            if(word[0] == '*' and word[-1] == '*' and len(word) > 1):
                print(f'--------{word}--------')
                in_comment = False
                
                
    print(f'Program Bytes: {byte_count}')
system('cls')
print("Welcome to TGRK!")
print_dots()

def convert_to_binary():
    read_program_file()
    for num in prog:
        print(f'{num:b}')

while(mode == menu):
    byte_count = 0
    prog = []
    print_dots()
    print('[1] Download from Device')
    print('[2] Upload To Device')
    print('[3] Print Byte Count')
    print('[4] Convert To Binary')
    print_dots()
    ui = input('> ')
    system('cls')
    
    if(int(ui) == read):
        print("Waiting for connection to device...")
        last_mode = mode
        mode = read
    elif(int(ui) == write):
        print("Waiting for connection to device...")
        last_mode = mode
        mode = write
    elif(int(ui) == get_bytes):
        read_program_file()
    elif(int(ui) == convert):
        convert_to_binary();
    else:
        print("Invalid selection!")

if(connected == False):
    ser = serial.Serial('COM12', 2400, timeout=20)
    ser.read_until(b'\xFF')
    
    print("Got byte!")
    print("Sending byte back...")
    
    if(mode == read):
        ser.flush()
        ser.write(b'\x01')
        sleep(0.4)
        ser.reset_input_buffer()
        
    elif(mode == write):
        ser.flush()
        ser.write(b'\x02')
        sleep(0.4)
        ser.reset_input_buffer()

if(mode == write):
    read_program_file()
    
    print("Attempting to send program...")
    print_dots()
    
    data_as_bytes = bytearray([(x + 256) % 256 for x in prog])
    ser.write(data_as_bytes)
    ser.reset_input_buffer()
    print(f'{byte_count} bytes sent:    {prog}')
    print("Waiting for data...")
    
    last_mode = mode
    mode = read

if(mode == read):
    count = 0;
    data = []
    
    getVal = ser.read_until(b'\x80')
    
    for x in getVal:
        
        for y in range(256):
                
            if(x == keywords['EOP']):
                x = 127
            else:
                x = x-1
            
        if( x == keywords['EOP']):
            data.append(x);
            count = count + 1;
            break;
        else:
            data.append(x);
            count = count + 1;


    print(f'{count} bytes received: {data}')
    
    if(last_mode == write):
        print_dots()
        if(prog == data):
            print("Success!")
        else:
            print("Data transfer error!")
    
ser.close()
