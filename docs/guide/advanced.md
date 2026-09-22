# Advanced usage of openFPGALoader

## Resetting an FPGA

``` bash
openFPGALoader [options] -r
```

## Using negative edge for TDO's sampling

If transaction are unstable you can try to change read edge by using

``` bash
openFPGALoader [options] --invert-read-edge
```

## Reading the bitstream from STDIN

``` bash
cat /path/to/bitstream.ext | openFPGALoader --file-type ext [options]
```

`--file-type` is required to detect file type.

<div class="note">

<div class="title">

Note

</div>

It's possible to load a bitstream through network:

``` bash
# FPGA side
nc -lp port | openFPGALoader --file-type xxx [option]

# Bitstream side
nc -q 0 host port < /path/to/bitstream.ext
```

</div>

## Automatic file type detection bypass

Default behavior is to use file extension to determine file parser. To avoid this mechanism `--file-type type` must be used.

## FT231/FT232 bitbang mode and pins configuration

FT232R and ft231X may be used as JTAG programmer. JTAG communications are emulated in bitbang mode.

To use these devices user needs to provides both the cable and the pin mapping:

``` bash
openFPGALoader [options] -cft23XXX --pins=TDI:TDO:TCK:TMS /path/to/bitstream.ext
```

where:

- ft23XXX may be `ft232RL` or `ft231X`.
- TDI:TDO:TCK:TMS may be the pin ID (0 \<= id \<= 7) or string value.

allowed values are:

<table>
<thead>
<tr class="header">
<th>value</th>
<th>ID</th>
</tr>
</thead>
<tbody>
<tr class="odd">
<td><blockquote>
<p>TXD</p>
</blockquote></td>
<td>0</td>
</tr>
<tr class="even">
<td><blockquote>
<p>RXD</p>
</blockquote></td>
<td>1</td>
</tr>
<tr class="odd">
<td><blockquote>
<p>RTS</p>
</blockquote></td>
<td>2</td>
</tr>
<tr class="even">
<td><blockquote>
<p>CTS</p>
</blockquote></td>
<td>3</td>
</tr>
<tr class="odd">
<td><blockquote>
<p>DTR</p>
</blockquote></td>
<td>4</td>
</tr>
<tr class="even">
<td><blockquote>
<p>DSR</p>
</blockquote></td>
<td>5</td>
</tr>
<tr class="odd">
<td><blockquote>
<p>DCD</p>
</blockquote></td>
<td>6</td>
</tr>
<tr class="even">
<td><blockquote>
<p>RI</p>
</blockquote></td>
<td>7</td>
</tr>
</tbody>
</table>

## Writing to an arbitrary address in flash memory

With FPGA using an external SPI flash (*xilinx*, *lattice ECP5/nexus/ice40*, *anlogic*, *efinix*) option `-o` allows one to write raw binary file to an arbitrary adress in FLASH.

## Detect/read/write on primary/secondary flash memories

With FPGA using two external SPI flash (some *xilinx* boards) option `--target-flash` allows to select the QSPI chip.

To detect:

``` bash
openFPGALoader -b kcu105 -f --target-flash {primary,secondary} --detect
```

To read the primary flash memory:

``` bash
openFPGALoader -b kcu105 -f --target-flash primary --dump-flash --file-size N_BYTES mydump.bin
```

When the SPI flash is known in openFPGALoader's flash database, `--file-size` may be omitted. In that case openFPGALoader dumps from `--offset` to the end of the flash:

``` bash
openFPGALoader -b kcu105 -f --target-flash primary --dump-flash mydump.bin
openFPGALoader -b kcu105 -f --target-flash primary --dump-flash -o 0x100000 mydump.bin
```

and the second flash memory:

``` bash
openFPGALoader -b kcu105 -f --target-flash secondary --dump-flash --file-size N_BYTES --secondary-bitstream mydump.bin
```

To write on secondary flash memory:

``` bash
openFPGALoader -b kcu105 -f --target-flash secondary --secondary-bitstream mySecondaryBitstream.bin
```

## Using an alternative directory for *spiOverJtag*

By setting `OPENFPGALOADER_SOJ_DIR` it's possible to override default *spiOverJtag* bitstreams directory:

``` bash
export OPENFPGALOADER_SOJ_DIR=/somewhere
openFPGALoader xxxx
```

or

``` bash
OPENFPGALOADER_SOJ_DIR=/somewhere openFPGALoader xxxx
```
