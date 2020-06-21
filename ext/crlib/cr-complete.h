//
// CRLib - Simple library for STL replacement in private projects.
// Copyright © 2020 YaPB Development Team <team@yapb.ru>.
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
// 

#pragma once

#include <stdio.h>

#include <crlib/cr-basic.h>
#include <crlib/cr-alloc.h>
#include <crlib/cr-array.h>
#include <crlib/cr-binheap.h>
#include <crlib/cr-files.h>
#include <crlib/cr-lambda.h>
#include <crlib/cr-http.h>
#include <crlib/cr-library.h>
#include <crlib/cr-hashmap.h>
#include <crlib/cr-logger.h>
#include <crlib/cr-math.h>
#include <crlib/cr-vector.h>
#include <crlib/cr-random.h>
#include <crlib/cr-ulz.h>
#include <crlib/cr-color.h>
#include <crlib/cr-detour.h>

CR_NAMESPACE_BEGIN

namespace types {
   using StringArray = Array <String>;
   using IntArray = Array <int>;
}

using namespace cr::types;

CR_NAMESPACE_END
