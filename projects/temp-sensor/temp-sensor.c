/*
 * Copyright (c) 2006, Swedish Institute of Computer Science.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * This file is part of the Contiki operating system.
 *
 */

/*---------------------------------------------------------------------------*/
/* ADC PIN = PF2 = ADC2 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <avr/eeprom.h>
#include <avr/pgmspace.h>
#include <avr/io.h>
#include "rs232.h"
#include "ringbuf.h"
#include "sys/clock.h"

#include "net/uip-ds6.h"
#include "net/rpl/rpl.h"

#include "er-coap-07.h"
#include "er-coap-07-transactions.h"
#include "er-coap-07-separate.h"

#include "adc.h"

#define MAX(a,b) ((a)<(b)?(b):(a))
#define TRUE 1
#define FALSE 0


/*--PROCESSES----------------------------------------------------------------*/
PROCESS(rfnode_test_process, "rfNode_test");
PROCESS(coap_process, "coap");

/*---------------------------------------------------------------------------*/
static struct ringbuf uart_buf;
static unsigned char uart_buf_data[128] = {0};

static int16_t temperature;
static int16_t temperature_last;
static process_event_t get_temperature_response_event;
static process_event_t changed_temperature_event;
clock_time_t last_temperature_reading;

static int16_t threshold = 10;
static uint8_t poll_time = 3;

const uint16_t lookup[] PROGMEM = {-285,-54,84,172,238,291,336,375,410,442,471,498,523,547,569,590,611,630,649,667,684,701,718,733,749,764,779,793,808,821,835,849,862,875,888,900,913,925,938,950,962,974,986,998,1009,1021,1033,1044,1056,1067,1079,1090,1102,1113,1124,1136,1147,1158,1170,1181,1193,1204,1216,1227,1239};

typedef struct application_separate_get_temperature_store {
	coap_separate_t request_metadata;
	uint8_t error;
} application_separate_get_temperature_store_t;

static uint8_t separate_get_temperature_active = 0;
static application_separate_get_temperature_store_t separate_get_temperature_store[1];


/*--ADC-PROCESS-IMPLEMENTATION-----------------------------------------*/
static int uart_get_char(unsigned char c)
{
	ringbuf_put(&uart_buf, c);
	if (c=='\n' || ringbuf_size(&uart_buf)==127) {
		ringbuf_put(&uart_buf, '\0');
		process_post(&rfnode_test_process, PROCESS_EVENT_MSG, NULL);
	}
	return 1;
}

static void read_temperature(void){
	static int16_t reading;
	int16_t new_temperature;
	
	adc_init(ADC_CHAN_ADC2, ADC_MUX5_0,ADC_TRIG_FREE_RUN, ADC_REF_INT_1_6, ADC_PS_32, ADC_ADJ_RIGHT);

	reading = doAdc(ADC_CHAN_ADC2, ADC_MUX5_0, 2);
	
	adc_deinit();
	/*--- Computation for real value, with 330 ohm resistor (Steinhart-Hart-Equation)
	temp = log(330/(reading/3.3*1.6/1024)-330);
	kelvin = 1 / (0.001129148 + (0.000234125 * temp) + (8.76741e-8 * temp * temp * temp) );
	celsius = temp-273.15;
	------*/
	int16_t delta = reading % 16;
	int16_t low = pgm_read_word(&lookup[reading/16]);
	int16_t high = pgm_read_word(&lookup[(reading/16)+1]);
	new_temperature=low+delta*(high-low)/16;

	last_temperature_reading =  clock_time();
	
	printf_P(PSTR("Temp: %d.%01d\n"),new_temperature/10, new_temperature>0 ? new_temperature%10 : (-1*new_temperature)%10);

	temperature =  new_temperature;
	if (temperature_last - threshold > new_temperature || temperature_last + threshold < new_temperature){
		temperature_last =  new_temperature;
		process_post(&coap_process, changed_temperature_event, NULL);
	}
	separate_get_temperature_store->error=FALSE;
	process_post(&coap_process, get_temperature_response_event, NULL);


}

PROCESS_THREAD(rfnode_test_process, ev, data)
{
	PROCESS_BEGIN();
	
	int rx;
	int buf_pos;
	char buf[128];
	static struct etimer etimer;


	ringbuf_init(&uart_buf, uart_buf_data, sizeof(uart_buf_data));
	rs232_set_input(RS232_PORT_0, uart_get_char);

	get_temperature_response_event = process_alloc_event();
	changed_temperature_event = process_alloc_event();


	// finish booting first
	PROCESS_PAUSE();

	DDRF = 0;      //set all pins of port b as inputs
	PORTF = 0;     //write data on port 
	etimer_set(&etimer, poll_time*CLOCK_SECOND);
	
	while (1) {
		PROCESS_WAIT_EVENT();
		if (ev == PROCESS_EVENT_MSG) {
			buf_pos = 0;
			while ((rx=ringbuf_get(&uart_buf))!=-1) {
				if (buf_pos<126 && (char)rx=='\n') {
					buf[buf_pos++] = '\n';
					buf[buf_pos] = '\0';
					buf_pos = 0;
					continue;
				} else {
					buf[buf_pos++] = (char)rx;
				}
				if (buf_pos==127) {
					buf[buf_pos] = 0;
					buf_pos = 0;
				}
			} // while
		} // events
		else  if (ev == PROCESS_EVENT_TIMER) {
			read_temperature();			
			etimer_set(&etimer, CLOCK_SECOND * poll_time);
		}
	}
	PROCESS_END();
}



/*--CoAP - Process---------------------------------------------------------*/

/*--------- Temperature ---------------------------------------------------------*/
EVENT_RESOURCE(temperature, METHOD_GET, "sensors/temperature", "title=\"Temperature\";ct=0;rt=\"temperature:C\"");
void temperature_handler(void* request, void* response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{
	if(last_temperature_reading > clock_time()-5*CLOCK_SECOND){
		snprintf_P((char*)buffer, preferred_size, PSTR(" %d.%01d\n"),temperature/10, temperature>0 ? temperature%10 : (-1*temperature)%10);
  		REST.set_header_content_type(response, REST.type.TEXT_PLAIN);
		REST.set_response_payload(response, buffer, strlen((char*)buffer));
	}
	else{
		if (!separate_get_temperature_active){
			coap_separate_accept(request, &separate_get_temperature_store->request_metadata);
			separate_get_temperature_active = 1;
			read_temperature();
		}
		else {
			coap_separate_reject();
			read_temperature();
		}
	}
}

void temperature_event_handler(resource_t *r) {
	static uint32_t event_i = 0;
	char content[6];

	++event_i;

  	coap_packet_t notification[1]; /* This way the packet can be treated as pointer as usual. */
  	coap_init_message(notification, COAP_TYPE_NON, CONTENT_2_05, 0 );
  	coap_set_payload(notification, content, snprintf_P(content, 6, PSTR(" %d.%01d\n"),temperature/10, temperature>0 ? temperature%10 : (-1*temperature)%10));

	REST.notify_subscribers(r, event_i, notification);

}

void temperature_finalize_handler() {
	if (separate_get_temperature_active){
		char buffer[10];
		coap_transaction_t *transaction = NULL;
		if ( (transaction = coap_new_transaction(separate_get_temperature_store->request_metadata.mid, &separate_get_temperature_store->request_metadata.addr, separate_get_temperature_store->request_metadata.port)) ){
			coap_packet_t response[1]; /* This way the packet can be treated as pointer as usual. */
      			coap_separate_resume(response, &separate_get_temperature_store->request_metadata, CONTENT_2_05);
			snprintf_P(buffer, 9, PSTR(" %d.%01d\n"),temperature/10, temperature>0 ? temperature%10 : (-1*temperature)%10);
			REST.set_header_content_type(response, REST.type.TEXT_PLAIN);
			coap_set_payload(response, buffer, strlen(buffer));
			coap_set_header_block2(response, separate_get_temperature_store->request_metadata.block2_num, 0, separate_get_temperature_store->request_metadata.block2_size);
			transaction->packet_len = coap_serialize_message(response, transaction->packet);
			coap_send_transaction(transaction);
			separate_get_temperature_active = 0;
		}
		else {
			separate_get_temperature_active = 0;
      			/*
		       * TODO: ERROR HANDLING: Set timer for retry, send error message, ...
		       */
		}
	}
}



/*--------- Poll Time ------------------------------------------------------------*/

RESOURCE(poll, METHOD_GET | METHOD_PUT, "config/poll", "title=\"Polling interval\";ct=0;rt=\"time:s\"");
void poll_handler(void* request, void* response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{
	if (REST.get_method_type(request)==METHOD_GET)
	{
		snprintf_P((char*)buffer, preferred_size, PSTR("%d"), poll_time);
	}
	else
	{
		const uint8_t * string = NULL;
		int success = 1;
		int len = coap_get_payload(request, &string);
		if(len == 0){
			success = 0;
		}
		else
		{
    			int i;
        		for(i=0; i<len; i++){
        			if (!isdigit(string[i])){
                        		success = 0;
                        		break;
                	  	}
            		}
            		if(success){
                		int poll_intervall = atoi((char*)string);
                		if(poll_intervall < 255 && poll_intervall > 0){
                			poll_time = poll_intervall;
        				REST.set_response_status(response, REST.status.CHANGED);
                	        	strncpy_P((char*)buffer, PSTR("Successfully set poll intervall"), preferred_size);
                	    	}
                	    	else{
                	        	success = 0;
                    		}
            		}
    		}
		if(!success){
        		REST.set_response_status(response, REST.status.BAD_REQUEST);
			strncpy_P((char*)buffer, PSTR("Payload format: aa, e.g. 15 sets the poll interval to 15 seconds"), preferred_size);
		}
	}

	REST.set_header_content_type(response, REST.type.TEXT_PLAIN);
	REST.set_response_payload(response, buffer, strlen((char*)buffer));
}


/*--------- Threshold ---------------------------------------------------------*/
RESOURCE(threshold, METHOD_GET | METHOD_PUT, "config/threshold", "title=\"Threshold temperature\";ct=0;rt=\"temperature:C\"");
void threshold_handler(void* request, void* response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{
	if (REST.get_method_type(request)==METHOD_GET)
	{
		snprintf_P((char*)buffer, preferred_size, PSTR("%d.%01d"), threshold/10, threshold%10);
		REST.set_header_content_type(response, REST.type.TEXT_PLAIN);
		REST.set_response_payload(response, buffer, strlen((char*)buffer));
	}
	else {
        	const uint8_t * string = NULL;
        	int success = 1;
        	int len = coap_get_payload(request, &string);
		uint16_t value;
		if(len == 1){
			if (isdigit(string[0])){
				value = (atoi((char*) string));
			}
			else {
				success = 0;
			}
		}
		else if(len == 2){
			if (isdigit(string[0]) && isdigit(string[1])){
				value = (atoi((char*) string)) * 10;
			}
			else {
				success = 0;
			}
		}
		else if (len == 3) {
			if (isdigit(string[0]) && isdigit(string[2]) && string[1]=='.'){
				value = (atoi((char*) string)) * 10;
				value += atoi((char*) string+2);
			}
			else {
				success = 0;
			}
		}
		else if(len == 4){
			if (isdigit(string[0]) && isdigit(string[1]) && isdigit(string[3]) && string[2]=='.'){
				value = (atoi((char*) string) *10);
				value += atoi((char*) string+3);
			}
			else {
				success = 0;
			}
		}
		else {
			success = 0;
		}
	        if(success){
			threshold=value;
        		REST.set_response_status(response, REST.status.CHANGED);
			REST.set_header_content_type(response, REST.type.TEXT_PLAIN);
	
	        }
        	else{
        		REST.set_response_status(response, REST.status.BAD_REQUEST);
                	strncpy_P((char*)buffer, PSTR("Payload format: tt.t, e.g. 1.0 sets the threshold to 1.0 deg"), preferred_size);
			REST.set_header_content_type(response, REST.type.TEXT_PLAIN);
			REST.set_response_payload(response, buffer, strlen((char*)buffer));
			return;
        	}
	}
}




PROCESS_THREAD(coap_process, ev, data)
{
	PROCESS_BEGIN();
  

	rest_init_engine();
	rest_activate_event_resource(&resource_temperature);	
	rest_activate_resource(&resource_threshold);
	rest_activate_resource(&resource_poll);

	//activate the resources


	while(1) {
		PROCESS_WAIT_EVENT();
		if (ev == get_temperature_response_event){
			temperature_finalize_handler();
		}
		else if (ev == changed_temperature_event){
			temperature_event_handler(&resource_temperature);	
		}
	}
	
	PROCESS_END();
}

/*---------------------------------------------------------------------------*/
AUTOSTART_PROCESSES(&rfnode_test_process, &coap_process);
/*---------------------------------------------------------------------------*/