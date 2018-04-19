/* Copyright (C) 2013, Carnegie Mellon University and Intel Corporation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * ---------------------------
 *
 * The third-party libraries have their own licenses, as detailed in their source
 * files.
*/

// Include guard
#ifndef INT_STR_TABLE_H
#define INT_STR_TABLE_H

// Below we define the constants the table template uses to fill in the
// interface.
//
// All table functions will be prefixed with `int_str_table`
#define CUCKOO_TABLE_NAME int_str_table
// The type of the key is `int`
#define CUCKOO_KEY_TYPE int
// The type of the mapped value is `const char *`
#define CUCKOO_MAPPED_TYPE const char *

// Including the header after filling in the constants will populate the
// interface. See the template file itself for specific function names; most of
// them correspond to methods in the C++ implementation.
#include "cuckoo_table_template.h"

#endif // INT_STR_TABLE_H
