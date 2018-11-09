/*
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The ASF licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package org.apache.arrow.gandiva.evaluator;

import org.apache.arrow.vector.types.Types;
import org.apache.arrow.vector.types.pojo.ArrowType;
import org.apache.arrow.vector.types.pojo.ArrowType.Decimal;

public class DecimalTypeUtil {

  private static final int MIN_ADJUSTED_SCALE = 6;
  /// The maximum precision representable by a 16-byte decimal
  private static final int MAX_PRECISION = 38;

  public static Decimal getResultTypeForAddOperation(Decimal operand1, Decimal operand2) {
    return getReturnTypeForAddSubtract(operand1, operand2);
  }

  public static Decimal getResultTypeForSubtractOperation(Decimal operand1, Decimal operand2) {
    return getReturnTypeForAddSubtract(operand1, operand2);
  }

  public static Decimal getResultTypeForMultiplyOperation(Decimal operand1, Decimal operand2) {
    return getReturnTypeForMultiplyDivide(operand1, operand2);
  }

  public static Decimal getResultTypeForDivideOperation(Decimal operand1, Decimal operand2) {
    return getReturnTypeForMultiplyDivide(operand1, operand2);
  }

  public static Decimal getResultTypeForModOperation(Decimal operand1, Decimal operand2) {
    int scale = Math.max(operand1.getScale(), operand2.getScale());
    int precision = Math.min(operand1.getPrecision() - operand1.getScale(),
                             operand2.getPrecision() - operand2.getScale()) +
                    scale;
    return adjustScaleIfNeeded(precision, scale);
  }

  private static Decimal getReturnTypeForAddSubtract(Decimal operand1, Decimal operand2) {
    int precision = 0;
    int scale = Math.max(operand1.getScale(), operand2.getScale());
    precision = scale + Math.max(operand1.getPrecision() - operand1.getScale(),
                                 operand2.getPrecision() - operand2.getScale()) + 1;
    return adjustScaleIfNeeded(precision, scale);
  }

  private static Decimal getReturnTypeForMultiplyDivide(Decimal operand1, Decimal operand2) {
    int precision = 0, scale = 0;
    scale =
            Math.max(MIN_ADJUSTED_SCALE, operand1.getScale() + operand2.getPrecision() + 1);
    precision =
            operand1.getPrecision() - operand1.getScale() + operand2.getScale() + scale;

    return adjustScaleIfNeeded(precision, scale);
  }

  private static Decimal adjustScaleIfNeeded(int precision, int scale) {
    if (precision > MAX_PRECISION) {
      int minScale = Math.min(scale, MIN_ADJUSTED_SCALE);
      int delta = precision - MAX_PRECISION;
      precision = MAX_PRECISION;
      scale = Math.max(scale - delta, minScale);
    }
    return new Decimal(precision, scale);
  }

}

