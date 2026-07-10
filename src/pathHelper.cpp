// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright (C) 2022 Greg Davill
 */

#if defined(_WIN64) || defined(_WIN32)

#include "pathHelper.hpp"

#include <algorithm>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <windows.h>

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
}

std::string absoluteWindowsPath(const std::filesystem::path &path)
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
            return absoluteWindowsPath(executableDirectory() / path);
        }

        return absoluteWindowsPath(path);
    } catch (...) {
        return input_path;
    }
}

#endif