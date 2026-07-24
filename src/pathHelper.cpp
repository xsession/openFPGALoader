// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright (C) 2022 Greg Davill
 */

#include "pathHelper.hpp"

#include <algorithm>
#include <filesystem>
#include <stdexcept>
#include <string>
#if defined(_WIN64) || defined(_WIN32)
#include <windows.h>
#endif

namespace {

std::string normalizeSlashes(std::string path)
{
    std::replace(path.begin(), path.end(), '\\', '/');
    return path;
}

bool isPortableDataPath(const std::string &input_path)
{
    const std::string path = normalizeSlashes(input_path);

    return path.rfind("../share/openFPGALoader/", 0) == 0 ||
           path.rfind("./share/openFPGALoader/", 0) == 0 ||
           path.rfind("share/openFPGALoader/", 0) == 0;
}

std::filesystem::path executableDirectory()
{
#if defined(_WIN64) || defined(_WIN32)
    std::string buffer(MAX_PATH, '\0');

    for (;;) {
        DWORD len = GetModuleFileNameA(
            nullptr,
            buffer.data(),
            static_cast<DWORD>(buffer.size())
        );

        if (len == 0) {
            throw std::runtime_error("GetModuleFileNameA failed");
        }

        if (len < buffer.size() - 1) {
            buffer.resize(len);
            return std::filesystem::path(buffer).parent_path();
        }

        buffer.resize(buffer.size() * 2);
    }
#else
    return std::filesystem::current_path();
#endif
}

std::string absoluteFilesystemPath(const std::filesystem::path &path)
{
    std::error_code ec;
    std::filesystem::path absolute = std::filesystem::absolute(path, ec);

    if (ec) {
        return path.string();
    }

    return absolute.lexically_normal().string();
}

} // namespace

std::string PathHelper::absolutePath(std::string input_path)
{
    try {
        std::filesystem::path path(input_path);

#if defined(_WIN64) || defined(_WIN32)
        /*
         * For portable Windows builds, DATA_DIR is compiled as "../share".
         * Resource paths such as:
         *
         *   ../share/openFPGALoader/spiOverJtag_xc7a35tfgg484.bit.gz
         *   ../share/openFPGALoader/xusb_emb.hex
         *
         * must be resolved relative to openFPGALoader.exe, not relative to the
         * shell working directory and not through MSYS2/cygpath.
         */
        if (path.is_relative() && isPortableDataPath(input_path)) {
            return absoluteFilesystemPath(executableDirectory() / path);
        }
#endif

        return absoluteFilesystemPath(path);
    } catch (...) {
        return input_path;
    }
}
