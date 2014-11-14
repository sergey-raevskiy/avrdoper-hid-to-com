#pragma once

err_t * avrdoper_hid_enum_devices(int (* callback)(const wchar_t *path),
                                  pool_t *pool);
