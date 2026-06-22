load("@rules_cc//cc:defs.bzl", "cc_test")

cc_test(
    name = "mem_pool_test",
    srcs = ["mem_pool_test.cpp"],
    copts = [
        "-Wall",
        "-Wextra",
        "-Werror",
        "-Wpedantic",
        "-Wno-character-conversion",
    ],
    deps = [
        "//project:mem_pool",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)
