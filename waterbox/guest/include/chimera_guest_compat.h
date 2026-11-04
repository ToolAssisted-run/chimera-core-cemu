/* Included before every source of the guest build: what the guest's C
 * library (musl) does not declare and Cemu's Linux code calls. Defined in
 * waterbox/guest/guest-libc.cpp. */
#pragma once
#include <sys/types.h>
#ifdef __cplusplus
extern "C" {
#endif
pid_t gettid(void);
#ifdef __cplusplus
}
#endif
