/* --- protobuf-c-private.h: private structures and functions --- */
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


/* === needs to be declared for the PROTOBUF_C_BUFFER_SIMPLE_INIT macro === */

void protobuf_c_buffer_simple_append (ProtobufCBuffer *buffer,
                                      size_t           len,
                                      const unsigned char *data);

/* === stuff which needs to be declared for use in the generated code === */

struct _ProtobufCEnumValueIndex
{
  const char *name;
  unsigned index;               /* into values[] array */
};

/* IntRange: helper structure for optimizing
     int => index lookups
   in the case where the keys are mostly consecutive values,
   as they presumably are for enums and fields.

   The data structures assumes that the values in the original
   array are sorted */
struct _ProtobufCIntRange
{
  int start_value;
  unsigned orig_index;
  /* NOTE: the number of values in the range can
     be inferred by looking at the next element's orig_index.
     a dummy element is added to make this simple */
};


/* === declared for exposition on ProtobufCIntRange === */
/* note: ranges must have an extra sentinel IntRange at the end whose
   orig_index is set to the number of actual values in the original array */
/* returns -1 if no orig_index found */
int protobuf_c_int_ranges_lookup (unsigned n_ranges,
                                  ProtobufCIntRange *ranges);

#define PROTOBUF_C_SERVICE_DESCRIPTOR_MAGIC  0x14159bc3
#define PROTOBUF_C_MESSAGE_DESCRIPTOR_MAGIC  0x28aaeef9
#define PROTOBUF_C_ENUM_DESCRIPTOR_MAGIC     0x114315af

/* === behind the scenes on the generated service's __init functions */
typedef void (*ProtobufCServiceDestroy) (ProtobufCService *service);
void
protobuf_c_service_generated_init (ProtobufCService *service,
                                   const ProtobufCServiceDescriptor *descriptor,
                                   ProtobufCServiceDestroy destroy);

