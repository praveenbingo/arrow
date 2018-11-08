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

#ifndef GANDIVA_FUNCTION_IR_BUILDER_H
#define GANDIVA_FUNCTION_IR_BUILDER_H

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "gandiva/engine.h"
#include "gandiva/gandiva_aliases.h"
#include "gandiva/llvm_types.h"

namespace gandiva {

/// Base class for building IR functions.
class FunctionIRBuilder {
 public:
  FunctionIRBuilder(Engine* engine) : function_(nullptr), engine_(engine) {}

 protected:
  LLVMTypes* types() { return engine_->types(); }
  llvm::Module* module() { return engine_->module(); }
  llvm::LLVMContext* context() { return engine_->context(); }
  llvm::IRBuilder<>* ir_builder() { return engine_->ir_builder(); }

  llvm::Value* BuildIfElse(llvm::Value* condition, llvm::Type* return_type,
                           std::function<llvm::Value*()> then_func,
                           std::function<llvm::Value*()> else_func);

  llvm::Function* function_;
  Engine* engine_;

 private:
};

}  // namespace gandiva

#endif  // GANDIVA_FUNCTION_IR_BUILDER_H
