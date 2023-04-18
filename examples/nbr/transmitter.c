/*
 * CS4222/5422: Project
 * Perform neighbour discovery
 */

#include "contiki.h"
#include "net/netstack.h"
#include "net/nullnet/nullnet.h"
#include "net/packetbuf.h"
#include <string.h>
#include <stdio.h>
#include "node-id.h"
#include "defs_and_types.h"
#include "board-peripherals.h"
#include "sys/rtimer.h"

// Configures the wake-up timer for neighbour discovery
#define WAKE_TIME (CLOCK_SECOND/20)    // 20 HZ, 50 ms
#define SLEEP_CYCLE  16
#define SLEEP_SLOT (CLOCK_SECOND/20)   // sleep slot should not be too large to prevent overflow

// For neighbour discovery, we would like to send message to everyone. We use Broadcast address:
linkaddr_t dest_addr;

typedef enum {
    NOT_CONNECTED,
    CONNECTING,
    CONNECTED
} Status;

typedef struct {
    unsigned long src_id;
    unsigned long last_within_proximity_s;
    unsigned long first_detected;
    Status stat;
} node_info;
/*---------------------------------------------------------------------------*/
// duty cycle = WAKE_TIME / (WAKE_TIME + SLEEP_SLOT * SLEEP_CYCLE)
/*---------------------------------------------------------------------------*/

// sender timer implemented using rtimer
static struct etimer et;
static struct etimer tc_etimer;
static struct etimer light_etimer;
static struct rtimer timer_rt;

// Structure holding the data to be transmitted
static data_packet_struct data_packet;

// Current time stamp of the node
unsigned long curr_timestamp;
unsigned long last_within_proximity_s;

// there can only be 1 sender and multiple receivers
static node_info node_information[MAX_RECEIVERS];
long receiver_id = EMPTY_ID;

// light reading data
unsigned long light_reading_data[10];
unsigned long light_reading_idx = 0;

// Starts the main contiki neighbour discovery process
PROCESS(detect_process, "disco process");
PROCESS(try_connecting_process, "15s connection");
PROCESS(sending_light_process, "requesting light");

AUTOSTART_PROCESSES(&detect_process);

// function prototypes
void handle_connecting_packet(data_packet_struct *received_packet_data);

static uint8_t is_init = 1;

static void init_opt_reading(void) {
    SENSORS_ACTIVATE(opt_3001_sensor);
}

static void get_light_reading() {
    int value = opt_3001_sensor.value(0);
    if (value != CC26XX_SENSOR_READING_ERROR) {
        unsigned long current_idx = (light_reading_idx++) % 10;
        light_reading_data[current_idx] = value;
    }

    init_opt_reading();
}

void do_light_sensor(struct rtimer *timer, void *ptr) {
    rtimer_set(&timer_rt, RTIMER_NOW() + SAMPLING_RATE / 10 * RTIMER_SECOND, 0, do_light_sensor, NULL);
    get_light_reading();
}

// Function called after reception of a packet
void receive_disco_callback(const void *data, uint16_t len, const linkaddr_t *src, const linkaddr_t *dest) {
    // Check if the received packet size matches with what we expect it to be
    if (len == sizeof(data_packet) && receiver_id == EMPTY_ID) {
        static data_packet_struct received_packet_data;

        // Copy the content of packet into the data structure
        memcpy(&received_packet_data, data, len);

        signed short rssi = (signed short) packetbuf_attr(PACKETBUF_ATTR_RSSI);
        curr_timestamp = clock_time();
        if (received_packet_data.role == LIGHT_SENSOR_REQUESTER && rssi >= RSSI_THRESHOLD) {
            // if it detects the any id for the first time
            node_information[0].src_id = received_packet_data.src_id;

            unsigned long time_s = curr_timestamp / CLOCK_SECOND;
            node_information[0].first_detected = time_s;
            node_information[0].last_within_proximity_s = time_s;
            node_information[0].stat = CONNECTING;

            etimer_stop(&et);
            process_start(&try_connecting_process, NULL);
            process_exit(&detect_process);

            // Print the details of the received packet
            printf("Received neighbour discovery packet %lu with rssi %d from %ld\n", received_packet_data.seq,
                   (signed short) packetbuf_attr(PACKETBUF_ATTR_RSSI), received_packet_data.src_id);
        } // do not allow provider to detect other provider / requester - requester
    }
}

void receive_connecting_callback(const void *data, uint16_t len, const linkaddr_t *src, const linkaddr_t *dest) {
    if (len == sizeof(data_packet)) {
        static data_packet_struct received_packet_data;

        // Copy the content of packet into the data structure
        memcpy(&received_packet_data, data, len);

        if (received_packet_data.type != CONNECTING_PACKET) {
            return; // ignore random packet
        }

        handle_connecting_packet(&received_packet_data);
    }
}

void handle_connecting_packet(data_packet_struct *received_packet_data) {
    static int i = 0;
    static signed short current_rssi_value = 0;
    current_rssi_value = (signed short) packetbuf_attr(PACKETBUF_ATTR_RSSI);

    uint8_t is_found = false;
    curr_timestamp = clock_time();
    unsigned long time_s = curr_timestamp / CLOCK_SECOND;
    for (i = 0; i < MAX_RECEIVERS; i++) {
        if (node_information[i].src_id == (*received_packet_data).src_id) {
            is_found = true;
            if (current_rssi_value < RSSI_THRESHOLD) {
                // not within proximity
                switch (node_information[i].stat) {
                    case CONNECTING:
                        node_information[i].src_id = EMPTY_ID; // immediately ignore it
                        node_information[i].stat = NOT_CONNECTED;
                        break;
                    case CONNECTED:
                    case NOT_CONNECTED: // do nothing if its connected
                        break;
                }
            } else {
                node_information[i].last_within_proximity_s = time_s;
            }
        }
    }

    if (!is_found && current_rssi_value >= RSSI_THRESHOLD) {
        for (i = 0; i < MAX_RECEIVERS; i++) {
            if (node_information[i].src_id == EMPTY_ID) {
                node_information[i].src_id = EMPTY_ID;
                node_information[i].first_detected = time_s;
                node_information[i].last_within_proximity_s = time_s;
                break;
            }
        }
    }
}

void receive_light_callback(const void *data, uint16_t len, const linkaddr_t *src, const linkaddr_t *dest) {
    if (len == sizeof(data_packet)) {
        static data_packet_struct received_packet_data;

        // Copy the content of packet into the data structure
        memcpy(&received_packet_data, data, len);

        switch (received_packet_data.type) {
            case LIGHT_SENSOR_PACKET:
                // read the light sensors and print them
                data_packet.type = LIGHT_SENSOR_PACKET;
                data_packet.dest_id = received_packet_data.src_id;
                memcpy(data_packet.data, light_reading_data, sizeof(light_reading_data));
                curr_timestamp = clock_time();
                data_packet.timestamp = curr_timestamp;
                data_packet.seq++;
                NETSTACK_NETWORK.output(&dest_addr);
                break;
            case CONNECTING_PACKET:
                handle_connecting_packet(&received_packet_data);
                break;
        }
    }
}

// at least 1 is connected
PROCESS_THREAD(sending_light_process, ev, data) {
    PROCESS_BEGIN();
    NETSTACK_RADIO.on();

    printf("Currently in sending light phase\n");

    nullnet_set_input_callback(receive_light_callback);
    static int j = 0;

    while (1) {
        NETSTACK_RADIO.on();
        data_packet.type = CONNECTING_PACKET;
        curr_timestamp = clock_time();
        data_packet.timestamp = curr_timestamp;
        data_packet.seq++;
        NETSTACK_NETWORK.output(&dest_addr);
        etimer_set(&light_etimer, CLOCK_SECOND * CONNECT_INTERVAL);
        PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&light_etimer));

        // check the status for every neighbours possible
        bool is_all_empty = true;
        bool at_least_one_is_connecting = false;
        bool at_least_one_is_connected = false;
        curr_timestamp = clock_time();
        for (j = 0; j < MAX_RECEIVERS; j++) {
            if (node_information[j].src_id != EMPTY_ID) {
                bool should_be_connected = (curr_timestamp / CLOCK_SECOND - node_information[j].first_detected) >= CONNECT_PERIOD;
                switch (node_information[j].stat) {
                    case CONNECTED:
                        if (curr_timestamp / CLOCK_SECOND - node_information[j].last_within_proximity_s >= DISCONNECT_PERIOD) {
                            printf("%lu ABSENT %ld\n", node_information[j].last_within_proximity_s, node_information[j].src_id);
                            node_information[j].src_id = EMPTY_ID;
                            node_information[j].stat = NOT_CONNECTED;
                        } else {
                            is_all_empty = false;
                            at_least_one_is_connected = true;
                        }
                        break;
                    case CONNECTING:
                        is_all_empty = false;
                        if (should_be_connected) {
                            printf("%lu DETECT %ld\n", node_information[j].first_detected, node_information[j].src_id);
                            at_least_one_is_connected = true;
                            node_information[j].stat = CONNECTED;
                        } else {
                            at_least_one_is_connecting = true;
                        }
                        break;
                    case NOT_CONNECTED:
                        break;
                }
            }
        }

        if (at_least_one_is_connected) {
          // do nothing
        } else if (is_all_empty) {
            etimer_stop(&light_etimer);

            NETSTACK_RADIO.off();
            process_start(&detect_process, NULL);
            process_exit(&sending_light_process);
            goto quit;
        } else if (at_least_one_is_connecting) {
            // go back to try connecting process
            etimer_stop(&light_etimer);
            NETSTACK_RADIO.off();
            process_start(&try_connecting_process, NULL);
            process_exit(&sending_light_process);
            goto quit;
        }
    }

quit:;
    PROCESS_END();
}

// as long as there is >= 1 process attempt to connect, we will be in this state
PROCESS_THREAD(try_connecting_process, ev, data) {
    PROCESS_BEGIN();
    NETSTACK_RADIO.on();

    nullnet_set_input_callback(receive_connecting_callback);

    printf("Currently in try connecting phase\n");

    static int j;

    while(1) {
        NETSTACK_RADIO.on(); // jic it get turned off anywhere else
        data_packet.type = CONNECTING_PACKET;
        data_packet.seq++;
        curr_timestamp = clock_time();
        data_packet.timestamp = curr_timestamp;

        printf("Send seq# %lu  @ %8lu ticks   %3lu.%03lu\n", data_packet.seq, curr_timestamp,
               curr_timestamp / CLOCK_SECOND, ((curr_timestamp % CLOCK_SECOND) * 1000) / CLOCK_SECOND);
        NETSTACK_NETWORK.output(&dest_addr); //Packet transmission

        etimer_set(&tc_etimer, CLOCK_SECOND * CONNECT_INTERVAL);
        PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&tc_etimer));

        // check the status for every neighbours possible
        bool is_all_empty = true;
        bool at_least_one_is_connected = false;
        curr_timestamp = clock_time();
        for (j = 0; j < MAX_RECEIVERS; j++) {
            if (node_information[j].src_id != EMPTY_ID) {
                is_all_empty = false;
                if (curr_timestamp / CLOCK_SECOND - node_information[j].first_detected >= CONNECT_PERIOD) {
                    printf("%lu DETECT %ld\n", node_information[j].first_detected, node_information[j].src_id);
                    at_least_one_is_connected = true;
                    node_information[j].stat = CONNECTED;
                }
            }
        }

        if (is_all_empty) {
            etimer_stop(&tc_etimer);

            NETSTACK_RADIO.off();
            process_start(&detect_process, NULL);
            process_exit(&try_connecting_process);
            goto quit;
        }

        if (at_least_one_is_connected) {
            etimer_stop(&tc_etimer);

            process_start(&sending_light_process, NULL);
            process_exit(&try_connecting_process);
            goto quit;
        }
    }

quit:;
    PROCESS_END();
}

// Scheduler function for the sender of neighbour discovery packets
PROCESS_THREAD(detect_process, ev, data) {
    // static struct etimer periodic_timer;
    PROCESS_BEGIN();
    static int i = 0;
    if (is_init) {
        init_opt_reading();
        rtimer_set(&timer_rt, RTIMER_NOW() + SAMPLING_RATE / 10 * RTIMER_SECOND, 0, do_light_sensor, NULL);

        // initialize data packet sent for neighbour discovery exchange
        data_packet.src_id = node_id; //Initialize the node ID
        data_packet.seq = 0; //Initialize the sequence number of the packet at the begin of disco
        data_packet.role = LIGHT_SENSOR_PROVIDER;
        linkaddr_copy(&dest_addr, &linkaddr_null);
        printf("Light sensor provider\n");

        for (i = 0; i < 10; i++) {
            light_reading_data[i] = 0;
        }

        for (i = 0; i < MAX_RECEIVERS; i++) {
            node_information[i].src_id = EMPTY_ID;
            node_information[i].stat = NOT_CONNECTED;
        }

        is_init = 0;
    }

    nullnet_set_input_callback(receive_disco_callback); //initialize receiver callback
    printf("Node %d will be sending packet of size %d Bytes\n", node_id, (int) sizeof(data_packet_struct));

    // Get the current time stamp
    curr_timestamp = clock_time();

    printf("Start clock %lu ticks, timestamp %3lu.%03lu\n", curr_timestamp, curr_timestamp / CLOCK_SECOND,
           ((curr_timestamp % CLOCK_SECOND) * 1000) / CLOCK_SECOND);

    while (1) {
        // radio on
        NETSTACK_RADIO.on();

        // send NUM_SEND number of neighbour discovery beacon packets
        for (i = 0; i < NUM_SEND; i++) {
            // Initialize the nullnet module with information of packet to be trasnmitted
            nullnet_buf = (uint8_t *) &data_packet; //data transmitted
            nullnet_len = sizeof(data_packet); //length of data transmitted

            data_packet.seq++;
            curr_timestamp = clock_time();
            data_packet.timestamp = curr_timestamp;
            data_packet.type = CONNECTING_PACKET;

            printf("Send seq# %lu  @ %8lu ticks   %3lu.%03lu\n", data_packet.seq, curr_timestamp,
                   curr_timestamp / CLOCK_SECOND, ((curr_timestamp % CLOCK_SECOND) * 1000) / CLOCK_SECOND);

            NETSTACK_NETWORK.output(&dest_addr); //Packet transmission

            // wait for WAKE_TIME before sending the next packet
            if (i != (NUM_SEND - 1)) {
                etimer_set(&et, WAKE_TIME);
                PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
            }
        }

        // radio off
        NETSTACK_RADIO.off();

        // NumSleep should be a constant or static int
        for (i = 0; i < SLEEP_CYCLE; i++) {
            etimer_set(&et, SLEEP_SLOT);
            PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
        }
    }

    PROCESS_END();
}