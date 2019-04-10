// Licensed to the Apache Software Foundation (ASF) under one
// or more contributor license agreements. See the NOTICE file
// distributed with this work for additional information
// regarding copyright ownership. The ASF licenses this file
// to you under the Apache License, Version 2.0 (the
// "License"); you may not use this file except in compliance
// with the License. You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing,
// software distributed under the License is distributed on an
// "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, either express or implied. See the License for the
// specific language governing permissions and limitations
// under the License.

#include <cassert>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include <arrow/io/file.h>
#include <arrow/util/logging.h>

#include <parquet/api/reader.h>
#include <parquet/api/writer.h>

using parquet::LogicalType;
using parquet::Repetition;
using parquet::Type;
using parquet::schema::GroupNode;
using parquet::schema::PrimitiveNode;
using parquet::schema::NodePtr;

//constexpr int FIXED_LENGTH = 10;

constexpr int SCHEMA_LINE_MAX = 255;
constexpr int64_t ROW_GROUP_SIZE = 1024;

std::map<std::string, Repetition::type> repetitionMap;
std::map<std::string, Type::type> physicalTypeMap;
std::map<std::string, LogicalType::type> logicalTypeMap;

int numRows = 128;
int randFloor = 0;
int randCeiling = INT_MAX;
 
void initTypeMaps() {
  //Populate the repetitionMap
  repetitionMap.insert({"required", Repetition::REQUIRED});
  repetitionMap.insert({"optional", Repetition::OPTIONAL});
  repetitionMap.insert({"repeated", Repetition::REPEATED});

  //Populate the physicalTypeMap
  physicalTypeMap.insert({"boolean", Type::BOOLEAN});
  physicalTypeMap.insert({"int32", Type::INT32});
  physicalTypeMap.insert({"int64", Type::INT64});
  physicalTypeMap.insert({"int96", Type::INT96});
  physicalTypeMap.insert({"float", Type::FLOAT});
  physicalTypeMap.insert({"double", Type::DOUBLE});
  physicalTypeMap.insert({"binary", Type::BYTE_ARRAY});
  physicalTypeMap.insert({"fixed_len_byte_array", Type::FIXED_LEN_BYTE_ARRAY});

  //Populate the logicalTypeMap
  logicalTypeMap.insert({"(NONE)", LogicalType::NONE});
  logicalTypeMap.insert({"(UTF8)", LogicalType::UTF8});
  logicalTypeMap.insert({"(MAP)", LogicalType::MAP});
  logicalTypeMap.insert({"(MAP)", LogicalType::MAP});
  logicalTypeMap.insert({"(MAP_KEY_VALUE)", LogicalType::MAP_KEY_VALUE});
  logicalTypeMap.insert({"(LIST)", LogicalType::LIST});
  logicalTypeMap.insert({"(ENUM)", LogicalType::ENUM});
  logicalTypeMap.insert({"(DECIMAL)", LogicalType::DECIMAL});
  logicalTypeMap.insert({"(DATE)", LogicalType::DATE});
  logicalTypeMap.insert({"(TIME_MILLIS)", LogicalType::TIME_MILLIS});
  logicalTypeMap.insert({"(TIME_MICROS)", LogicalType::TIME_MICROS});
  logicalTypeMap.insert({"(TIMESTAMP_MILLIS)", LogicalType::TIMESTAMP_MILLIS});
  logicalTypeMap.insert({"(TIMESTAMP_MICROS)", LogicalType::TIMESTAMP_MICROS});
  logicalTypeMap.insert({"(UINT_8)", LogicalType::UINT_8});
  logicalTypeMap.insert({"(UINT_16)", LogicalType::UINT_16});
  logicalTypeMap.insert({"(UINT_32)", LogicalType::UINT_32});
  logicalTypeMap.insert({"(UINT_64)", LogicalType::UINT_64});
  logicalTypeMap.insert({"(INT_8)", LogicalType::INT_8});
  logicalTypeMap.insert({"(INT_16)", LogicalType::INT_16});
  logicalTypeMap.insert({"(INT_32)", LogicalType::INT_32});
  logicalTypeMap.insert({"(INT_64)", LogicalType::INT_64});
  logicalTypeMap.insert({"(JSON)", LogicalType::JSON});
  logicalTypeMap.insert({"(BSON)", LogicalType::BSON});
  logicalTypeMap.insert({"(INTERVAL)", LogicalType::INTERVAL});
}

//////////////////// START of reading schema /////////////////////

std::vector<std::string> split(const char *str, char delimiter) {
  printf("Splitting %s\n", str);
  std::vector<std::string> result;
  do {
    const char *begin = str;
    while(*str != delimiter && *str) {
      str++;
    }
    if (!(begin - str)) {
      continue;
    }
    result.push_back(std::string(begin, str));
  } while (0 != *str++);
  return result;
}

int populateRepetitionType(std::string input, Repetition::type *type) {
  auto it = repetitionMap.find(input);
  if (it == repetitionMap.end()) {
    printf("Cannot get the repetition type %s\n", input.c_str());
    return ENOENT;
  }
  *type = it->second;
  return 0;
}

int populatePhysicalType(std::string input, Type::type *type) {
  auto it = physicalTypeMap.find(input);
  if (it == physicalTypeMap.end()) {
    printf("Cannot get the physical type %s\n", input.c_str());
    return ENOENT;
  }
  *type = it->second;
  return 0;
}

int populateLogicalType(std::string input, LogicalType::type *type) {
  auto it = logicalTypeMap.find(input);
  if (it == logicalTypeMap.end()) {
    printf("Cannot get the logical type %s.\n", input.c_str());
    return ENOENT;
  }
  *type = it->second;
  return 0;
}

int populateScaleAndPrecision(Type::type physicalType, int *precision, int *scale) {
  switch(physicalType) {
    case parquet::Type::INT32:
      *precision = 9;
      *scale = 3;
      break;
    case parquet::Type::INT64:
      *precision = 5;
      *scale = 2;
      break;
    case parquet::Type::BYTE_ARRAY:
      *precision = 38;
      *scale = 10;
      break;
    default:
      return -1;
  }
  return 0;
}

int addFieldToSchema(char *schema_field, parquet::schema::NodeVector *fields) {
  std::vector<std::string> tokens = split(schema_field, ' ');
  int tokenNum = 0, err = 0;
  std::string fieldName;
  Repetition::type repetitionType = Repetition::REQUIRED;
  Type::type physicalType = Type::BOOLEAN;
  LogicalType::type logicalType = LogicalType::NONE;
  int precision = -1;
  int scale = -1;
  for (std::string token : tokens) {
    switch (tokenNum) {
      case 0:
        err = populateRepetitionType(token, &repetitionType);
        if (err) {
          return err;
        }
        break;
      case 1:
        err = populatePhysicalType(token, &physicalType);
        if (err) {
          return err;
        }
        break;
      case 2:
        fieldName = token;
        break;
      case 3:
        err = populateLogicalType(token, &logicalType);
        if (err) {
          return err;
        }
        break;
      default:
        printf("Error reading field %s", schema_field);
        return -1;
    }
    tokenNum++;
  }
  if (logicalType == LogicalType::DECIMAL) {
    err = populateScaleAndPrecision(physicalType, &precision, &scale);
    if (err) {
      printf("Cannot populate scale and precision for %s", fieldName.c_str());
      return -1;
    }
  }
  parquet::schema::NodePtr field = PrimitiveNode::Make(fieldName, repetitionType, physicalType, logicalType, -1,
                                                       precision, scale);
  if (!field) {
    printf("Error reading field %s\n", schema_field);
    return -1;
  }
  fields->push_back(field);
  return 0;
}

void sanitizeSchemaField(char *schemaField) {
  size_t len = strlen(schemaField);
  if (schemaField[len -  1] == '\n') {
    schemaField[len - 1] = '\0';
  }
}

std::shared_ptr<GroupNode> constructSchema(char *filename) {
  FILE *fp = fopen(filename, "r");
  char line[SCHEMA_LINE_MAX];
  int err = 0;
  if (fp == NULL) {
    printf("Cannot open the schema file\n");
    return NULL;
  }
  parquet::schema::NodeVector fields;
  while(fgets(line, SCHEMA_LINE_MAX, fp) != NULL) {
    sanitizeSchemaField(line);
    err = addFieldToSchema(line, &fields);
    if (err) {
      printf("Invalid schema %s", line);
      return NULL;
    }
  }
  fclose(fp);
  return std::static_pointer_cast<GroupNode>(
    GroupNode::Make("schema", Repetition::REQUIRED, fields));
}

////////////////// END of reading schema /////////////////////////////////

////////////////// START of data generation /////////////////////////////

int generateRandom() {
  int range = randCeiling - randFloor;
  return (randFloor + (rand() % range));
}

int64_t generateData(parquet::RowGroupWriter *rg_writer, std::shared_ptr<GroupNode> schema, int columnId) {
  PrimitiveNode* field = static_cast<PrimitiveNode *>(schema->field(columnId).get()); 
  int random = generateRandom();
  if (field->is_primitive()) {
    Repetition::type repetition = field->repetition();
    Type::type physicalType = field->physical_type();
    int16_t definitionLevel = 1;
    if (physicalType == parquet::Type::BOOLEAN) {
      // Write the Bool column
      parquet::BoolWriter* bool_writer =
        static_cast<parquet::BoolWriter*>(rg_writer->column(columnId));
      bool bool_value = ((random % 2) == 0) ? true : false; 
      switch(repetition) {
	case parquet::Repetition::OPTIONAL:
          if (random % 3 == 0) {
            definitionLevel = 0;
            bool_writer->WriteBatch(1, &definitionLevel, nullptr, nullptr);
          } else {
            bool_writer->WriteBatch(1, &definitionLevel, nullptr, &bool_value);
          }
          break;
        case parquet::Repetition::REPEATED:
	  for (int16_t repetitionLevel = 0; repetitionLevel < 2; repetitionLevel++) {
	    bool_value = (((random + repetitionLevel) % 2) == 0) ? true : false;
            bool_writer->WriteBatch(1, &definitionLevel, &repetitionLevel, &bool_value);
	  }
	  break;
	default:
          bool_writer->WriteBatch(1, nullptr, nullptr, &bool_value);
      }
      return bool_writer->EstimatedBufferedValueBytes();
    } else if (physicalType == parquet::Type::INT32) {
      parquet::Int32Writer* int32_writer =
      static_cast<parquet::Int32Writer*>(rg_writer->column(columnId));
      int32_t int32_value = random;
      switch(repetition) {
        case parquet::Repetition::OPTIONAL:
          if (random % 3 == 0) {
            definitionLevel = 0;
            int32_writer->WriteBatch(1, &definitionLevel, nullptr, nullptr);
          } else {
            int32_writer->WriteBatch(1, &definitionLevel, nullptr, &int32_value);
          }
          break;
        case parquet::Repetition::REPEATED:
          for (int16_t repetitionLevel = 0; repetitionLevel < 2; repetitionLevel++) {
            int32_value += repetitionLevel;
            int32_writer->WriteBatch(1, &definitionLevel, &repetitionLevel, &int32_value);
          }
          break;
        default:
          int32_writer->WriteBatch(1, nullptr, nullptr, &int32_value);
      }
      return int32_writer->EstimatedBufferedValueBytes();
    } else if (physicalType == parquet::Type::INT64) {
      parquet::Int64Writer* int64_writer =
          static_cast<parquet::Int64Writer*>(rg_writer->column(columnId));
      int64_t int64_value = random;
      switch(repetition) {
        case parquet::Repetition::OPTIONAL:
          if (random % 3 == 0) {
            definitionLevel = 0;
            int64_writer->WriteBatch(1, &definitionLevel, nullptr, nullptr);
          } else {
            int64_writer->WriteBatch(1, &definitionLevel, nullptr, &int64_value);
          }
	  break;
        case parquet::Repetition::REPEATED:
          for (int16_t repetitionLevel = 0; repetitionLevel < 2; repetitionLevel++) {
            int64_value += repetitionLevel;
            int64_writer->WriteBatch(1, &definitionLevel, &repetitionLevel, &int64_value);
          }
          break;
        default:
          int64_writer->WriteBatch(1, nullptr, nullptr, &int64_value);
      }
      return int64_writer->EstimatedBufferedValueBytes();
    } else if (physicalType == parquet::Type::INT96) {
      parquet::Int96Writer* int96_writer =
          static_cast<parquet::Int96Writer*>(rg_writer->column(columnId));
      parquet::Int96 int96_value;
      int96_value.value[0] = random;
      int96_value.value[1] = random + 1;
      int96_value.value[2] = random + 2;
      switch(repetition) {
        case parquet::Repetition::OPTIONAL:
          if (random % 3 == 0) {
            definitionLevel = 0;
            int96_writer->WriteBatch(1, &definitionLevel, nullptr, nullptr);
          } else {
            int96_writer->WriteBatch(1, &definitionLevel, nullptr, &int96_value);
          }
	  break;
        case parquet::Repetition::REPEATED:
          for (int16_t repetitionLevel = 0; repetitionLevel < 2; repetitionLevel++) {
            int96_value.value[0] = random + repetitionLevel;
            int96_value.value[1] = random + repetitionLevel + 1;
            int96_value.value[2] = random + repetitionLevel + 2;
            int96_writer->WriteBatch(1, &definitionLevel, &repetitionLevel, &int96_value);
          }
          break;
        default:
          int96_writer->WriteBatch(1, nullptr, nullptr, &int96_value);
      }
      return int96_writer->EstimatedBufferedValueBytes();
    } else if (physicalType == parquet::Type::FLOAT) {
      parquet::FloatWriter* float_writer =
          static_cast<parquet::FloatWriter*>(rg_writer->column(columnId));
      float float_value = static_cast<float>(random) * 1.1f;
      switch(repetition) {
        case parquet::Repetition::OPTIONAL:
          if (random % 3 == 0) {
            definitionLevel = 0;
            float_writer->WriteBatch(1, &definitionLevel, nullptr, nullptr);
          } else {
            float_writer->WriteBatch(1, &definitionLevel, nullptr, &float_value);
          }
	  break;
        case parquet::Repetition::REPEATED:
          for (int16_t repetitionLevel = 0; repetitionLevel < 2; repetitionLevel++) {
            float_value = static_cast<float>(random + repetitionLevel) * 1.1f;
            float_writer->WriteBatch(1, &definitionLevel, &repetitionLevel, &float_value);
          }
          break;
        default:
          float_writer->WriteBatch(1, nullptr, nullptr, &float_value);
      }
      return float_writer->EstimatedBufferedValueBytes();
    } else if (physicalType == parquet::Type::DOUBLE) {
      parquet::DoubleWriter* double_writer =
          static_cast<parquet::DoubleWriter*>(rg_writer->column(columnId));
      double double_value = random * 1.1111111;
      switch(repetition) {
        case parquet::Repetition::OPTIONAL:
          if (random % 3 == 0) {
            definitionLevel = 0;
            double_writer->WriteBatch(1, &definitionLevel, nullptr, nullptr);
          } else {
            double_writer->WriteBatch(1, &definitionLevel, nullptr, &double_value);
          }
	  break;
        case parquet::Repetition::REPEATED:
          for (int16_t repetitionLevel = 0; repetitionLevel < 2; repetitionLevel++) {
            double_value = (random + repetitionLevel) * 1.1111111;
            double_writer->WriteBatch(1, &definitionLevel, &repetitionLevel, &double_value);
          }
          break;
        default:
          double_writer->WriteBatch(1, nullptr, nullptr, &double_value);
      }
      return double_writer->EstimatedBufferedValueBytes();
    } else if (physicalType == parquet::Type::BYTE_ARRAY) {
      parquet::ByteArrayWriter* ba_writer =
          static_cast<parquet::ByteArrayWriter*>(rg_writer->column(columnId));
      parquet::ByteArray ba_value;
      char hello[10] = "parquet";
      hello[7] = static_cast<char>(static_cast<int>('0') + random / 100);
      hello[8] = static_cast<char>(static_cast<int>('0') + (random / 10) % 10);
      hello[9] = static_cast<char>(static_cast<int>('0') + random % 10);
      ba_value.ptr = reinterpret_cast<const uint8_t*>(&hello[0]);
      ba_value.len = 10;
      switch(repetition) {
        case parquet::Repetition::OPTIONAL:
          if (random % 3 == 0) {
            definitionLevel = 0;
            ba_writer->WriteBatch(1, &definitionLevel, nullptr, nullptr);
          } else {
            ba_writer->WriteBatch(1, &definitionLevel, nullptr, &ba_value);
          }
	  break;
        case parquet::Repetition::REPEATED:
          for (int16_t repetitionLevel = 0; repetitionLevel < 2; repetitionLevel++) {
            hello[7] = static_cast<char>(static_cast<int>('0') + (random + repetitionLevel) / 100);
            hello[8] = static_cast<char>(static_cast<int>('0') + ((random + repetitionLevel) / 10) % 10);
            hello[9] = static_cast<char>(static_cast<int>('0') + (random + repetitionLevel) % 10);
            ba_value.ptr = reinterpret_cast<const uint8_t*>(&hello[0]);
            ba_value.len = 10;
            ba_writer->WriteBatch(1, &definitionLevel, &repetitionLevel, &ba_value);
          }
          break;
        default:
          ba_writer->WriteBatch(1, nullptr, nullptr, &ba_value);
      }
      return ba_writer->EstimatedBufferedValueBytes();
    }
    return -1;
  }
  return -1;
}

int generateParquetFile(std::shared_ptr<GroupNode> schema, char *outFile) {
  try {
    // Create a local file output stream instance.
    using FileClass = ::arrow::io::FileOutputStream;
    std::shared_ptr<FileClass> out_file;
    PARQUET_THROW_NOT_OK(FileClass::Open(outFile, &out_file));

    // Add writer properties
    parquet::WriterProperties::Builder builder;
    builder.compression(parquet::Compression::SNAPPY);
    std::shared_ptr<parquet::WriterProperties> props = builder.build();

    // Create a ParquetFileWriter instance
    std::shared_ptr<parquet::ParquetFileWriter> file_writer =
        parquet::ParquetFileWriter::Open(out_file, schema, props);

    // Append a BufferedRowGroup to keep the RowGroup open until a certain size
    parquet::RowGroupWriter* rg_writer = file_writer->AppendBufferedRowGroup();

    int num_columns = file_writer->num_columns();
    std::vector<int64_t> buffered_values_estimate(num_columns, 0);
    for (int i = 0; i < numRows; i++) {
      int64_t estimated_bytes = 0;
      // Get the estimated size of the values that are not written to a page yet
      for (int n = 0; n < num_columns; n++) {
        estimated_bytes += buffered_values_estimate[n];
      }

      // We need to consider the compressed pages
      // as well as the values that are not compressed yet
      if ((rg_writer->total_bytes_written() + rg_writer->total_compressed_bytes() +
           estimated_bytes) > ROW_GROUP_SIZE) {
        rg_writer->Close();
        std::fill(buffered_values_estimate.begin(), buffered_values_estimate.end(), 0);
        rg_writer = file_writer->AppendBufferedRowGroup();
      }

      for (int j = 0; j < schema->field_count(); j++) {
        buffered_values_estimate[j] = generateData(rg_writer, schema, j);
        if (buffered_values_estimate[j] < 0) {
          printf("Cannot generate data for field %s", schema->field(j)->name().c_str());
	  return -1;
	}
      }
    }
    // Close the RowGroupWriter
    rg_writer->Close();
    // Close the ParquetFileWriter
    file_writer->Close();

    // Write the bytes to file
    DCHECK(out_file->Close().ok());
  } catch (const std::exception& e) {
    std::cerr << "Parquet write error: " << e.what() << std::endl;
    return -1;
  }

  std::cout << "Parquet Writing Complete" << std::endl;

  return 0;
}

// Create a directory with the name and generate files inside that directory with ranges.
int generatePartitionedParquetFile(std::shared_ptr<GroupNode> schema, int numPartitions, char *outFile) {
  int err = mkdir (outFile, 0755);
  if (err) {
    printf("Error creating the directory %s\n", outFile);
    return err;
  }
  char parquetFile[256];
  int range = INT_MAX / numPartitions;
  for (int i = 0; i < numPartitions; i++) {
    sprintf(parquetFile, "%s/%d.parquet", outFile, i);
    err = generateParquetFile(schema, parquetFile);
    if (err) {
      printf("Error in generating parquet file %s\n", parquetFile);
      return err;
    }
    randFloor += range;
    randCeiling += range;
  }
  return 0;
}

void printUsage() {
  printf("Usage : ./a.out -s schemaFile -o outFile [-r numRows] [-p numPartitions]\n");
}

int main(int argc, char **argv) {
  if (argc < 2) {
    printUsage();
    return -1;
  }
  char *schemaFile = NULL;
  char *outFile = NULL;
  int opt, err;
  int numPartitions = 1;
  while((opt = getopt(argc, argv, "r:s:o:p:")) != -1) {
    switch(opt) {
      case 'r':
        numRows = atoi(optarg);
        break;
      case 's':
        schemaFile = optarg;
        break;
      case 'o':
        outFile = optarg;
        break;
      case 'p':
        numPartitions = atoi(optarg);
        break;
      default:
        printf("incorrect usage\n");
        printUsage();
        return -1;
    }
  }

  if (!schemaFile) {
    printf("Schema file argument is missing\n");
    printUsage();
    return -1;
  }

  if (!outFile) {
    printf("outFile argument is missing\n");
    printUsage();
    return -1;
  }

  srand((unsigned)time(0));
  initTypeMaps();
  std::shared_ptr<GroupNode> schema = constructSchema(schemaFile);
  if (!schema) {
    printf("Unable to read the schema file\n");
    return -1;
  }
  if (numPartitions > 1) {
    numRows /= numPartitions;
    err = generatePartitionedParquetFile(schema, numPartitions, outFile);
  } else {
    err = generateParquetFile(schema, outFile);
  }

  if (err) {
    printf("Unable to create parquet file");
    return err;
  }
  return err;
}
