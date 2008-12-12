/*
 * Author: Landon Fuller <landonf@plausiblelabs.com>
 *
 * Copyright (c) 2008 Plausible Labs Cooperative, Inc.
 * All rights reserved.
 */

#import "PLCrashAsync.h"

#import <google/protobuf-c/protobuf-c.h>

size_t plcrash_writer_pack (plasync_file_t *file, uint32_t field_id, ProtobufCType field_type, const void *value);