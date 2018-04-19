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

// Include the header file with "C" linkage
extern "C" {
#include "int_str_table.h"
}

// Include the implementation template, which uses the constants defined in
// `int_str_table.h`.  The implementation file defines all functions under "C"
// linkage, and should be linked with the corresponding header to generate a
// linkable library. See `CMakeLists.txt` for an example of how this is done to
// create `c_hash`.
#include "cuckoo_table_template.cc"
