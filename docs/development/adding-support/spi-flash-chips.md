# Adding a New SPI Flash to openFPGALoader

## Overview

openFPGALoader maintains a static flash database in `src/spiFlashdb.hpp`. Each entry maps a
JEDEC ID (the key) to a `flash_t` struct describing the chip's capabilities. This guide walks
you through adding an unknown flash chip by hand.

---

## Step 1: Discover the JEDEC ID

Run openFPGALoader on your board. If the flash is not in the database, the tool prints:

```
Read ID failed: SPI RDID raw bytes: XX XX XX XX -> 0xXXXXXXXX
Manufacturer byte: 0xXX (name)
Memory type byte: 0xXX
Memory capacity byte: 0xXX
Starter spiFlashdb.hpp entry:
    {0xXXXXXX, {
        ...
    }},
```

The tool already emits a skeleton entry. You still need to fill in the fields from the
datasheet (Step 3), but this template is a great starting point.

### How the ID is built

The code issues the **RDID command `0x9F`** and reads 4 bytes:

```
_jedec_id = 0;
for (int i = 0; i < 4; i++)
    _jedec_id = (_jedec_id << 8) | rx[i];
jedec24 = _jedec_id >> 8;    // drops the 4th byte
```

Lookup key = `jedec24` (24-bit for most chips). Some chips return 4 significant bytes, in
which case the full 32-bit value is the key (e.g. `0x4000190c`, `0x05059093`).

To decide which key to use:
- Most SPI NOR flashes: use the first 3 response bytes (e.g. `BA 21 19` → `0xBA2119`).
- If the 4th byte is non-zero and meaningful per the datasheet, use all 4 bytes
  (e.g. `40 00 19 0C` → `0x4000190c`).

You can add both keys pointing to the same struct so the chip is recognised regardless of
which response variant your bridge returns.

### Manufacturer codes (cheat sheet)

| Manufacturer ID | Name          |
|-----------------|---------------|
| 0xC2            | Macronix      |
| 0xEF            | Winbond       |
| 0xBF            | SST / Esma    |
| 0x1C            | Micron/N25Q   |
| 0xBA            | Micron/Numonyx|
| 0x20            | Micron (1.8V) |
| 0x01            | Spansion/Infineon |
| 0x05            | Xilinx        |
| 0x7F            | Cypress/Spansion |
| 0x4000          | Numonyx/Micron (4-byte sig) |

---

## Step 2: Read the Datasheet

Look up the exact part number on the chip label. Get the datasheet and note:

1. **Total capacity** (Mbit or MB) — needed to compute `nr_sector`
2. **Erase commands supported** — 64 KB block erase (`0xD8`), 4 KB subsector erase (`0x20`)
3. **Status register layout** — where BP (Block Protect) and TB (Top/Bottom) bits live
4. **Quad Enable (QE) bit** — which register and which bit enables quad I/O
5. **Whether BP/TB bits are OTP** (one-time programmable, cannot be changed after programming)
6. **Extended address register** — chips >= 16 MB need a 3-byte address register (`0xB7`)

---

## Step 3: Fill in the `flash_t` struct

Open `src/spiFlashdb.hpp`. The struct definition:

```cpp
typedef struct {
    std::string manufacturer;  /**< manufacturer name                  */
    std::string model;         /**< chip name                          */
    uint32_t  nr_sector;       /**< number of 64 KiB sectors           */
    bool      sector_erase;    /**< 64 KB erase support                */
    bool      subsector_erase; /**< 4 KB erase support                 */
    bool      has_extended;    /**< Extended Address Register (0xB7)   */
    bool      tb_otp;          /**< TOP/BOTTOM is One-Time Programmable */
    uint16_t  tb_offset;       /**< TOP/BOTTOM bit mask                */
    tb_loc_t  tb_register;     /**< register where BP/TB bits live     */
    uint8_t   bp_len;          /**< number of BP bits (0-4)            */
    uint8_t   bp_offset[4];    /**< BP[0..3] bit masks                 */
    tb_loc_t  quad_register;   /**< register containing QE bit         */
    uint16_t  quad_mask;       /**< QE bit mask                        */
    bool      global_lock;     /**< global lock/unlock bit present     */
} flash_t;
```

### Field-by-field reference

| Field | How to determine | Typical values |
|-------|-----------------|----------------|
| `manufacturer` | From datasheet / JEDEC ID | `"Micron"`, `"Winbond"`, `"Spansion"`, `"Macronix"`, `"ST"` |
| `model` | Exact part number | `"MT25QL256ABA"`, `"W25Q256"` |
| `nr_sector` | Capacity / 65536 (64 KiB) | 8 (512Kbit), 128 (8Mbit), 256 (16Mbit), 512 (256Mbit) |
| `sector_erase` | Does the chip support 64 KB erase (`0xD8`/`0xC7`)? | `true` (almost always) |
| `subsector_erase` | Does the chip support 4 KB block erase (`0x20`)? | `true` for most modern chips; `false` for some Spansion parts |
| `has_extended` | Does the chip need Extended Address Register (`0xB7`)? Required for chips >= 16 MB. | `true` for 16 Mbit+ that use the 3-byte address mode; `false` otherwise |
| `tb_otp` | Is the TB bit OTP (one-time programmable)? | `true` for Spansion S25FL256S/512S/128S; `false` for most others |
| `tb_offset` | Bit position of TB bit in the register, as `(1 << N)` | `(1 << 5)` (most common), `(1 << 6)`, `(1 << 14)` |
| `tb_register` | Which register holds BP/TB | `STATR`, `FUNCR`, `CONFR`, `NVCONFR` (see register table below) |
| `bp_len` | Number of BP bits (1-4) | `3` or `4` |
| `bp_offset` | Bit masks for BP0-BP3 as `(1 << N)` | `{(1<<2), (1<<3), (1<<4), 0}` (3 BP bits) or `{(1<<2), (1<<3), (1<<4), (1<<5)}` (4 BP bits) |
| `quad_register` | Which register holds the QE bit | `STATR`, `CONFR`, or `NONER` if the chip is quad-only |
| `quad_mask` | Bit mask for QE bit | `(1 << 6)` (most common), or `0` if quad-only (`NONER`) |
| `global_lock` | Global lock/unlock (GL) bit in status register | `false` for most; `true` for some ISSI / low-density chips |

### Register enum values (`tb_register` and `quad_register`)

| Value | Constant | Meaning |
|-------|----------|---------|
| 0 | `STATR` | Status Register (read `0x05`) |
| 1 | `FUNCR` | Function Register |
| 2 | `CONFR` | Configuration Register |
| 3 | `NVCONFR` | Non-Volatile Configuration Register |
| 99 | `NONER` | None — not applicable (use for quad-only chips) |

### Calculating `nr_sector`

```
nr_sector = (capacity in Mbit × 131072) / 65536
         = capacity_in_Mbit × 2
```

| Capacity | nr_sector |
|----------|-----------|
| 512 Kbit  | 8   |
| 1 Mbit    | 16  |
| 2 Mbit    | 32  |
| 4 Mbit    | 64  |
| 8 Mbit    | 128 |
| 16 Mbit   | 256 |
| 32 Mbit   | 512 |
| 64 Mbit   | 1024 |
| 128 Mbit  | 2048 |
| 256 Mbit  | 4096 |
| 512 Mbit  | 8192 |

---

## Step 4: Add the entry to `spiFlashdb.hpp`

Insert the new entry into `flash_list` (a `std::map<uint32_t, flash_t>`) at an appropriate
position (sorted by JEDEC ID is conventional but not strictly required).

### Complete example: Micron MT25QL256ABA

This chip responds to 0x9F with `BA 21 19` (3-byte JEDEC), giving key `0xBA2119`.
It also has a 4-byte electronic signature `40 00 19 0C` (from command `0xAB`),
giving key `0x4000190c`. Both keys should point to the same struct.

```cpp
// 4-byte electronic signature variant (cmd 0xAB: 40 00 19 0C)
{0x4000190c, {
    /* Micron MT25QL256ABA1EW9 - 256Mb Serial NOR Flash, 3.0V.
     * Electronic Signature (0xAB): 40 00 19 0C
     * https://media-www.micron.com/...mt25q_qlhs_u_256_aba_0.pdf */
    .manufacturer = "Micron",
    .model = "MT25QL256ABA",
    .nr_sector = 512,
    .sector_erase = true,
    .subsector_erase = true,
    .has_extended = false,
    .tb_otp = false,
    .tb_offset = (1 << 5),
    .tb_register = STATR,
    .bp_len = 3,
    .bp_offset = {(1 << 2), (1 << 3), (1 << 4), 0},
    .quad_register = NONER,   // MT25QL is quad-only, no QE bit needed
    .quad_mask = 0,
    .global_lock = false,
}},

// 3-byte JEDEC RDID variant (cmd 0x9F: BA 21 19)
{0xba2119, {
    /* Micron MT25QL256ABA1EW9 - 256Mb Serial NOR Flash, 3.0V.
     * RDID (0x9F): BA 21 19
     * Manufacturer: 0xBA (Numonyx/Micron)
     * Memory type:  0x21 (Serial NOR)
     * Capacity:     0x19 (256Mb / 32MB)
     * https://media-www.micron.com/...mt25q_qlhs_u_256_aba_0.pdf */
    .manufacturer = "Micron",
    .model = "MT25QL256ABA",
    .nr_sector = 512,
    .sector_erase = true,
    .subsector_erase = true,
    .has_extended = false,
    .tb_otp = false,
    .tb_offset = (1 << 5),
    .tb_register = STATR,
    .bp_len = 3,
    .bp_offset = {(1 << 2), (1 << 3), (1 << 4), 0},
    .quad_register = NONER,
    .quad_mask = 0,
    .global_lock = false,
}},
```

---

## Step 5: Build and test

```bash
cmake -B build
cmake --build build -j
```

Run the tool against your board. It should now detect the flash:

```
Detected: Micron MT25QL256ABA 512 sectors size: 32Mb
```

If it still shows "unknown", double-check:
- The JEDEC ID key matches what `spiFlash.cpp` actually reads from the 0x9F command.
- The entry is inside the `flash_list` map braces (no missing comma from the previous entry).

---

## Quick-reference: common BP/TB patterns

### Pattern A — BP bits in Status Register (most common)
```cpp
.tb_offset = (1 << 5),
.tb_register = STATR,
.bp_len = 3,
.bp_offset = {(1 << 2), (1 << 3), (1 << 4), 0},
.quad_register = STATR,
.quad_mask = (1 << 6),
```

### Pattern B — Quad-only chip, no QE bit needed
```cpp
.tb_offset = (1 << 5),
.tb_register = STATR,
.bp_len = 3,
.bp_offset = {(1 << 2), (1 << 3), (1 << 4), 0},
.quad_register = NONER,
.quad_mask = 0,
```

### Pattern C — 4 BP bits, BP3 in bit 6
```cpp
.tb_offset = (1 << 5),
.tb_register = STATR,
.bp_len = 4,
.bp_offset = {(1 << 2), (1 << 3), (1 << 4), (1 << 6)},
.quad_register = NONER,
.quad_mask = 0,
```

### Pattern D — BP/TB in Configuration Register (Spansion)
```cpp
.tb_offset = (1 << 5),
.tb_register = CONFR,
.bp_len = 3,
.bp_offset = {(1 << 2), (1 << 3), (1 << 4), 0},
.quad_register = CONFR,
.quad_mask = (1 << 1),
```

---

## Troubleshooting

**Flash still unknown after adding entry:**
- Check the raw bytes printed by the tool. The key must match `(rx[0] << 16) | (rx[1] << 8) | rx[2]`
  for 3-byte IDs.
- If rx[0] is `0x40` or `0x05`, the chip reports a 4-byte ID — use the full 32-bit value.

**Erase/write fails but flash is detected:**
- `nr_sector` is wrong — verify capacity from datasheet.
- `has_extended` should be `true` for chips >= 16 MB that use the Extended Address Register
  (command `0xB7` to enable 3-byte addressing).

**Protection bits don't match expected:**
- `tb_register` might be wrong — try `CONFR` instead of `STATR`.
- `bp_offset` bit positions vary by manufacturer; check the register map in the datasheet.