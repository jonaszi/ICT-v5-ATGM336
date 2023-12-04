echo "upload to balloon board via USBasp programmer"
cd "C:\Users\User\AppData\Local\Temp\arduino_build*"
avrdude -c usbasp -p Atmega328p -U flash:w:ICT-v5-ATGM336.ino.hex
pause