load("@bazel_tools//tools/build_defs/pkg:pkg.bzl", "pkg_tar")
load("@capnp//:rules.bzl", "cc_capnp_library")
load("//:tools/rules.bzl", "slipstream_test")
package(default_visibility = ["//visibility:public"])

cc_capnp_library(
    name = "slipstream-capnp",
    include_prefix = "lt/slipstream/capnp",
    strip_include_prefix =
        "src",
    srcs = [
        "src/slipstream.capnp",
    ],
)

cc_library(
    name = "slipstream",
    strip_include_prefix =
        "include",
    hdrs = glob([
        "include/**/*.h",
    ]),
    srcs = glob([
        "src/**/*.cpp",
        "src/**/*.h",
    ]),
    deps = [
        "@cframework//:core",
        "slipstream-capnp",
    ],
)

cc_library(
    name = "cli-support",
    strip_include_prefix =
        "cli-support",
    hdrs = glob([
        "cli-support/**/*.h",
    ]),
    deps = [
        "@boost//:headers",
        "@dimcli",
        "slipstream",
    ],
)

cc_binary(
    name = "bin/slipstream",
    srcs = glob([
        "cli/**/*.cpp",
        "cli/**/*.h",
    ]),
    deps = [
        "cli-support",
    ],
)

cc_library(
    name = "server",
    strip_include_prefix =
        "server",
    hdrs = glob([
        "server/**/*.h",
    ]),
    deps = [
        "@boost//:regex",
        "@seasocks",
        "slipstream",
    ],
)

cc_binary(
    name = "bin/slipstream-log-server",
    copts = [
        "-O0",
    ],
    srcs = glob([
        "log-server/**/*.cpp",
        "log-server/**/*.h",
    ]),
    deps = [
        "@dimcli",
        "server",
    ],
)

cc_library(
    name = "testing-support",
    strip_include_prefix =
        "testing-support",
    hdrs = glob([
        "testing-support/**/*.h",
    ]),
    deps = [
        "@boost//:unit_test_framework",
        "@rapidcheck",
        "slipstream",
    ],
)

cc_capnp_library(
    name = "test-delta-capnp",
    include_prefix = "lt/slipstream/capnp",
    strip_include_prefix =
        "test",
    srcs = [
        "test/test_delta.capnp",
    ],
)

slipstream_test("test_timestamp")
slipstream_test("test_framing")
slipstream_test("test_envelope")
slipstream_test("test_binary")
slipstream_test("test_plaintext")
slipstream_test("test_variant")
slipstream_test("test_integer", deps=["test-delta-capnp"])

pkg_tar(
    name = "package/slipstream",
    extension = "tar.gz",
    package_dir = "slipstream/bin",
    srcs = [
        "bin/slipstream",
        "bin/slipstream-log-server",
    ],
)
