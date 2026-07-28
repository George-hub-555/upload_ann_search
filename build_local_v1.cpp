#include <fstream>
#include <sstream>
#include "common/public/utils/logging.h"
#include "common/utils/filesystem/file.h"
#include "gflags/gflags.h"
#include "gflags/gflags_declare.h"
#include "index_factory/builder_public/index_builder.h"
#include "index_factory/native/blacklist_impl.h"

DEFINE_string(schema_path, "tools/index_factory/configs/build_local_v1_schema.json", "schema file path");
DEFINE_string(data_path, "", "data path (only used in full-build mode)");
DEFINE_string(output_path, "", "output dir");
DEFINE_bool(blacklist_only, false,
            "if true, skip full index build; only produce shard*.docids.blacklist + shard*.meta "
            "(meta carries docId_blacklist only). Requires --blacklist_path and --source_docids");
DEFINE_string(blacklist_path, "",
              "blacklist text file (one GlobalDocID per line). Only used with --blacklist_only");
DEFINE_string(source_docids, "",
              "existing shard*.docids bundle file. Only used with --blacklist_only. "
              "The blacklist bundle mirrors its ldocid order, replacing blacklisted gids with BLACKLIST_GLOBAL_DOC_ID");
DEFINE_uint32(shard_id, 0, "shard id (default 0)");
DEFINE_string(weight_path, "",
              "optional per-doc downweight file (each line \"<GlobalDocID> <weight>\"). Only used with "
              "--blacklist_only; adds a DOC_DOWNWEIGHT bundle to shard*.docids.blacklist");

namespace {

std::string ReadFile(const std::string& filePath)
{
    const Falcon::Common::Utils::FileSystem::File file(filePath);
    if (!file.Exists()) {
        LOG(INFO) << "file not exist:" << filePath;
        return "";
    }
    if (file.GetLength() == 0) {
        LOG(INFO) << "file content is empty:" << filePath;
        return "";
    }
    std::ifstream readFile(filePath);
    std::stringstream buffer;
    buffer << readFile.rdbuf();
    if (!buffer) {
        LOG(ERROR) << "failed to read file, file path:" << filePath;
        return "";
    }
    return buffer.str();
}

bool RunFullBuildPath()
{
    std::string schema = ReadFile(FLAGS_schema_path);
    if (!Falcon::IndexFactory::Building::Build(schema, FLAGS_shard_id, FLAGS_data_path, FLAGS_output_path)) {
        LOG(ERROR) << "Build failed";
        return false;
    }
    if (!FLAGS_blacklist_path.empty()) {
        LOG(WARNING) << "--blacklist_path is only honored with --blacklist_only; ignored in full-build mode";
    }
    return true;
}

bool RunBlacklistOnlyPath()
{
    Falcon::IndexFactory::Native::BlacklistOptions opts;
    opts.outputDir = FLAGS_output_path;
    opts.shardId = FLAGS_shard_id;
    opts.blacklistPath = FLAGS_blacklist_path;
    opts.sourceDocIdsPath = FLAGS_source_docids;
    opts.weightPath = FLAGS_weight_path;
    return Falcon::IndexFactory::Native::BuildBlacklistOnly(opts);
}

}  // namespace

int main(int args, char* argv[])
{
    gflags::ParseCommandLineFlags(&args, &argv, true);
    if (FLAGS_blacklist_only) {
        return RunBlacklistOnlyPath() ? 0 : 2;
    }
    return RunFullBuildPath() ? 0 : 1;
}
