/*
 * Copyright (c) 2010, Swedish Institute of Computer Science.
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
 *
 */

#ifndef __PROJECT_RPL_WEB_CONF_H__
#define __PROJECT_RPL_WEB_CONF_H__

#define IN_COOJA 0
#define WITH_RPL 0

#ifndef NETSTACK_CONF_RDC
//#define NETSTACK_CONF_RDC             nullrdc_driver
#define NETSTACK_CONF_RDC             contikimac_driver
#endif

#define COAP_SERVER_PORT 61616
#define COAP_MAX_OPEN_TRANSACTIONS 1

#define UART1_CONF_RX_WITH_DMA !IN_COOJA

#undef UIP_CONF_IPV6_RPL
#define UIP_CONF_IPV6_RPL               WITH_RPL

#ifndef UIP_FALLBACK_INTERFACE
#define UIP_FALLBACK_INTERFACE rpl_interface
#endif

#define SICSLOWPAN_CONF_FRAG	1

#ifndef QUEUEBUF_CONF_NUM
#define QUEUEBUF_CONF_NUM          6
#endif

#ifndef REST_MAX_CHUNK_SIZE
#define REST_MAX_CHUNK_SIZE    512
#endif

#ifndef UIP_CONF_BUFFER_SIZE
#define UIP_CONF_BUFFER_SIZE    REST_MAX_CHUNK_SIZE+92
#endif

#ifndef UIP_CONF_RECEIVE_WINDOW
#define UIP_CONF_RECEIVE_WINDOW  60
#endif

#ifndef WEBSERVER_CONF_CFS_CONNS
#define WEBSERVER_CONF_CFS_CONNS 2
#endif

#define UIP_CONF_DS6_ROUTE_NBU   2
#define UIP_CONF_DS6_NBR_NBU     2
#define UIP_CONF_DS6_DEFRT_NBU   2
#define UIP_CONF_DS6_PREFIX_NBU  2
#define UIP_CONF_DS6_ADDR_NBU    2
#define UIP_CONF_DS6_MADDR_NBU   0
#define UIP_CONF_DS6_AADDR_NBU   0

#endif /* __PROJECT_RPL_WEB_CONF_H__ */