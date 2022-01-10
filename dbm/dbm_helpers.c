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

#include "dbm_helpers.h"

int
handle_unreference(dbm_device *dev, uint32_t handle)
{
  struct drm_gem_close close_req =
  {
    .handle = handle,
  };

  assert(handle < dev->num_handles);

  assert(dev->handle_ref[handle] > 0);

  dev->handle_ref[handle]--;

  if (dev->handle_ref[handle])
    return 0;

  return drmIoctl(dev->fd, DRM_IOCTL_GEM_CLOSE, &close_req);
}
