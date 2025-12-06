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
UNIT = 0.02    # seconds for one Morse unit (was .5), fast was .2, super fast was .05
# UNIT = .018 DID NOT WORK EVEN WITH 7 DELAY IN 5_3 main (aprx 1 char per .19 sec)
#current best is UNIT = 0.02

led = LED(LED_PIN)

def blink(symbol):
    if symbol == '.':
        led.on()
        time.sleep(UNIT)
    elif symbol == '-':
        led.on()
        time.sleep(UNIT * 3)
    led.off()
    time.sleep(UNIT) #space(led off for 1 unit of time)

def send_message(message):
    for ch in message.upper():
        if ch not in MORSE_CODE:
            continue
        code = MORSE_CODE[ch]
        for symbol in code:
            blink(symbol)
        # Space between letters
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
            time.sleep(UNIT*8)  # pause between repetitions
    finally:
        led.off()
