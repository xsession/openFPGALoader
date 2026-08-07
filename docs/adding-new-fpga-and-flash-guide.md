# Adding New FPGA and Flash Support to openFPGALoader - Comprehensive Guide

This document covers all modification points required to add support for a new FPGA family, device variant, or SPI flash chip to openFPGALoader.

---

## Table of Contents

1. [Architecture Overview](#1-architecture-overview)
2. [Adding a New FPGA Device](#2-adding-a-new-fpga-device)
3. [Adding a New FPGA Family](#3-adding-a-new-fpga-family)
4. [Adding a New SPI Flash Device](#4-adding-a-new-spi-flash-device)
5. [Adding a New Board](#5-adding-a-new-board)
6. [Adding SPI-over-JTAG Bridge Bitstreams](#6-adding-spi-over-jtag-bridge-bitstreams)
7. [Adding a New JTAG Cable](#7-adding-a-new-jtag-cable)
8. [IR Code Maps and Multi-byte Instructions](#8-ir-code-maps-and-multi-byte-instructions)
9. [SOJ (SPI-over-JTAG) Version Detection](#9-soj-version-detection)
10. [Vendor-specific Implementation Patterns](#10-vendor-specific-implementation-patterns)
11. [Build System and Data File Installation](#11-build-system-and-data-file-installation)
12. [Testing Checklist](#12-testing-checklist)

---

## 1. Architecture Overview

openFPGALoader follows a layered architecture:

```
Command line (main.cpp)
    |
    v
Device abstraction (Device class tree)
    |-- Xilinx
    |-- Lattice
    |-- Gowin
    |-- Altera/Intel
    |-- Efinix
    |-- Anlogic
    |-- CologneChip/GateMate
    |
    v
Flash Interface (FlashInterface abstract class)
    |-- SPIFlash (SPI NOR flash)
    |-- BPIFlash (BPI flash - Xilinx)
    |
    v
JTAG/SPI transport (Cable classes)
    |-- Digilent (HS-2, HS-3)
    |-- FT2232/FT4232 (MPSSE)
    |-- Xilinx Platform USB
    |-- CMSIS-DAP
    |-- ...
```

### Key files:

| File | Purpose |
|------|---------|
| `src/part.hpp` | FPGA JTAG IDCODE database |
| `src/spiFlashdb.hpp` | SPI flash JEDEC ID database |
| `src/board.hpp` | Board definitions (cable + FPGA mapping) |
| `src/xilinx.cpp/hpp` | Xilinx family implementation |
| `src/lattice.cpp/hpp` | Lattice family implementation |
| `src/gowin.cpp/hpp` | Gowin family implementation |
| `src/altera.cpp/hpp` | Altera/Intel family implementation |
| `src/efinix.cpp/hpp` | Efinix family implementation |
| `src/anlogic.cpp/hpp` | Anlogic family implementation |
| `src/colognechip.cpp/hpp` | CologneChip/GateMate implementation |
| `src/spiFlash.cpp/hpp` | SPI flash operations |
| `src/flashInterface.cpp/hpp` | Flash abstraction layer |
| `src/bpiFlash.cpp/hpp` | BPI flash operations |
| `src/main.cpp` | CLI argument parsing and dispatch |

---

## 2. Adding a New FPGA Device

The simplest case: your FPGA family is already supported (e.g., Xilinx Artix-7, Lattice ECP5), but a specific device variant is missing from the database.

### 2.1 Find the JTAG IDCODE

Connect the FPGA and run openFPGALoader with verbose output:

```bash
openFPGALoader -c <cable> -v
```

Look for the JTAG chain output:

```
found 1 devices
JTAG chain: [0]=0x03636093
```

The 32-bit value (e.g., `0x03636093`) is the device IDCODE.

**IDCODE structure (IEEE 1149.1):**
```
| Version (4 bits) | Part Number (16 bits) | Manufacturer (11 bits) | '1' (1 bit) |
|         31:28    |          20:5             |         4:1         |     0      |
```

Manufacturer IDs:
- `0x049` (bits 4:1 = `0x93`) = Xilinx
- `0x021` (bits 4:1 = `0x43`) = Lattice
- `0x06E` (bits 4:1 = `0x6E`) = Altera/Intel
- `0x40D` (bits 4:1 = `0x1B`) = Gowin
- `0x53C` (bits 4:1 = `0x35`) = Efinix
- `0x61A` (bits 4:1 = `0x35`) = Anlogic
- `0x000` (bits 4:1 = `0x01`) = CologneChip/Efinix Trion T4/T8

### 2.2 Add to FPGA Database

File: `src/part.hpp`

Find the appropriate family section and add your device:

```cpp
typedef struct {
    std::string manufacturer;
    std::string family;
    std::string model;
    int irlength;   // IR (Instruction Register) length in bits
} fpga_model;

static std::map <uint32_t, fpga_model> fpga_list = {
    // Example: Add new Xilinx Artix-7 variant
    {0x03637093, {"xilinx", "artix a7 35t", "xc7a35t", 6}},
    
    // Example: Add new Lattice ECP5 variant
    {0x41114043, {"lattice", "ECP5", "LFE5U-85F", 8}},
};
```

### 2.3 IR Length Reference

Common IR lengths by family:

| Family | Typical IR Length |
|--------|-------------------|
| Xilinx 7-series (Artix, Kintex, Virtex, Zynq) | 6 bits |
| Xilinx Spartan-6 | 6 bits |
| Xilinx Spartan-7 | 6 bits |
| Xilinx UltraScale/UltraScale+ | 6 bits (up to 24 bits for large Virtex) |
| Xilinx ZynqMP (PL side) | 6 bits |
| Xilinx ZynqMP (PS/config side) | 4 bits |
| Xilinx Spartan-3 | 6 bits |
| Xilinx XCF | 8 or 16 bits |
| Lattice iCE40 | 8 bits |
| Lattice ECP5 | 8 bits |
| Lattice MachXO2/3 | 8 bits |
| Gowin GW1N/GW2A/GW5A | 8 bits |
| Altera Cyclone/Max | 10 bits |
| Efinix Trion/Titanium | 8 bits |
| Anlogic | 8 bits |

### 2.4 Add Bridge Bitstream (if needed for SPI flash access)

If the device needs SPI flash access via JTAG, you need a corresponding SPI-over-JTAG bitstream. See [Section 6](#6-adding-spi-over-jtag-bridge-bitstreams).

---

## 3. Adding a New FPGA Family

When an entirely new FPGA family needs support (e.g., a new vendor or fundamentally different architecture).

### 3.1 Create Vendor Files

Create a new pair of files:
- `src/<vendor>.cpp` - Implementation
- `src/<vendor>.hpp` - Header

### 3.2 Header File Structure

```cpp
#ifndef SRC_VENDOR_HPP_
#define SRC_VENDOR_HPP_

#include "device.hpp"

class Vendor : public Device {
public:
    Vendor(Jtag *jtag, std::string filename, const std::string &file_type,
           bool verify, int8_t verbose);
    virtual ~Vendor();

    void flash() override;
    void dump(const std::string &filename, const std::string &file_ext) override;
    void verify(const std::string &filename) override;
    void reset() override;

private:
    // SPI flash interface (overrides FlashInterface)
    int spi_put(uint8_t cmd, const uint8_t *tx, uint8_t *rx, uint32_t len) override;
    int spi_put(const uint8_t *tx, uint8_t *rx, uint32_t len) override;
    int spi_wait(uint8_t cmd, uint8_t mask, uint8_t cond,
                 uint32_t timeout, bool verbose = false) override;
    
    bool prepare_flash_access() override;
    bool post_flash_access() override;
    
    // JTAG instructions
    void shift_ir(uint8_t ir);
    void shift_dr(const uint8_t *tx, uint8_t *rx, uint32_t bits);
    
    // Configuration
    void program(ConfigBitstreamParser *bitfile);
    
    // Member variables
    int _irlen;
    std::string _user_instruction;
};

#endif
```

### 3.3 Implementation Key Methods

**Constructor:**
```cpp
Vendor::Vendor(Jtag *jtag, std::string filename, const std::string &file_type,
               bool verify, int8_t verbose)
    : Device(jtag, filename, file_type, verify, verbose),
      _irlen(8)  // default IR length
{
}
```

**flash() - Program FPGA from file:**
```cpp
void Vendor::flash()
{
    ConfigBitstreamParser *bitfile = nullptr;
    open_bitfile(_filename, _file_extension, &bitfile, _reverse, _verbose);
    
    // Family-specific programming sequence
    program(bitfile);
    
    if (_verify) {
        verify(_filename);
    }
    delete bitfile;
}
```

**spi_put() - SPI command through JTAG:**
```cpp
int Vendor::spi_put(uint8_t cmd, const uint8_t *tx, uint8_t *rx, uint32_t len)
{
    uint8_t jtx[len + 2];
    uint8_t jrx[len + 2];
    
    jtx[0] = reverseByte(cmd);  // SPI commands are bit-reversed over JTAG
    if (tx != NULL) {
        for (uint32_t i = 0; i < len; i++)
            jtx[i + 1] = reverseByte(tx[i]);
    }
    
    _jtag->shiftIR(get_ircode("USER1"), NULL, _irlen);
    _jtag->shiftDR(jtx, jrx, 8 * (len + 2));
    _jtag->flush();
    
    if (rx != NULL) {
        for (uint32_t i = 0; i < len; i++)
            rx[i] = reverseByte(jrx[i + 1]);
    }
    
    return 0;
}
```

### 3.4 Register the New Family

**In `src/main.cpp`, add the family to device detection:**

Find the device creation section (search for `case XILINX:` or similar pattern):

```cpp
// Add your vendor enum/constant
if (family == "vendor") {
    device = new Vendor(_jtag, filename, file_type, verify, verbose);
}
```

### 3.5 Bitstream Parser (if needed)

If the FPGA uses a proprietary configuration format:
- Create `src/<vendor>BitParser.cpp/hpp`
- Inherit from `ConfigBitstreamParser`
- Implement `parse()` method
- Register in `open_bitfile()` function in the vendor's cpp file

---

## 4. Adding a New SPI Flash Device

### 4.1 Find the JEDEC ID

When openFPGALoader encounters an unknown flash, it outputs:

```
SPI flash RDID succeeded, but this chip is not in openFPGALoader's SPI flash database
JEDEC ID: 0x20ba18
Manufacturer byte: 0x20 (ST/Micron)
Memory type byte: 0xba
Memory capacity byte: 0x18
Common JEDEC capacity decode: 16 MiB (128 Mbit)
```

The program also generates a starter entry for `spiFlashdb.hpp`:
```
{0x20ba18, {
    .manufacturer = "ST/Micron",
    .model = "<exact part number>",
    .nr_sector = 256,
    .sector_erase = true,      /* check datasheet */
    .subsector_erase = true,   /* check 4 KiB erase support */
    .has_extended = false,
    .tb_otp = false,
    .tb_offset = (1 << 5),     /* check BP/TB bits */
    .tb_register = STATR,
    .bp_len = 3,
    .bp_offset = {(1 << 2), (1 << 3), (1 << 4), 0},
    .quad_register = NONER,
    .quad_mask = 0,
    .global_lock = false,
}},
```

### 4.2 SPI Flash Database Structure

File: `src/spiFlashdb.hpp`

```cpp
typedef struct {
    std::string manufacturer;   /**< Manufacturer name */
    std::string model;          /**< Chip part number */
    uint32_t  nr_sector;        /**< Number of 64 KiB sectors */
    bool      sector_erase;     /**< 64KB erase support (FLASH_BE64 = 0xD8) */
    bool      subsector_erase;  /**< 4KB erase support (FLASH_SE = 0x20) */
    bool      has_extended;     /**< Extended address mode (4-byte addresses) */
    bool      tb_otp;          /**< TOP/BOTTOM is One-Time Programmable */
    uint16_t  tb_offset;        /**< TOP/BOTTOM bit position in status register */
    tb_loc_t  tb_register;      /**< Which register holds TB bit */
    uint8_t   bp_len;           /**< Number of BP (Block Protect) bits */
    uint8_t   bp_offset[4];     /**< Bit positions for BP[0:3] (0 = not present) */
    tb_loc_t  quad_register;    /**< Which register holds Quad Enable bit */
    uint16_t  quad_mask;        /**< Quad Enable bit position (0 = not applicable) */
    bool      global_lock;      /**< Has global lock/unlock mechanism */
} flash_t;

// tb_loc_t enum:
typedef enum {
    STATR   = 0,  /**< Status Register (0x05 read) */
    FUNCR   = 1,  /**< Function Register (0x48 read) - ISSI */
    CONFR   = 2,  /**< Configuration Register (0x35 read) */
    NVCONFR = 3,  /**< Non-volatile Configuration Register */
    NONER   = 99, /**< Not present / not applicable */
} tb_loc_t;
```

### 4.3 Key Fields Explained

**nr_sector:**
- Number of 64 KiB sectors in the flash
- Calculated from datasheet: `capacity_bytes / 0x10000`
- Example: 16 MiB = 16 * 1024 * 1024 / 0x10000 = 256 sectors

**sector_erase / subsector_erase:**
- Check the SPI flash command table in the datasheet
- Most modern flashes support both 4KB (0x20) and 64KB (0xD8) erase
- Some older flashes (like ST M25P series) only support 64KB erase
- If `subsector_erase = false`, the tool falls back to 64KB-only erase

**has_extended:**
- Set to `true` if the flash supports 4-byte address commands
- Required for flashes > 128 MiB (addresses > 0xFFFFFF)
- Check for FLASH_4READ (0x13), FLASH_4PP (0x12), FLASH_4SE (0x21) in datasheet

**tb_otp:**
- Set to `true` if the TOP/BOTTOM bit is OTP (One-Time Programmable)
- Example: ST M25P series has OTP TB bit
- If OTP, the tool will warn about irreversible changes

**tb_offset:**
- Bit position of the TOP/BOTTOM select bit in the register
- `(1 << 5)` means bit 5
- Check status register bit map in datasheet

**tb_register:**
- `STATR` = Status Register (most common)
- `CONFR` = Configuration Register (Spansion/Macronix)
- `FUNCR` = Function Register (ISSI)
- `NVCONFR` = Non-volatile Configuration Register
- `NONER` = No TB support

**bp_len:**
- Number of Block Protect bits (typically 3 or 4)
- Determines how many protection levels are available

**bp_offset[4]:**
- Bit positions for BP0 through BP3 in the status register
- `0` in any position means that BP bit doesn't exist
- Example: `{(1 << 2), (1 << 3), (1 << 4), 0}` = BP0=bit2, BP1=bit3, BP2=bit4

**quad_register / quad_mask:**
- Where the Quad Enable (QE) bit lives
- `quad_register = STATR, quad_mask = (1 << 6)` = bit 6 of status register
- `quad_register = NVCONFR, quad_mask = (1 << 3)` = bit 3 of NV config register
- `quad_register = NONER, quad_mask = 0` = no QE bit (always supports quad or not)

**global_lock:**
- Set to `true` for Microchip SST26VF032B/BA family
- These chips have a global unlock command (0x98) instead of per-sector protection

### 4.4 Adding Multiple Keys (Aliases)

Some flashes need multiple entries for the same chip:

```cpp
// Standard JEDEC ID
{0xba2119, { /* MT25QL256ABA standard RDID */ }},

// SOJ v1 bridge transforms RDID (0x9F) -> RFP (0x5F), 
// so the flash returns different bytes
{0x5ffb8c, { /* Same MT25QL256ABA but via SOJ v1 */ }},

// Extended electronic signature (0xAB command)
{0x4000190c, { /* Same MT25QL256ABA via electronic signature */ }},
```

**SOJ v1 Bridge RDID Transformation:**
When using Xilinx SPI-over-JTAG v1 bridge through certain FPGA families (Artix-7, Kintex-7), the JTAG-to-SPI bridge may transform the RDID command (0x9F) into Read Flash Parameters (0x5F). This causes the flash to return its RFP response instead of its JEDEC ID. You need to add an alias entry with the RFP-based ID.

### 4.5 Adding New JEDEC Manufacturer

If you encounter a manufacturer not in the name lookup:

File: `src/spiFlash.cpp` (line ~179)

```cpp
const char *jedec_manufacturer_name(uint8_t id)
{
    switch (id) {
    case 0x01: return "Spansion/Cypress/AMD";
    case 0x1c: return "EON";
    case 0x1f: return "Atmel/Adesto";
    case 0x20: return "ST/Micron";
    case 0x85: return "Puya";
    case 0x9d: return "ISSI";
    case 0xbf: return "Microchip/SST";
    case 0xc2: return "Macronix";
    case 0xc8: return "GigaDevice";
    case 0xef: return "Winbond";
    // Add new manufacturer:
    case 0x38: return "ISSI (alternative)";
    default: return "unknown";
    }
}
```

### 4.6 Adding New SPI Commands

If the flash uses non-standard SPI commands:

File: `src/spiFlash.cpp` (top of file, ~line 27)

```cpp
// Add your new command definition
#define FLASH_VENDOR_ERASE 0x44  /* Example: vendor-specific erase */
```

Then modify the relevant methods (`sector_erase()`, `block64_erase()`, `write_page()`) to use the new command when appropriate. You may need to check `*_flash_model` properties to conditionally select commands.

### 4.7 Extended ID Handling

Some flashes (like Xilinx XCF) use 4-byte IDs instead of 3-byte JEDEC IDs. The key in `flash_list` is the full ID value:

```cpp
// XCF32P uses 4-byte electronic signature
{0x05059093, { /* XCF entry */ }},
```

The `has_extended` field in `flash_t` controls whether the flash uses 4-byte address commands for read/write/erase operations.

---

## 5. Adding a New Board

### 5.1 Board Definition Structure

File: `src/board.hpp`

```cpp
typedef struct {
    std::string manufacturer;        // Board manufacturer name
    std::string cable_name;          // Reference to cable in cable_list
    std::string fpga_part;           // FPGA part name (must match part.hpp model)
    uint16_t    reset_pin;           // Reset pin (FT MPSSE pin or 0)
    uint16_t    done_pin;            // DONE pin (FT MPSSE pin or 0)
    uint16_t    oe_pin;              // Output Enable pin (SPI mode)
    uint16_t    mode;                // COMM_JTAG, COMM_SPI, or COMM_DFU
    uint8_t     spi_bpi;             // SPI_FLASH (0) or BPI_FLASH (1)
    jtag_pins_conf_t jtag_pins_config; // JTAG pin mapping (bitbang mode)
    spi_pins_conf_t  spi_pins_config;  // SPI pin mapping (SPI mode)
    uint32_t    default_freq;        // Default clock (0 = cable default)
    uint16_t    vid;                 // USB VID (DFU mode)
    uint16_t    pid;                 // USB PID (DFU mode)
    int16_t     altsetting;          // DFU altsetting
} target_board_t;
```

### 5.2 Board Macros

Use the provided macros for convenience:

```cpp
// JTAG board (simplest)
JTAG_BOARD("board_name", "fpga_part", "cable_name", SPI_FLASH, reset, done, freq)

// JTAG board with bitbang pins (FT232R/FT2232)
JTAG_BITBANG_BOARD("board_name", "fpga_part", "cable_name", SPI_FLASH,
                   reset, done, tms_pin, tck_pin, tdi_pin, tdo_pin, freq)

// Direct SPI board (no JTAG)
SPI_BOARD("board_name", "manufacturer", "fpga_part", "cable_name",
          reset, done, oe,
          cs_pin, sck_pin, mosi_pin, miso_pin, holdn_pin, wpn_pin, freq)

// DFU board (USB DFU programming)
DFU_BOARD("board_name", "fpga_part", "dfu", vid, pid, altsetting)
```

### 5.3 Example Board Additions

```cpp
// Add a new Digilent-style JTAG board
JTAG_BOARD("my_board", "xc7a35tcsg324", "digilent", SPI_FLASH, 0, 0, CABLE_DEFAULT),

// Add a board with custom FT2232 bitbang JTAG
JTAG_BITBANG_BOARD("my_ft_board", "", "ft2232", SPI_FLASH, 0, 0,
                   FT232RL_RTS, FT232RL_TXD, FT232RL_DTR, FT232RL_RXD,
                   CABLE_DEFAULT),

// Add a direct SPI board
SPI_BOARD("my_spi_board", "lattice", "ice40", "ft2232",
          0, 0, 0,
          DBUS3, DBUS0, DBUS1, DBUS2, 0, 0, CABLE_DEFAULT),
```

### 5.4 Pin Constants

```cpp
// FT232R bitbang pins
FT232RL_TXD = 0, FT232RL_RXD = 1, FT232RL_RTS = 2, FT232RL_CTS = 3
FT232RL_DTR = 4, FT232RL_DSR = 5, FT232RL_DCD = 6, FT232RL_RI = 7

// FT2232 MPSSE pins
DBUS0-DBUS7 = data bus pins (bits 0-7)
CBUS0-CBUS7 = CBUS configurable pins (bits 8-15)
```

### 5.5 BPI vs SPI Flash

Set `spi_bpi` to `BPI_FLASH` (1) for boards that use BPI (Bidirectional Parallel Interface) flash instead of SPI:

```cpp
JTAG_BOARD("ypcb003381p1", "xc7k480tffg1156", "", BPI_FLASH, 0, 0, CABLE_DEFAULT),
```

BPI flash uses a different protocol entirely (parallel reads, one word per USB round-trip).

---

## 6. Adding SPI-over-JTAG Bridge Bitstreams

### 6.1 Bridge Bitstream Naming Convention

Bridge bitstreams are stored in the installation share directory:

```
<install_prefix>/share/openFPGALoader/
├── spiOverJtag_<fpga_part>.bit.gz      # SPI-over-JTAG bridge
├── bpiOverJtag_<fpga_part>.bit.gz      # BPI-over-JTAG bridge (Xilinx)
└── from_ise/spartan-6/                 # Spartan-6 COR files
    └── <model>_spi.cor
```

**Naming:** `<type>OverJtag_<fpga_part>.bit.gz`

Where `<fpga_part>` must match the `_device_package` string from `part.hpp`. For example:
- `spiOverJtag_xc7a200tffg1156.bit.gz` for Artix-7 xc7a200tffg1156
- `spiOverJtag_xc7a35tcsg324.bit.gz` for Artix-7 xc7a35tcsg324

### 6.2 Generating Bridge Bitstreams

**For Xilinx devices:**
1. Open Vivado and create a new project targeting your FPGA
2. Add the appropriate SPI-over-JTAG IP core (BSCAN_SPIM for 7-series/UltraScale)
3. Generate the bitstream
4. Compress: `gzip -9 spiOverJtag_<part>.bit`

**For Spartan-6:**
1. Use ISE to generate a .cor file with the SPI bridge
2. Place in `share/openFPGALoader/from_ise/spartan-6/`

**For Lattice devices:**
1. Generate .rbf files from the Lattice programming environment
2. Compress: `gzip -9 spiOverJtag_<part>.rbf`

### 6.3 Bridge Bitstream Loading in Code

The bridge loading logic in `Xilinx::load_bridge()` resolves the path:

```cpp
// Auto-resolution: bridge_dir + "/spiOverJtag_" + device_package + ".bit.gz"
// Override: --spi-bridge /path/to/custom.bit
// Environment: OPENFPGALOADER_SOJ_DIR overrides bridge_dir
```

### 6.4 Custom Bridge Path

Users can specify a custom bridge:
```bash
openFPGALoader --spi-bridge /path/to/my_bridge.bit.gz --dump-flash dump.bin
```

### 6.5 SOJ Version Differences

**SOJ v1 (7-series, Spartan-6, Spartan-7):**
- Uses USER1/USER2/USER3/USER4 JTAG instructions
- SPI commands are bit-reversed
- Version probe via USER4 instruction

**SOJ v2 (UltraScale, UltraScale+, ZynqMP):**
- Uses packetized protocol over USER1
- Packets have headers with length fields
- Version probe returns ASCII version string (e.g., "02.00\0")

The SOJ version is auto-detected during initialization. If detection fails, the tool tries both framings and selects the one that returns a valid JEDEC ID.

### 6.6 SOJ v2 Detection Fix (Recent)

If your board returns a plain-ASCII version string in the raw JTAG response (e.g., `00 30 32 2e 30 30 00` = `"02.00\0"` at offset 1-5 in jrx), the code now detects this pattern before the shifted decode can corrupt it. This fix handles cases where `decode_shifted_jtag_stream()` with `shift > 0` mangles the ASCII into garbage.

---

## 7. Adding a New JTAG Cable

### 7.1 Cable Implementation

Create:
- `src/<cable>.cpp`
- `src/<cable>.hpp`

### 7.2 Cable Class Structure

```cpp
#include "cable.hpp"

class MyCable : public Cable {
public:
    MyCable(int8_t verbose, uint32_t freq = 0);
    virtual ~MyCable();
    
    // Required interface
    std::vector<Jtag *> scan_chain(uint32_t max_devices) override;
    std::vector<Jtag *> *open_chains() override;
    void close_chains() override;
};
```

### 7.3 Register the Cable

In `src/main.cpp` (or wherever cable registration happens):

```cpp
// Add to cable_list map or factory function
cable_list["my_cable"] = []() { return new MyCable(); };
```

### 7.4 Common Cable Types

| Cable Type | Implementation | Key Technology |
|------------|---------------|----------------|
| Digilent HS-2 | `digilent` | FT2232 MPSSE |
| Digilent HS-3 | `digilent_hs3` | FT4232 Hi-Speed MPSSE |
| Xilinx Platform USB | `xilinxPlatformCableUsb` | Custom USB protocol |
| FT2232 generic | `ft2232` | FT2232 MPSSE |
| FT4232 generic | `ft4232` | FT4232 MPSSE |
| CMSIS-DAP | `cmsisdap` | DAPLink protocol |
| USB Blaster | `usb-blaster` | Altera USB-Blaster |
| DFU | `dfu` | USB DFU protocol |

---

## 8. IR Code Maps and Multi-byte Instructions

### 8.1 IR Code Map Structure

File: `src/xilinx.cpp` (line ~272)

```cpp
static std::map<std::string, std::map<std::string, std::vector<uint8_t>>>
ircode_mapping {
    {
        "default",  // Key used by family selection
        {
            { "USER1",       {0x02} },     // 6-bit: 000010
            { "USER2",       {0x03} },
            { "USER3",       {0x22} },
            { "USER4",       {0x23} },
            { "CFG_OUT",     {0x04} },
            { "CFG_IN",      {0x05} },
            { "USERCODE",    {0x08} },
            { "IDCODE",      {0x09} },
            { "ISC_ENABLE",  {0x10} },
            { "JPROGRAM",    {0x0B} },
            { "JSTART",      {0x0C} },
            { "JSHUTDOWN",   {0x0D} },
            { "ISC_PROGRAM", {0x11} },
            { "ISC_DISABLE", {0x16} },
            { "BYPASS",      {0xff} },
        }
    },
```

### 8.2 Multi-byte IR Instructions

For FPGAs with IR > 8 bits (like Virtex UltraScale+), the IR is stored as a vector of bytes (LSB first):

```cpp
{
    "virtexusp",  // 12-bit IR
    {
        { "USER1",   {0b00100100, 0b00101001, 0b00} },
        { "IDCODE",  {0b01001001, 0b10010010, 0b00} },
        { "BYPASS",  {0b11111111, 0b11111111, 0b11} },
    }
},
```

For Virtex-4 FX with 14-bit IR and two PowerPC blocks:
```cpp
{
    "virtex4_fx_dual_ppc",  // 14-bit IR
    {
        { "USER1", {0xc2, 0x3f} },  // 0x3fc2 = 16128 decimal, 14 bits
        { "BYPASS", {0xff, 0x3f} }, // All ones: 0x3fff
    }
},
```

### 8.3 Family-to-IR-Map Selection

In `Xilinx::set_fpga()` (line ~539):

```cpp
_ircode_map = ircode_mapping.at("default");  // 7-series default

if (_fpga_family == SPARTAN6_FAMILY)
    _ircode_map = ircode_mapping.at("spartan6");

if (_fpga_family == VIRTEXUSP_FAMILY)
    _ircode_map = ircode_mapping.at("virtexusp");
```

When adding a new family with different IR codes:
1. Add a new entry to `ircode_mapping`
2. Add the family-to-map selection in `set_fpga()`

### 8.4 Adding New IR Instructions

If your FPGA family needs additional JTAG instructions:

1. Add the instruction to the appropriate `ircode_mapping` entry
2. Use it via `get_ircode(_ircode_map, "NEW_INST")` in your code

---

## 9. SOJ Version Detection

### 9.1 How SOJ Version Detection Works

File: `src/xilinx.cpp`, `get_spiOverJtag_version()`

The SOJ version detection process:

1. **v1 probe:** Send version query via USER4 instruction with 6 bytes
2. **Decode:** Apply `decode_shifted_jtag_stream()` to extract version
3. **Parse:** Use `atof()` on decoded string to get version number
4. **Fallback:** If v1 probe fails, try SOJ v2 packet format via USER1

### 9.2 Common SOJ Detection Issues

**Issue: ASCII version string mangled by shift decode**

When `jtag_chain_len > 1` or certain bridge implementations return plain ASCII (e.g., `"02.00\0"`), the `decode_shifted_jtag_stream()` function with `shift > 0` bit-reverses and shifts the bytes, producing garbage like `18 98 e8 18 18`.

**Fix (already applied):** After the shifted decode fails to produce a valid version, the code now scans the raw `jrx` buffer for a plain ASCII version pattern (`DD.DD\0`). This catches cases where the bridge returns unencoded ASCII.

**Issue: SOJ v1 bridge transforms RDID command**

On some Xilinx Artix-7/Kintex-7 boards, the SOJ v1 bridge converts RDID (0x9F) to Read Flash Parameters (0x5F), causing the flash to return RFP bytes instead of JEDEC ID.

**Fix:** Add alias entries in `spiFlashdb.hpp` with the RFP-based ID:
```cpp
{0x5ffb8c, { /* MT25QL256ABA via SOJ v1 RFP response */ }},
```

**Issue: USER instruction mismatch**

Some custom bridge bitstreams use different USER instructions (USER1-4) than expected.

**Fix (built-in):** The code automatically probes all USER instructions when the initial RDID fails:
```
SPI RDID probe USER1: 00 00 00 00
SPI RDID probe USER2: 00 00 00 00
SPI RDID probe USER3: 00 00 00 00
SPI RDID probe USER4: 20 ba 18
SPI RDID selected USER4
```

### 9.3 SOJ v2 Packet Format

SOJ v2 uses a packetized protocol:
- Byte 0: Header with length and start bit
- Byte 1+: Command and data (bit-reversed per byte)

Packet construction:
```cpp
uint32_t v2_real_len = 6;  // 1 cmd + 5 padding
uint32_t v2_kPktLen = v2_real_len + 2;  // header + extra
v2_pkt[0] = ((0x1f & v2_real_len) << 3) | ((0x03 & 0x01) << 1) | 1;
v2_pkt[1] = reverseByte(0x01);  // version query cmd
```

---

## 10. Vendor-specific Implementation Patterns

### 10.1 Xilinx

**Key methods in `xilinx.cpp`:**
- `set_fpga()` - Detect family, set IR codes
- `load_bridge()` - Load SPI-over-JTAG bitstream
- `spi_put()` - SPI over JTAG (v1 and v2)
- `spi_put_v2()` - SOJ v2 packetized SPI
- `program_spi()` - Write to SPI flash
- `dumpFlash()` - Read from SPI flash
- `program_mem()` - SRAM programming via JTAG

**Family detection:** Based on `_fpga_family` enum matching `part.hpp` family strings:
- `"artix a7 *"` → ARTIX7_FAMILY
- `"spartan6"` → SPARTAN6_FAMILY
- `"kintex7"` → KINTX7_FAMILY
- `"zynq"` → ZYNQ_FAMILY
- `"virtexusp"` → VIRTEXUSP_FAMILY

**Special handling:**
- ZynqMP requires PL TAP activation (write 0x03 to JTAG_CTRL)
- Spartan-6 has different USER3/USER4 IR codes
- UltraScale+ has multi-byte IR instructions
- XCF devices use 4-byte flash IDs

### 10.2 Lattice

**Key methods in `lattice.cpp`:**
- `fpga_write()` - Configuration via SPI
- `spi_put()` - SPI commands
- `fpga_dump()` - Readback

**Families:**
- iCE40: SPI programming, FUSE mode for configuration
- ECP5: SRAM, FLASH, IAP modes
- MachXO2/3: SPI programming
- CrossLink-NX/Certus-NX/CertusPro-NX

**Bridge files:** `.rbf.gz` format instead of `.bit.gz`

### 10.3 Gowin

**Key methods in `gowin.cpp`:**
- `fpga_write()` - Configuration
- `spi_put()` - SPI via JTAG
- `fpga_dump()` - Readback

**Families:** GW1N, GW1NR, GW2A, GW5A

### 10.4 Altera/Intel

**Key methods in `altera.cpp`:**
- `flash()` - Configuration via AS or passive serial
- `spi_put()` - SPI commands

**Flash types:** EPCS, EPCQ (SPI QDR)

### 10.5 Efinix

**Key methods in `efinix.cpp`:**
- `flash()` - Configuration
- `fpga_dump()` - Readback

**Families:** Trion, Titanium

### 10.6 Anlogic

**Key methods in `anlogic.cpp`:**
- `flash()` - Configuration
- `fpga_dump()` - Readback

**Families:** Eagle, Elf2

---

## 11. Build System and Data File Installation

### 11.1 CMakeLists.txt Modifications

When adding a new source file:

```cmake
# Add to the source list
list(APPEND SRC_FILES src/myvendor.cpp)
list(APPEND HDR_FILES src/myvendor.hpp)
```

### 11.2 Data File Installation

Bridge bitstreams are installed via CMake:

```cmake
# CMakeLists.txt - data files installation
install(DIRECTORY share/openFPGALoader
    DESTINATION ${CMAKE_INSTALL_PREFIX}/share
    COMPONENT runtime
)
```

The share directory structure:
```
share/openFPGALoader/
├── spiOverJtag_xc7a*.bit.gz
├── spiOverJtag_xc7k*.bit.gz
├── bpiOverJtag_xc7k*.bit.gz
└── from_ise/spartan-6/
    └── *_spi.cor
```

### 11.3 Cross-compilation (Windows)

Windows builds use MinGW-w64 cross-compilation:
- Toolchain: `cmake/Toolchain-x86_64-w64-mingw32.cmake`
- Docker: `deploy/docker/cross/windows/alpine.Dockerfile`

**Compilation note:** On Alpine with mingw-w64 GCC, `-O3` can cause ICE (Internal Compiler Error) on large static initializers. The CMake release flags use `-O2 -DNDEBUG`.

### 11.4 Build Commands

```bash
# Standard Linux build
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)

# Windows cross-compile
cmake -B build-win -DCMAKE_TOOLCHAIN_FILE=cmake/Toolchain-x86_64-w64-mingw32.cmake
cmake --build build-win -j$(nproc)
```

---

## 12. Testing Checklist

### 12.1 New FPGA Device

- [ ] JTAG chain detection: `openFPGALoader -c <cable> -v`
- [ ] IDCODE matches expected value
- [ ] Manufacturer and family displayed correctly
- [ ] SRAM programming: `openFPGALoader -c <cable> --fpga-part <part> test.bit`
- [ ] DONE pin goes high after programming
- [ ] Verify mode: `openFPGALoader -c <cable> --fpga-part <part> --verify test.bit`

### 12.2 New SPI Flash Device

- [ ] RDID returns expected JEDEC ID
- [ ] Flash capacity detected correctly
- [ ] Dump works: `openFPGALoader -c <cable> --fpga-part <part> --dump-flash dump.bin`
- [ ] Program works: `openFPGALoader -c <cable> --fpga-part <part> --program-flash file.mcs`
- [ ] Bulk erase works
- [ ] Sector erase works (if supported)
- [ ] Subsector erase works (if supported)
- [ ] Block protection/unprotection works
- [ ] Quad mode works (if applicable)
- [ ] MCS format works (for Xilinx)
- [ ] Intel HEX format works (for non-Xilinx families): `--dump-flash dump.hex`

### 12.3 New Board

- [ ] Board name recognized: `openFPGALoader -b <board_name> -v`
- [ ] Correct cable detected
- [ ] Correct FPGA part loaded
- [ ] Correct bridge bitstream found (if applicable)
- [ ] Full flash cycle works

### 12.4 Common Pitfalls

1. **SOJ version detection:** If `SOJ version raw` shows garbage or `atof` returns 0, check if the raw jrx contains an ASCII version string that needs plain-text detection.

2. **USER instruction mismatch:** If RDID returns all zeros, the tool auto-probes USER1-USER4. If none work, check the bridge bitstream configuration.

3. **RDID command transformation:** On SOJ v1 through Xilinx 7-series, RDID (0x9F) may become RFP (0x5F). Add alias entries in `spiFlashdb.hpp`.

4. **IR length mismatch:** If JTAG operations hang, verify the IR length matches the FPGA datasheet.

5. **Bridge bitstream missing:** If flash operations fail with "Can't program SPI flash", ensure the bridge bitstream exists and matches the FPGA package exactly.

6. **Large static initializers cause ICE:** When cross-compiling for Windows on Alpine, large `fpga_list` or `flash_list` entries may trigger GCC ICE at `-O3`. Use `-O2` for release builds.

7. **Multi-device JTAG chains:** When multiple devices are in the JTAG chain, use `--index-chain` to select the correct device for flash operations.

8. **Extended address mode:** For flashes > 128 MiB, ensure `has_extended = true` is set; otherwise read/write operations will wrap at 128 MiB boundary.

---

## Quick Reference: File Modification Summary

| Goal | Files to Modify |
|------|----------------|
| Add new FPGA device variant | `src/part.hpp` |
| Add new FPGA family | `src/<vendor>.cpp/hpp`, `src/main.cpp`, `CMakeLists.txt` |
| Add new SPI flash | `src/spiFlashdb.hpp` |
| Add new flash manufacturer name | `src/spiFlash.cpp` |
| Add new SPI command | `src/spiFlash.cpp` |
| Add new board | `src/board.hpp` |
| Add new JTAG cable | `src/<cable>.cpp/hpp`, `src/main.cpp`, `CMakeLists.txt` |
| Add bridge bitstream | `share/openFPGALoader/spiOverJtag_<part>.bit.gz` |
| Add new IR code map | `src/xilinx.cpp` (ircode_mapping) |
| Fix SOJ version detection | `src/xilinx.cpp` (get_spiOverJtag_version) |
| Add custom bitstream parser | `src/<vendor>BitParser.cpp/hpp` |

---

## Appendix: JEDEC ID Calculation

```
JEDEC ID = (manufacturer << 16) | (memory_type << 8) | capacity

Example: Micron N25Q128 (0x20ba18)
  Manufacturer: 0x20 (Micron/ST)
  Memory type:  0xba (Serial NOR, Quad)
  Capacity:     0x18 (2^(0x18) = 2^24 = 16 MiB = 128 Mbit)

Capacity byte reference:
  0x13 = 4 Mbit  (512 KiB)
  0x14 = 8 Mbit  (1 MiB)
  0x15 = 16 Mbit (2 MiB)
  0x16 = 32 Mbit (4 MiB)
  0x17 = 64 Mbit (8 MiB)
  0x18 = 128 Mbit (16 MiB)
  0x19 = 256 Mbit (32 MiB)
  0x20 = 512 Mbit (64 MiB)
  0x21 = 1 Gbit  (128 MiB)
  0x22 = 2 Gbit  (256 MiB)
```

---

*Last updated: 2025*
*Based on openFPGALoader source code analysis*