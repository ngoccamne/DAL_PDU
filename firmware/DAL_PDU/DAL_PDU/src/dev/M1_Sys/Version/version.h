#ifndef VERSION_H_
#define VERSION_H_

/* Firmware version. Change it by hand in a reviewed commit: MAJOR for an incompatible change
 * to the OBC interface, MINOR for new registers or commands, PATCH for fixes. The build ID is
 * separate and comes from the build. */
#define FIRMWARE_VERSION_MAJOR 0
#define FIRMWARE_VERSION_MINOR 1
#define FIRMWARE_VERSION_PATCH 0
#define FIRMWARE_VERSION_STRINGIFY_IMPL(value) #value
#define FIRMWARE_VERSION_STRINGIFY(value) FIRMWARE_VERSION_STRINGIFY_IMPL(value)
#define FIRMWARE_VERSION   FIRMWARE_VERSION_STRINGIFY(FIRMWARE_VERSION_MAJOR) "." \
                           FIRMWARE_VERSION_STRINGIFY(FIRMWARE_VERSION_MINOR) "." \
                           FIRMWARE_VERSION_STRINGIFY(FIRMWARE_VERSION_PATCH)
/* Release stage: "Development" until the first bench-verified build. */
#define FIRMWARE_STATUS    "Development"

/* FIRMWARE_BUILD_ID and FIRMWARE_BUILD_CODE, from tools/gen_build_id.py. */
#include "build_id.h"

#endif /* VERSION_H_ */
