// SPDX-License-Identifier: Apache-2.0
#ifndef SRC_TOOLS_SPI_DEBUG_HPP_
#define SRC_TOOLS_SPI_DEBUG_HPP_

#include <string>

class Jtag;

int run_spi_debug(Jtag *jtag, const std::string &fpga_part);

#endif
