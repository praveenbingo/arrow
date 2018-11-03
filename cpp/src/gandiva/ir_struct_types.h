// Licensed to the Apache Software Foundation (ASF) under one
// or more contributor license agreements.  See the NOTICE file
// distributed with this work for additional information
// regarding copyright ownership.  The ASF licenses this file
// to you under the Apache License, Version 2.0 (the
// "License"); you may not use this file except in compliance
// with the License.  You may obtain a copy of the License at
//
//   http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing,
// software distributed under the License is distributed on an
// "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, either express or implied.  See the License for the
// specific language governing permissions and limitations
// under the License.

#ifndef GANDIVA_IR_STRUCT_TYPES_H
#define GANDIVA_IR_STRUCT_TYPES_H

#include "gandiva/decimal_large_ints.h"

/// Used for calls between runtime generated IR, and pre-compiled IR.
namespace gandiva {

// Decimal with 128-bit storage
struct IRDecimal128 {
  gandiva::int128_t value;
  int32_t precision;
  int32_t scale;
};

}  // namespace gandiva
#endif  // GANDIVA_IR_STRUCT_TYPES_H
