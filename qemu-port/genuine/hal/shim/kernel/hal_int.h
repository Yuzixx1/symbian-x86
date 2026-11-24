/* Port shim: the genuine HAL sources include <kernel/hal_int.h>, which the
   Symbian build exports to epoc32/include/kernel/. kernelhwsrv/ is read-only,
   so this shim (in qemu-port/genuine/hal/shim) forwards to the real header. */
#include "../../../../../kernelhwsrv/halservices/hal/inc/hal_int.h"
