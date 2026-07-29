/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: Build a Falcon index from locally extracted column data.
 * Create: 2026-07-29
 */

#include <fstream>
#include <sstream>
#include <string>

#include "common/public/utils/logging.h"
#include "common/utils/filesystem/file.h"
#include "gflags/gflags.h"
#include "index_factory/builder_public/index_builder.h"

DEFINE_string(schema_path, "", "path to the Falcon index schema JSON file");
DEFINE_string(data_path, "", "root directory of the extracted Falcon column data");
DEFINE_string(output_path, "", "existing directory for generated index files");
DEFINE_uint32(shard_id, 0, "shard id");

namespace {

bool ReadSchemaFile(const std::string& filePath, std::string& schema)
{
    if (filePath.empty()) {
        LOG(ERROR) << "--schema_path must not be empty";
        return false;
    }

    const Falcon::Common::Utils::FileSystem::File file(filePath);
    if (!file.Exists()) {
        LOG(ERROR) << "schema file does not exist: " << filePath;
        return false;
    }
    if (file.GetLength() == 0) {
        LOG(ERROR) << "schema file is empty: " << filePath;
        return false;
    }

    std::ifstream input(filePath);
    if (!input.is_open()) {
        LOG(ERROR) << "failed to open schema file: " << filePath;
        return false;
    }

    std::ostringstream buffer;
    buffer << input.rdbuf();
    if (input.bad()) {
        LOG(ERROR) << "failed to read schema file: " << filePath;
        return false;
    }

    schema = buffer.str();
    return !schema.empty();
}

bool ValidateFlags()
{
    if (FLAGS_data_path.empty()) {
        LOG(ERROR) << "--data_path must not be empty";
        return false;
    }
    if (FLAGS_output_path.empty()) {
        LOG(ERROR) << "--output_path must not be empty";
        return false;
    }
    return true;
}

}  // namespace

int main(int argc, char* argv[])
{
    gflags::ParseCommandLineFlags(&argc, &argv, true);
    if (!ValidateFlags()) {
        return 2;
    }

    std::string schema;
    if (!ReadSchemaFile(FLAGS_schema_path, schema)) {
        return 2;
    }

    if (!Falcon::IndexFactory::Building::Build(
            schema, FLAGS_shard_id, FLAGS_data_path, FLAGS_output_path)) {
        LOG(ERROR) << "local index build failed";
        return 1;
    }

    LOG(INFO) << "local index build succeeded";
    return 0;
}
