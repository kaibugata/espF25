#!/usr/bin/env python3
from gpiozero import LED
import time
import sys

MORSE_CODE = {
    'A': '.-', 'B': '-...', 'C': '-.-.', 'D': '-..', 'E': '.',
    'F': '..-.', 'G': '--.', 'H': '....', 'I': '..', 'J': '.---',
    'K': '-.-', 'L': '.-..', 'M': '--', 'N': '-.', 'O': '---',
    'P': '.--.', 'Q': '--.-', 'R': '.-.', 'S': '...', 'T': '-',
    'U': '..-', 'V': '...-', 'W': '.--', 'X': '-..-', 'Y': '-.--',
    'Z': '--..', '1': '.----', '2': '..---', '3': '...--', '4': '....-',
    '5': '.....', '6': '-....', '7': '--...', '8': '---..', '9': '----.',
    '0': '-----', ' ': '/'
}

LED_PIN = 18  # GPIO pin number (BCM numbering)
UNIT = 0.2    # seconds for one Morse unit (was .2)
dotlist = []

led = LED(LED_PIN)

def blink(symbol):
    if symbol == '.':
        dotlist.append('.')
        led.on()
        time.sleep(UNIT)
    elif symbol == '-':
        dotlist.append('-')
        led.on()
        time.sleep(UNIT * 3)
    led.off()
    time.sleep(UNIT) #space(led off for 1 unit of time)

def send_message(message):
    for ch in message.upper():
        if ch == ' ':
            dotlist.append('/')
        if ch not in MORSE_CODE:
            continue
        code = MORSE_CODE[ch]
        for symbol in code:
            blink(symbol)
        # Space between letters
        dotlist.append(' ')
        time.sleep(UNIT * 3)#was * 2
    # Space between words
    time.sleep(UNIT * 7)#was * 4

if __name__ == "__main__":
    if len(sys.argv) < 3:
        print("Usage: ./send <repeat> \"<message>\"")
        sys.exit(1)

    repeat = int(sys.argv[1])
    message = " ".join(sys.argv[2:])


    try:
        for _ in range(repeat):
            send_message(message)
            print(*dotlist, sep = '')
            dotlist.clear()
            time.sleep(UNIT*8)  # pause between repetitions
    finally:
        led.off()
