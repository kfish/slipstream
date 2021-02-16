def slipstream_test(name, timeout = "short", srcs = [], deps = []):
    native.cc_test(
        name = name,
        timeout = "short",
        srcs = srcs + [
            "test/" + name + ".cpp",
        ],
        deps = deps + [
            "testing-support",
        ],
    )
