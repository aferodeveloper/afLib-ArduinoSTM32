/**
 * Copyright 2018 Afero, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <HardwareSerial.h>
#include "stm32_transport.h"
#include "stm32_uart.h"
#include "af_logger.h"
#include "stm32_logger.h"

typedef enum {
    STM32_TRANSPORT_SPI,
    STM32_TRANSPORT_UART
} stm32_transport_t;

static stm32_transport_t s_transport_type;

af_transport_t* stm32_transport_create_uart(HardwareSerial *port, uint32_t baud_rate) {
    s_transport_type = STM32_TRANSPORT_UART;
    return stm32_uart_create(port, baud_rate);
}

void stm32_transport_destroy(af_transport_t *af_transport) {
        stm32_uart_destroy(af_transport);
}

void af_transport_check_for_interrupt(af_transport_t *af_transport, volatile int *interrupts_pending, bool idle) {
        af_transport_check_for_interrupt_uart(af_transport, interrupts_pending, idle);
}

int af_transport_exchange_status(af_transport_t *af_transport, af_status_command_t *af_status_command_tx, af_status_command_t *af_status_command_rx) {
        return af_transport_exchange_status_uart(af_transport, af_status_command_tx, af_status_command_rx);
}

int af_transport_write_status(af_transport_t *af_transport, af_status_command_t *af_status_command) {
        return af_transport_write_status_uart(af_transport, af_status_command);
}

void af_transport_send_bytes_offset(af_transport_t *af_transport, uint8_t *bytes, uint16_t *bytes_to_send, uint16_t *offset) {
        af_transport_send_bytes_offset_uart(af_transport, bytes, bytes_to_send, offset);
}

int af_transport_recv_bytes_offset(af_transport_t *af_transport, uint8_t **bytes, uint16_t *bytes_len, uint16_t *bytes_to_recv, uint16_t *offset) {
        return af_transport_recv_bytes_offset_uart(af_transport, bytes, bytes_len, bytes_to_recv, offset);
}
