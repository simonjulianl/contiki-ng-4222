1. Value of `CLOCK_SECOND` = 128 
2. Number of clock ticks per s in 1s using `etimer` = 128 (same as `CLOCK_SECOND`)
3. Value of `RTIMER_SECOND` = 65536
4. Number of clock ticks per s in 1s using `rtimer` = 32 * 4 = 128 (same as `CLOCK_SECOND`) as the same clock_time() function is used

Instruction for buzz.c
1. put the program in the `hello-world` folder of `contiki-ng` examples. Add the buzz into the Makefile (i.e. CONTIKI_PROJECT = ... buzz). 
2. Compile the program using the following command: `sudo make TARGET=cc26x0-cc13x0 BOARD=sensortag/cc2650 PORT=/dev/ttyACM0 buzz` (for linux based OS).
3. Flash the program using Uniflash to the sensortag.
4. Open the console, it will print the value of the gyrometer when it enter the active state and print the lux value in the active state while buzzing.(tbh 300 lux is a very big value, you need to shine torch into it). 
