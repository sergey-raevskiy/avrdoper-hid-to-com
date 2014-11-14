#pragma once

#include "runtime.h"

typedef int (* avrdoper_enum_callback_t)(void *data,
                                         const wchar_t *id,
                                         pool_t *pool);

err_t * avrdoper_hid_enum_devices(avrdoper_enum_callback_t callback,
                                  void *data,
                                  pool_t *pool);
