# MachXO3 Recovery After `--bulk-erase --flash-sector ALL`

## Scenario

This note documents a likely recovery path for a `LCMXO3LF-9400C` that stopped enumerating normally after running:

```powershell
.\openFPGALoader.exe -c ft2232 --index-chain 0 --bulk-erase --flash-sector ALL -v
```

Observed symptom in Lattice Diamond Programmer:

- device appears as `Generic JTAG Device`
- operation shows `JTAG-NOP`
- scan may fail or the board may no longer identify as `LCMXO3LF-9400C`

## What likely happened

In this fork, MachXO2/MachXO3 handling maps `--flash-sector ALL` to an internal erase mask that includes:

- configuration flash
- UFM
- feature bits / feature row
- SRAM

Relevant local code:

- [src/lattice.cpp](C:/Users/livanyi/Desktop/WORK/GIT/openFPGALoader/src/lattice.cpp:47)
- [src/lattice.cpp](C:/Users/livanyi/Desktop/WORK/GIT/openFPGALoader/src/lattice.cpp:49)
- [src/lattice.cpp](C:/Users/livanyi/Desktop/WORK/GIT/openFPGALoader/src/lattice.cpp:297)

This means the command above likely erased more than the user bitstream. It probably also erased the feature row, which can return the device to hardware-default configuration behavior.

That usually does **not** mean the FPGA is physically destroyed. A more likely outcome is:

1. the FPGA falls back to default sysCONFIG and JTAG behavior
2. board-level circuitry is now driving one or more pins in an unexpected way
3. JTAG scan becomes unreliable, so Diamond reports a generic or unknown device

## Why `JTAG-NOP` can happen here

`JTAG-NOP` usually means the cable sees activity on the chain but cannot identify the device reliably. After a feature-row erase, that can happen because:

- `PROGRAMN` is being held low or pulsed
- `INITN` is being held low
- `JTAGENB` is not in the state needed for JTAG access
- the board is loading the JTAG pins or shared configuration pins
- JTAG clocking or signal integrity is marginal

## Most likely recovery mechanism

The highest-probability explanation is:

- the device is blank
- the feature row is blank or reset
- the JTAG path is still physically present
- the board around the FPGA is now interfering with the default pin roles

So this is more often a **recovery and pin-state problem** than a dead-chip problem.

## Immediate bench recovery steps

Try these in this order:

1. Power the board fully off, then back on.
2. Force `PROGRAMN` high during scan and programming.
3. Make sure `INITN` is not held low.
4. If `JTAGENB` is available on the board, force it high.
5. Temporarily isolate anything attached to:
   - `TCK`
   - `TMS`
   - `TDI`
   - `TDO`
   - `PROGRAMN`
   - `INITN`
   - `DONE`
6. Retry with the slowest JTAG clock available.
7. Use a short cable and solid ground reference.
8. In Diamond Programmer, target the exact `LCMXO3LF-9400C` if manual selection is possible.
9. Program a known-good `.jed`.
10. If Diamond offers it, restore the feature row from the `.jed` or use full flash programming from the `.jed`.

## Important pin notes

### `PROGRAMN`

Lattice documents that `PROGRAMN` must be held high during JTAG configuration/programming. If it goes low during the operation, SRAM can be cleared and programming can fail.

This makes `PROGRAMN` the first pin to check on a board that suddenly became unprogrammable after erase.

### `JTAGENB`

On MachXO3, `JTAGENB` can control access to the dedicated JTAG port depending on feature-row settings. If the board exposes it, forcing it high is a strong recovery step.

### Shared pin contention

Even if the FPGA itself is fine, attached MCUs, transceivers, pull networks, level shifters, or other peripherals may now be fighting pins that changed role after erase.

If the board has:

- 0-ohm links
- series resistors
- jumpers
- muxes
- shared JTAG chains

temporarily isolating those nets may be the difference between `JTAG-NOP` and a recoverable scan.

## Suggested recovery workflow in Diamond

1. Reduce JTAG speed.
2. Rescan after power-cycle.
3. Keep `PROGRAMN` high and `JTAGENB` high.
4. Try programming a known-good `.jed`.
5. Prefer restoring the feature row as part of full programming if available.

If the device still appears only as a generic JTAG device, the next suspect is board-level contention or signal integrity rather than irreversible silicon damage.

## Secondary recovery options

If JTAG cannot be stabilized but other configuration interfaces are physically accessible on the board, recovery through alternate sysCONFIG ports may still be possible. That depends entirely on the board wiring and whether the required pins are exposed.

## Practical conclusion

The command:

```powershell
.\openFPGALoader.exe -c ft2232 --index-chain 0 --bulk-erase --flash-sector ALL -v
```

can plausibly make a MachXO3 board appear "bricked" in practice because it erases the feature row and changes the effective configuration environment of the chip.

The most likely rescue path is:

- restore stable JTAG electrical conditions
- force the critical control pins into sane states
- reprogram a known-good `.jed`

This is bad, but it is still often recoverable.

## Future safety recommendation

For MachXO2/MachXO3, `--flash-sector ALL` should be treated as a dangerous operation because it can erase feature bits, not just the main user image. A safer UX would be one of:

- require an explicit force flag for feature-row erase
- refuse the operation by default for these families
- split `ALL` into user image vs feature-row aware options

## Sources

- Lattice MachXO3 product page: <https://www.latticesemi.com/en/Products/FPGAandCPLD/MachXO3>
- Lattice MachXO3 Programming and Configuration User Guide `TN-02055`, v3.4, May 6, 2026: <https://www.latticesemi.com/view_document?document_id=50123>
- Lattice FAQ 1323 (`PROGRAMN` during JTAG programming): <https://www.latticesemi.com/support/answerdatabase/1/3/2/1323>
- Lattice FAQ 7095 (default behavior after erase/offline mode): <https://www.latticesemi.com/support/answerdatabase/7/0/9/7095>
- Lattice FAQ 5499 (`PROGRAMN` / `INITN` startup behavior): <https://www.latticesemi.com/support/answerdatabase/5/4/9/5499>
- Lattice FAQ 1226 (`JTAG-NOP` behavior and scan issues): <https://www.latticesemi.com/support/answerdatabase/1/2/2/1226>
