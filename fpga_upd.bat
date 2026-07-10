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

