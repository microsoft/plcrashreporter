/* --- protobuf-c.c: public protobuf c runtime implementation --- */

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

#include <stdio.h>                      /* for occasional printf()s */
#include <stdlib.h>                     /* for abort(), malloc() etc */
#include <string.h>                     /* for strlen(), memcpy(), memmove() */

#define PRINT_UNPACK_ERRORS              1

#include "protobuf-c.h"

#define MAX_UINT64_ENCODED_SIZE 10

/* convenience macros */
#define TMPALLOC(allocator, size) ((allocator)->tmp_alloc ((allocator)->allocator_data, (size)))
#define ALLOC(allocator, size) ((allocator)->alloc ((allocator)->allocator_data, (size)))
#define FREE(allocator, ptr)   ((allocator)->free ((allocator)->allocator_data, (ptr)))
#define UNALIGNED_ALLOC(allocator, size) ALLOC (allocator, size) /* placeholder */
#define STRUCT_MEMBER_P(struct_p, struct_offset)   \
    ((void *) ((uint8_t*) (struct_p) + (struct_offset)))
#define STRUCT_MEMBER(member_type, struct_p, struct_offset)   \
    (*(member_type*) STRUCT_MEMBER_P ((struct_p), (struct_offset)))
#define STRUCT_MEMBER_PTR(member_type, struct_p, struct_offset)   \
    ((member_type*) STRUCT_MEMBER_P ((struct_p), (struct_offset)))
#define TRUE 1
#define FALSE 0

#define ASSERT_IS_ENUM_DESCRIPTOR(desc) \
  assert((desc)->magic == PROTOBUF_C_ENUM_DESCRIPTOR_MAGIC)
#define ASSERT_IS_MESSAGE_DESCRIPTOR(desc) \
  assert((desc)->magic == PROTOBUF_C_MESSAGE_DESCRIPTOR_MAGIC)
#define ASSERT_IS_MESSAGE(message) \
  ASSERT_IS_MESSAGE_DESCRIPTOR((message)->descriptor)
#define ASSERT_IS_SERVICE_DESCRIPTOR(desc) \
  assert((desc)->magic == PROTOBUF_C_SERVICE_DESCRIPTOR_MAGIC)

/* --- allocator --- */

static void protobuf_c_out_of_memory_default (void)
{
  fprintf (stderr, "Out Of Memory!!!\n");
  abort ();
}
void (*protobuf_c_out_of_memory) (void) = protobuf_c_out_of_memory_default;

static void *system_alloc(void *allocator_data, size_t size)
{
  void *rv;
  (void) allocator_data;
  if (size == 0)
    return NULL;
  rv = malloc (size);
  if (rv == NULL)
    protobuf_c_out_of_memory ();
  return rv;
}

static void system_free (void *allocator_data, void *data)
{
  (void) allocator_data;
  if (data)
    free (data);
}

ProtobufCAllocator protobuf_c_default_allocator =
{
  system_alloc,
  system_free,
  NULL,
  8192,
  NULL
};

ProtobufCAllocator protobuf_c_system_allocator =
{
  system_alloc,
  system_free,
  NULL,
  8192,
  NULL
};

/* === buffer-simple === */
void
protobuf_c_buffer_simple_append (ProtobufCBuffer *buffer,
                                 size_t           len,
                                 const uint8_t   *data)
{
  ProtobufCBufferSimple *simp = (ProtobufCBufferSimple *) buffer;
  size_t new_len = simp->len + len;
  if (new_len > simp->alloced)
    {
      size_t new_alloced = simp->alloced * 2;
      uint8_t *new_data;
      while (new_alloced < new_len)
        new_alloced += new_alloced;
      new_data = ALLOC (&protobuf_c_default_allocator, new_alloced);
      memcpy (new_data, simp->data, simp->len);
      if (simp->must_free_data)
        FREE (&protobuf_c_default_allocator, simp->data);
      else
        simp->must_free_data = 1;
      simp->data = new_data;
      simp->alloced = new_alloced;
    }
  memcpy (simp->data + simp->len, data, len);
  simp->len = new_len;
}

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

static size_t
required_field_get_packed_size (const ProtobufCFieldDescriptor *field,
                                const void *member)
{
  size_t rv = get_tag_size (field->id);
  switch (field->type)
    {
    case PROTOBUF_C_TYPE_SINT32:
      return rv + sint32_size (*(const int32_t *) member);
    case PROTOBUF_C_TYPE_INT32:
      return rv + int32_size (*(const uint32_t *) member);
    case PROTOBUF_C_TYPE_UINT32:
      return rv + uint32_size (*(const uint32_t *) member);
    case PROTOBUF_C_TYPE_SINT64:
      return rv + sint64_size (*(const int64_t *) member);
    case PROTOBUF_C_TYPE_INT64:
    case PROTOBUF_C_TYPE_UINT64:
      return rv + uint64_size (*(const uint64_t *) member);
    case PROTOBUF_C_TYPE_SFIXED32:
    case PROTOBUF_C_TYPE_FIXED32:
      return rv + 4;
    case PROTOBUF_C_TYPE_SFIXED64:
    case PROTOBUF_C_TYPE_FIXED64:
      return rv + 8;
    case PROTOBUF_C_TYPE_BOOL:
      return rv + 1;
    case PROTOBUF_C_TYPE_FLOAT:
      return rv + 4;
    case PROTOBUF_C_TYPE_DOUBLE:
      return rv + 8;
    case PROTOBUF_C_TYPE_ENUM:
      // TODO: is this correct for negative-valued enums?
      return rv + uint32_size (*(const uint32_t *) member);
    case PROTOBUF_C_TYPE_STRING:
      {
        size_t len = strlen (*(char * const *) member);
        return rv + uint32_size (len) + len;
      }
    case PROTOBUF_C_TYPE_BYTES:
      {
        size_t len = ((const ProtobufCBinaryData*) member)->len;
        return rv + uint32_size (len) + len;
      }
    //case PROTOBUF_C_TYPE_GROUP:
    case PROTOBUF_C_TYPE_MESSAGE:
      {
        size_t subrv = protobuf_c_message_get_packed_size (*(ProtobufCMessage * const *) member);
        return rv + uint32_size (subrv) + subrv;
      }
    }
  PROTOBUF_C_ASSERT_NOT_REACHED ();
  return 0;
}
static size_t
optional_field_get_packed_size (const ProtobufCFieldDescriptor *field,
                                const protobuf_c_boolean *has,
                                const void *member)
{
  if (field->type == PROTOBUF_C_TYPE_MESSAGE
   || field->type == PROTOBUF_C_TYPE_STRING)
    {
      const void *ptr = * (const void * const *) member;
      if (ptr == NULL
       || ptr == field->default_value)
        return 0;
    }
  else
    {
      if (!*has)
        return 0;
    }
  return required_field_get_packed_size (field, member);
}

static size_t
repeated_field_get_packed_size (const ProtobufCFieldDescriptor *field,
                                size_t count,
                                const void *member)
{
  size_t rv = get_tag_size (field->id) * count;
  unsigned i;
  void *array = * (void * const *) member;
  switch (field->type)
    {
    case PROTOBUF_C_TYPE_SINT32:
      for (i = 0; i < count; i++)
        rv += sint32_size (((int32_t*)array)[i]);
      break;
    case PROTOBUF_C_TYPE_INT32:
      for (i = 0; i < count; i++)
        rv += int32_size (((uint32_t*)array)[i]);
      break;
    case PROTOBUF_C_TYPE_UINT32:
    case PROTOBUF_C_TYPE_ENUM:
      for (i = 0; i < count; i++)
        rv += uint32_size (((uint32_t*)array)[i]);
      break;
    case PROTOBUF_C_TYPE_SINT64:
      for (i = 0; i < count; i++)
        rv += sint64_size (((int64_t*)array)[i]);
      break;
    case PROTOBUF_C_TYPE_INT64:
    case PROTOBUF_C_TYPE_UINT64:
      for (i = 0; i < count; i++)
        rv += uint64_size (((uint64_t*)array)[i]);
      break;
    case PROTOBUF_C_TYPE_SFIXED32:
    case PROTOBUF_C_TYPE_FIXED32:
    case PROTOBUF_C_TYPE_FLOAT:
      rv += 4 * count;
      break;
    case PROTOBUF_C_TYPE_SFIXED64:
    case PROTOBUF_C_TYPE_FIXED64:
    case PROTOBUF_C_TYPE_DOUBLE:
      rv += 8 * count;
      break;
    case PROTOBUF_C_TYPE_BOOL:
      rv += count;
      break;
    case PROTOBUF_C_TYPE_STRING:
      for (i = 0; i < count; i++)
        {
          size_t len = strlen (((char**) array)[i]);
          rv += uint32_size (len) + len;
        }
      break;
      
    case PROTOBUF_C_TYPE_BYTES:
      for (i = 0; i < count; i++)
        {
          size_t len = ((ProtobufCBinaryData*) array)[i].len;
          rv += uint32_size (len) + len;
        }
      break;
    case PROTOBUF_C_TYPE_MESSAGE:
      for (i = 0; i < count; i++)
        {
          size_t len = protobuf_c_message_get_packed_size (((ProtobufCMessage **) array)[i]);
          rv += uint32_size (len) + len;
        }
      break;
    //case PROTOBUF_C_TYPE_GROUP:          // NOT SUPPORTED
    }
  return rv;
}

static inline size_t
unknown_field_get_packed_size (const ProtobufCMessageUnknownField *field)
{
  return get_tag_size (field->tag) + field->len;
}

size_t    protobuf_c_message_get_packed_size(const ProtobufCMessage *message)
{
  unsigned i;
  size_t rv = 0;
  ASSERT_IS_MESSAGE (message);
  for (i = 0; i < message->descriptor->n_fields; i++)
    {
      const ProtobufCFieldDescriptor *field = message->descriptor->fields + i;
      const void *member = ((const char *) message) + field->offset;
      const void *qmember = ((const char *) message) + field->quantifier_offset;

      if (field->label == PROTOBUF_C_LABEL_REQUIRED)
        rv += required_field_get_packed_size (field, member);
      else if (field->label == PROTOBUF_C_LABEL_OPTIONAL)
        rv += optional_field_get_packed_size (field, qmember, member);
      else
        rv += repeated_field_get_packed_size (field, * (const size_t *) qmember, member);
    }
  for (i = 0; i < message->n_unknown_fields; i++)
    rv += unknown_field_get_packed_size (&message->unknown_fields[i]);

  return rv;
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
#if __LITTLE_ENDIAN__
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
#if __LITTLE_ENDIAN__
  memcpy (out, &value, 8);
#else
  fixed32_pack (value, out);
  fixed32_pack (value>>32, out+4);
#endif
  return 8;
}
static inline size_t boolean_pack (protobuf_c_boolean value, uint8_t *out)
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

/* wire-type will be added in required_field_pack() */
static size_t tag_pack (uint32_t id, uint8_t *out)
{
  if (id < (1<<(32-3)))
    return uint32_pack (id<<3, out);
  else
    return uint64_pack (((uint64_t)id) << 3, out);
}
static size_t
required_field_pack (const ProtobufCFieldDescriptor *field,
                     const void *member,
                     uint8_t *out)
{
  size_t rv = tag_pack (field->id, out);
  switch (field->type)
    {
    case PROTOBUF_C_TYPE_SINT32:
      out[0] |= PROTOBUF_C_WIRE_TYPE_VARINT;
      return rv + sint32_pack (*(const int32_t *) member, out + rv);
    case PROTOBUF_C_TYPE_INT32:
      out[0] |= PROTOBUF_C_WIRE_TYPE_VARINT;
      return rv + int32_pack (*(const uint32_t *) member, out + rv);
    case PROTOBUF_C_TYPE_UINT32:
    case PROTOBUF_C_TYPE_ENUM:
      out[0] |= PROTOBUF_C_WIRE_TYPE_VARINT;
      return rv + uint32_pack (*(const uint32_t *) member, out + rv);
    case PROTOBUF_C_TYPE_SINT64:
      out[0] |= PROTOBUF_C_WIRE_TYPE_VARINT;
      return rv + sint64_pack (*(const int64_t *) member, out + rv);
    case PROTOBUF_C_TYPE_INT64:
    case PROTOBUF_C_TYPE_UINT64:
      out[0] |= PROTOBUF_C_WIRE_TYPE_VARINT;
      return rv + uint64_pack (*(const uint64_t *) member, out + rv);
    case PROTOBUF_C_TYPE_SFIXED32:
    case PROTOBUF_C_TYPE_FIXED32:
    case PROTOBUF_C_TYPE_FLOAT:
      out[0] |= PROTOBUF_C_WIRE_TYPE_32BIT;
      return rv + fixed32_pack (*(const uint64_t *) member, out + rv);
    case PROTOBUF_C_TYPE_SFIXED64:
    case PROTOBUF_C_TYPE_FIXED64:
    case PROTOBUF_C_TYPE_DOUBLE:
      out[0] |= PROTOBUF_C_WIRE_TYPE_64BIT;
      return rv + fixed64_pack (*(const uint64_t *) member, out + rv);
    case PROTOBUF_C_TYPE_BOOL:
      out[0] |= PROTOBUF_C_WIRE_TYPE_VARINT;
      return rv + boolean_pack (*(const protobuf_c_boolean *) member, out + rv);
    case PROTOBUF_C_TYPE_STRING:
      {
        out[0] |= PROTOBUF_C_WIRE_TYPE_LENGTH_PREFIXED;
        return rv + string_pack (*(char * const *) member, out + rv);
      }
      
    case PROTOBUF_C_TYPE_BYTES:
      {
        const ProtobufCBinaryData * bd = ((const ProtobufCBinaryData*) member);
        out[0] |= PROTOBUF_C_WIRE_TYPE_LENGTH_PREFIXED;
        return rv + binary_data_pack (bd, out + rv);
      }
    //case PROTOBUF_C_TYPE_GROUP:          // NOT SUPPORTED
    case PROTOBUF_C_TYPE_MESSAGE:
      {
        out[0] |= PROTOBUF_C_WIRE_TYPE_LENGTH_PREFIXED;
        return rv + prefixed_message_pack (*(ProtobufCMessage * const *) member,
                                           out + rv);
      }
    }
  PROTOBUF_C_ASSERT_NOT_REACHED ();
  return 0;
}
static size_t
optional_field_pack (const ProtobufCFieldDescriptor *field,
                     const protobuf_c_boolean *has,
                     const void *member,
                     uint8_t *out)
{
  if (field->type == PROTOBUF_C_TYPE_MESSAGE
   || field->type == PROTOBUF_C_TYPE_STRING)
    {
      const void *ptr = * (const void * const *) member;
      if (ptr == NULL
       || ptr == field->default_value)
        return 0;
    }
  else
    {
      if (!*has)
        return 0;
    }
  return required_field_pack (field, member, out);
}

/* TODO: implement as a table lookup */
static inline size_t sizeof_elt_in_repeated_array (ProtobufCType type)
{
  switch (type)
    {
    case PROTOBUF_C_TYPE_SINT32:
    case PROTOBUF_C_TYPE_INT32:
    case PROTOBUF_C_TYPE_UINT32:
    case PROTOBUF_C_TYPE_SFIXED32:
    case PROTOBUF_C_TYPE_FIXED32:
    case PROTOBUF_C_TYPE_FLOAT:
    case PROTOBUF_C_TYPE_ENUM:
      return 4;
    case PROTOBUF_C_TYPE_SINT64:
    case PROTOBUF_C_TYPE_INT64:
    case PROTOBUF_C_TYPE_UINT64:
    case PROTOBUF_C_TYPE_SFIXED64:
    case PROTOBUF_C_TYPE_FIXED64:
    case PROTOBUF_C_TYPE_DOUBLE:
      return 8;
    case PROTOBUF_C_TYPE_BOOL:
      return sizeof (protobuf_c_boolean);
    case PROTOBUF_C_TYPE_STRING:
    case PROTOBUF_C_TYPE_MESSAGE:
      return sizeof (void *);
    case PROTOBUF_C_TYPE_BYTES:
      return sizeof (ProtobufCBinaryData);
    }
  PROTOBUF_C_ASSERT_NOT_REACHED ();
  return 0;
}
static size_t
repeated_field_pack (const ProtobufCFieldDescriptor *field,
                     size_t count,
                     const void *member,
                     uint8_t *out)
{
  char *array = * (char * const *) member;
  size_t siz;
  unsigned i;
  size_t rv = 0;
  /* CONSIDER: optimize this case a bit (by putting the loop inside the switch) */
  siz = sizeof_elt_in_repeated_array (field->type);
  for (i = 0; i < count; i++)
    {
      rv += required_field_pack (field, array, out + rv);
      array += siz;
    }
  return rv;
}
static size_t
unknown_field_pack (const ProtobufCMessageUnknownField *field, uint8_t *out)
{
  size_t rv = tag_pack (field->tag, out);
  out[0] |= field->wire_type;
  memcpy (out + rv, field->data, field->len);
  return rv + field->len;
}

size_t    protobuf_c_message_pack           (const ProtobufCMessage *message,
                                             uint8_t                *out)
{
  unsigned i;
  size_t rv = 0;
  ASSERT_IS_MESSAGE (message);
  for (i = 0; i < message->descriptor->n_fields; i++)
    {
      const ProtobufCFieldDescriptor *field = message->descriptor->fields + i;
      const void *member = ((const char *) message) + field->offset;
      const void *qmember = ((const char *) message) + field->quantifier_offset;

      if (field->label == PROTOBUF_C_LABEL_REQUIRED)
        rv += required_field_pack (field, member, out + rv);
      else if (field->label == PROTOBUF_C_LABEL_OPTIONAL)
        rv += optional_field_pack (field, qmember, member, out + rv);
      else
        rv += repeated_field_pack (field, * (const size_t *) qmember, member, out + rv);
    }
  for (i = 0; i < message->n_unknown_fields; i++)
    rv += unknown_field_pack (&message->unknown_fields[i], out + rv);
  return rv;
}

/* === pack_to_buffer() === */
static size_t
required_field_pack_to_buffer (const ProtobufCFieldDescriptor *field,
                               const void *member,
                               ProtobufCBuffer *buffer)
{
  size_t rv;
  uint8_t scratch[MAX_UINT64_ENCODED_SIZE * 2];
  rv = tag_pack (field->id, scratch);
  switch (field->type)
    {
    case PROTOBUF_C_TYPE_SINT32:
      scratch[0] |= PROTOBUF_C_WIRE_TYPE_VARINT;
      rv += sint32_pack (*(const int32_t *) member, scratch + rv);
      buffer->append (buffer, rv, scratch);
      break;
    case PROTOBUF_C_TYPE_INT32:
      scratch[0] |= PROTOBUF_C_WIRE_TYPE_VARINT;
      rv += int32_pack (*(const uint32_t *) member, scratch + rv);
      buffer->append (buffer, rv, scratch);
      break;
    case PROTOBUF_C_TYPE_UINT32:
    case PROTOBUF_C_TYPE_ENUM:
      scratch[0] |= PROTOBUF_C_WIRE_TYPE_VARINT;
      rv += uint32_pack (*(const uint32_t *) member, scratch + rv);
      buffer->append (buffer, rv, scratch);
      break;
    case PROTOBUF_C_TYPE_SINT64:
      scratch[0] |= PROTOBUF_C_WIRE_TYPE_VARINT;
      rv += sint64_pack (*(const int64_t *) member, scratch + rv);
      buffer->append (buffer, rv, scratch);
      break;
    case PROTOBUF_C_TYPE_INT64:
    case PROTOBUF_C_TYPE_UINT64:
      scratch[0] |= PROTOBUF_C_WIRE_TYPE_VARINT;
      rv += uint64_pack (*(const uint64_t *) member, scratch + rv);
      buffer->append (buffer, rv, scratch);
      break;
    case PROTOBUF_C_TYPE_SFIXED32:
    case PROTOBUF_C_TYPE_FIXED32:
    case PROTOBUF_C_TYPE_FLOAT:
      scratch[0] |= PROTOBUF_C_WIRE_TYPE_32BIT;
      rv += fixed32_pack (*(const uint64_t *) member, scratch + rv);
      buffer->append (buffer, rv, scratch);
      break;
    case PROTOBUF_C_TYPE_SFIXED64:
    case PROTOBUF_C_TYPE_FIXED64:
    case PROTOBUF_C_TYPE_DOUBLE:
      scratch[0] |= PROTOBUF_C_WIRE_TYPE_64BIT;
      rv += fixed64_pack (*(const uint64_t *) member, scratch + rv);
      buffer->append (buffer, rv, scratch);
      break;
    case PROTOBUF_C_TYPE_BOOL:
      scratch[0] |= PROTOBUF_C_WIRE_TYPE_VARINT;
      rv += boolean_pack (*(const protobuf_c_boolean *) member, scratch + rv);
      buffer->append (buffer, rv, scratch);
      break;
    case PROTOBUF_C_TYPE_STRING:
      {
        size_t sublen = strlen (*(char * const *) member);
        scratch[0] |= PROTOBUF_C_WIRE_TYPE_LENGTH_PREFIXED;
        rv += uint32_pack (sublen, scratch + rv);
        buffer->append (buffer, rv, scratch);
        buffer->append (buffer, sublen, *(uint8_t * const *)member);
        rv += sublen;
        break;
      }
      
    case PROTOBUF_C_TYPE_BYTES:
      {
        const ProtobufCBinaryData * bd = ((const ProtobufCBinaryData*) member);
        size_t sublen = bd->len;
        scratch[0] |= PROTOBUF_C_WIRE_TYPE_LENGTH_PREFIXED;
        rv += uint32_pack (sublen, scratch + rv);
        buffer->append (buffer, rv, scratch);
        buffer->append (buffer, sublen, bd->data);
        rv += sublen;
        break;
      }
    //PROTOBUF_C_TYPE_GROUP,          // NOT SUPPORTED
    case PROTOBUF_C_TYPE_MESSAGE:
      {
        uint8_t simple_buffer_scratch[256];
        size_t sublen;
        ProtobufCBufferSimple simple_buffer
          = PROTOBUF_C_BUFFER_SIMPLE_INIT (simple_buffer_scratch);
        scratch[0] |= PROTOBUF_C_WIRE_TYPE_LENGTH_PREFIXED;
        sublen = protobuf_c_message_pack_to_buffer (*(ProtobufCMessage * const *) member,
                                           &simple_buffer.base);
        rv += uint32_pack (sublen, scratch + rv);
        buffer->append (buffer, rv, scratch);
        buffer->append (buffer, sublen, simple_buffer.data);
        rv += sublen;
        PROTOBUF_C_BUFFER_SIMPLE_CLEAR (&simple_buffer);
        break;
      }
    default:
      PROTOBUF_C_ASSERT_NOT_REACHED ();
    }
  return rv;
}
static size_t
optional_field_pack_to_buffer (const ProtobufCFieldDescriptor *field,
                               const protobuf_c_boolean *has,
                               const void *member,
                               ProtobufCBuffer *buffer)
{
  if (field->type == PROTOBUF_C_TYPE_MESSAGE
   || field->type == PROTOBUF_C_TYPE_STRING)
    {
      const void *ptr = * (const void * const *) member;
      if (ptr == NULL
       || ptr == field->default_value)
        return 0;
    }
  else
    {
      if (!*has)
        return 0;
    }
  return required_field_pack_to_buffer (field, member, buffer);
}

static size_t
repeated_field_pack_to_buffer (const ProtobufCFieldDescriptor *field,
                               unsigned count,
                               const void *member,
                               ProtobufCBuffer *buffer)
{
  char *array = * (char * const *) member;
  size_t siz;
  unsigned i;
  /* CONSIDER: optimize this case a bit (by putting the loop inside the switch) */
  unsigned rv = 0;
  siz = sizeof_elt_in_repeated_array (field->type);
  for (i = 0; i < count; i++)
    {
      rv += required_field_pack_to_buffer (field, array, buffer);
      array += siz;
    }
  return rv;
}

static size_t
unknown_field_pack_to_buffer (const ProtobufCMessageUnknownField *field,
                              ProtobufCBuffer *buffer)
{
  uint8_t header[MAX_UINT64_ENCODED_SIZE];
  size_t rv = tag_pack (field->tag, header);
  header[0] |= field->wire_type;
  buffer->append (buffer, rv, header);
  buffer->append (buffer, field->len, field->data);
  return rv + field->len;
}

size_t
protobuf_c_message_pack_to_buffer (const ProtobufCMessage *message,
                                   ProtobufCBuffer  *buffer)
{
  unsigned i;
  size_t rv = 0;
  ASSERT_IS_MESSAGE (message);
  for (i = 0; i < message->descriptor->n_fields; i++)
    {
      const ProtobufCFieldDescriptor *field = message->descriptor->fields + i;
      const void *member = ((const char *) message) + field->offset;
      const void *qmember = ((const char *) message) + field->quantifier_offset;

      if (field->label == PROTOBUF_C_LABEL_REQUIRED)
        rv += required_field_pack_to_buffer (field, member, buffer);
      else if (field->label == PROTOBUF_C_LABEL_OPTIONAL)
        rv += optional_field_pack_to_buffer (field, qmember, member, buffer);
      else
        rv += repeated_field_pack_to_buffer (field, * (const size_t *) qmember, member, buffer);
    }
  for (i = 0; i < message->n_unknown_fields; i++)
    rv += unknown_field_pack_to_buffer (&message->unknown_fields[i], buffer);

  return rv;
}

/* === unpacking === */
#if PRINT_UNPACK_ERRORS
# define UNPACK_ERROR(args)  do { printf args;printf("\n"); }while(0)
#else
# define UNPACK_ERROR(args)  do { } while (0)
#endif

static inline int
int_range_lookup (unsigned n_ranges,
                  const ProtobufCIntRange *ranges,
                  int value)
{
  unsigned start, n;
  if (n_ranges == 0)
    return -1;
  start = 0;
  n = n_ranges;
  while (n > 1)
    {
      unsigned mid = start + n / 2;
      if (value < ranges[mid].start_value)
        {
          n = mid - start;
        }
      else if (value >= ranges[mid].start_value + (int)(ranges[mid+1].orig_index-ranges[mid].orig_index))
        {
          unsigned new_start = mid + 1;
          n = start + n - new_start;
          start = new_start;
        }
      else
        return (value - ranges[mid].start_value) + ranges[mid].orig_index;
    }
  if (n > 0)
    {
      unsigned start_orig_index = ranges[start].orig_index;
      unsigned range_size = ranges[start+1].orig_index - start_orig_index;

      if (ranges[start].start_value <= value
       && value < (int)(ranges[start].start_value + range_size))
        return (value - ranges[start].start_value) + start_orig_index;
    }
  return -1;
}

static size_t
parse_tag_and_wiretype (size_t len,
                        const uint8_t *data,
                        uint32_t *tag_out,
                        ProtobufCWireType *wiretype_out)
{
  unsigned max_rv = len > 5 ? 5 : len;
  uint32_t tag = (data[0]&0x7f) >> 3;
  unsigned shift = 4;
  unsigned rv;
  *wiretype_out = data[0] & 7;
  if ((data[0] & 0x80) == 0)
    {
      *tag_out = tag;
      return 1;
    }
  for (rv = 1; rv < max_rv; rv++)
    if (data[rv] & 0x80)
      {
        tag |= (data[rv] & 0x7f) << shift;
        shift += 7;
      }
    else
      {
        tag |= data[rv] << shift;
        *tag_out = tag;
        return rv + 1;
      }
  return 0;                   /* error: bad header */
}

typedef struct _ScannedMember ScannedMember;
struct _ScannedMember
{
  uint32_t tag;
  const ProtobufCFieldDescriptor *field;
  uint8_t wire_type;
  uint8_t length_prefix_len;
  size_t len;
  const uint8_t *data;
};

#define MESSAGE_GET_UNKNOWNS(message) \
  STRUCT_MEMBER_PTR (ProtobufCMessageUnknownFieldArray, \
                     (message), (message)->descriptor->unknown_field_array_offset)

static inline uint32_t
scan_length_prefixed_data (size_t len, const uint8_t *data, size_t *prefix_len_out)
{
  unsigned hdr_max = len < 5 ? len : 5;
  unsigned hdr_len;
  uint32_t val = 0;
  unsigned i;
  unsigned shift = 0;
  for (i = 0; i < hdr_max; i++)
    {
      val |= (data[i] & 0x7f) << shift;
      shift += 7;
      if ((data[i] & 0x80) == 0)
        break;
    }
  if (i == hdr_max)
    {
      UNPACK_ERROR (("error parsing length for length-prefixed data"));
      return 0;
    }
  hdr_len = i + 1;
  *prefix_len_out = hdr_len;
  if (hdr_len + val > len)
    {
      UNPACK_ERROR (("data too short after length-prefix of %u",
                     val));
      return 0;
    }
  return hdr_len + val;
}

static inline uint32_t
parse_uint32 (unsigned len, const uint8_t *data)
{
  unsigned rv = data[0] & 0x7f;
  if (len > 1)
    {
      rv |= ((data[1] & 0x7f) << 7);
      if (len > 2)
        {
          rv |= ((data[2] & 0x7f) << 14);
          if (len > 3)
            {
              rv |= ((data[3] & 0x7f) << 21);
              if (len > 4)
                rv |= (data[4] << 28);
            }
        }
    }
  return rv;
}
static inline uint32_t
parse_int32 (unsigned len, const uint8_t *data)
{
  return parse_uint32 (len, data);
}
static inline int32_t
unzigzag32 (uint32_t v)
{
  if (v&1)
    return -(v>>1) - 1;
  else
    return v>>1;
}
static inline uint32_t
parse_fixed_uint32 (const uint8_t *data)
{
#if __LITTLE_ENDIAN__
  uint32_t t;
  memcpy (&t, data, 4);
  return t;
#else
  return data[0] | (data[1] << 8) | (data[2] << 16) | (data[3] << 24);
#endif
}
static uint64_t
parse_uint64 (unsigned len, const uint8_t *data)
{
  unsigned shift, i;
  if (len < 5)
    return parse_uint32 (len, data);
  uint64_t rv = ((data[0] & 0x7f))
              | ((data[1] & 0x7f)<<7)
              | ((data[2] & 0x7f)<<14)
              | ((data[3] & 0x7f)<<21);
  shift = 28;
  for (i = 4; i < len; i++)
    {
      rv |= (((uint64_t)(data[i]&0x7f)) << shift);
      shift += 7;
    }
  return rv;
}
static inline int64_t
unzigzag64 (uint64_t v)
{
  if (v&1)
    return -(v>>1) - 1;
  else
    return v>>1;
}
static inline uint64_t
parse_fixed_uint64 (const uint8_t *data)
{
#if __LITTLE_ENDIAN__
  uint64_t t;
  memcpy (&t, data, 8);
  return t;
#else
  return (uint64_t)parse_fixed_uint32 (data)
      | (((uint64_t)parse_fixed_uint32(data+4)) << 32);
#endif
}
static protobuf_c_boolean
parse_boolean (unsigned len, const uint8_t *data)
{
  unsigned i;
  for (i = 0; i < len; i++)
    if (data[i] & 0x7f)
      return 1;
  return 0;
}
static protobuf_c_boolean
parse_required_member (ScannedMember *scanned_member,
                       void *member,
                       ProtobufCAllocator *allocator,
                       protobuf_c_boolean maybe_clear)
{
  unsigned len = scanned_member->len;
  const uint8_t *data = scanned_member->data;
  ProtobufCWireType wire_type = scanned_member->wire_type;
  switch (scanned_member->field->type)
    {
    case PROTOBUF_C_TYPE_INT32:
      if (wire_type != PROTOBUF_C_WIRE_TYPE_VARINT)
        return 0;
      *(uint32_t*)member = parse_int32 (len, data);
      return 1;
    case PROTOBUF_C_TYPE_UINT32:
      if (wire_type != PROTOBUF_C_WIRE_TYPE_VARINT)
        return 0;
      *(uint32_t*)member = parse_uint32 (len, data);
      return 1;
    case PROTOBUF_C_TYPE_SINT32:
      if (wire_type != PROTOBUF_C_WIRE_TYPE_VARINT)
        return 0;
      *(int32_t*)member = unzigzag32 (parse_uint32 (len, data));
      return 1;
    case PROTOBUF_C_TYPE_SFIXED32:
    case PROTOBUF_C_TYPE_FIXED32:
    case PROTOBUF_C_TYPE_FLOAT:
      if (wire_type != PROTOBUF_C_WIRE_TYPE_32BIT)
        return 0;
      *(uint32_t*)member = parse_fixed_uint32 (data);
      return 1;

    case PROTOBUF_C_TYPE_INT64:
    case PROTOBUF_C_TYPE_UINT64:
      if (wire_type != PROTOBUF_C_WIRE_TYPE_VARINT)
        return 0;
      *(uint64_t*)member = parse_uint64 (len, data);
      return 1;
    case PROTOBUF_C_TYPE_SINT64:
      if (wire_type != PROTOBUF_C_WIRE_TYPE_VARINT)
        return 0;
      *(int64_t*)member = unzigzag64 (parse_uint64 (len, data));
      return 1;
    case PROTOBUF_C_TYPE_SFIXED64:
    case PROTOBUF_C_TYPE_FIXED64:
    case PROTOBUF_C_TYPE_DOUBLE:
      if (wire_type != PROTOBUF_C_WIRE_TYPE_64BIT)
        return 0;
      *(uint64_t*)member = parse_fixed_uint64 (data);
      return 1;

    case PROTOBUF_C_TYPE_BOOL:
      *(protobuf_c_boolean*)member = parse_boolean (len, data);
      return 1;

    case PROTOBUF_C_TYPE_ENUM:
      if (wire_type != PROTOBUF_C_WIRE_TYPE_VARINT)
        return 0;
      *(uint32_t*)member = parse_uint32 (len, data);
      return 1;

    case PROTOBUF_C_TYPE_STRING:
      if (wire_type != PROTOBUF_C_WIRE_TYPE_LENGTH_PREFIXED)
        return 0;
      {
        char **pstr = member;
        unsigned pref_len = scanned_member->length_prefix_len;
        if (maybe_clear && *pstr != NULL)
          {
            const char *def = scanned_member->field->default_value;
            if (*pstr != NULL && *pstr != def)
              FREE (allocator, *pstr);
          }
        *pstr = ALLOC (allocator, len - pref_len + 1);
        memcpy (*pstr, data + pref_len, len - pref_len);
        (*pstr)[len-pref_len] = 0;
        return 1;
      }
    case PROTOBUF_C_TYPE_BYTES:
      if (wire_type != PROTOBUF_C_WIRE_TYPE_LENGTH_PREFIXED)
        return 0;
      {
        ProtobufCBinaryData *bd = member;
        const ProtobufCBinaryData *def_bd;
        unsigned pref_len = scanned_member->length_prefix_len;
        def_bd = scanned_member->field->default_value;
        if (maybe_clear && bd->data != NULL && bd->data != def_bd->data)
          FREE (allocator, bd->data);
        bd->data = ALLOC (allocator, len - pref_len);
        memcpy (bd->data, data + pref_len, len - pref_len);
        bd->len = len - pref_len;
        return 1;
      }
    //case PROTOBUF_C_TYPE_GROUP,          // NOT SUPPORTED
    case PROTOBUF_C_TYPE_MESSAGE:
      if (wire_type != PROTOBUF_C_WIRE_TYPE_LENGTH_PREFIXED)
        return 0;
      {
        ProtobufCMessage **pmessage = member;
        ProtobufCMessage *subm;
        const ProtobufCMessage *def_mess;
        unsigned pref_len = scanned_member->length_prefix_len;
        def_mess = scanned_member->field->default_value;
        if (maybe_clear && *pmessage != NULL && *pmessage != def_mess)
          protobuf_c_message_free_unpacked (*pmessage, allocator);
        subm = protobuf_c_message_unpack (scanned_member->field->descriptor,
                                          allocator,
                                          len - pref_len, data + pref_len);
        *pmessage = subm;
        if (subm == NULL)
          return 0;
        return 1;
      }
    }
  return 0;
}

static protobuf_c_boolean
parse_optional_member (ScannedMember *scanned_member,
                       void *member,
                       ProtobufCMessage *message,
                       ProtobufCAllocator *allocator)
{
  if (!parse_required_member (scanned_member, member, allocator, TRUE))
    return 0;
  if (scanned_member->field->quantifier_offset != 0)
    STRUCT_MEMBER (protobuf_c_boolean,
                   message,
                   scanned_member->field->quantifier_offset) = 1;
  return 1;
}

static protobuf_c_boolean
parse_repeated_member (ScannedMember *scanned_member,
                       void *member,
                       ProtobufCMessage *message,
                       ProtobufCAllocator *allocator)
{
  const ProtobufCFieldDescriptor *field = scanned_member->field;
  size_t *p_n = STRUCT_MEMBER_PTR(size_t, message, field->quantifier_offset);
  size_t siz = sizeof_elt_in_repeated_array (field->type);
  char *array = *(char**)member;
  if (!parse_required_member (scanned_member,
                              array + siz * (*p_n),
                              allocator,
                              FALSE))
    return 0;
  *p_n += 1;
  return 1;
}

static protobuf_c_boolean
parse_member (ScannedMember *scanned_member,
              ProtobufCMessage *message,
              ProtobufCAllocator *allocator)
{
  const ProtobufCFieldDescriptor *field = scanned_member->field;
  void *member;
  if (field == NULL)
    {
      ProtobufCMessageUnknownField *ufield = message->unknown_fields + (message->n_unknown_fields++);
      ufield->tag = scanned_member->tag;
      ufield->wire_type = scanned_member->wire_type;
      ufield->len = scanned_member->len;
      ufield->data = UNALIGNED_ALLOC (allocator, scanned_member->len);
      memcpy (ufield->data, scanned_member->data, ufield->len);
      return 1;
    }
  member = (char*)message + field->offset;
  switch (field->label)
    {
    case PROTOBUF_C_LABEL_REQUIRED:
      return parse_required_member (scanned_member, member, allocator, TRUE);
    case PROTOBUF_C_LABEL_OPTIONAL:
      return parse_optional_member (scanned_member, member, message, allocator);
    case PROTOBUF_C_LABEL_REPEATED:
      return parse_repeated_member (scanned_member, member, message, allocator);
    }
  PROTOBUF_C_ASSERT_NOT_REACHED ();
  return 0;
}

static inline void
setup_default_values (ProtobufCMessage *message)
{
  const ProtobufCMessageDescriptor *desc = message->descriptor;
  unsigned i;
  for (i = 0; i < desc->n_fields; i++)
    if (desc->fields[i].default_value != NULL
     && desc->fields[i].label != PROTOBUF_C_LABEL_REPEATED)
      {
        void *field = STRUCT_MEMBER_P (message, desc->fields[i].offset);
        const void *dv = desc->fields[i].default_value;
        switch (desc->fields[i].type)
        {
        case PROTOBUF_C_TYPE_INT32:
        case PROTOBUF_C_TYPE_SINT32:
        case PROTOBUF_C_TYPE_SFIXED32:
        case PROTOBUF_C_TYPE_UINT32:
        case PROTOBUF_C_TYPE_FIXED32:
        case PROTOBUF_C_TYPE_FLOAT:
        case PROTOBUF_C_TYPE_ENUM:
          memcpy (field, dv, 4);
          break;

        case PROTOBUF_C_TYPE_INT64:
        case PROTOBUF_C_TYPE_SINT64:
        case PROTOBUF_C_TYPE_SFIXED64:
        case PROTOBUF_C_TYPE_UINT64:
        case PROTOBUF_C_TYPE_FIXED64:
        case PROTOBUF_C_TYPE_DOUBLE:
          memcpy (field, dv, 8);
          break;

        case PROTOBUF_C_TYPE_BOOL:
          memcpy (field, dv, sizeof (protobuf_c_boolean));
          break;

        case PROTOBUF_C_TYPE_BYTES:
          memcpy (field, dv, sizeof (ProtobufCBinaryData));
          break;

        case PROTOBUF_C_TYPE_STRING:
        case PROTOBUF_C_TYPE_MESSAGE:
          /* the next line essentially implements a cast from const,
             which is totally unavoidable. */
          *(const void**)field = dv;
          break;
        }
      }
}

ProtobufCMessage *
protobuf_c_message_unpack         (const ProtobufCMessageDescriptor *desc,
                                   ProtobufCAllocator  *allocator,
                                   size_t               len,
                                   const uint8_t       *data)
{
  ProtobufCMessage *rv;
  size_t rem = len;
  const uint8_t *at = data;
  const ProtobufCFieldDescriptor *last_field = desc->fields + 0;
  ScannedMember first_member_slab[16];
  ScannedMember *scanned_member_slabs[30];               /* size of member i is (1<<(i+4)) */
  unsigned which_slab = 0;
  unsigned in_slab_index = 0;
  size_t n_unknown = 0;
  unsigned f;
  unsigned i_slab;

  ASSERT_IS_MESSAGE_DESCRIPTOR (desc);

  if (allocator == NULL)
    allocator = &protobuf_c_default_allocator;
  rv = ALLOC (allocator, desc->sizeof_message);
  scanned_member_slabs[0] = first_member_slab;

  memset (rv, 0, desc->sizeof_message);
  rv->descriptor = desc;

  setup_default_values (rv);

  while (rem > 0)
    {
      uint32_t tag = 0; // landonf - 12/17/2008 (uninitialized compiler warning)
      ProtobufCWireType wire_type;
      size_t used = parse_tag_and_wiretype (rem, at, &tag, &wire_type);
      const ProtobufCFieldDescriptor *field;
      ScannedMember tmp;
      memset(&tmp, 0, sizeof(tmp)); // landonf - 12/17/2008 (uninitialized compiler warning)
      if (used == 0)
        {
          UNPACK_ERROR (("error parsing tag/wiretype at offset %u",
                         (unsigned)(at-data)));
          goto error_cleanup;
        }
      /* XXX: consider optimizing for field[1].id == tag, if field[1] exists! */
      if (last_field->id != tag)
        {
          /* lookup field */
          int field_index = int_range_lookup (desc->n_field_ranges,
                                              desc->field_ranges,
                                              tag);
          if (field_index < 0)
            {
              field = NULL;
              n_unknown++;
            }
          else
            {
              field = desc->fields + field_index;
              last_field = field;
            }
        }
      else
        field = last_field;

      at += used;
      rem -= used;
      tmp.tag = tag;
      tmp.wire_type = wire_type;
      tmp.field = field;
      tmp.data = at;
      switch (wire_type)
        {
        case PROTOBUF_C_WIRE_TYPE_VARINT:
          {
            unsigned max_len = rem < 10 ? rem : 10;
            unsigned i;
            for (i = 0; i < max_len; i++)
              if ((at[i] & 0x80) == 0)
                break;
            if (i == max_len)
              {
                UNPACK_ERROR (("unterminated varint at offset %u",
                               (unsigned)(at-data)));
                goto error_cleanup;
              }
            tmp.len = i + 1;
          }
          break;
        case PROTOBUF_C_WIRE_TYPE_64BIT:
          if (rem < 8)
            {
              UNPACK_ERROR (("too short after 64bit wiretype at offset %u",
                             (unsigned)(at-data)));
              goto error_cleanup;
            }
          tmp.len = 8;
          break;
        case PROTOBUF_C_WIRE_TYPE_LENGTH_PREFIXED:
          {
            size_t pref_len = 0; // landonf - 12/17/2008 (uninitialized compiler warning)
            tmp.len = scan_length_prefixed_data (rem, at, &pref_len);
            if (tmp.len == 0)
              {
                /* NOTE: scan_length_prefixed_data calls UNPACK_ERROR */
                goto error_cleanup;
              }
            tmp.length_prefix_len = pref_len;
            break;
          }
        case PROTOBUF_C_WIRE_TYPE_START_GROUP:
        case PROTOBUF_C_WIRE_TYPE_END_GROUP:
          // landonf - 12/18/2008 (Do not assert unreachable if a group tag is found, as this
          // crashed the decoder on valid input)
          UNPACK_ERROR (("unsupported group tag at offset %u",
                         (unsigned)(at-data)));
          goto error_cleanup;
        case PROTOBUF_C_WIRE_TYPE_32BIT:
          if (rem < 4)
            {
              UNPACK_ERROR (("too short after 32bit wiretype at offset %u",
                             (unsigned)(at-data)));
              goto error_cleanup;
            }
          tmp.len = 4;
          break;
        }
      if (in_slab_index == (1U<<(which_slab+4)))
        {
          size_t size;
          in_slab_index = 0;
          which_slab++;
          size = sizeof(ScannedMember) << (which_slab+4);
          /* TODO: consider using alloca() ! */
          if (allocator->tmp_alloc != NULL)
            scanned_member_slabs[which_slab] = TMPALLOC(allocator, size);
          else
            scanned_member_slabs[which_slab] = ALLOC(allocator, size);
        }
      scanned_member_slabs[which_slab][in_slab_index++] = tmp;

      if (field != NULL && field->label == PROTOBUF_C_LABEL_REPEATED)
        {
          size_t *n_ptr = (size_t*)((char*)rv + field->quantifier_offset);
          (*n_ptr) += 1;
        }

      at += tmp.len;
      rem -= tmp.len;
    }

  /* allocate space for repeated fields */
  for (f = 0; f < desc->n_fields; f++)
    if (desc->fields[f].label == PROTOBUF_C_LABEL_REPEATED)
      {
        const ProtobufCFieldDescriptor *field = desc->fields + f;
        size_t siz = sizeof_elt_in_repeated_array (field->type);
        size_t *n_ptr = STRUCT_MEMBER_PTR (size_t, rv, field->quantifier_offset);
        if (*n_ptr != 0)
          {
            STRUCT_MEMBER (void *, rv, field->offset) = ALLOC (allocator, siz * (*n_ptr));
            *n_ptr = 0;
          }
      }

  /* allocate space for unknown fields */
  if (n_unknown)
    {
      rv->unknown_fields = ALLOC (allocator, n_unknown * sizeof (ProtobufCMessageUnknownField));
    }

  /* do real parsing */
  for (i_slab = 0; i_slab <= which_slab; i_slab++)
    {
      unsigned max = (i_slab == which_slab) ? in_slab_index : (1U<<(i_slab+4));
      ScannedMember *slab = scanned_member_slabs[i_slab];
      unsigned j;
      for (j = 0; j < max; j++)
        {
          if (!parse_member (slab + j, rv, allocator))
            {
              UNPACK_ERROR (("error parsing member %s of %s",
                             slab->field ? slab->field->name : "(unknown)", desc->name));
              goto error_cleanup;
            }
        }
    }

  /* cleanup */
  if (allocator->tmp_alloc == NULL)
    {
      unsigned j;
      for (j = 1; j <= which_slab; j++)
        FREE (allocator, scanned_member_slabs[j]);
    }

  return rv;

error_cleanup:
  protobuf_c_message_free_unpacked (rv, allocator);
  if (allocator->tmp_alloc == NULL)
    {
      unsigned j;
      for (j = 1; j <= which_slab; j++)
        FREE (allocator, scanned_member_slabs[j]);
    }
  return NULL;
}

/* === free_unpacked === */
void     
protobuf_c_message_free_unpacked  (ProtobufCMessage    *message,
                                   ProtobufCAllocator  *allocator)
{
  const ProtobufCMessageDescriptor *desc = message->descriptor;
  unsigned f;
  ASSERT_IS_MESSAGE (message);
  if (allocator == NULL)
    allocator = &protobuf_c_default_allocator;
  message->descriptor = NULL;
  for (f = 0; f < desc->n_fields; f++)
    {
      if (desc->fields[f].label == PROTOBUF_C_LABEL_REPEATED)
        {
          size_t n = STRUCT_MEMBER (size_t, message, desc->fields[f].quantifier_offset);
          void * arr = STRUCT_MEMBER (void *, message, desc->fields[f].offset);
          if (desc->fields[f].type == PROTOBUF_C_TYPE_STRING && arr != NULL)
            {
              unsigned i;
              for (i = 0; i < n; i++)
                FREE (allocator, ((char**)arr)[i]);
            }
          else if (desc->fields[f].type == PROTOBUF_C_TYPE_BYTES && arr != NULL)
            {
              unsigned i;
              for (i = 0; i < n; i++)
                FREE (allocator, ((ProtobufCBinaryData*)arr)[i].data);
            }
          else if (desc->fields[f].type == PROTOBUF_C_TYPE_MESSAGE && arr != NULL)
            {
              unsigned i;
              for (i = 0; i < n; i++)
                protobuf_c_message_free_unpacked (((ProtobufCMessage**)arr)[i], allocator);
            }
          if (arr != NULL)
            FREE (allocator, arr);
        }
      else if (desc->fields[f].type == PROTOBUF_C_TYPE_STRING)
        {
          char *str = STRUCT_MEMBER (char *, message, desc->fields[f].offset);
          if (str && str != desc->fields[f].default_value)
            FREE (allocator, str);
        }
      else if (desc->fields[f].type == PROTOBUF_C_TYPE_BYTES)
        {
          void *data = STRUCT_MEMBER (ProtobufCBinaryData, message, desc->fields[f].offset).data;
          const ProtobufCBinaryData *default_bd;
          default_bd = desc->fields[f].default_value;
          if (data != NULL
           && (default_bd == NULL || default_bd->data != data))
            FREE (allocator, data);
        }
      else if (desc->fields[f].type == PROTOBUF_C_TYPE_MESSAGE)
        {
          ProtobufCMessage *sm;
          sm = STRUCT_MEMBER (ProtobufCMessage *, message,desc->fields[f].offset);
          if (sm && sm != desc->fields[f].default_value)
            protobuf_c_message_free_unpacked (sm, allocator);
        }
    }
  FREE (allocator, message);
}

/* === services === */
typedef struct _ServiceMachgen ServiceMachgen;
struct _ServiceMachgen
{
  ProtobufCService base;
  void *service;
};

typedef void (*DestroyHandler)(void *service);
typedef void (*GenericHandler)(void *service,
                               const ProtobufCMessage *input,
                               ProtobufCClosure  closure,
                               void             *closure_data);
static void 
service_machgen_invoke(ProtobufCService *service,
                       unsigned          method_index,
                       const ProtobufCMessage *input,
                       ProtobufCClosure  closure,
                       void             *closure_data)
{
  GenericHandler *handlers;
  GenericHandler handler;
  ServiceMachgen *machgen = (ServiceMachgen *) service;
  PROTOBUF_C_ASSERT (method_index < service->descriptor->n_methods);
  handlers = (GenericHandler *) machgen->service;
  handler = handlers[method_index];
  (*handler) (machgen->service, input, closure, closure_data);
}

void
protobuf_c_service_generated_init (ProtobufCService *service,
                                   const ProtobufCServiceDescriptor *descriptor,
                                   ProtobufCServiceDestroy destroy)
{
  ASSERT_IS_SERVICE_DESCRIPTOR(descriptor);
  service->descriptor = descriptor;
  service->destroy = destroy;
  service->invoke = service_machgen_invoke;
  memset (service + 1, 0, descriptor->n_methods * sizeof (GenericHandler));
}

void protobuf_c_service_destroy (ProtobufCService *service)
{
  service->destroy (service);
}

/* --- querying the descriptors --- */
const ProtobufCEnumValue *
protobuf_c_enum_descriptor_get_value_by_name 
                         (const ProtobufCEnumDescriptor    *desc,
                          const char                       *name)
{
  unsigned start = 0, count = desc->n_value_names;
  while (count > 1)
    {
      unsigned mid = start + count / 2;
      int rv = strcmp (desc->values_by_name[mid].name, name);
      if (rv == 0)
        return desc->values + desc->values_by_name[mid].index;
      else if (rv < 0)
        {
          count = start + count - (mid - 1);
          start = mid + 1;
        }
      else
        count = mid - start;
    }
  if (count == 0)
    return NULL;
  if (strcmp (desc->values_by_name[start].name, name) == 0)
    return desc->values + desc->values_by_name[start].index;
  return NULL;
}
const ProtobufCEnumValue *
protobuf_c_enum_descriptor_get_value        
                         (const ProtobufCEnumDescriptor    *desc,
                          int                               value)
{
  int rv = int_range_lookup (desc->n_value_ranges, desc->value_ranges, value);
  if (rv < 0)
    return NULL;
  return desc->values + rv;
}

const ProtobufCFieldDescriptor *
protobuf_c_message_descriptor_get_field_by_name
                         (const ProtobufCMessageDescriptor *desc,
                          const char                       *name)
{
  unsigned start = 0, count = desc->n_fields;
  const ProtobufCFieldDescriptor *field;
  while (count > 1)
    {
      unsigned mid = start + count / 2;
      int rv;
      field = desc->fields + desc->fields_sorted_by_name[mid];
      rv = strcmp (field->name, name);
      if (rv == 0)
        return field;
      else if (rv < 0)
        {
          count = start + count - (mid + 1);
          start = mid + 1;
        }
      else
        count = mid - start;
    }
  if (count == 0)
    return NULL;
  field = desc->fields + desc->fields_sorted_by_name[start];
  if (strcmp (field->name, name) == 0)
    return field;
  return NULL;
}

const ProtobufCFieldDescriptor *
protobuf_c_message_descriptor_get_field        
                         (const ProtobufCMessageDescriptor *desc,
                          unsigned                          value)
{
  int rv = int_range_lookup (desc->n_field_ranges,
                             desc->field_ranges,
                             value);
  if (rv < 0)
    return NULL;
  return desc->fields + rv;
}

const ProtobufCMethodDescriptor *
protobuf_c_service_descriptor_get_method_by_name
                         (const ProtobufCServiceDescriptor *desc,
                          const char                       *name)
{
  unsigned start = 0, count = desc->n_methods;
  while (count > 1)
    {
      unsigned mid = start + count / 2;
      int rv = strcmp (desc->methods[mid].name, name);
      if (rv == 0)
        return desc->methods + mid;
      if (rv < 0)
        {
          count = start + count - (mid - 1);
          start = mid + 1;
        }
      else
        {
          count = mid - start;
        }
    }
  if (count == 0)
    return NULL;
  if (strcmp (desc->methods[start].name, name) == 0)
    return desc->methods + start;
  return NULL;
}
