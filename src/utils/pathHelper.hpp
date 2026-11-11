// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright (C) 2022 Greg Davill <greg.davill@gmail.com>
 */

#ifndef SRC_PATHHELPER_HPP_
#define SRC_PATHHELPER_HPP_

#include <string>

class PathHelper{
    public:
        /*
         * Absolute path of the openFPGALoader data directory (the directory
         * containing spiOverJtag_*.bit.gz, bpiOverJtag_*.bit.gz, xusb_*.hex,
         * from_ise/ and drivers/).
         *
         * Resolution order — the first existing directory wins, the last
         * candidate is returned when none exists:
         *   1. $OPENFPGALOADER_DATA_DIR           explicit override
         *   2. <exe dir>/share/openFPGALoader     installed layout
         *   3. <exe dir>/../share/openFPGALoader  portable layout (bin/ + share/)
         *   4. <cwd>/share/openFPGALoader         source tree layout
         *   5. <cwd>/../share/openFPGALoader
         *   6. compiled-in DATA_DIR/openFPGALoader (install prefix)
         */
        static std::string dataDir();
        static std::string absolutePath(std::string input_path);
};

#endif  // SRC_PATHHELPER_HPP_
