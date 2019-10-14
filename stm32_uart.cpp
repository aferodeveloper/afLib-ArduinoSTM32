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

#include <SPI.h>
#include "stm32_uart.h"
#include "af_lib.h"
#include "af_logger.h"
#include "af_msg_types.h"
#include "af_utils.h"

#define INT_CHAR                            0x32
#define MAX_WAIT_TIME                       1000

class STM32UART {
public:
    STM32UART(HardwareSerial *port, uint32_t baud_rate);

    void checkForInterrupt(volatile int *interrupts_pending, bool idle);
    int exchangeStatus(af_status_command_t *tx, af_status_command_t *rx);
    int writeStatus(af_status_command_t *c);
    void sendBytes(uint8_t *bytes, int len);
    int recvBytes(uint8_t *bytes, int len);
    void sendBytesOffset(uint8_t *bytes, uint16_t *bytesToSend, uint16_t *offset);
    int recvBytesOffset(uint8_t **bytes, uint16_t *bytesLen, uint16_t *bytesToRecv, uint16_t *offset);

private:
    HardwareSerial *_uart;

    int available();
    char peek();
    int read(uint8_t *buffer, int len);
    char read();
    void write(uint8_t *buffer, int len);
};

struct af_transport_t {
    STM32UART *stm32UART;
};

af_transport_t* stm32_uart_create(HardwareSerial *port, uint32_t baud_rate) {
    af_transport_t* result = new af_transport_t();
    result->stm32UART = new STM32UART(port, baud_rate);
    return result;
}

void stm32_uart_destroy(af_transport_t *af_transport) {
    delete af_transport->stm32UART;
    delete af_transport;
}

void af_transport_check_for_interrupt_uart(af_transport_t *af_transport, volatile int *interrupts_pending, bool idle) {
    af_transport->stm32UART->checkForInterrupt(interrupts_pending, idle);
}

int af_transport_exchange_status_uart(af_transport_t *af_transport, af_status_command_t *af_status_command_tx, af_status_command_t *af_status_command_rx) {
    return af_transport->stm32UART->exchangeStatus(af_status_command_tx, af_status_command_rx);
}

int af_transport_write_status_uart(af_transport_t *af_transport, af_status_command_t *af_status_command) {
    return af_transport->stm32UART->writeStatus(af_status_command);
}

void af_transport_send_bytes_offset_uart(af_transport_t *af_transport, uint8_t *bytes, uint16_t *bytes_to_send, uint16_t *offset) {
    af_transport->stm32UART->sendBytesOffset(bytes, bytes_to_send, offset);
}

int af_transport_recv_bytes_offset_uart(af_transport_t *af_transport, uint8_t **bytes, uint16_t *bytes_len, uint16_t *bytes_to_recv, uint16_t *offset) {
    return af_transport->stm32UART->recvBytesOffset(bytes, bytes_len, bytes_to_recv, offset);
}

STM32UART::STM32UART(HardwareSerial *port, uint32_t baud_rate)
{
    _uart = port;
    _uart->begin(baud_rate);
}

int STM32UART::available()
{
    return _uart->available();
}

char STM32UART::peek()
{
    return _uart->peek();
}

char STM32UART::read()
{
    return _uart->read();
}

int STM32UART::read(uint8_t *buffer, int len)
{
    memset(buffer, 0, len);
    for (int i = 0; i < len; i++) {
        int b;
        unsigned long time = af_utils_millis();
        while (((b = _uart->read()) == -1)) {
            if (af_utils_millis() - time > MAX_WAIT_TIME) {
                return -1;
            }
        }
        buffer[i] = (uint8_t )b;
        //af_logger_print_buffer("<"); af_logger_println_formatted_value(buffer[i], AF_LOGGER_HEX);
    }
    return len;
}

void STM32UART::write(uint8_t *buffer, int len)
{
    for (int i = 0; i < len; i++) {
        //af_logger_print_buffer(">"); af_logger_println_formatted_value(buffer[i], AF_LOGGER_HEX);
        _uart->write(buffer[i]);
    }
}

void STM32UART::checkForInterrupt(volatile int *interrupts_pending, bool idle) {
    if (available()) {
        if (peek() == INT_CHAR) {
            if (*interrupts_pending == 0) {
                //af_logger_println_buffer("INT");
                read();
                *interrupts_pending += 1;
            } else if (idle) {
                read();
            } else {
                //af_logger_println_buffer("INT(Pending)");
            }
        } else {
            if (*interrupts_pending == 0) {
                //af_logger_print_buffer("Skipping: "); af_logger_println_formatted_value(peek(), AF_LOGGER_HEX);
                read();
            }
        }
    }
}

int STM32UART::exchangeStatus(af_status_command_t *tx, af_status_command_t *rx) {
    int result = AF_SUCCESS;
    uint16_t len = af_status_command_get_size(tx);
    uint8_t bytes[len];
    uint8_t rbytes[len + 1];
    int index = 0;
    af_status_command_get_bytes(tx, bytes);

    for (int i=0; i < len; i++)
    {
        rbytes[i]=bytes[i];
    }
    rbytes[len]=af_status_command_get_checksum(tx);
    sendBytes(rbytes, len + 1);

    // Skip any interrupts that may have come in.
    int read_result = recvBytes(rbytes, 1);
    if (read_result < 0) {
        return AF_ERROR_TIMEOUT;
    }
    while (rbytes[0] == INT_CHAR) {
        read_result = recvBytes(rbytes, 1);
        if (read_result < 0) {
            return AF_ERROR_TIMEOUT;
        }
    }

    // Okay, we have a good first char, now read the rest.
    read_result = recvBytes(&rbytes[1], len);
    if (read_result < 0) {
        return AF_ERROR_TIMEOUT;
    }

    uint8_t cmd = bytes[index++];
    if (cmd != SYNC_REQUEST && cmd != SYNC_ACK) {
        af_logger_print_buffer("exchangeStatus bad cmd: ");
        af_logger_println_formatted_value(cmd, AF_LOGGER_HEX);
        result = AF_ERROR_INVALID_COMMAND;
    }

    af_status_command_set_bytes_to_send(rx, rbytes[index + 0] | (rbytes[index + 1] << 8));
    af_status_command_set_bytes_to_recv(rx, rbytes[index + 2] | (rbytes[index + 3] << 8));
    af_status_command_set_checksum(rx, rbytes[index+4]);

    return result;
}

int STM32UART::writeStatus(af_status_command_t *c) {
    int result = AF_SUCCESS;
    uint16_t len = af_status_command_get_size(c);
    uint8_t bytes[len];
    uint8_t rbytes[len+1];
    int index = 0;
    af_status_command_get_bytes(c, bytes);

    for (int i=0;i<len;i++)
    {
        rbytes[i]=bytes[i];
    }
    rbytes[len]=af_status_command_get_checksum(c);

    sendBytes(rbytes, len + 1);

    uint8_t cmd = rbytes[index++];
    if (cmd != SYNC_REQUEST && cmd != SYNC_ACK) {
        af_logger_print_buffer("writeStatus bad cmd: ");
        af_logger_println_formatted_value(cmd, AF_LOGGER_HEX);
        result = AF_ERROR_INVALID_COMMAND;
    }

    //af_status_command_dump(c);
    //af_status_command_dump_bytes(c);

    return result;
}

void STM32UART::sendBytes(uint8_t *bytes, int len) {
    write(bytes, len);
}

int STM32UART::recvBytes(uint8_t *bytes, int len) {
    return read(bytes, len);
}

void STM32UART::sendBytesOffset(uint8_t *bytes, uint16_t *bytesToSend, uint16_t *offset)
{
    uint16_t len = 0;

    len = *bytesToSend;

    sendBytes(bytes, len);

  //dumpBytes("Sending:", len, bytes);

    *offset += len;
    *bytesToSend -= len;
}

int STM32UART::recvBytesOffset(uint8_t **bytes, uint16_t *bytesLen, uint16_t *bytesToRecv, uint16_t *offset)
{
    uint16_t len = 0;

    len = *bytesToRecv;

    if (*offset == 0) {
        *bytesLen = *bytesToRecv;
        *bytes = (uint8_t*)malloc(*bytesLen);
    }

    uint8_t * start = *bytes + *offset;

    int read_result = recvBytes(start, len);
    if (read_result < 0) {
        return AF_ERROR_TIMEOUT;
    }

  //dumpBytes("Receiving:", len, _readBuffer);

    *offset += len;
    *bytesToRecv -= len;

    return AF_SUCCESS;
}
