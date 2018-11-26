#!/bin/bash

# Licensed to the Apache Software Foundation (ASF) under one
# or more contributor license agreements.  See the NOTICE file
# distributed with this work for additional information
# regarding copyright ownership.  The ASF licenses this file
# to you under the Apache License, Version 2.0 (the
# "License"); you may not use this file except in compliance
# with the License.  You may obtain a copy of the License at
#
#   http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing,
# software distributed under the License is distributed on an
# "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
# KIND, either express or implied.  See the License for the
# specific language governing permissions and limitations
# under the License.

set -e

source arrow/ci/travis_env_common.sh

# Builds arrow + gandiva and tests the same.
pushd arrow/cpp
  mkdir build
  pushd build
    if ("$TRAVIS_OS_NAME" -eq "linux" -a "$GANDIVA_STATIC_STD_CPP" = "ON")
    then 
      cmake -DCMAKE_BUILD_TYPE=Release \
            -DARROW_GANDIVA=ON \
            -DARROW_BUILD_UTILITIES=OFF \
            -DCMAKE_SHARED_LINKER_FLAGS="-static-libstdc++ -static-libgcc"
      make -j4
      assert_val = $(ldd debug/libgandiva_jni.so | grep "std-c++" | wc -l)
      # assert that std c++ is not a dynamic dependency when we want to link
      # statically.
      if ($assert_val -ne 0)
      then
      	exit -1;
      fi
    else
      cmake -DCMAKE_BUILD_TYPE=Release \
            -DARROW_GANDIVA=ON \
            -DARROW_BUILD_UTILITIES=OFF \
          ..
      make -j4    
    fi
    ctest
  popd
popd
