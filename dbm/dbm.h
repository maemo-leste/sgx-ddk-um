/*
 * dbm.h
 *
 * Copyright (C) 2022 Ivaylo Dimitrov <ivo.g.dimitrov.75@gmail.com>
 *
 * This library is free software: you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library. If not, see <https://www.gnu.org/licenses/>.
 *
 */

#ifndef __DBM_H__
#define __DBM_H__

#include <drm.h>
#include <gbm.h>

#include <pthread.h>
#include <stdint.h>
#include <stdbool.h>

typedef struct _dbm_device dbm_device;
typedef struct _dbm_buffer dbm_buffer;

typedef struct
{
  int (*cpu_access_prepare)(dbm_buffer *buf);
  void (*cpu_access_finish)(dbm_buffer *buf, bool, bool);
  void (*destroy)(dbm_buffer *buf);
} dbm_buffer_functions;

struct _dbm_buffer
{
  dbm_device *dev;
  dbm_buffer_functions *funcs;
  uint32_t handle;
  uint32_t size;
  uint32_t name;
  void *ptr;
  bool flag1;
  bool flag2;
  void *user_data;
  void (*closure)(void *);
};


typedef struct
{
  uint32_t fourcc;
  enum gbm_bo_format format;
  uint32_t allowed_flags;
} dbm_format;

typedef struct
{
  void (*destroy)();
  int (*get_buffer_stride_and_size)(dbm_device *dev,
                                    uint32_t bpp,
                                    uint32_t width,
                                    uint32_t height,
                                    uint32_t unk1,
                                    uint32_t flags,
                                    uint32_t *stride,
                                    uint32_t *size);
  int (*buffer_create)(dbm_device *dev,
                       uint32_t size,
                       uint32_t flags,
                       dbm_buffer **buf);
  int (*buffer_create_from_handle)(dbm_device *dev,
                                   uint32_t handle,
                                   uint32_t size,
                                   dbm_buffer **buf);
} dbm_device_functions;

struct _dbm_device
{
  int fd;
  uint32_t format_count;
  dbm_format *formats;
  dbm_device_functions *funcs;
  pthread_mutex_t mutex;
  uint32_t num_handles;
  int *handle_ref;
};

#include "dbm_helpers.h"

#endif /* __DBM_H__ */
