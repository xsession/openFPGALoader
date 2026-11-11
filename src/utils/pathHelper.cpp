// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright (C) 2022 Greg Davill
 */

#include "utils/pathHelper.hpp"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <vector>
#if defined(_WIN64) || defined(_WIN32)
#include <windows.h>
#endif

namespace {

#if defined(_WIN64) || defined(_WIN32)
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
#endif

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

bool directoryExists(const std::filesystem::path &path)
{
    std::error_code ec;
    return std::filesystem::is_directory(path, ec);
}

} // namespace

std::string PathHelper::dataDir()
{
    /*
     * Resolved once per process: the environment and the install layout are
     * fixed before openFPGALoader starts.
     *
     * The same cross-compiled executable ships in two different layouts, so
     * the data directory cannot be a single compile-time path:
     *
     *   portable ZIP:   <root>/bin/openFPGALoader.exe + <root>/share/openFPGALoader
     *   installed:      <app>/openFPGALoader.exe      + <app>/share/openFPGALoader
     *   native (e.g. /usr/local): <prefix>/bin/...    + <prefix>/share/openFPGALoader
     *
     * Candidates are tried in order; the first existing directory wins. The
     * last candidate (compiled-in DATA_DIR) is returned when nothing exists,
     * so error messages still point at a concrete path.
     */
    static const std::string resolved = []() -> std::string {
        std::vector<std::string> candidates;

        const char *env = std::getenv("OPENFPGALOADER_DATA_DIR");
        if (env != nullptr && env[0] != '\0') {
            candidates.push_back(std::string(env));
        }

        try {
            const std::filesystem::path exe_dir = executableDirectory();
            candidates.push_back((exe_dir / "share" / "openFPGALoader").string());
            candidates.push_back((exe_dir / ".." / "share" / "openFPGALoader").string());
        } catch (...) {
            /* fall through to cwd-based candidates */
        }

        const std::filesystem::path cwd = std::filesystem::current_path();
        candidates.push_back((cwd / "share" / "openFPGALoader").string());
        candidates.push_back((cwd / ".." / "share" / "openFPGALoader").string());

        candidates.push_back(std::string(DATA_DIR) + "/openFPGALoader");

        for (const auto &candidate : candidates) {
            const std::string absolute = absoluteFilesystemPath(candidate);
            if (directoryExists(absolute)) {
                return absolute;
            }
        }

        return absoluteFilesystemPath(candidates.back());
    }();

    return resolved;
}

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
