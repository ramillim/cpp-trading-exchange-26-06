load("@rules_cc//cc:defs.bzl", "cc_binary", "cc_library")

cc_library(
    name = "mem_pool",
    hdrs = ["mem_pool.hpp"],
    visibility = ["//visibility:public"],
)

cc_library(
    name = "mem_pool_separate_free_structure",
    hdrs = ["mem_pool_separate_free_structure.hpp"],
    visibility = ["//visibility:public"],
)

cc_binary(
    name = "mem_pool_benchmark",
    srcs = ["mem_pool_benchmark.cpp"],
    copts = [
        "-Wall",
        "-Wextra",
        "-Werror",
        "-Wpedantic",
    ],
    deps = [
        "//common:perf_utils",
        "//project:mem_pool",
    ],
)

cc_binary(
    name = "mem_pool_separate_free_structure_benchmark",
    srcs = ["mem_pool_separate_free_structure_benchmark.cpp"],
    copts = [
        "-Wall",
        "-Wextra",
        "-Werror",
        "-Wpedantic",
    ],
    deps = [
        "//common:perf_utils",
        "//project:mem_pool_separate_free_structure",
    ],
)

cc_library(
    name = "intrusive_list",
    hdrs = ["intrusive_list.hpp"],
    visibility = ["//visibility:public"],
)

cc_library(
    name = "order",
    hdrs = ["order.hpp"],
    visibility = ["//visibility:public"],
    deps = [
        ":intrusive_list",
        ":order_type",
    ],
)

cc_library(
    name = "order_book",
    srcs = ["order_book.cpp"],
    hdrs = ["order_book.hpp"],
    visibility = ["//visibility:public"],
    deps = [
        ":intrusive_list",
        ":mem_pool",
        ":order",
    ],
)

cc_library(
    name = "order_type",
    hdrs = ["order_type.hpp"],
    visibility = ["//visibility:public"],
)
