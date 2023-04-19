To run the project, please unzip the folder in `/contiki-ng/examples/` and then create a new `Makefile` in the 
same folder with the following content

```makefile
CONTIKI_PROJECT = transmitter receiver
all: $(CONTIKI_PROJECT)

CONTIKI = ../..


MAKE_NET = MAKE_NET_NULLNET
include $(CONTIKI)/Makefile.include
```

In this code, there are 2 main `C` files and 1 `.h` file, namely: `receiver.c`, `transmitter.c`, and `defs_and_types.h`. The `receiver` is the node
that is responsible that will be requesting the light data and the `transmitter` is the node that is responsible 
in collecting the light reading data and send them to the `receiver`.

Run the following command to compile the receiver and transmitter, replacing the port with any available port such as `/dev/ttyACM0`
```
# for transmitter
sudo make TARGET=cc26x0-cc13x0 BOARD=sensortag/cc2650 PORT=(ANY AVAILABLE PORT) transmitter        

# for receiver
sudo make TARGET=cc26x0-cc13x0 BOARD=sensortag/cc2650 PORT=(ANY AVAILABLE PORT) receiver        
```

Use `uniflash` to flash the program to the nodes and observe the output the in terminal connected to the ports that you have selected. 
