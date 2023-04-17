#include "contiki.h"
#include "net/netstack.h"
#include "net/nullnet/nullnet.h"
#include "net/packetbuf.h"

#include <string.h>
#include <stdio.h> 

/* Log configuration */
#include "sys/log.h"
#define LOG_MODULE "App"
#define LOG_LEVEL LOG_LEVEL_INFO

static signed total = 0; 
static signed message_count = 0; 
/*---------------------------------------------------------------------------*/
PROCESS(unicast_process, "One to One Communication");
AUTOSTART_PROCESSES(&unicast_process);

/*---------Callback executed immediately after reception---------*/
void input_callback(const void *data, uint16_t len,
  const linkaddr_t *src, const linkaddr_t *dest) 
{
  if(len == sizeof(unsigned)) {
    unsigned count;
    memcpy(&count, data, sizeof(count));
    signed short value = (signed short)packetbuf_attr(PACKETBUF_ATTR_RSSI);
    total += value;
    message_count += 1; 
    LOG_INFO("Received %u with rssi %d, total: %d\n, counter: %d\n", count, value, total, message_count);
  }
}
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(unicast_process, ev, data)
{
  static unsigned count = 0;
  
  PROCESS_BEGIN();

  /* Initialize NullNet */
  nullnet_buf = (uint8_t *)&count; //data transmitted
  nullnet_len = sizeof(count); //length of data transmitted
  nullnet_set_input_callback(input_callback); //initialize receiver callback
  
  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
