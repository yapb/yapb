//
// YaPB, started from PODBot by Count Floyd
// Maintained by YaPB Team <yapb@jeefo.net>
//
// SPDX-License-Identifier: Unlicense
//

#pragma once

// fallback if no git or custom build
#define MODULE_COMMIT_COUNT "0"
#define MODULE_COMMIT_HASH "0"
#define MODULE_AUTHOR "local@yapb.jeefo.net"
#define MODULE_MACHINE "localhost"
#define MODULE_COMPILER "default"
#define MODULE_VERSION "4.8"
#define MODULE_VERSION_FILE 4,8,0,000
#define MODULE_BUILD_ID "0:0"

// fallback for non-git builds (has __DATE__, so no pch under clang)
#define MODULE_BUILD_DATE __DATE__
#define MODULE_BUILD_TIME __TIME__
#define MODULE_BUILD_YEAR &__DATE__[7]
