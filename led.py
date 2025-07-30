import RPi.GPIO as GPIO
import time


def main():
    GPIO.setmode(GPIO.BCM)
    GPIO.setwarnings(False)
    GPIO.setup(23, GPIO.OUT)
    print("LED on")
    GPIO.output(23, GPIO.HIGH)
    time.sleep(5)
    print("LED off")
    GPIO.output(23, GPIO.LOW)


if __name__ == "__main__":
    main()
