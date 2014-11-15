#pragma once

#include "runtime.h"

typedef int (* avrdoper_enum_callback_t)(void *data,
                                         const wchar_t *id,
                                         pool_t *pool);

typedef struct avrdoper_dev avrdoper_dev_t;

err_t * avrdoper_hid_enum_devices(avrdoper_enum_callback_t callback,
                                  void *data,
                                  pool_t *pool);

err_t * avrdoper_hid_open(avrdoper_dev_t **dev,
                          const wchar_t *id,
                          pool_t *pool);

err_t * avrdoper_hid_write(avrdoper_dev_t *dev,
                           const char *data,
                           size_t len,
                           pool_t *pool);

err_t * avrdoper_hid_read(avrdoper_dev_t *dev,
                          char *buffer,
                          size_t *len,
                          pool_t *pool);
