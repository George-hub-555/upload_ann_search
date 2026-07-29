# Copyright (c) Huawei Technologies Co., Ltd. 2023-2023. All rights reserved.

cc_binary(
    name = "builder_util",
    srcs = ["builder_util.cpp"],
    visibility = ["//visibility:public"],
    deps = [
        "//common/utils/hash",
        "//common/utils/time:timer",
        "//index_factory/public/builder:attachment",
        "//index_factory/public/builder:doc_id",
        "//index_factory/public/builder:section",
        "//index_factory/public/proto:document_cc_proto",
        "//third_party/gflags",
        "@com_google_protobuf//:json",
        "@rapidjson",
    ],
)

cc_binary(
    name = "attachment_checker",
    srcs = ["attachment_checker.cpp"],
    visibility = ["//visibility:public"],
    deps = [
        "//common/shard_format/attachment:attachment_factory",
        "//common/shard_format/docids:doc_id",
        "//common/utils/filesystem:cache_file_reader",
        "//index_factory/public/proto:check_doc_recall_attr_cc_proto",
        "//index_factory/public/writer/attachment:data_array_attachment_writer",
        "//third_party/gflags",
        "@rapidjson",
    ],
)

cc_binary(
    name = "build_section_checker",
    srcs = ["build_section_checker.cpp"],
    visibility = ["//visibility:public"],
    deps = [
        "//common/utils:base",
        "//common/utils/time:timer",
        "//index_factory/public/builder:section",
        "//third_party/gflags",
    ],
)

cc_binary(
    name = "doc_invert_index_section_checker",
    srcs = ["doc_invert_index_section_checker.cpp"],
    visibility = ["//visibility:public"],
    deps = [
        "//common/shard_format/section:doc_invert_doc_weight_index_section",
        "//common/shard_format/section:section_producer",
        "//common/utils:base",
        "//index_factory/base:posting_token",
        "//index_factory/base:token_pool",
        "//index_factory/public/writer/section/invert_index:doc_invert_doc_weight_index_writer",
        "//index_factory/public/writer/section/invert_index:doc_invert_index_writer",
        "//third_party/gflags",
    ],
)

cc_binary(
    name = "pos_invert_index_section_checker",
    srcs = ["pos_invert_index_section_checker.cpp"],
    visibility = ["//visibility:public"],
    deps = [
        "//common/shard_format/section:section_producer",
        "//common/utils:base",
        "//index_factory/base:pos_token",
        "//index_factory/base:token_pool",
        "//index_factory/public/writer/section/invert_index:pos_invert_index_writer",
        "//third_party/gflags",
    ],
)

cc_binary(
    name = "index_checker",
    srcs = ["index_checker.cpp"],
    visibility = ["//visibility:public"],
    deps = [
        "//common/shard_format/attachment:attachment_factory",
        "//common/shard_format/attachment/codec/decoder:memory_token_forward_pos_decoder",
        "//common/shard_format/docids:doc_id",
        "//common/shard_format/docids:doc_id_factory",
        "//common/shard_format/section:section_manager",
        "//common/shard_format/utils:index_impl",
        "//common/utils/filesystem:cache_file_reader",
        "//common/utils/filesystem:cache_file_writer",
        "//index_factory/base:doc_weight_token",
        "//index_factory/base:token_pool",
        "//index_factory/codec:section_data",
        "//index_factory/public/proto:check_doc_recall_attr_cc_proto",
        "//index_factory/public/proto:falcon_index_builder_cc_proto",
        "//index_factory/public/writer/snap:snap_reader",
        "//third_party/gflags",
    ],
)

cc_binary(
    name = "hbase_builder_util",
    srcs = ["hbase_builder_util.cpp"],
    visibility = ["//visibility:public"],
    deps = [
        "//common/utils/time:timer",
        "//index_factory/codec:section_data",
        "//index_factory/plugin_impl:data_handler_impl",
        "//index_factory/public/builder:attachment",
        "//index_factory/public/builder:doc_id",
        "//index_factory/public/builder:section",
        "//third_party/gflags",
    ],
)

cc_library(
    name = "checker_util",
    srcs = ["checker_util.cpp"],
    hdrs = ["checker_util.h"],
    visibility = ["//visibility:public"],
    deps = [
        "//common/shard_format/bundles:bundle_file_writer",
        "//common/shard_format/fusion_index/embedding_index/brute_force:brute_force_builder",
        "//common/shard_format/fusion_index/embedding_index/quantizer_index/rabitq_index/utils",
        "//common/shard_format/fusion_index/space_calculator/space:space_factory",
        "//common/shard_format/utils:types",
        "//common/utils/filesystem:file",
    ],
)

cc_library(
    name = "gid_text",
    srcs = ["data_format/gid_text.cpp"],
    hdrs = ["data_format/gid_text.h"],
    visibility = ["//visibility:public"],
    deps = [
        ":checker_util",
    ],
)

cc_library(
    name = "brute_force_index_utils",
    srcs = ["brute_force_index_utils.cpp"],
    hdrs = ["brute_force_index_utils.h"],
    visibility = ["//visibility:public"],
    deps = [
        ":checker_util",
        ":gid_text",
        "//common/shard_format/fusion_index/space_calculator/space:space_factory",
        "//common/shard_format/section:section_producer",
        "//common/utils/time",
        "//third_party/gflags",
    ],
)

cc_binary(
    name = "brute_force_index_checker",
    srcs = ["brute_force_index_checker.cpp"],
    visibility = ["//visibility:public"],
    deps = [
        ":brute_force_index_utils",
        ":gid_text",
    ],
)

cc_binary(
    name = "hierarchical_nsw_index_checker",
    srcs = ["hierarchical_nsw_index_checker.cpp"],
    visibility = ["//visibility:public"],
    deps = [
        ":brute_force_index_utils",
        ":checker_util",
        ":gid_text",
        "//common/shard_format/fusion_index/realtime/section:hnsw_realtime_section",
        "//common/shard_format/fusion_index/space_calculator/space:space_factory",
        "//common/shard_format/section:section_producer",
        "//common/utils/thread:simple_thread_pool",
        "//common/utils/time",
        "//index_factory/processors/falcon:build_section",
        "//index_factory/public/writer/section/fusion_index:hierarchical_nsw_index_writer",
        "//index_factory/public/writer/section/fusion_index:hnsw_sq_index_writer",
        "//third_party/gflags",
    ],
)

cc_binary(
    name = "fusion_index_checker",
    srcs = ["fusion_index_checker.cpp"],
    visibility = ["//visibility:public"],
    deps = [
        ":checker_util",
        "//common/shard_format/attachment:attachment_factory",
        "//common/shard_format/section:section_producer",
        "//common/utils/time",
        "//index_factory/codec:section_data",
        "//index_factory/public/proto:check_doc_recall_attr_cc_proto",
        "//index_factory/public/writer/section/fusion_index:fusion_index_writer",
        "//index_factory/reader:disk_pb_doc_reader",
        "//third_party/gflags",
    ],
)

cc_binary(
    name = "check_and_export_data",
    srcs = ["check_and_export_data.cpp"],
    visibility = ["//visibility:public"],
    deps = [
        "//common/utils/time:timer",
        "//index_factory/codec:section_data",
        "//index_factory/plugin_impl:data_handler_impl",
        "//index_factory/public/builder:attachment",
        "//index_factory/public/builder:doc_id",
        "//index_factory/public/builder:section",
        "//index_factory/public/proto:check_doc_recall_attr_cc_proto",
        "//index_factory/public/proto:document_cc_proto",
        "//third_party/gflags",
        "@com_google_protobuf//:json",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_binary(
    name = "invert_index_checker",
    srcs = ["invert_index_checker.cpp"],
    visibility = ["//visibility:public"],
    deps = [
        "//common/shard_format/docids:doc_id",
        "//common/shard_format/section:section_manager",
        "//common/shard_format/utils:index_impl",
        "//common/utils:base",
        "//falcon/serving/leaf/indexing/iterator:phrase_pos_iterator",
        "//third_party/gflags",
    ],
)

cc_binary(
    name = "check_doc_recall_attr",
    srcs = ["check_doc_recall_attr.cpp"],
    visibility = ["//visibility:public"],
    deps = [
        "//common/shard_format/attachment:attachment_factory",
        "//common/shard_format/attachment/codec/encoder:memory_token_forward_pos_encoder",
        "//common/shard_format/docids:doc_id",
        "//common/shard_format/section:section_producer",
        "//index_factory/public/builder:attachment",
        "//index_factory/public/builder:builder_schema",
        "//index_factory/public/proto:check_doc_recall_attr_cc_proto",
        "//third_party/gflags",
    ],
)

cc_binary(
    name = "clear_doc_recall_attr",
    srcs = ["clear_doc_recall_attr.cpp"],
    visibility = ["//visibility:public"],
    deps = [
        "//common/shard_format/attachment:attachment_factory",
        "//common/shard_format/attachment/codec/encoder:memory_token_forward_pos_encoder",
        "//common/shard_format/docids:doc_id",
        "//common/shard_format/section:section_producer",
        "//index_factory/public/builder:attachment",
        "//index_factory/public/builder:builder_schema",
        "//index_factory/public/proto:check_doc_recall_attr_cc_proto",
        "//third_party/gflags",
    ],
)

cc_binary(
    name = "snap_data_check",
    srcs = ["snap_data_check.cpp"],
    visibility = ["//visibility:public"],
    deps = [
        "//common/public/shard_format/utils:index",
        "//common/shard_format/docids:doc_id",
        "//common/shard_format/utils:index_impl",
        "//index_factory/codec:section_data",
        "//index_factory/public/proto:check_doc_recall_attr_cc_proto",
        "//index_factory/public/writer/snap:snap_reader",
    ],
)

cc_binary(
    name = "test_data_processor",
    srcs = ["test_data_processor.cpp"],
    visibility = ["//visibility:public"],
    deps = [
        "//common/shard_format/utils:index_impl",
        "//common/utils/filesystem:file",
        "//index_factory/native:libFalconForwardPos-lib",
        "//index_factory/public/proto:check_doc_recall_attr_cc_proto",
        "//index_factory/public/writer/snap:snap_writer",
        "//third_party/gflags",
        "//third_party/grpc:grpc++",
        "@com_google_protobuf//:json",
        "@rapidjson",
    ],
)

cc_binary(
    name = "falcon_builder_main",
    srcs = ["falcon_builder_main.cpp"],
    visibility = ["//visibility:public"],
    deps = [
        "//common/minidump:memory_region",
        "//common/secures:secure_boot",
        "//common/utils/auth:auth_util",
        "//index_factory/master/config_manager",
        "//index_factory/processors/falcon:build_attachment",
        "//index_factory/processors/falcon:build_section",
        "//index_factory/processors/falcon:copy",
        "//index_factory/processors/falcon:create_doc_id",
        "//index_factory/processors/falcon:finalize",
        "//index_factory/processors/falcon:snap",
        "//index_factory/public:common_func",
        "//third_party/gflags",
        "@com_google_absl//absl/debugging:failure_signal_handler",
        "@jemalloc",
    ],
)

cc_binary(
    name = "falcon_timeliness_builder_main",
    srcs = ["falcon_timeliness_builder_main.cpp"],
    visibility = ["//visibility:public"],
    deps = [
        "//common/secures:secure_boot",
        "//index_factory/public:common_func",
        "//index_factory/public/builder:timeliness_builder",
        "//index_factory/reader:disk_doc_reader",
        "//index_factory/reader:disk_pb_doc_reader",
        "//index_factory/timeliness:timeliness_doc_reader",
        "//third_party/gflags",
        "@com_google_absl//absl/debugging:failure_signal_handler",
        "@com_google_protobuf//:json",
        "@com_google_protobuf//:protobuf",
        "@jemalloc",
    ],
)

cc_binary(
    name = "snap_main",
    srcs = ["snap_main.cpp"],
    visibility = ["//visibility:public"],
    deps = [
        "//index_factory/plugin_impl:data_handler_impl",
        "//index_factory/public/writer/snap:snap_reader",
        "//index_factory/public/writer/snap:snap_writer",
    ],
)

cc_binary(
    name = "snap_statistics",
    srcs = ["snap_statistics.cpp"],
    visibility = ["//visibility:public"],
    deps = [
        "//index_factory/plugin_impl:data_handler_impl",
        "//index_factory/public/writer/snap:snap_reader",
        "//index_factory/public/writer/snap:snap_writer",
    ],
)

cc_binary(
    name = "disk_token_forward_pos_checker",
    srcs = ["disk_token_forward_pos_checker.cpp"],
    visibility = ["//visibility:public"],
    deps = [
        "//common/shard_format/attachment/manager:disk_token_forward_pos_manager",
        "//common/shard_format/attachment/manager:memory_token_forward_pos_manager",
        "//common/utils:rand_generator",
        "//common/utils/filesystem",
        "//index_factory/public/writer/attachment:data_array_attachment_writer",
        "//third_party/gflags",
    ],
)

cc_binary(
    name = "memory_token_forward_pos_checker",
    srcs = ["memory_token_forward_pos_checker.cpp"],
    visibility = ["//visibility:public"],
    deps = [
        "//common/shard_format/attachment/manager:memory_token_forward_pos_manager",
        "//common/utils:rand_generator",
        "//common/utils/filesystem",
        "//index_factory/native:libFalconForwardPos-lib",
        "//index_factory/public/writer/attachment:data_array_attachment_writer",
        "//third_party/gflags",
    ],
)

cc_binary(
    name = "create_data_from_index",
    srcs = ["create_data_from_index.cpp"],
    visibility = ["//visibility:public"],
    deps = [
        "//common/shard_format/section:section_producer",
        "//common/utils/filesystem:cache_file_writer",
        "//third_party/gflags",
    ],
)

cc_binary(
    name = "snap_builder_util",
    srcs = ["snap_builder_util.cpp"],
    visibility = ["//visibility:public"],
    deps = [
        "//common/utils/hash",
        "//common/utils/time:timer",
        "//index_factory/codec:section_data",
        "//index_factory/public/builder:attachment",
        "//index_factory/public/builder:doc_id",
        "//index_factory/public/builder:section",
        "//index_factory/public/proto:document_cc_proto",
        "//index_factory/public/writer/snap:snap_reader",
        "//third_party/gflags",
        "@com_google_protobuf//:json",
        "@rapidjson",
    ],
)

cc_binary(
    name = "index_statistics",
    srcs = ["index_statistics.cpp"],
    visibility = ["//visibility:public"],
    deps = [
        "//common/shard_format/docids:doc_id",
        "//common/shard_format/section:section_manager",
        "//common/shard_format/utils:index_impl",
        "//third_party/gflags",
    ],
)

cc_binary(
    name = "shard_cycle_statistics",
    srcs = ["shard_cycle_statistics.cpp"],
    visibility = ["//visibility:public"],
    deps = [
        "//common/utils/time:time_convert",
        "//index_factory/public/persistent:pessistent",
        "//index_factory/public/proto:shard_cycle_cc_proto",
        "//third_party/gflags",
    ],
)

cc_binary(
    name = "bundle_name",
    srcs = ["bundle_name.cpp"],
    visibility = ["//visibility:public"],
    deps = [
        "//common/indexing:base",
        "//common/shard_format/bundles:bundle_id_name_manager",
        "//third_party/gflags",
    ],
)

cc_binary(
    name = "embedding_id_checker",
    srcs = ["embedding_id_checker.cpp"],
    visibility = ["//visibility:public"],
    deps = [
        "//common/shard_format/docids:embedding_id_factory",
        "//common/shard_format/utils/hash:bucket_hash_unsorted_multi",
        "//common/shard_format/utils/hash:bucket_hash_unsorted_multi_writer",
        "//index_factory/public/writer/section/fusion_index:embedding_id_writer",
        "//third_party/gflags",
    ],
)

cc_binary(
    name = "timeliness_indexer_main",
    srcs = ["timeliness_indexer_main.cpp"],
    visibility = ["//visibility:public"],
    deps = [
        "//index_factory/timeliness:timeliness_indexer",
        "@com_google_googletest//:gtest_main",
        "@com_google_protobuf//:protobuf",
    ],
)

#cc_binary(
#    name = "hdfs_access_main",
#    srcs = ["hdfs_access_main.cpp"],
#    visibility = ["//visibility:public"],
#    deps = [
#        "//index_factory/hdfs_access",
#    ],
#)

cc_binary(
    name = "bucket_invert_index_checker",
    srcs = ["bucket_invert_index_checker.cpp"],
    deps = [
        "//common/public/shard_format/proto:shard_meta_cc_proto",
        "//common/public/shard_format/section/syntax_node:syntax_node_base",
        "//common/public/shard_format/utils:index",
        "//common/shard_format/docids:doc_id",
        "//common/shard_format/section:section_producer",
        "//common/shard_format/section/syntax_node",
        "//common/shard_format/section/syntax_node:all_node",
        "//common/shard_format/section/syntax_node:syntax_tool",
        "//common/utils:logging",
        "//index_factory/processors/falcon:build_section",
        "//index_factory/public/writer:doc_id_writer",
        "//index_factory/public/writer/section/fusion_index:fusion_index_writer",
        "//third_party/gflags",
        "@rapidjson",
    ],
)

cc_binary(
    name = "bucket_checker",
    srcs = ["bucket_checker.cpp"],
    deps = [
        "//common/public/shard_format/proto:shard_meta_cc_proto",
        "//common/public/shard_format/section/syntax_node:syntax_node_base",
        "//common/public/shard_format/utils:index",
        "//common/shard_format/docids:doc_id",
        "//common/shard_format/section:section_producer",
        "//common/shard_format/section/syntax_node",
        "//common/shard_format/section/syntax_node:all_node",
        "//common/shard_format/section/syntax_node:syntax_tool",
        "//common/utils:logging",
        "//index_factory/processors/falcon:build_section",
        "//index_factory/public/writer:doc_id_writer",
        "//index_factory/public/writer/section/fusion_index:fusion_index_writer",
        "//third_party/gflags",
        "@rapidjson",
    ],
)

cc_binary(
    name = "test_ivf_vector",
    srcs = ["test_ivf_vector.cpp"],
    deps = [
        "//common/public/shard_format/proto:shard_meta_cc_proto",
        "//common/public/shard_format/section/syntax_node:syntax_node_base",
        "//common/public/shard_format/utils:index",
        "//common/shard_format/section:section_producer",
        "//common/shard_format/section/syntax_node:all_node",
        "//common/shard_format/section/syntax_node:and_not_node",
        "//common/shard_format/section/syntax_node:not_node",
        "//common/shard_format/section/syntax_node:or_node",
        "//common/utils:logging",
        "//index_factory/codec:section_data",
        "//index_factory/public/writer/section/fusion_index:fusion_index_writer",
        "//third_party/gflags",
        "@jemalloc",
        "@rapidjson",
    ],
)

cc_binary(
    name = "blacklist_builder",
    srcs = [
        "blacklist_builder.cpp",
    ],
    deps = [
        "//falcon/public/proto/root:blacklist_cc_proto",
    ],
)

cc_binary(
    name = "create_meta",
    srcs = ["create_meta.cpp"],
    visibility = ["//visibility:public"],
    deps = [
        "//common/shard_format/bundles:bundle_file_reader",
        "//common/shard_format/docids:doc_id",
        "//common/utils:base",
        "//common/utils/filesystem:cache_file_writer",
        "//common/utils/time:timer",
        "//index_factory/public:common_func",
        "//index_factory/public/proto:falcon_index_builder_cc_proto",
        "@com_github_gflags_gflags//:gflags",
        "@com_google_absl//absl/strings",
        "@com_google_protobuf//:json",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_binary(
    name = "compare_index",
    srcs = ["compare_index.cpp"],
    visibility = ["//visibility:public"],
    deps = [
        "//common/shard_format/attachment:attachment_factory",
        "//common/shard_format/attachment/codec/decoder:memory_token_forward_pos_decoder",
        "//common/shard_format/docids:doc_id",
        "//common/shard_format/docids:doc_id_factory",
        "//common/shard_format/section:section_manager",
        "//common/shard_format/utils:index_impl",
        "//common/utils/filesystem:cache_file_reader",
        "//common/utils/filesystem:cache_file_writer",
        "//index_factory/base:doc_weight_token",
        "//index_factory/base:token_pool",
        "//index_factory/codec:section_data",
        "//index_factory/public/proto:check_doc_recall_attr_cc_proto",
        "//index_factory/public/proto:falcon_index_builder_cc_proto",
        "//index_factory/public/writer/snap:snap_reader",
        "@com_github_gflags_gflags//:gflags",
    ],
)

cc_binary(
    name = "blink_graph_index_checker",
    srcs = ["blink_graph_index_checker.cpp"],
    visibility = ["//visibility:public"],
    deps = [
        ":checker_util",
        "//common/shard_format/bundles:bundle_file_reader",
        "//common/shard_format/bundles:bundle_file_writer",
        "//common/shard_format/bundles/codec:bundle_decoder",
        "//common/shard_format/bundles/codec:bundle_decoder_interface",
        "//common/shard_format/fusion_index/embedding_index/quantizer_index/rabitq_index/rabitq_codec:blink_graph_rabitq_builder",
        "//common/shard_format/fusion_index/embedding_index/quantizer_index/rabitq_index/rabitq_codec:blink_graph_rabitq_searcher",
        "//common/shard_format/fusion_index/embedding_index/quantizer_index/rabitq_index/rabitq_codec:blink_graph_rabitq_searcher_adaptive",
        "//common/shard_format/fusion_index/embedding_index/quantizer_index/rabitq_index/rabitq_codec:blink_graph_zsq_searcher_adaptive",
        "//common/shard_format/fusion_index/embedding_index/quantizer_index/rabitq_index/rabitq_codec:blink_graph_zsq_builder",
        "//common/shard_format/section:section_producer",
        "//common/utils/time",
        "//index_factory/public/writer/section/fusion_index:fusion_index_writer",
        "@com_github_gflags_gflags//:gflags",
    ],
)

cc_binary(
    name = "build_local",
    srcs = ["build_local.cpp"],
    visibility = ["//visibility:public"],
    deps = [
        "//common/utils/filesystem:file",
        "//index_factory/index_builder:index_builder_impl",
        "//index_factory/native:libFalconBlacklist-lib",
        "@com_github_gflags_gflags//:gflags",
    ],
)

cc_binary(
    name = "build_local_v1",
    srcs = ["build_local_v1.cpp"],
    visibility = ["//visibility:public"],
    deps = [
        "//common/utils/filesystem:file",
        "//index_factory/index_builder:index_builder_impl",
        "@com_github_gflags_gflags//:gflags",
    ],
)

cc_binary(
    name = "realtime_invert_index_checker",
    srcs = ["realtime_invert_index_checker.cpp"],
    visibility = ["//visibility:public"],
    deps = [
        "//common/shard_format/realtime/invert_index/section:doc_invert_index_realtime_section",
        "//falcon/public/proto/realtime:realtime_document_cc_proto",
        "//index_factory/public/writer/snap:snap_reader",
        "@com_github_gflags_gflags//:gflags",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_binary(
    name = "hnsw_realtime_checker",
    srcs = ["hnsw_realtime_checker.cpp"],
    visibility = ["//visibility:public"],
    deps = [
        ":checker_util",
        "//common/shard_format/fusion_index/realtime/section:hnsw_realtime_section",
        "//common/utils/time",
        "//third_party/gflags",
        "@com_github_gflags_gflags//:gflags",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_binary(
    name = "realtime_builder_main",
    srcs = ["realtime_builder_main.cpp"],
    visibility = ["//visibility:public"],
    deps = [
        "//falcon/realtime/realtime_leaf/bootstrap:realtime_leaf_node",
        "//index_factory/public:common_func",
        "//index_factory/timeliness:timeliness_doc_reader",
        "//third_party/gflags",
        "@jemalloc",
    ],
)

cc_binary(
    name = "realtime_compare_index",
    srcs = ["realtime_compare_index.cpp"],
    visibility = ["//visibility:public"],
    deps = [
        "//common/index_store/network_store:network_index_store",
        "//common/shard_format/attachment:attachment_factory",
        "//common/shard_format/attachment/codec/decoder:memory_token_forward_pos_decoder",
        "//common/shard_format/docids:doc_id",
        "//common/shard_format/docids:doc_id_factory",
        "//common/shard_format/realtime/attachment_realtime:attachment_realtime_disk",
        "//common/shard_format/realtime/doc_id_realtime",
        "//common/shard_format/realtime/invert_index/section:doc_invert_index_realtime_section",
        "//common/shard_format/section:section_manager",
        "//common/shard_format/utils:index_impl",
        "//common/utils/filesystem:cache_file_reader",
        "//common/utils/filesystem:cache_file_writer",
        "//falcon/realtime/realtime_leaf/index:stable_segment",
        "//falcon/realtime/realtime_leaf/index:writable_memory_segment",
        "//index_factory/base:doc_weight_token",
        "//index_factory/base:token_pool",
        "//index_factory/codec:section_data",
        "//index_factory/public/proto:check_doc_recall_attr_cc_proto",
        "//index_factory/public/proto:falcon_index_builder_cc_proto",
        "//index_factory/public/writer/snap:snap_reader",
        "@com_github_gflags_gflags//:gflags",
    ],
)

cc_binary(
    name = "ivf_rabitq_index_checker",
    srcs = ["ivf_rabitq_index_checker.cpp"],
    visibility = ["//visibility:public"],
    deps = [
        ":checker_util",
        "//common/shard_format/bundles:bundle_file_writer",
        "//common/shard_format/bundles/codec:bundle_decoder",
        "//common/shard_format/bundles/codec:bundle_decoder_interface",
        "//common/shard_format/fusion_index/embedding_index/quantizer_index/rabitq_index/rabitq_codec:rabitq_searcher_binary",
        "//common/shard_format/section:section_producer",
        "//common/shard_format/fusion_index/embedding_index/quantizer_index/rabitq_index/rabitq_codec:extended_rabitq_binary_builder",
        "//common/shard_format/fusion_index/embedding_index/quantizer_index/rabitq_index/rabitq_codec:extended_rabitq_searcher",
        "//common/utils/time",
        "//index_factory/public/writer/section/fusion_index:fusion_index_writer",
        "@com_github_gflags_gflags//:gflags",
    ],
)

cc_binary(
    name = "bundle2fvecs",
    srcs = ["bundle2fvecs.cpp"],
    visibility = ["//visibility:public"],
    deps = [
        ":checker_util",
        "//common/shard_format/bundles:bundle_file_reader",
        "//common/utils/thread:simple_thread_pool",
        "//common/utils/time:timer",
        "//index_factory/codec:section_data",
        "//index_factory/public/writer/snap:snap_reader",
        "//third_party/gflags",
    ],
)

cc_binary(
    name = "vector_duplicate_checker",
    srcs = ["vector_duplicate_checker.cpp"],
    visibility = ["//visibility:public"],
    deps = [
        "//common/utils/filesystem:file",
        "//index_factory/codec:section_data",
        "//index_factory/public/writer/snap:snap_reader",
        "//third_party/gflags",
        "//tools/index_factory:checker_util",
    ],
)

cc_binary(
    name = "ground_truth_cpu",
    srcs = [
        "ground_truth_cpu.cpp",
    ],
    deps = [
        "//common/shard_format/fusion_index/space_calculator/space:space_factory",
        "//common/utils/filesystem:file",
        "//index_factory/codec:section_data",
        "//index_factory/public/writer/snap:snap_reader",
        "//third_party/gflags",
    ],
)

cc_binary(
    name = "compute_recall",
    srcs = [
        "compute_recall.cpp",
    ],
    deps = [
        "//common/shard_format/docids:doc_id_factory",
        "//common/shard_format/fusion_index/space_calculator/space:space_factory",
        "//common/shard_format/section:section_manager",
        "//common/utils/filesystem:file",
        "//index_factory/codec:section_data",
        "//index_factory/public/writer/snap:snap_reader",
        "//third_party/gflags",
    ],
)

cc_binary(
    name = "ivf_rabitq_bucket_uniformity",
    srcs = ["ivf_rabitq_bucket_uniformity.cpp"],
    visibility = ["//visibility:public"],
    deps = [
        "//common/shard_format/bundles:bundle_file_reader",
        "//common/shard_format/fusion_index/embedding_index/quantizer_index/rabitq_index/rabitq_codec:extended_rabitq_searcher",
        "//common/shard_format/utils:types",
        "@com_github_gflags_gflags//:gflags",
    ],
)

cc_binary(
    name = "bucketivf_section_size_checker",
    srcs = ["bucketivf_section_size_checker.cpp"],
    visibility = ["//visibility:public"],
    deps = [
        "//common/public/shard_format/proto:shard_meta_cc_proto",
        "//common/public/shard_format/utils:index",
        "//common/shard_format/bundles:bundle_file_reader",
        "//common/shard_format/bundles:bundle_ids",
        "//common/shard_format/bundles/codec:bundle_decoder",
        "//common/shard_format/bundles/codec:bundle_decoder_interface",
        "//common/shard_format/utils:index_impl",
        "//index_factory/public/proto:document_cc_proto",
        "//index_factory/public/writer/snap:snap_reader",
        "@com_github_gflags_gflags//:gflags",
    ],
)

# blacklist docids 构建 CLI（原 //tools:blacklist_main）
cc_binary(
    name = "blacklist_main",
    srcs = ["blacklist_main.cpp"],
    visibility = ["//visibility:public"],
    deps = [
        "//index_factory/native:libFalconBlacklist-lib",
        "@com_github_gflags_gflags//:gflags",
    ],
)

# 索引下载 / NSP 推送（原 //tools/load）
cc_binary(
    name = "download_main",
    srcs = ["download_main.cpp"],
    deps = [
        "//common/utils/load:download_producer",
        "@com_github_gflags_gflags//:gflags",
    ],
)

cc_binary(
    name = "nsp_main",
    srcs = ["nsp_main.cpp"],
    visibility = ["//visibility:public"],
    deps = [
        "//common/utils/nsp:nsp_client",
        "@com_github_gflags_gflags//:gflags",
    ],
)
