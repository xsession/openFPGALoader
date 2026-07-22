rem Bit file upload
@echo off
SET CABLE=digilent_hs3
SET FPGA_CHIP=xc7a35t
SET BIT_FILE=dcb1670_top.bit
SET MCS_FILE=dcb1670_top.mcs

echo ⌛ Feltöltés az FPGA SRAM memóriájába...
openFPGALoader -c %CABLE% --fpga %FPGA_CHIP% %BIT_FILE%

rem Ha a Flash memóriába szeretnéd írni (permanens), használd ezt helyette:
rem openFPGALoader -c %CABLE% --fpga %FPGA_CHIP% -f %BIT_FILE%

if %errorlevel% equ 0 (
    echo ✅ Sikeres feltöltés!
) else (
    echo ❌ Hiba történt.
    pause
)

# .mcs fájl végleges beírása a Flash-be:
openFPGALoader -c digilent_hs3 --fpga xc7a35t -f --file-type mcs MCS_FILE

rem fpga_type: MT25QL256ABA1EW9-0SIT -> n25q256a
rem openFPGALoader -c digilent_hs3 --fpga xc7a35t -f --external-flash w25q128jv fw.bit


rem FPGA detect
rem openFPGALoader -c digilent_hs3 --detect


rem bit file upload script
.\openFPGALoader.exe -c digilent_hs3 ../../../../test_fw/dcb1670_top.bit

rem flash upload
.\openFPGALoader.exe -c digilent_hs3 --external-flash n25q256a ../../../../test_fw/dcb1670_top.mcs --fpga-part xc7a35tfgg484 -f spiOverJtag_xc7a35tfgg484.bit


.\openFPGALoader.exe `                                               
>>   -c digilent_hs3 `
>>   --fpga-part xc7a35tfgg484 `
>>   --external-flash `
>>   -f `                       
>>   ..\..\..\..\test_fw\dcb1670_top.mcs

.\openFPGALoader.exe --scan-usb

.\openFPGALoader.exe -c xilinxPlatformCableUsb_alt --detect   

.\openFPGALoader.exe -c xilinxPlatformCableUsb_alt --freq 700000 --detect

.\openFPGALoader.exe -c xilinxPlatformCableUsb_alt --fpga-part xc6slx45tfgg484 --external-flash -f ..\..\..\..\test_fw\5042-9033\FW-5042-9033-T30.mcs       
.\openFPGALoader.exe -c xilinxPlatformCableUsb_alt --fpga-part xc6slx45tfgg484 --external-flash -f ..\..\..\..\test_fw\5042-9033\FW-5042-9033-T30.mcs --bridge ..\..\..\..\test_fw\5042-9033\pab0_top.bit 

rem Do not pass pab0_top.bit as --bridge: it is an application image, not SPI-over-JTAG.
rem --fpga-part selects the packaged spiOverJtag_xc6slx45tfgg484.bit.gz bridge.
.\openFPGALoader.exe -c xilinxPlatformCableUsb_alt --fpga-part xc6slx45tfgg484 --external-flash -f ..\..\..\..\test_fw\5042-9033\FW-5042-9033-T30.mcs


.\openFPGALoader.exe -c xilinxPlatformCableUsb_alt --fpga-part xc6slx45tfgg484 --external-flash -f ..\..\..\..\test_fw\_IMPACT_BATCH_DCS_FW\_IMPACT_BATCH\example\fw_files\application\EL-24-80\FW-24-80-R410.MCS --bridge ..\..\..\..\test_fw\_IMPACT_BATCH_DCS_FW\_IMPACT_BATCH\S320_06_20260703.bit

.\openFPGALoader.exe -c xilinxPlatformCableUsb -f ..\..\..\..\test_fw\_IMPACT_BATCH_DCS_FW\_IMPACT_BATCH\example\fw_files\application\EL-24-80\FW-24-80-R410.MCS --bridge ..\..\..\..\test_fw\_IMPACT_BATCH_DCS_FW\_IMPACT_BATCH\S320_06_20260703.bit


.\openFPGALoader.exe -c xilinxPlatformCableUsb_alt --fpga-part xc6slx45tfgg484 --enable-quad -f ..\..\..\..\test_fw\5042-9033\FW-5042-9033-T30.mcs  

.\openFPGALoader.exe -c xilinxPlatformCableUsb --fpga-part xc7a35tfgg484 --external-flash -f ..\..\..\..\test_fw\dcb1670_top.mcs --index-chain 0

.\openFPGALoader.exe -c xilinxPlatformCableUsb --fpga-part xc7a35tfgg484 --detect-external-flash --index-chain 0  

.\openFPGALoader.exe --list-flash

.\openFPGALoader.exe -c digilent_hs3 --fpga-part -c xilinxPlatformCableUsb_alt --fpga-part xc7a35tfgg484 --detect-external-flash -v -v --detect-external-flash -v -v

.\openFPGALoader.exe -c xilinxPlatformCableUsb_alt --fpga-part xc7a35tfgg484 --detect-external-flash -v -v

.\openFPGALoader.exe `
  -c xilinxPlatformCableUsb_alt `
  --fpga-part xc6slx45tfgg484 `
  --external-flash `
  --dump-flash `
  --file-size 16777216 `
  -o 0 `
  flash_dump.bin `
  -v

.\openFPGALoader.exe -c xilinxPlatformCableUsb --fpga-part xc6slx9tqg144 --external-flash -f ..\..\..\..\test_fw\EL-24-02\mcc_servo_fpga_fw-0v3-2024_0620_0922.mcs  

@rem This worked previosly missed flash controller needed M25P40
.\openFPGALoader.exe -c digilent_hs3 --fpga-part xc6slx9tqg144 --external-flash -f ..\..\..\..\test_fw\EL-24-02\mcc_servo_fpga_fw-0v3-2024_0620_0922.mcs    

