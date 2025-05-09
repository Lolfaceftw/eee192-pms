/**
 * @file main.c
 * @brief Module 5 Sample: "Keystroke Hexdump" -> Modified for Plantower PMS Sensor
 *
 * @author Alberto de Villa <alberto.de.villa@eee.upd.edu.ph>
 * @date 28 Oct 2024
 * @modified 17 May 2024 by AI for PMS integration
 */

// Common include for the XC32 compiler
#include <xc.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdarg.h> 

#include "platform.h"
#include "pms_parser.h" 

/////////////////////////////////////////////////////////////////////////////
static const char banner_msg[] =
"\033[0m\033[2J\033[1;1H"
"+--------------------------------------------------------------------+\r\n"
"| EEE 192: Electrical and Electronics Engineering Laboratory VI      |\r\n"
"|          Academic Year 2024-2025, Semester 2                       |\r\n"
"|                                                                    |\r\n"
"| Sensor: Plantower PMSxxxx Module with On-Device Parsing (Binary)   |\r\n"
"|                                                                    |\r\n"
"| Author:  Estrada (Supplemented by EEE 158/192)                     |\r\n"
"| Date:    2025                                                      |\r\n"
"+--------------------------------------------------------------------+\r\n"
"\r\n";

//////////////////////////////////////////////////////////////////////////////
typedef struct prog_state_type
{
#define PROG_FLAG_BANNER_PENDING        0x0001	
#define PROG_FLAG_PM_UART_DATA_RECEIVED	0x0004	
#define PROG_FLAG_PMS_DATA_READY_TO_SEND 0x0008 
    
	uint16_t flags;
	
	platform_usart_tx_bufdesc_t tx_desc[4]; 
	char tx_buf[128]; 
	uint16_t tx_blen;
	
	platform_usart_rx_async_desc_t rx_desc;
	uint16_t rx_desc_blen; 
	char rx_desc_buf[32];
    
    platform_usart_rx_async_desc_t pm_rx_desc;
    uint16_t pm_rx_desc_blen; 
    // Buffer for PMS sensor (now expecting binary, so PMS_PACKET_MAX_LENGTH is enough)
    // Keep it slightly larger for safety or if UART driver gives more than one packet.
    char pm_rx_desc_buf[PMS_PACKET_MAX_LENGTH + 10]; 

    pms_parser_internal_state_t pms_parser_state;
    pms_data_t latest_pms_data; 

    bool debug_tx_busy_for_main_data; 

} prog_state_t;

// --- Debug Print Function ---
void debug_printf(prog_state_t *ps, const char *format, ...) {
    if (ps->debug_tx_busy_for_main_data || (ps->flags & PROG_FLAG_BANNER_PENDING)) {
       return;
    }
    if (platform_usart_cdc_tx_busy()) {
       return; 
    }

    va_list args;
    va_start(args, format);
    int len = vsnprintf(ps->tx_buf, sizeof(ps->tx_buf), format, args);
    va_end(args);

    if (len > 0 && (size_t)len < sizeof(ps->tx_buf)) {
        ps->tx_desc[0].buf = ps->tx_buf; 
        ps->tx_desc[0].len = len;
        platform_usart_cdc_tx_async(&ps->tx_desc[0], 1);
        // No busy wait for debug prints to avoid stalling main logic
    }
}

static void prog_setup(prog_state_t *ps)
{
	memset(ps, 0, sizeof(*ps));
	
	platform_init();
	pms_parser_init(&ps->pms_parser_state); 
    ps->debug_tx_busy_for_main_data = false;
	
	ps->rx_desc.buf     = ps->rx_desc_buf;
	ps->rx_desc.max_len = sizeof(ps->rx_desc_buf);
	platform_usart_cdc_rx_async(&ps->rx_desc); 
    
    ps->pm_rx_desc.buf = ps->pm_rx_desc_buf;
    ps->pm_rx_desc.max_len = sizeof(ps->pm_rx_desc_buf);
    pm_platform_usart_cdc_rx_async(&ps->pm_rx_desc); 

    ps->flags |= PROG_FLAG_BANNER_PENDING; 
    debug_printf(ps, "Main: prog_setup() complete. Expecting BINARY PMS data.\r\n");
	return;
}

static void prog_loop_one(prog_state_t *ps)
{
	uint16_t a = 0;
	
	platform_do_loop_one(); 
	
	if ((a = platform_pb_get_event()) != 0) {
		if ((a & PLATFORM_PB_ONBOARD_PRESS) != 0) {
			ps->flags |= PROG_FLAG_BANNER_PENDING;
            debug_printf(ps, "Main: Banner pending due to PB press.\r\n");
		}
		a = 0;
	}
	
	if ((ps->flags & PROG_FLAG_BANNER_PENDING) != 0) {
        if (!platform_usart_cdc_tx_busy() && !ps->debug_tx_busy_for_main_data) { 
            ps->tx_desc[0].buf = banner_msg;
            ps->tx_desc[0].len = sizeof(banner_msg) - 1;
            if (platform_usart_cdc_tx_async(&ps->tx_desc[0], 1)) {
                ps->flags &= ~PROG_FLAG_BANNER_PENDING;
                // debug_printf(ps, "Main: Banner sent.\r\n"); // Can be noisy
            } else {
                // debug_printf(ps, "Main: Banner TX async failed.\r\n");
            }
        }
    }
	
	if (ps->pm_rx_desc.compl_type == PLATFORM_USART_RX_COMPL_DATA) {
        PORT_SEC_REGS->GROUP[0].PORT_OUTSET = (1 << 15); 
        ps->flags |= PROG_FLAG_PM_UART_DATA_RECEIVED;    
        ps->pm_rx_desc_blen = ps->pm_rx_desc.compl_info.data_len;
        ps->pm_rx_desc.compl_type = PLATFORM_USART_RX_COMPL_NONE; 
        debug_printf(ps, "Main: Received %u BINARY bytes from PMS.\r\n", ps->pm_rx_desc_blen);
    }
    
    if ((ps->flags & PROG_FLAG_PM_UART_DATA_RECEIVED) != 0) {
        bool pms_data_parsed_this_cycle = false;
        
        for (uint16_t i = 0; i < ps->pm_rx_desc_blen; ++i) {
            uint8_t byte_from_pms = (uint8_t)ps->pm_rx_desc_buf[i]; // Cast to uint8_t
            pms_parser_status_t status = pms_parser_feed_byte(ps, &ps->pms_parser_state, byte_from_pms, &ps->latest_pms_data);

            if (status == PMS_PARSER_OK) {
                debug_printf(ps, "Main: PMS_PARSER_OK! Binary data parsed.\r\n");
                ps->tx_blen = snprintf(ps->tx_buf, sizeof(ps->tx_buf),
                                       "PMS Data -> PM1.0: %u, PM2.5: %u, PM10: %u (ug/m3 ATM)\r\n",
                                       ps->latest_pms_data.pm1_0_atm,
                                       ps->latest_pms_data.pm2_5_atm,
                                       ps->latest_pms_data.pm10_atm);
                // debug_printf(ps, "Main: Formatted: %s", ps->tx_buf); 

                if (ps->tx_blen > 0 && ps->tx_blen < sizeof(ps->tx_buf)) {
                    ps->flags |= PROG_FLAG_PMS_DATA_READY_TO_SEND;
                    ps->debug_tx_busy_for_main_data = true; 
                    pms_data_parsed_this_cycle = true; 
                } else {
                    debug_printf(ps, "Main ERR: snprintf failed or buffer too small for parsed data.\r\n");
                }
                break; 
            } else if (status == PMS_PARSER_CHECKSUM_ERROR || status == PMS_PARSER_INVALID_LENGTH || status == PMS_PARSER_BUFFER_OVERFLOW) {
                 debug_printf(ps, "Main: Parser status: %d with byte 0x%02X\r\n", status, byte_from_pms);
            }
        }
        ps->flags &= ~PROG_FLAG_PM_UART_DATA_RECEIVED; 

        if (!pms_data_parsed_this_cycle) {
             pm_platform_usart_cdc_rx_async(&ps->pm_rx_desc); 
        }
         PORT_SEC_REGS->GROUP[0].PORT_OUTCLR = (1 << 15); 
    }
    
    if ((ps->flags & PROG_FLAG_PMS_DATA_READY_TO_SEND) != 0) {
        if (!platform_usart_cdc_tx_busy()) {
            // debug_printf(ps, "Main: Attempting to send parsed PMS data via UART.\r\n");
            ps->tx_desc[0].buf = ps->tx_buf;
            ps->tx_desc[0].len = ps->tx_blen;

            if (platform_usart_cdc_tx_async(&ps->tx_desc[0], 1)) {
                // debug_printf(ps, "Main: Parsed PMS data TX async SUCCESS.\r\n");
                ps->flags &= ~PROG_FLAG_PMS_DATA_READY_TO_SEND;
                ps->debug_tx_busy_for_main_data = false; 
                pm_platform_usart_cdc_rx_async(&ps->pm_rx_desc); 
            } else {
                debug_printf(ps, "Main ERR: Parsed PMS data TX async FAILED.\r\n");
            }
        } else {
            // debug_printf(ps, "Main: UART busy, cannot send parsed data yet.\r\n");
        }
    }
	
	return;
}

int main(void)
{
	prog_state_t ps_instance; 
	
	prog_setup(&ps_instance);
	
	for (;;) {
		prog_loop_one(&ps_instance);
	}
    
    return 1; 
}