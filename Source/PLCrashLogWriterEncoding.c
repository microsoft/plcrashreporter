/*
 * Copyright 2008, Dave Benson.
 * 
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with
 * the License. You may obtain a copy of the License
 * at http://www.apache.org/licenses/LICENSE-2.0 Unless
 * required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on
 * an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied. See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

/*
 * Extracted from protobuf-c and modified to support zero-allocation,
 * async-safe file encoding.
 *
 * -landonf dec 12, 2008
 */

#import <stdint.h>
#import <string.h>
#import <stdlib.h>

#include "PLCrashLogWriterEncoding.h"

#define MAX_UINT64_ENCODED_SIZE 10

/* === get_packed_size() === */
static inline size_t
get_tag_size (unsigned number)
{
    if (number < (1<<4))
        return 1;
    else if (number < (1<<11))
        return 2;
    else if (number < (1<<18))
        return 3;
    else if (number < (1<<25))
        return 4;
    else
        return 5;
}
static inline size_t
uint32_size (uint32_t v)
{
    if (v < (1<<7))
        return 1;
    else if (v < (1<<14))
        return 2;
    else if (v < (1<<21))
        return 3;
    else if (v < (1<<28))
        return 4;
    else
        return 5;
}
static inline size_t
int32_size (int32_t v)
{
    if (v < 0)
        return 10;
    else
        if (v < (1<<7))
            return 1;
        else if (v < (1<<14))
            return 2;
        else if (v < (1<<21))
            return 3;
        else if (v < (1<<28))
            return 4;
        else
            return 5;
}
static inline uint32_t
zigzag32 (int32_t v)
{
    if (v < 0)
        return ((uint32_t)(-v)) * 2 - 1;
    else
        return v * 2;
}
static inline size_t
sint32_size (int32_t v)
{
    return uint32_size(zigzag32(v));
}
static inline size_t
uint64_size (uint64_t v)
{
    uint32_t upper_v = (v>>32);
    if (upper_v == 0)
        return uint32_size ((uint32_t)v);
    else if (upper_v < (1<<3))
        return 5;
    else if (upper_v < (1<<10))
        return 6;
    else if (upper_v < (1<<17))
        return 7;
    else if (upper_v < (1<<24))
        return 8;
    else if (upper_v < (1U<<31))
        return 9;
    else
        return 10;
}
static inline uint64_t
zigzag64 (int64_t v)
{
    if (v < 0)
        return ((uint64_t)(-v)) * 2 - 1;
    else
        return v * 2;
}
static inline size_t
sint64_size (int64_t v)
{
    return uint64_size(zigzag64(v));
}


/* === pack() === */
static inline size_t
uint32_pack (uint32_t value, uint8_t *out)
{
    unsigned rv = 0;
    if (value >= 0x80)
    {
        out[rv++] = value | 0x80;
        value >>= 7;
        if (value >= 0x80)
        {
            out[rv++] = value | 0x80;
            value >>= 7;
            if (value >= 0x80)
            {
                out[rv++] = value | 0x80;
                value >>= 7;
                if (value >= 0x80)
                {
                    out[rv++] = value | 0x80;
                    value >>= 7;
                }
            }
        }
    }
    /* assert: value<128 */
    out[rv++] = value;
    return rv;
}
static inline size_t
int32_pack (int32_t value, uint8_t *out)
{
    if (value < 0)
    {
        out[0] = value | 0x80;
        out[1] = (value>>7) | 0x80;
        out[2] = (value>>14) | 0x80;
        out[3] = (value>>21) | 0x80;
        out[4] = (value>>28) | 0x80;
        out[5] = out[6] = out[7] = out[8] = 0xff;
        out[9] = 0x01;
        return 10;
    }
    else
        return uint32_pack (value, out);
}
static inline size_t sint32_pack (int32_t value, uint8_t *out)
{
    return uint32_pack (zigzag32 (value), out);
}
static size_t
uint64_pack (uint64_t value, uint8_t *out)
{
    uint32_t hi = value>>32;
    uint32_t lo = value;
    unsigned rv;
    if (hi == 0)
        return uint32_pack ((uint32_t)lo, out);
    out[0] = (lo) | 0x80;
    out[1] = (lo>>7) | 0x80;
    out[2] = (lo>>14) | 0x80;
    out[3] = (lo>>21) | 0x80;
    if (hi < 8)
    {
        out[4] = (hi<<4) | (lo>>28);
        return 5;
    }
    else
    {
        out[4] = ((hi&7)<<4) | (lo>>28) | 0x80;
        hi >>= 3;
    }
    rv = 5;
    while (hi >= 128)
    {
        out[rv++] = hi | 0x80;
        hi >>= 7;
    }
    out[rv++] = hi;
    return rv;
}
static inline size_t sint64_pack (int64_t value, uint8_t *out)
{
    return uint64_pack (zigzag64 (value), out);
}
static inline size_t fixed32_pack (uint32_t value, uint8_t *out)
{
#if LITTLE_ENDIAN
    memcpy (out, &value, 4);
#else
    out[0] = value;
    out[1] = value>>8;
    out[2] = value>>16;
    out[3] = value>>24;
#endif
    return 4;
}
static inline size_t fixed64_pack (uint64_t value, uint8_t *out)
{
#if LITTLE_ENDIAN
    memcpy (out, &value, 8);
#else
    fixed32_pack (value, out);
    fixed32_pack (value>>32, out+4);
#endif
    return 8;
}
static inline size_t boolean_pack (bool value, uint8_t *out)
{
    *out = value ? 1 : 0;
    return 1;
}
static inline size_t string_pack (const char * str, uint8_t *out)
{
    size_t len = strlen (str);
    size_t rv = uint32_pack (len, out);
    memcpy (out + rv, str, len);
    return rv + len;
}

#if 0
static inline size_t binary_data_pack (const ProtobufCBinaryData *bd, uint8_t *out)
{
    size_t len = bd->len;
    size_t rv = uint32_pack (len, out);
    memcpy (out + rv, bd->data, len);
    return rv + len;
}

static inline size_t
prefixed_message_pack (const ProtobufCMessage *message, uint8_t *out)
{
    size_t rv = protobuf_c_message_pack (message, out + 1);
    uint32_t rv_packed_size = uint32_size (rv);
    if (rv_packed_size != 1)
        memmove (out + rv_packed_size, out + 1, rv);
    return uint32_pack (rv, out) + rv;
}
#endif

/* wire-type will be added in required_field_pack() */
static size_t tag_pack (uint32_t id, uint8_t *out)
{
    if (id < (1<<(32-3)))
        return uint32_pack (id<<3, out);
    else
        return uint64_pack (((uint64_t)id) << 3, out);
}

/* === pack_to_buffer() === */
// file argument may be NULL
size_t plcrash_writer_pack (plasync_file_t *file, uint32_t field_id, ProtobufCType field_type, const void *value) {
    size_t rv;
    uint8_t scratch[MAX_UINT64_ENCODED_SIZE * 2];
    rv = tag_pack (field_id, scratch);
    switch (field_type)
    {
        case PROTOBUF_C_TYPE_SINT32:
            scratch[0] |= PROTOBUF_C_WIRE_TYPE_VARINT;
            rv += sint32_pack (*(const int32_t *) value, scratch + rv);
            if (file != NULL)
                plasync_file_write(file, scratch, rv);
            break;
        case PROTOBUF_C_TYPE_INT32:
            scratch[0] |= PROTOBUF_C_WIRE_TYPE_VARINT;
            rv += int32_pack (*(const uint32_t *) value, scratch + rv);
            if (file != NULL)
                plasync_file_write(file, scratch, rv);
            break;
        case PROTOBUF_C_TYPE_UINT32:
        case PROTOBUF_C_TYPE_ENUM:
            scratch[0] |= PROTOBUF_C_WIRE_TYPE_VARINT;
            rv += uint32_pack (*(const uint32_t *) value, scratch + rv);
            if (file != NULL)
                plasync_file_write(file, scratch, rv);
            break;
        case PROTOBUF_C_TYPE_SINT64:
            scratch[0] |= PROTOBUF_C_WIRE_TYPE_VARINT;
            rv += sint64_pack (*(const int64_t *) value, scratch + rv);
            if (file != NULL)
                plasync_file_write(file, scratch, rv);
            break;
        case PROTOBUF_C_TYPE_INT64:
        case PROTOBUF_C_TYPE_UINT64:
            scratch[0] |= PROTOBUF_C_WIRE_TYPE_VARINT;
            rv += uint64_pack (*(const uint64_t *) value, scratch + rv);
            if (file != NULL)
                plasync_file_write(file, scratch, rv);
            break;
        case PROTOBUF_C_TYPE_SFIXED32:
        case PROTOBUF_C_TYPE_FIXED32:
        case PROTOBUF_C_TYPE_FLOAT:
            scratch[0] |= PROTOBUF_C_WIRE_TYPE_32BIT;
            rv += fixed32_pack (*(const uint64_t *) value, scratch + rv);
            if (file != NULL)
                plasync_file_write(file, scratch, rv);
            break;
        case PROTOBUF_C_TYPE_SFIXED64:
        case PROTOBUF_C_TYPE_FIXED64:
        case PROTOBUF_C_TYPE_DOUBLE:
            scratch[0] |= PROTOBUF_C_WIRE_TYPE_64BIT;
            rv += fixed64_pack (*(const uint64_t *) value, scratch + rv);
            if (file != NULL)
                plasync_file_write(file, scratch, rv);
            break;
        case PROTOBUF_C_TYPE_BOOL:
            scratch[0] |= PROTOBUF_C_WIRE_TYPE_VARINT;
            rv += boolean_pack (*(const bool *) value, scratch + rv);
            if (file != NULL)
                plasync_file_write(file, scratch, rv);
            break;
            
        case PROTOBUF_C_TYPE_STRING:
        {
            size_t sublen = strlen (value);
            scratch[0] |= PROTOBUF_C_WIRE_TYPE_LENGTH_PREFIXED;
            rv += uint32_pack (sublen, scratch + rv);
            if (file != NULL) {
                plasync_file_write(file, scratch, rv);
                plasync_file_write(file, value, sublen);
            }
            rv += sublen;
            break;
        }
#if 0            
        case PROTOBUF_C_TYPE_BYTES:
        {
            const ProtobufCBinaryData * bd = ((const ProtobufCBinaryData*) value);
            size_t sublen = bd->len;
            scratch[0] |= PROTOBUF_C_WIRE_TYPE_LENGTH_PREFIXED;
            rv += uint32_pack (sublen, scratch + rv);
            buffer->append (buffer, rv, scratch);
            buffer->append (buffer, sublen, bd->data);
            rv += sublen;
            break;
        }
#endif
            
            //PROTOBUF_C_TYPE_GROUP,          // NOT SUPPORTED
        case PROTOBUF_C_TYPE_MESSAGE:
        {
            scratch[0] |= PROTOBUF_C_WIRE_TYPE_LENGTH_PREFIXED;
            rv += uint32_pack (*(const uint32_t *) value, scratch + rv);
            if (file != NULL)
                plasync_file_write(file, scratch, rv);
            break;
        }
        default:
            PLCF_DEBUG("Unhandled field type %d", field_type);
            abort();
    }
    return rv;
}
