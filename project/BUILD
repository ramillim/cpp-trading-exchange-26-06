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
