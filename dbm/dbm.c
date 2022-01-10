/*
 * dbm.c
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

#include "config.h"

#include <xf86drm.h>

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/mman.h>
#include <sys/fcntl.h>
#include <sys/types.h>
#include <unistd.h>

/* #define DEBUG_TRACE */

#ifdef DEBUG_TRACE
#include <stdio.h>
#define TRACE(x, ...) printf(x, __VA_ARGS__)
#else
#define TRACE(x, ...)
#endif

#include "dbm.h"


static inline
uint32_t align(uint32_t v, int a)
{
  return (v + (a - 1)) & ~(a - 1);
}

inline static void
handle_lock(dbm_device *dev)
{
  int err = pthread_mutex_lock(&dev->mutex);

  TRACE("%s %p\n", __func__, (void *)&dev->mutex);
  assert(!err);
}

inline static void
handle_unlock(dbm_device *dev)
{
  int err = pthread_mutex_unlock(&dev->mutex);
  TRACE("%s %p\n", __func__, (void *)&dev->mutex);
  assert(!err);
}

/* dbm_buffer implemantation */
static int
buffer_cpu_access_prepare(dbm_buffer *buf)
{
  struct drm_mode_map_dumb map_req;
  void *ptr;

  memset(&map_req, 0, sizeof(map_req));

  map_req.handle = buf->handle;
  map_req.offset = 0LL;

  if (drmIoctl(buf->dev->fd, DRM_IOCTL_MODE_MAP_DUMB, &map_req))
    return -errno;

  ptr = mmap(0, buf->size, PROT_READ | PROT_WRITE, MAP_SHARED, buf->dev->fd,
             map_req.offset);

  if (ptr == MAP_FAILED)
    return -errno;

  buf->ptr = ptr;

  return 0;
}

static void
buffer_cpu_access_finish(dbm_buffer *buf, bool flag1, bool flag2)
{
  munmap(buf->ptr, buf->size);
  buf->ptr = NULL;
}

static void
buffer_destroy(dbm_buffer *buf)
{
  handle_unreference(buf->dev, buf->handle);
  free(buf);
}

static dbm_buffer_functions buffer_functions =
{
  .cpu_access_prepare = buffer_cpu_access_prepare,
  .cpu_access_finish = buffer_cpu_access_finish,
  .destroy = buffer_destroy
};

/* dbm_buiffer wrappers */
static int
buffer_init(dbm_device *dev, int handle, uint32_t size, dbm_buffer **buf)
{
  dbm_buffer *new_buf = NULL;
  int err;

  TRACE("%s size %d\n", __func__, size);

  err = handle_reference(dev, handle);

  if (err)
    return err;

  err = dev->funcs->buffer_create_from_handle(dev, handle, size, &new_buf);

  if (err)
    handle_unreference(dev, handle);
  else
    *buf = new_buf;

  return err;
}

dbm_buffer *
dbm_buffer_create(dbm_device *dev, uint32_t size, uint32_t flags)
{
  dbm_buffer *buf = NULL;
  int err;

  TRACE("%s size %d, flags %x\n", __func__, size, flags);

  if (!size)
  {
    errno = EINVAL;
    return NULL;
  }

  handle_lock(dev);
  err = dev->funcs->buffer_create(dev, size, flags, &buf);
  handle_unlock(dev);

  if (err)
  {
    assert(!buf);
    errno = -err;
    return NULL;
  }

  assert(buf->dev && buf->funcs && buf->funcs->cpu_access_prepare &&
         buf->funcs->cpu_access_finish && buf->funcs->destroy);

  return buf;
}

dbm_buffer *
dbm_buffer_from_name(dbm_device *dev, uint32_t name, uint32_t size)
{
  dbm_buffer *buf = NULL;
  struct drm_gem_open open_req;

  TRACE("%s size %d name %x\n", __func__, size, name);

  open_req.name = name;
  open_req.size = 0LL;
  open_req.handle = 0;

  handle_lock(dev);

  if (!drmIoctl(dev->fd, DRM_IOCTL_GEM_OPEN, &open_req))
  {
    int err = buffer_init(dev, open_req.handle, size, &buf);

    if (err)
    {
        assert(!buf);
        errno = -err;
    }
    else
    {
      assert(buf->dev && buf->funcs && buf->funcs->cpu_access_prepare &&
             buf->funcs->cpu_access_finish && buf->funcs->destroy);
    }
  }

  handle_unlock(dev);

  return buf;
}

dbm_buffer *
dbm_buffer_from_fd(dbm_device *dev, int fd)
{
  dbm_buffer *buf = NULL;
  uint32_t handle;
  off_t size = lseek(fd, 0, SEEK_END);

  TRACE("%s size %jd fd %d\n", __func__, size, fd);

  if (size == -1)
    return NULL;

  handle_lock(dev);

  if (!drmPrimeFDToHandle(dev->fd, fd, &handle))
  {
    int err = buffer_init(dev, handle, size, &buf);

    if (err)
    {
      assert(!buf);
      errno = -err;
    }
    else {
      assert(buf->dev && buf->funcs && buf->funcs->cpu_access_prepare &&
             buf->funcs->cpu_access_finish && buf->funcs->destroy);
    }
  }

  handle_unlock(dev);

  return buf;
}

void
dbm_buffer_destroy(dbm_buffer *buf)
{
  assert(!buf->ptr);
  dbm_device *dev = buf->dev;
  TRACE("%s\n", __func__);

  if (buf->closure)
  {
    buf->closure(buf->user_data);
    buf->user_data = NULL;
    buf->closure = NULL;
  }

  handle_lock(dev);
  buf->funcs->destroy(buf);
  handle_unlock(dev);
}

void *
dbm_buffer_cpu_access_prepare(dbm_buffer *buf, bool flag1, bool flag2)
{
  int err;

  if (buf->ptr)
  {
    errno = EBUSY;
    return NULL;
  }

  err = buf->funcs->cpu_access_prepare(buf);

  if (err)
  {
    assert(!buf->ptr);
    errno = -err;
    return NULL;
  }

  assert(buf->ptr);

  buf->flag1 = flag1;
  buf->flag2 = flag2;

  return buf->ptr;
}

void
dbm_buffer_cpu_access_finish(dbm_buffer *buf)
{
  if (buf->ptr)
  {
    buf->funcs->cpu_access_finish(buf, buf->flag1, buf->flag2);
    assert(!buf->ptr);
  }
}

uint32_t
dbm_buffer_get_handle(dbm_buffer *buf)
{
  return buf->handle;
}

uint32_t
dbm_buffer_get_name(dbm_buffer *buf)
{
  struct drm_gem_flink flink_req;

  if (buf->name)
      return buf->name;

  flink_req.handle = buf->handle;

  if (drmIoctl(buf->dev->fd, DRM_IOCTL_GEM_FLINK, &flink_req))
    return 0;

  buf->name = flink_req.name;

  return buf->name;
}

int
dbm_buffer_get_fd(dbm_buffer *buf)
{
  int prime_fd;

  if (drmPrimeHandleToFD(buf->dev->fd, buf->handle, DRM_CLOEXEC, &prime_fd))
    prime_fd = -1;

  return prime_fd;
}

uint32_t
dbm_buffer_get_size(dbm_buffer *buf)
{
  return buf->size;
}

void *
dbm_buffer_get_user_data(dbm_buffer *buf)
{
  return buf->user_data;
}

void
dbm_buffer_set_user_data(dbm_buffer *buf, void *user_data,
                         void (*closure)(void *))
{
  buf->user_data = user_data;
  buf->closure = closure;
}

int
dbm_get_buffer_stride_and_size(dbm_device *dev, uint32_t width, uint32_t height,
                               uint32_t bpp, uint32_t unk1, uint32_t flags,
                               uint32_t *stride, uint32_t *size)
{
  int err;

  TRACE("%s %dx%dx%d flags %d\n", __func__, width, height, bpp, flags);

  if (flags)
  {
    err = dev->funcs->get_buffer_stride_and_size(dev, width, height, bpp, unk1,
                                                 flags, stride, size);

    if (!err)
      return 0;

    errno = -err;
  }
  else
    errno = EINVAL;

  return -1;
}

bool
dbm_is_format_and_layout_supported(dbm_device *dev, uint32_t fourcc,
                                   uint32_t format, uint32_t flags)
{
  int i;

  if (!flags)
    return false;

  if (flags != GBM_BO_USE_SCANOUT && flags != GBM_BO_USE_CURSOR &&
      flags != GBM_BO_USE_RENDERING)
  {
    return true;
  }

  for (i = 0; i < dev->format_count; i++)
  {
    dbm_format *fmt = &dev->formats[i];

    if (fmt->fourcc == fourcc && fmt->format == format &&
        !(flags & ~fmt->allowed_flags))
    {
      return true;
    }
  }

  return false;
}

/* dbm_device implementation */
static int
get_buffer_stride_and_size(dbm_device *dev, uint32_t width, uint32_t height,
                           uint32_t bpp, uint32_t unk1, uint32_t flags,
                           uint32_t *stride, uint32_t *size)
{
  if (unk1)
    return -EINVAL;

  *stride = align(bpp, 8) / 8 * align(width, 8);
  *size = *stride * height;

  return 0;
}

static int
buffer_create(dbm_device *dev, uint32_t size, uint32_t flags, dbm_buffer **buf)
{
  struct drm_mode_create_dumb create_dumb_req;

  TRACE("%s size %d, flags %x\n", __func__, size, flags);

  create_dumb_req.flags = 0;
  create_dumb_req.height = size;
  create_dumb_req.width = 1;
  create_dumb_req.bpp = 8;

  if (drmIoctl(dev->fd, DRM_IOCTL_MODE_CREATE_DUMB, &create_dumb_req))
    return -errno;

  return buffer_init(dev, create_dumb_req.handle, size, buf);
}

static int
buffer_create_from_handle(dbm_device *dev, uint32_t handle, uint32_t size,
                          dbm_buffer **buf)
{
  dbm_buffer *new_buf = (dbm_buffer *)calloc(1, sizeof(dbm_buffer));

  TRACE("%s size %d\n", __func__, size);

  if (!new_buf)
    return -ENOMEM;

  new_buf->dev = dev;
  new_buf->handle = handle;
  new_buf->size = size;
  new_buf->funcs = &buffer_functions;

  *buf = new_buf;

  return 0;
}

static dbm_device_functions device_functions =
{
  .destroy = free,
  .get_buffer_stride_and_size = get_buffer_stride_and_size,
  .buffer_create = buffer_create,
  .buffer_create_from_handle = buffer_create_from_handle
};

dbm_format formats[4] =
{
  {
    GBM_FORMAT_ARGB8888,
    GBM_BO_FORMAT_XRGB8888,
    GBM_BO_USE_SCANOUT |
    GBM_BO_USE_CURSOR |
    GBM_BO_USE_RENDERING |
    GBM_BO_USE_WRITE |
    GBM_BO_USE_LINEAR
  },
  {
    GBM_FORMAT_ABGR8888,
    GBM_BO_FORMAT_XRGB8888,
    GBM_BO_USE_SCANOUT |
    GBM_BO_USE_CURSOR |
    GBM_BO_USE_RENDERING |
    GBM_BO_USE_WRITE |
    GBM_BO_USE_LINEAR
  },
  {
    GBM_FORMAT_XRGB8888,
    GBM_BO_FORMAT_XRGB8888,
    GBM_BO_USE_SCANOUT |
    GBM_BO_USE_CURSOR |
    GBM_BO_USE_RENDERING |
    GBM_BO_USE_WRITE |
    GBM_BO_USE_LINEAR
  },
  {
    GBM_FORMAT_RGB565,
    GBM_BO_FORMAT_XRGB8888,
    GBM_BO_USE_SCANOUT |
    GBM_BO_USE_CURSOR |
    GBM_BO_USE_RENDERING |
    GBM_BO_USE_WRITE |
    GBM_BO_USE_LINEAR
  }
};

static int
dbm_device_alloc(int fd, dbm_device **dev)
{
  dbm_device *new_dev = malloc(sizeof(dbm_device));

  if (!new_dev)
    return -ENOMEM;

  new_dev->fd = fd;
  new_dev->formats = formats;
  new_dev->format_count = sizeof(formats) / sizeof(formats[0]);
  new_dev->funcs = &device_functions;

  *dev = new_dev;

  return 0;
}

dbm_device *
dbm_device_create(int fd)
{
  drmVersionPtr ver = drmGetVersion(fd);
  dbm_device *dev = NULL;
  int err;

  TRACE("%s\n", __func__);

  if (!ver)
  {
    errno = ENODEV;
    return NULL;
  }

  if (strcmp("omapdrm", ver->name))
  {
    drmFreeVersion(ver);
    errno = ENODEV;
    return NULL;
  }

  drmFreeVersion(ver);

  err = dbm_device_alloc(fd, &dev);
  assert(!err || !dev);

  if (err)
  {
    errno = -err;
    return NULL;
  }

  assert(dev && dev->format_count && dev->formats && dev->funcs &&
         dev->funcs->destroy && dev->funcs->get_buffer_stride_and_size &&
         dev->funcs->buffer_create && dev->funcs->buffer_create_from_handle);

  dev->handle_ref = drmHashCreate();
  err = pthread_mutex_init(&dev->mutex, NULL);

  if (err)
  {
    dev->funcs->destroy(dev);
    errno = err;
    return NULL;
  }

  return dev;
}

void
dbm_device_destroy(dbm_device *dev)
{
  TRACE("%s\n", __func__);

  pthread_mutex_destroy(&dev->mutex);
  drmHashDestroy(dev->handle_ref);
  dev->funcs->destroy(dev);
}

int
dbm_bpp_from_pixel_format(int fourcc)
{
  switch (fourcc)
  {
    /* case GBM_FORMAT_C8: */
    case GBM_FORMAT_R8:
      return 8;

    case GBM_FORMAT_GR88:
      return 16;

    case GBM_FORMAT_RGB332:
    case GBM_FORMAT_BGR233:
      return 8;

    case GBM_FORMAT_XRGB4444:
    case GBM_FORMAT_XBGR4444:
    case GBM_FORMAT_RGBX4444:
    case GBM_FORMAT_BGRX4444:

    case GBM_FORMAT_ARGB4444:
    case GBM_FORMAT_ABGR4444:
    case GBM_FORMAT_RGBA4444:
    case GBM_FORMAT_BGRA4444:

    case GBM_FORMAT_XRGB1555:
    case GBM_FORMAT_XBGR1555:
    case GBM_FORMAT_RGBX5551:
    case GBM_FORMAT_BGRX5551:

    case GBM_FORMAT_ARGB1555:
    case GBM_FORMAT_ABGR1555:
    case GBM_FORMAT_RGBA5551:
    case GBM_FORMAT_BGRA5551:

    case GBM_FORMAT_RGB565:
    case GBM_FORMAT_BGR565:
      return 16;

    case GBM_FORMAT_RGB888:
    case GBM_FORMAT_BGR888:
      return 24;

    case GBM_FORMAT_XRGB8888:
    case GBM_FORMAT_XBGR8888:
    case GBM_FORMAT_RGBX8888:
    case GBM_FORMAT_BGRX8888:

    case GBM_FORMAT_ARGB8888:
    case GBM_FORMAT_ABGR8888:
    case GBM_FORMAT_RGBA8888:
    case GBM_FORMAT_BGRA8888:

    case GBM_FORMAT_XRGB2101010:
    /* case GBM_FORMAT_XBGR2101010: */
    case GBM_FORMAT_RGBX1010102:
    case GBM_FORMAT_BGRX1010102:

    case GBM_FORMAT_ARGB2101010:
    case GBM_FORMAT_ABGR2101010:
    /* case GBM_FORMAT_RGBA1010102:
    case GBM_FORMAT_BGRA1010102: */
      return 32;

   /* case GBM_FORMAT_XBGR16161616F:
    case GBM_FORMAT_ABGR16161616F:
      return 16; */

    /* packed YCbCr */
    case GBM_FORMAT_YUYV:
    case GBM_FORMAT_YVYU:
    case GBM_FORMAT_UYVY:
    case GBM_FORMAT_VYUY:
      return 16;

    /* case GBM_FORMAT_AYUV: */

    case GBM_FORMAT_NV12:
    case GBM_FORMAT_NV21:
      return 12;
    /* case GBM_FORMAT_NV16:
    case GBM_FORMAT_NV61:
      return 16; */


    /* case GBM_FORMAT_YUV410:
    case GBM_FORMAT_YVU410:
    case GBM_FORMAT_YUV411:
    case GBM_FORMAT_YVU411: */

    case GBM_FORMAT_YUV420:
    /* case GBM_FORMAT_YVU420: */
      return 12;
    case GBM_FORMAT_YUV422:
    /* case GBM_FORMAT_YVU422:
    case GBM_FORMAT_YUV444:
    case GBM_FORMAT_YVU444: */
      return 16;
  }

  return -1;
}
