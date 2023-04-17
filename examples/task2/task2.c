/*
 * CS4222/5422: Project 
 * Perform neighbour discovery
 */

#include "contiki.h"
#include "sys/clock.h"
#include <stdio.h> 

PROCESS(nbr_discovery_process, "cc2650 neighbour discovery process");
AUTOSTART_PROCESSES(&nbr_discovery_process);

// Main thread that handles the neighbour discovery process
PROCESS_THREAD(nbr_discovery_process, ev, data)
{

    PROCESS_BEGIN();

    clock_time_t current_time = clock_time();

    // Convert clock ticks to seconds
    uint32_t seconds = (uint32_t)(current_time / CLOCK_SECOND);

    // Convert seconds to human-readable format
    uint8_t second = seconds % 60;
    uint8_t minutes = (seconds / 60) % 60;
    uint8_t hours = (seconds / 3600) % 24;

    // Print the current time
    printf("Current time: %02d:%02d:%02d\n", hours, minutes, second);

    PROCESS_END();
}
