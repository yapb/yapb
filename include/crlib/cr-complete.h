//
// Yet Another POD-Bot, based on PODBot by Markus Klinge ("CountFloyd").
// Copyright (c) YaPB Development Team.
//
// This software is licensed under the BSD-style license.
// Additional exceptions apply. For full license details, see LICENSE.txt or visit:
//     https://yapb.ru/license
//

#pragma once

#include <stdio.h>

#include <crlib/cr-platform.h>
#include <crlib/cr-basic.h>
#include <crlib/cr-alloc.h>
#include <crlib/cr-array.h>
#include <crlib/cr-binheap.h>
#include <crlib/cr-files.h>
#include <crlib/cr-lambda.h>
#include <crlib/cr-http.h>
#include <crlib/cr-library.h>
#include <crlib/cr-dict.h>
#include <crlib/cr-logger.h>
#include <crlib/cr-math.h>
#include <crlib/cr-vector.h>
#include <crlib/cr-random.h>
#include <crlib/cr-ulz.h>
#include <crlib/cr-color.h>
#include <crlib/cr-hook.h>

CR_NAMESPACE_BEGIN

namespace types {
   using StringArray = Array <String>;
   using IntArray = Array <int>;
};

using namespace cr::types;

CR_NAMESPACE_END