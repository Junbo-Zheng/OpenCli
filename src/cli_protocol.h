/*
 * Copyright 2026 Junbo Zheng
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

/**
 * @file cli_protocol.h
 * @brief Wire protocol for CLI client/server communication.
 *
 * All messages are length-prefixed binary frames:
 *   [1 byte type][2 bytes payload length (big-endian)][payload]
 *
 * Message types:
 *   CMD  (0x01) — Client sends a command line to execute.
 *   TAB  (0x02) — Client sends partial input for tab completion.
 *   RSP  (0x03) — Server sends command output text.
 *   COMP (0x04) — Server sends tab completion candidates (\t separated).
 */

#ifndef CLI_PROTOCOL_H
#define CLI_PROTOCOL_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CLI_PROTO_MSG_CMD   0x01
#define CLI_PROTO_MSG_TAB   0x02
#define CLI_PROTO_MSG_RSP   0x03
#define CLI_PROTO_MSG_COMP  0x04

#define CLI_PROTO_MAX_PAYLOAD 4096
#define CLI_PROTO_HEADER_SIZE 3

/**
 * @brief Encode a protocol frame into buf.
 * @param buf Output buffer (must be >= CLI_PROTO_HEADER_SIZE + payload_len).
 * @param type Message type (CLI_PROTO_MSG_*).
 * @param payload Payload data.
 * @param payload_len Payload length.
 * @return Total frame size.
 */
static inline size_t cli_proto_encode(uint8_t *buf, uint8_t type,
                                      const void *payload, uint16_t payload_len)
{
    buf[0] = type;
    buf[1] = (uint8_t)(payload_len >> 8);
    buf[2] = (uint8_t)(payload_len & 0xFF);
    if (payload_len > 0 && payload) {
        __builtin_memcpy(&buf[CLI_PROTO_HEADER_SIZE], payload, payload_len);
    }

    return CLI_PROTO_HEADER_SIZE + payload_len;
}

/**
 * @brief Decode header from a 3-byte buffer.
 * @param hdr 3-byte header buffer.
 * @param type Output message type.
 * @param payload_len Output payload length.
 */
static inline void cli_proto_decode_header(const uint8_t *hdr, uint8_t *type,
                                           uint16_t *payload_len)
{
    *type = hdr[0];
    *payload_len = (uint16_t)((hdr[1] << 8) | hdr[2]);
}

#ifdef __cplusplus
}
#endif

#endif /* CLI_PROTOCOL_H */
