/*
 * dbm_helpers.c
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

#include <xf86drm.h>
#include <assert.h>
#include <stdlib.h>
#include <errno.h>

#include "dbm_helpers.h"

int
handle_unreference(dbm_device *dev, uint32_t handle)
{
  uint32_t *ref;
  struct drm_gem_close close_req =
  {
    .handle = handle,
  };

  assert(drmHashLookup(dev->handle_ref, handle, (void *)&ref) == 0);
  assert(*ref > 0);

  (*ref)--;

  if (*ref)
    return 0;

  free(ref);
  drmHashDelete(dev->handle_ref, handle);

  return drmIoctl(dev->fd, DRM_IOCTL_GEM_CLOSE, &close_req);
}

int
handle_reference(dbm_device *dev, uint32_t handle)
{
  uint32_t *ref;
  int err = drmHashLookup(dev->handle_ref, handle, (void *)&ref);

  if (err == 1)
  {
    err = 0;
    ref = malloc(sizeof(*ref));

    if (!ref)
      err = -ENOMEM;
    else
    {
      *ref = 0;

      if (drmHashInsert(dev->handle_ref, handle, ref) != 0)
      {
        free(ref);
        err = -ENOMEM;
      }
    }
  }
  else if (err == -1)
    err = -EINVAL;

  if (!err)
    (*ref)++;
  else
  {
    struct drm_gem_close close_req;

    close_req.handle = handle;
    drmIoctl(dev->fd, DRM_IOCTL_GEM_CLOSE, &close_req);

  }

  return err;
}
