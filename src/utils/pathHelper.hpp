// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright (C) 2022 Greg Davill <greg.davill@gmail.com>
 */

#ifndef SRC_PATHHELPER_HPP_
#define SRC_PATHHELPER_HPP_

#include <string>

class PathHelper{
    public:
        static std::string absolutePath(std::string input_path);
};

#endif  // SRC_PATHHELPER_HPP_
