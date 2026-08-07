param(
    [Parameter(Mandatory = $true)]
    [string]$DriverPath,

    [Parameter(Mandatory = $true)]
    [string]$OutputPath
)

$ErrorActionPreference = 'Stop'
$driver = [IO.Path]::GetFullPath($DriverPath)
$output = [IO.Path]::GetFullPath($OutputPath)
$bytes = [IO.File]::ReadAllBytes($driver)

if ($bytes.Length -lt 0x100 -or $bytes[0] -ne 0x4d -or $bytes[1] -ne 0x5a) {
    throw "Not a PE driver: $driver"
}

$pe = [BitConverter]::ToInt32($bytes, 0x3c)
if ($pe -lt 0 -or $pe + 24 -ge $bytes.Length -or
        $bytes[$pe] -ne 0x50 -or $bytes[$pe + 1] -ne 0x45) {
    throw "Invalid PE header: $driver"
}

$sectionCount = [BitConverter]::ToUInt16($bytes, $pe + 6)
$optionalSize = [BitConverter]::ToUInt16($bytes, $pe + 20)
$sectionTable = $pe + 24 + $optionalSize
$dataOffset = 0
$dataSize = 0

for ($index = 0; $index -lt $sectionCount; $index++) {
    $entry = $sectionTable + 40 * $index
    $name = [Text.Encoding]::ASCII.GetString($bytes, $entry, 8).Trim([char]0)
    if ($name -eq '.data') {
        $dataSize = [BitConverter]::ToInt32($bytes, $entry + 16)
        $dataOffset = [BitConverter]::ToInt32($bytes, $entry + 20)
        break
    }
}

if ($dataOffset -le 0 -or $dataSize -le 0 -or
        $dataOffset + $dataSize -gt $bytes.Length) {
    throw "PE .data section was not found or is invalid: $driver"
}

# The Xilinx xusb_*.sys drivers place globals in the first 0x100 bytes of
# .data, followed by zero-padded firmware records. Each record is:
# uint16 length, uint16 FX2 address, uint8 type, data[length], uint8 padding.
$records = [Collections.Generic.List[object]]::new()
$offset = $dataOffset + 0x100
$end = $dataOffset + $dataSize

while ($offset -lt $end) {
    $found = $false
    while ($offset + 6 -lt $end) {
        $length = [BitConverter]::ToUInt16($bytes, $offset)
        if ($length -ge 1 -and $length -le 16 -and
                $offset + 5 + $length -lt $end -and
                $bytes[$offset + 4] -eq 0 -and
                $bytes[$offset + 5 + $length] -eq 0) {
            $address = [BitConverter]::ToUInt16($bytes, $offset + 2)
            $data = [byte[]]::new($length)
            [Array]::Copy($bytes, $offset + 5, $data, 0, $length)
            $records.Add([pscustomobject]@{
                Offset = $offset
                Address = [int]$address
                Data = $data
            })
            $offset += 6 + $length
            $found = $true
            break
        }
        $offset++
    }
    if (-not $found) {
        break
    }
}

# Discard scanner artifacts which overlap an earlier real firmware record.
$occupied = [Collections.Generic.HashSet[int]]::new()
$firmware = [Collections.Generic.List[object]]::new()
foreach ($record in $records) {
    $overlap = $false
    for ($i = 0; $i -lt $record.Data.Length; $i++) {
        if ($occupied.Contains($record.Address + $i)) {
            $overlap = $true
            break
        }
    }
    if ($overlap) {
        continue
    }
    $firmware.Add($record)
    for ($i = 0; $i -lt $record.Data.Length; $i++) {
        [void]$occupied.Add($record.Address + $i)
    }
}

$versionRecord = $firmware | Where-Object {
    $_.Address -eq 0x19b9 -and $_.Data.Length -eq 2
} | Select-Object -First 1
$descriptorRecord = $firmware | Where-Object {
    $_.Address -eq 0x0090 -and $_.Data.Length -ge 12
} | Select-Object -First 1
$resetRecord = $firmware | Where-Object {
    $_.Address -eq 0 -and $_.Data.Length -ge 3
} | Select-Object -First 1

if ($firmware.Count -lt 500 -or -not $versionRecord -or
        -not $descriptorRecord -or -not $resetRecord) {
    throw "Embedded Xilinx FX2 firmware validation failed in $driver"
}

$vid = $descriptorRecord.Data[8] + 256 * $descriptorRecord.Data[9]
$productId = $descriptorRecord.Data[10] + 256 * $descriptorRecord.Data[11]
if ($vid -ne 0x03fd -or $productId -ne 0x0008) {
    throw ('Unexpected embedded USB identity {0:X4}:{1:X4}' -f
        $vid, $productId)
}

$lines = [Collections.Generic.List[string]]::new()
foreach ($record in $firmware) {
    $length = $record.Data.Length
    $sum = $length + (($record.Address -shr 8) -band 0xff) +
        ($record.Address -band 0xff)
    $dataText = [Text.StringBuilder]::new()
    foreach ($value in $record.Data) {
        $sum += $value
        [void]$dataText.Append($value.ToString('X2'))
    }
    $checksum = (-$sum) -band 0xff
    $lines.Add((':{0:X2}{1:X4}00{2}{3:X2}' -f
        $length, $record.Address, $dataText.ToString(), $checksum))
}
$lines.Add(':00000001FF')

$parent = Split-Path -Parent $output
if ($parent) {
    [IO.Directory]::CreateDirectory($parent) | Out-Null
}
[IO.File]::WriteAllLines($output, $lines, [Text.Encoding]::ASCII)

$rawVersion = $versionRecord.Data[0] * 256 + $versionRecord.Data[1]
Write-Output ('Extracted {0} records to {1}' -f $firmware.Count, $output)
Write-Output ('Firmware record: 0x{0:X4} (decimal {1})' -f
    $rawVersion, $rawVersion)
