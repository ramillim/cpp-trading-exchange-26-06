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

cc_test(
    name = "order_book_test",
    srcs = ["order_book_test.cpp"],
    copts = [
        "-Wall",
        "-Wextra",
        "-Werror",
        "-Wpedantic",
    ],
    deps = [
        "//project:order_book",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)
