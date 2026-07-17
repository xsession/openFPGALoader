// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright (C) 2022-2026 Gwenhael Goavec-Merou <gwenhael.goavec-merou@trabucayre.com>
 */

#include "pathHelper.hpp"

#include <cstdlib>
#include <fstream>

#include <cstdio>
#include <cstring>
#include <unistd.h>

#include <chrono>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include "display.hpp"
#include "fx2_ll.hpp"
#include "xilinxPlatformCableUSB.hpp"

#define XPCU_BREQUEST             0xB0
#define XPCU_INITIALIZED_VID      0x03fd
#define XPCU_INITIALIZED_PID      0x0008
#define XPCU_CMD_DISABLE          0x10
#define XPCU_CMD_ENABLE           0x18
#define XPCU_CMD_SET_SPEED        0x28
#define XPCU_CMD_GPIO_WRITE       0x30
#define XPCU_CMD_STATUS           0x38
#define XPCU_CMD_RETURN_CONSTANT  0x40
#define XPCU_CMD_GET_VERSION      0x50
#define XPCU_CMD_SELECT_GPIO      0x52
#define XPCU_CMD_GPIO_TRANSFER    0xA6
#define XPCU_EP_JTAG_OUT_DEFAULT  0x02
#define XPCU_EP_JTAG_IN_DEFAULT   0x86
#define XPCU_STATUS_CONNECTED     0x40
#define XPCU_GPIO_TDI             0x01
#define XPCU_GPIO_TDO             0x01
#define XPCU_GPIO_TMS             0x02
#define XPCU_GPIO_TCK             0x04
#define XPCU_GPIO_PROG            0x08
#define XPCU_SPEED_CLASS_ENABLE   0x10
#define XPCU_VERSION_CPLD         0x01
#define XPCU_VERSION_CONST1       0x02
#define XPCU_VERSION_CONST2       0x03

#define TCK_OFFSET    0
#define TDO_OFFSET    4
#define TDI_OFFSET    0
#define TMS_OFFSET    4

#define TCK_IDX       (1 << TCK_OFFSET)
#define TDO_IDX       (1 << TDO_OFFSET)
#define TDI_IDX       (1 << TDI_OFFSET)
#define TMS_IDX       (1 << TMS_OFFSET)

namespace {

uint8_t xpcu_ep_jtag_out = XPCU_EP_JTAG_OUT_DEFAULT;
uint8_t xpcu_ep_jtag_in = XPCU_EP_JTAG_IN_DEFAULT;

bool fileExists(const std::string &path)
{
    std::ifstream f(path, std::ios::binary);
    return f.good();
}

std::string firmwareLeafForPid(uint16_t pid)
{
    return (pid == 0x0d) ? "xusb_emb.hex" : "xusb_xp2.hex";
}

std::string findPackagedFirmware(uint16_t pid)
{
	const std::string leaf = (pid == 0x0013) ?
		"xusb_xp2_loader.hex" : firmwareLeafForPid(pid);
    const std::string candidate = PathHelper::absolutePath(
        std::string(DATA_DIR) + "/openFPGALoader/" + leaf
    );

    if (fileExists(candidate)) {
        return candidate;
    }

	return {};
}

std::string findUpgradeFirmware(uint16_t pid)
{
	const bool xp2 = pid == 0x0013;
	const char *env = std::getenv(xp2 ?
		"OPENFPGALOADER_XUSB_XP2_FIRMWARE" :
		"OPENFPGALOADER_XUSB_XLP_FIRMWARE");
	if (env != nullptr && env[0] != '\0') {
		const std::string env_path = PathHelper::absolutePath(env);
		if (fileExists(env_path))
			return env_path;
	}

	const std::string packaged = PathHelper::absolutePath(
		std::string(DATA_DIR) + "/openFPGALoader/" +
		(xp2 ? "xusb_xp2.hex" : "xusb_xlp.hex"));
	return fileExists(packaged) ? packaged : std::string();
}

uint16_t firmwareHexVersion(const std::string &path)
{
	std::ifstream firmware(path);
	std::string line;
	while (std::getline(firmware, line)) {
		if (line.rfind(":0219B900", 0) != 0 || line.size() < 13)
			continue;
		const std::string version_text = line.substr(9, 4);
		char *end = nullptr;
		const unsigned long raw = std::strtoul(version_text.c_str(), &end, 16);
		if (end == nullptr || *end != '\0' || raw > 0xffff)
			return 0;
		return static_cast<uint16_t>(raw);
	}
	return 0;
}

std::string findXusbFirmwareFile(uint16_t pid, const std::string &firmware_path)
{
    if (!firmware_path.empty()) {
        return PathHelper::absolutePath(firmware_path);
    }

    const char *env = std::getenv("OPENFPGALOADER_XUSB_FIRMWARE");
    if (env != nullptr && env[0] != '\0') {
        const std::string envPath = PathHelper::absolutePath(env);
        if (fileExists(envPath)) {
            return envPath;
        }
    }

    const std::string packaged = findPackagedFirmware(pid);
    if (!packaged.empty()) {
        return packaged;
    }

    std::string firmware_file;

    if (strlen(VIVADO_DIR) > 0) {
        firmware_file = VIVADO_DIR "/data/xicom/";
    } else if (strlen(ISE_DIR) > 0) {
        firmware_file = ISE_DIR "/ISE_DS/ISE/bin/lin64/";
    } else {
        printError("missing FX2 firmware");
        printError("use --probe-firmware with something");
        printError("or set OPENFPGALOADER_XUSB_FIRMWARE=C:/path/to/xusb_emb.hex");
        printError("or place xusb_emb.hex in share/openFPGALoader beside the portable package");
        throw std::runtime_error("xilinxPlatformCableUSB: missing firmware");
    }

    firmware_file += firmwareLeafForPid(pid);
    return firmware_file;
}

bool parseEndpointEnv(const char *name, uint8_t &endpoint)
{
	const char *value = std::getenv(name);
	if (value == nullptr || value[0] == '\0')
		return false;

	char *end = nullptr;
	const unsigned long parsed = std::strtoul(value, &end, 0);
	if (end == value || *end != '\0' || parsed > 0xff) {
		printWarn(std::string("ignoring invalid ") + name + "=" + value);
		return false;
	}

	endpoint = static_cast<uint8_t>(parsed);
	return true;
}

std::string endpointToHex(uint8_t endpoint)
{
	char buf[8];
	snprintf(buf, sizeof(buf), "0x%02x", endpoint);
	return std::string(buf);
}

unsigned int xpcuRetryAttempts()
{
	const char *value = std::getenv("OPENFPGALOADER_XPCU_CTRL_RETRIES");
	if (value == nullptr || value[0] == '\0')
		return 8;

	char *end = nullptr;
	const unsigned long parsed = std::strtoul(value, &end, 0);
	if (end == value || *end != '\0' || parsed < 1 || parsed > 200)
		return 8;

	return static_cast<unsigned int>(parsed);
}

unsigned int xpcuRetryDelayMs()
{
	const char *value = std::getenv("OPENFPGALOADER_XPCU_CTRL_RETRY_DELAY_MS");
	if (value == nullptr || value[0] == '\0')
		return 150;

	char *end = nullptr;
	const unsigned long parsed = std::strtoul(value, &end, 0);
	if (end == value || *end != '\0' || parsed < 10 || parsed > 5000)
		return 150;

	return static_cast<unsigned int>(parsed);
}


bool xpcuBoolEnv(const char *name)
{
	const char *value = std::getenv(name);
	return value != nullptr && value[0] != '\0' && std::string(value) != "0";
}

bool xpcuTdoMaskWasExplicit()
{
	const char *value = std::getenv("OPENFPGALOADER_XPCU_TDO_MASK");
	return value != nullptr && value[0] != '\0';
}

bool xpcuForceControlBitbang()
{
	/* The 0xA6 trigger stalls endpoint zero with the Xilinx EMB firmware on
	 * the Windows libusb/WinUSB backend.  Start with the reliable control
	 * path there; retain an explicit override for protocol development. */
	if (xpcuBoolEnv("OPENFPGALOADER_XPCU_ACCELERATED"))
		return false;
#ifdef _WIN32
	return true;
#else
	return xpcuBoolEnv("OPENFPGALOADER_XPCU_CONTROL_BITBANG") ||
		xpcuBoolEnv("OPENFPGALOADER_XPCU_SKIP_GPIO_TRANSFER_CTRL");
#endif
}

bool xpcuExplicitControlBitbang()
{
	return xpcuBoolEnv("OPENFPGALOADER_XPCU_CONTROL_BITBANG") ||
		xpcuBoolEnv("OPENFPGALOADER_XPCU_SKIP_GPIO_TRANSFER_CTRL");
}

uint8_t xpcuTdoMask()
{
	const char *value = std::getenv("OPENFPGALOADER_XPCU_TDO_MASK");
	if (value == nullptr || value[0] == '\0')
		return XPCU_GPIO_TDO;

	char *end = nullptr;
	const unsigned long parsed = std::strtoul(value, &end, 0);
	if (end == value || *end != '\0' || (parsed != 0x01 && parsed != 0x02)) {
		printWarn(std::string("ignoring invalid OPENFPGALOADER_XPCU_TDO_MASK=") + value);
		return XPCU_GPIO_TDO;
	}

	return static_cast<uint8_t>(parsed);
}

void xpcuRecoverUsbPipes(FX2_ll *fx2)
{
	fx2->clear_halt(xpcu_ep_jtag_out);
	fx2->clear_halt(xpcu_ep_jtag_in);
	std::this_thread::sleep_for(std::chrono::milliseconds(100));
}

bool xpcuGpioTransferCtrl(FX2_ll *fx2, uint32_t bit_count)
{
	/* The transfer count is 24-bit and zero-indexed.  Its high byte shares
	 * wValue with the command, while the low 16 bits are carried in wIndex. */
	const uint16_t value = XPCU_CMD_GPIO_TRANSFER |
		static_cast<uint16_t>(((bit_count >> 16) & 0xffu) << 8);
	return fx2->write_ctrl(XPCU_BREQUEST, value, nullptr, 0,
		static_cast<uint16_t>(bit_count & 0xffffu));
}

bool xpcuReadCtrlRetry(FX2_ll *fx2, uint8_t request, uint16_t value,
		uint8_t *buf, uint16_t len, uint16_t index, const char *what)
{
	const unsigned int attempts = xpcuRetryAttempts();
	const unsigned int delay_ms = xpcuRetryDelayMs();

	for (unsigned int attempt = 1; attempt <= attempts; attempt++) {
		if (fx2->read_ctrl(request, value, buf, len, index))
			return true;

		if (attempt != attempts)
			std::this_thread::sleep_for(std::chrono::milliseconds(delay_ms));
	}

	printError(std::string("XPCU control read failed after retries: ") + what);
	return false;
}

bool xpcuWriteCtrlRetry(FX2_ll *fx2, uint8_t request, uint16_t value,
		uint8_t *buf, uint16_t len, uint16_t index, const char *what)
{
	const unsigned int attempts = xpcuRetryAttempts();
	const unsigned int delay_ms = xpcuRetryDelayMs();

	for (unsigned int attempt = 1; attempt <= attempts; attempt++) {
		if (fx2->write_ctrl(request, value, buf, len, index))
			return true;

		if (attempt != attempts)
			std::this_thread::sleep_for(std::chrono::milliseconds(delay_ms));
	}

	printError(std::string("XPCU control write failed after retries: ") + what);
	return false;
}

bool xpcuPrimeAcceleratedTransfer(FX2_ll *fx2)
{
	uint8_t zero[2] = {0, 0};
	if (!xpcuWriteCtrlRetry(fx2, XPCU_BREQUEST, XPCU_CMD_DISABLE,
			nullptr, 0, 0, "disable before accelerated prime") ||
			!xpcuWriteCtrlRetry(fx2, XPCU_BREQUEST, XPCU_CMD_SET_SPEED,
			nullptr, 0, 0x11, "accelerated prime speed") ||
			!xpcuWriteCtrlRetry(fx2, XPCU_BREQUEST, XPCU_CMD_ENABLE,
			nullptr, 0, 0, "enable before accelerated prime") ||
			!xpcuGpioTransferCtrl(fx2, 2))
		return false;

	if (fx2->write(xpcu_ep_jtag_out, zero, sizeof(zero)) != sizeof(zero))
		return false;

	return xpcuWriteCtrlRetry(fx2, XPCU_BREQUEST, XPCU_CMD_SET_SPEED,
		nullptr, 0, 0x12, "post-prime speed");
}

void waitForXpcuControlReady(FX2_ll *fx2)
{
	uint8_t buf[2] = {0, 0};
	if (!xpcuReadCtrlRetry(fx2, XPCU_BREQUEST, XPCU_CMD_RETURN_CONSTANT,
			buf, 2, 0, "initial constant/readiness probe")) {
		throw std::runtime_error("Xilinx Platform Cable USB did not become control-ready after reload");
	}
}

void configureXpcuUsbInterface(FX2_ll *fx2)
{
	uint8_t env_out_ep = XPCU_EP_JTAG_OUT_DEFAULT;
	uint8_t env_in_ep = XPCU_EP_JTAG_IN_DEFAULT;
	const bool out_from_env = parseEndpointEnv("OPENFPGALOADER_XPCU_OUT_EP", env_out_ep);
	const bool in_from_env = parseEndpointEnv("OPENFPGALOADER_XPCU_IN_EP", env_in_ep);

	if (out_from_env || in_from_env) {
		xpcu_ep_jtag_out = env_out_ep;
		xpcu_ep_jtag_in = env_in_ep;
		printWarn("using Xilinx Platform Cable USB endpoint override: OUT=" +
				endpointToHex(xpcu_ep_jtag_out) + " IN=" +
				endpointToHex(xpcu_ep_jtag_in));
		return;
	}

	const char *skip_alt = std::getenv("OPENFPGALOADER_XPCU_SKIP_ALT_SETTING");
	if (skip_alt != nullptr && skip_alt[0] != '\0' && std::string(skip_alt) != "0") {
		printWarn("skipping Xilinx Platform Cable USB alternate setting selection");
		return;
	}

	uint8_t out_ep = XPCU_EP_JTAG_OUT_DEFAULT;
	uint8_t in_ep = XPCU_EP_JTAG_IN_DEFAULT;
	if (fx2->select_bulk_endpoints(0, out_ep, in_ep, 1)) {
		xpcu_ep_jtag_out = out_ep;
		xpcu_ep_jtag_in = in_ep;
		printInfo("XPCU bulk endpoints: OUT=" + endpointToHex(xpcu_ep_jtag_out) +
				" IN=" + endpointToHex(xpcu_ep_jtag_in));
		return;
	}

	printWarn("unable to select a descriptor-discovered bulk endpoint pair");
	printWarn("falling back to Xilinx Platform Cable USB default endpoints 0x02/0x86");
	xpcu_ep_jtag_out = XPCU_EP_JTAG_OUT_DEFAULT;
	xpcu_ep_jtag_in = XPCU_EP_JTAG_IN_DEFAULT;
}

} // namespace

XilinxPlatformCableUSB::XilinxPlatformCableUSB(const uint16_t vid,
	const uint16_t pid,
	uint32_t clkHz,
	const std::string &firmware_path,
	int8_t verbose): _verbose(verbose), _nb_bit(0), _nb_tdo_bit(0),
		_curr_tms(0), _curr_tdi(0), _buffer_size(4096),
		_buffer_bit_size((_buffer_size / 2 * 4) - 1),
		_use_control_bitbang(xpcuForceControlBitbang()),
		_tdo_mask(xpcuTdoMask()),
		_primary_tdo_mask(_tdo_mask),
		_tdo_mask_explicit(xpcuTdoMaskWasExplicit())
{
	// std::string firmware_file;
	/* firmare path must be known:
	 * 1/ provided by user
	 * 2/ from Vivado install directory
	 * 3/ from ISE install directory
	 */

	const bool already_initialized = vid == XPCU_INITIALIZED_VID &&
		pid == XPCU_INITIALIZED_PID;
	std::string firmware_file;
	if (!already_initialized)
		firmware_file = findXusbFirmwareFile(pid, firmware_path);
	// if (firmware_path.empty() && strlen(ISE_DIR) == 0 && strlen(VIVADO_DIR) == 0) {
	// 	printError("missing FX2 firmware");
	// 	printError("use --probe-firmware with something");
	// 	printError("like /opt/Xilinx/14.7/ISE_DS/ISE/bin/lin64/xusb_xp2.hex for ISE");
	// 	printError("or   /opt/Xilinx/Vivado/VERSION/data/xicom/xusb_xp2.hex for Vivado");
	// 	printError("Or use -DISE_DIR=/opt/Xilinx/14.7 / -DVIVADO_DIR=/opt/Xilinx/Vivado/VERSION at build time");
	// 	throw std::runtime_error("xilinxPlatformCableUSB: missing firmware");
	// }

	// /* Extract firmware according to possibilities */
	// if (!firmware_path.empty())
	// 	firmware_file = firmware_path;
	// else if (strlen(VIVADO_DIR) > 0)
	// 	firmware_file = VIVADO_DIR "/data/xicom/";
	// else if (strlen(ISE_DIR) > 0)
	// 	firmware_file = ISE_DIR "/ISE_DS/ISE/bin/lin64/";

	// if (firmware_path.empty()) {
	// 	if (pid == 0x0d)
	// 		firmware_file += "xusb_emb.hex";
	// 	else
	// 		firmware_file += "xusb_xp2.hex";
	// }
	if (!firmware_file.empty())
		printInfo("firmware_file : " + firmware_file);

	try {
		/* The initialized identity is also exposed as a cable definition so
		 * --scan-usb can report it.  Do not treat that identity as an FX2
		 * bootloader or attempt to upload firmware to it again. */
		fx2 = std::make_unique<FX2_ll>(already_initialized ? 0 : vid,
				already_initialized ? 0 : pid, XPCU_INITIALIZED_VID,
				XPCU_INITIALIZED_PID, firmware_file);
	} catch (std::exception &e) {
		printError(e.what());
		throw std::runtime_error("lowlevel init failed");
	}

	const std::string upgrade_firmware = findUpgradeFirmware(pid);
	const uint16_t upgrade_version = upgrade_firmware.empty() ? 0 :
		firmwareHexVersion(upgrade_firmware);
	const bool xp2_upgrade = pid == 0x0013;
	const bool force_upgrade = xpcuBoolEnv(xp2_upgrade ?
		"OPENFPGALOADER_XPCU_XP2_UPGRADE" :
		"OPENFPGALOADER_XPCU_XLP_UPGRADE");
	const bool auto_upgrade = !already_initialized &&
		(pid == 0x000d || pid == 0x0013) && upgrade_version != 0;
	const bool skip_upgrade =
		xpcuBoolEnv("OPENFPGALOADER_XPCU_SKIP_FIRMWARE_UPGRADE") ||
		xpcuBoolEnv(xp2_upgrade ? "OPENFPGALOADER_XPCU_SKIP_XP2_UPGRADE" :
			"OPENFPGALOADER_XPCU_SKIP_XLP_UPGRADE");
	if (!skip_upgrade && (force_upgrade || auto_upgrade)) {
		if (upgrade_firmware.empty() || upgrade_version == 0)
			throw std::runtime_error("XPCU firmware upgrade requested but the upgrade HEX was not found or is invalid");
		char version_text[16];
		snprintf(version_text, sizeof(version_text), "%04x", upgrade_version);
		printInfo(std::string("XPCU ") + (xp2_upgrade ? "XP2" : "XLP") +
			" upgrade firmware: " + upgrade_firmware + " (version " +
			version_text + ")");
		if (!fx2->reload_firmware(upgrade_firmware, XPCU_INITIALIZED_VID,
				XPCU_INITIALIZED_PID))
			throw std::runtime_error("XPCU firmware upgrade/reload failed");
	}

	configureXpcuUsbInterface(fx2.get());
	waitForXpcuControlReady(fx2.get());

	const uint16_t fx2_firmware_version = displayCableVersion();
	if (fx2_firmware_version != 0x0404 && !xpcuExplicitControlBitbang())
		_use_control_bitbang = false;

	/* Write GPIO bit */
	if (!xpcuWriteCtrlRetry(fx2.get(), XPCU_BREQUEST, 0x030, nullptr, 0,
			(1 << 3), "GPIO setup"))
		throw std::runtime_error("Unable to setup GPIO bit");

	if (!enableDevice(true))
		throw std::runtime_error("Unable to enable device");

	if (!xpcuWriteCtrlRetry(fx2.get(), XPCU_BREQUEST, XPCU_CMD_SELECT_GPIO,
			nullptr, 0, 0, "select GPIO JTAG chain"))
		throw std::runtime_error("Unable to select GPIO JTAG chain");

	uint8_t buf[1];
	if (!xpcuReadCtrlRetry(fx2.get(), XPCU_BREQUEST, XPCU_CMD_STATUS, buf, 1,
			0, "status"))
		throw std::runtime_error("Unable to read status.");

	char mess[64];
	snprintf(mess, sizeof(mess), "status %02x connected: %s",
			buf[0], (buf[0] & XPCU_STATUS_CONNECTED) ? "yes" : "no");
	printInfo(mess);

	_in_buf = std::make_unique<uint8_t[]>(_buffer_size);

	setClkFreq(clkHz);
	if (!_use_control_bitbang && fx2_firmware_version != 0x0404) {
		if (!xpcuPrimeAcceleratedTransfer(fx2.get()))
			throw std::runtime_error("Unable to prime XPCU accelerated transfer engine");
		/* The prime sequence ends at speed class 0x12. Restore the requested
		 * rate after the accelerator is active. */
		setClkFreq(clkHz);
		printInfo("XPCU accelerated transfer engine primed");
	}
	if (_use_control_bitbang) {
		printWarn("XPCU control-transfer JTAG mode forced; transfers will be slower");
	}
}

XilinxPlatformCableUSB::~XilinxPlatformCableUSB()
{
	flush();
	enableDevice(false);
}

int XilinxPlatformCableUSB::setClkFreq(uint32_t clkHz)
{
	/* speed table: index Hz; bit 4 must always be set in the speed class */
	static constexpr uint32_t speeds[] = {12000000, 6000000, 3000000, 1500000, 750000};
	uint8_t speed = 4;  /* default: slowest */
	for (uint8_t i = 0; i < sizeof(speeds) / sizeof(speeds[0]); i++) {
		if (speeds[i] <= clkHz) {
			speed = i;
			break;
		}
	}
	if (!xpcuWriteCtrlRetry(fx2.get(), XPCU_BREQUEST, XPCU_CMD_SET_SPEED, nullptr, 0,
				speed | XPCU_SPEED_CLASS_ENABLE, "set speed")) {
		printError("setClkFreq: failed to set speed");
		return -1;
	}
	_clkHZ = speeds[speed];
	printInfo("Jtag frequency : requested " + std::to_string(clkHz) +
			" Hz -> real " + std::to_string(_clkHZ) + " Hz");
	return _clkHZ;
}

int XilinxPlatformCableUSB::writeTMS(const uint8_t *tms, uint32_t len,
		bool flush_buffer, const uint8_t tdi)
{
	int ret;

	if (len == 0)
		return flush_buffer ? flush() : 0;

	_curr_tdi = tdi ? 1 : 0;

	for (uint32_t i = 0; i < len; i++) {
		_curr_tms = (tms[i >> 3] >> (i & 0x07)) & 0x01;
		if (storeBit(_curr_tdi, _curr_tms, 1, 0)) {
			if (write(nullptr, 0) < 0)
				return -1;
		}
	}

	if (flush_buffer) {
		ret = flush();
		if (ret < 0)
			return ret;
	}

	return len;
}

int XilinxPlatformCableUSB::writeTDI(const uint8_t *tx, uint8_t *rx, uint32_t len, bool end)
{
	if (len == 0)
		return 0;

	if (rx && _nb_bit != 0) {
		if (write(nullptr, 0) < 0)
			return -1;
	}

	uint32_t rx_offset = 0;

	for (uint32_t i = 0; i < len; i++) {
		bool last_bit = (i == len - 1 && end);
		_curr_tdi = tx ? (0x01 & ((tx[i >> 3]) >> (i & 0x07))) : 0;

		if (last_bit)
			_curr_tms = 1;

		if (storeBit(_curr_tdi, _curr_tms, 1, rx ? 1 : 0)) {
			uint32_t bits = _nb_tdo_bit;
			if (write(rx, rx_offset) < 0)
				return -1;
			rx_offset += bits;
		}
	}

	if (_nb_bit != 0 && (end || rx)) {
		if (write(rx, rx_offset) < 0)
			return -1;
	}

	return len;
}

int XilinxPlatformCableUSB::toggleClk([[maybe_unused]] uint8_t tms,
	[[maybe_unused]] uint8_t tdi, uint32_t clk_len)
{
	for (uint32_t i = 0; i < clk_len; i++) {
		if (storeBit(_curr_tdi, _curr_tms, 1, 0)) {
			if (write(nullptr, 0) < 0)
				return -1;
		}
	}

	/* Flush buffer if not empty */
	if (_nb_bit != 0) {
		if (flush() < 0)
			return -1;
	}

	return clk_len;
}

int XilinxPlatformCableUSB::flush()
{
	return write(nullptr, 0);
}

/* TMS 1st nibble, TDI 2nd nibble, TDO 3rd nibble, TCK 4th nibble
 *   byte n    byte n+1
 *  [7:4 3:0] [7:4 3:0]
 *   TMS TDI   TDO TCK
 */
bool XilinxPlatformCableUSB::storeBit(uint8_t tdi, uint8_t tms,
		uint8_t tck, uint8_t tdo) noexcept
{
	const uint32_t buf_pos = (_nb_bit >> 2) << 1;
	const uint8_t bit_pos = _nb_bit & 0x03;

	if (bit_pos == 0)
		_in_buf[buf_pos] = _in_buf[buf_pos + 1] = 0;

	if (tms)
		_in_buf[buf_pos] |= (TMS_IDX << bit_pos);
	if (tdi)
		_in_buf[buf_pos] |= (TDI_IDX << bit_pos);
	if (tdo)
		_in_buf[buf_pos + 1] |= (TDO_IDX << bit_pos);
	if (tck)
		_in_buf[buf_pos + 1] |= (TCK_IDX << bit_pos);

	_nb_bit++;
	if (tdo)
		_nb_tdo_bit++;

	return _nb_bit >= _buffer_bit_size;
}

/* Compute how many bytes EP6 will return for nb_bit TDO bits.
 * The device uses a shift-register encoding: a 16-bit register that grows
 * to 32-bit after 16 bits, then a new register starts every 32 bits.
 */
uint32_t XilinxPlatformCableUSB::rxBufSize(uint32_t nb_bit) noexcept
{
	const uint32_t full_groups = nb_bit / 32;
	const uint32_t rem = nb_bit & 31u;
	return full_groups * 4 + (rem == 0 ? 0 : (rem > 16 ? 4 : 2));
}

int XilinxPlatformCableUSB::write(uint8_t *rx, uint32_t rx_offset)
{
	if (_nb_bit == 0)
		return 0;

	/* The old GPIO command is slow but works with cable/Windows driver
	 * combinations which accept normal control requests and reject the A6
	 * accelerated-transfer trigger. */
	if (_use_control_bitbang) {
		uint32_t tdo_bit = 0;
		uint32_t raw_tdo = 0;
		uint32_t raw_tdo_bit = 0;
		const uint8_t tdo_mask = _tdo_mask;
		const bool capture_batch = _nb_tdo_bit != 0;
		/* Command 0x38 returns the bit following the one shifted by the most
		 * recent 0x30 clock.  The initial IDCODE/IR capture bit is mandated to
		 * be one, so seed it and place subsequent status samples one bit later.
		 * The sample following the final requested clock belongs to the next
		 * TAP bit and is intentionally discarded. */
		if (capture_batch && rx != nullptr) {
			const uint32_t out_bit = rx_offset;
			rx[out_bit >> 3] |= (1u << (out_bit & 7));
			tdo_bit = 1;
		}
		for (uint32_t i = 0; i < _nb_bit; i++) {
			const uint32_t buf_pos = (i >> 2) << 1;
			const uint8_t bit_pos = i & 0x03;
			uint16_t pins = XPCU_GPIO_PROG;
			if (_in_buf[buf_pos] & (TDI_IDX << bit_pos))
				pins |= XPCU_GPIO_TDI;
			if (_in_buf[buf_pos] & (TMS_IDX << bit_pos))
				pins |= XPCU_GPIO_TMS;

			const bool capture = _in_buf[buf_pos + 1] & (TDO_IDX << bit_pos);

			/* Set TDI/TMS while TCK is low. */
			if (!fx2->write_ctrl(XPCU_BREQUEST, XPCU_CMD_GPIO_WRITE,
					nullptr, 0, pins)) {
				printError("XPCU control-transfer JTAG write failed");
				_nb_bit = 0;
				_nb_tdo_bit = 0;
				return -1;
			}

			/* Complete the bit with one JTAG rising edge. */
			if (!fx2->write_ctrl(XPCU_BREQUEST, XPCU_CMD_GPIO_WRITE,
					nullptr, 0, pins | XPCU_GPIO_TCK)) {
				printError("XPCU control-transfer JTAG write failed");
				_nb_bit = 0;
				_nb_tdo_bit = 0;
				return -1;
			}

			if (capture && tdo_bit < _nb_tdo_bit) {
				uint8_t status = 0;
				if (!fx2->read_ctrl(XPCU_BREQUEST, XPCU_CMD_STATUS,
						&status, 1, 0)) {
					printError("XPCU control-transfer JTAG read failed");
					_nb_bit = 0;
					_nb_tdo_bit = 0;
					return -1;
				}
				if (rx != nullptr) {
					const uint32_t out_bit = rx_offset + tdo_bit;
					if (status & tdo_mask)
						rx[out_bit >> 3] |= (1u << (out_bit & 7));
					else
						rx[out_bit >> 3] &= ~(1u << (out_bit & 7));
				}
				if (status & tdo_mask)
					raw_tdo |= (1u << raw_tdo_bit);
				raw_tdo_bit++;
				tdo_bit++;
			}
		}
		if (_verbose > 1 && capture_batch && _nb_tdo_bit == 32) {
			char raw_message[96];
			snprintf(raw_message, sizeof(raw_message),
				"XPCU raw delayed TDO: 0x%08x (%u samples)",
				raw_tdo, raw_tdo_bit);
			printInfo(raw_message);
		}
		_nb_bit = 0;
		_nb_tdo_bit = 0;
		return 0;
	}

	/* N ops: N/4 pairs of 2 bytes each (round up to complete pair) */
	uint32_t xfer_tx = (_nb_bit >> 1) & ~0x01u;
	xfer_tx += ((_nb_bit & 0x03) != 0) ? 2 : 0;
	if (_verbose > 1) {
		char message[256];
		int off = snprintf(message, sizeof(message),
			"XPCU A6 TX: bits=%u tdo=%u bytes=%u data=",
			_nb_bit, _nb_tdo_bit, xfer_tx);
		for (uint32_t i = 0; i < xfer_tx && i < 16 && off > 0 &&
				off < static_cast<int>(sizeof(message)); i++)
			off += snprintf(message + off, sizeof(message) - off, "%02x",
					_in_buf[i]);
		printInfo(message);
	}

	/* This firmware expects the zero-based transfer count.  Sending N directly
	 * also makes common 32-bit scans a multiple of four, a known XPCU CPLD
	 * failure case which stalls the bulk IN endpoint. */
	if (!xpcuGpioTransferCtrl(fx2.get(), _nb_bit - 1)) {
		printWarn("XPCU accelerated transfer is unavailable; switching to control-transfer JTAG mode");
		printWarn("set OPENFPGALOADER_XPCU_CONTROL_BITBANG=1 to select this mode immediately");
		_use_control_bitbang = true;
		return write(rx, rx_offset);
	}

	if (fx2->write(xpcu_ep_jtag_out, _in_buf.get(), xfer_tx) != (int)xfer_tx) {
		xpcuRecoverUsbPipes(fx2.get());
		printError("XPCU bulk OUT transfer failed on endpoint " + endpointToHex(xpcu_ep_jtag_out));
		printError("try Zadig WinUSB/libusbK for the post-firmware 03fd:0008 interface, or test OPENFPGALOADER_XPCU_OUT_EP / OPENFPGALOADER_XPCU_IN_EP");
		return -1;
	}

	if (rx) {
		if (_nb_tdo_bit != _nb_bit) {
			printError("Unable to decode mixed TDO/non-TDO transfer");
			return -1;
		}

		uint32_t xfer_rx = rxBufSize(_nb_tdo_bit);
		std::vector<uint8_t> rx_buf(xfer_rx);
		if (fx2->read(xpcu_ep_jtag_in, rx_buf.data(), xfer_rx) != (int)xfer_rx) {
			xpcuRecoverUsbPipes(fx2.get());
			printError("XPCU bulk IN transfer failed on endpoint " + endpointToHex(xpcu_ep_jtag_in));
			printError("try Zadig WinUSB/libusbK for the post-firmware 03fd:0008 interface, or test OPENFPGALOADER_XPCU_OUT_EP / OPENFPGALOADER_XPCU_IN_EP");
			return -1;
		}
		if (_verbose > 1 || xpcuBoolEnv("OPENFPGALOADER_XPCU_TRACE_A6")) {
			char message[256];
			int off = snprintf(message, sizeof(message), "XPCU A6 RX: bytes=%u data=",
				xfer_rx);
			for (uint32_t i = 0; i < xfer_rx && i < 32 && off > 0 &&
					off < static_cast<int>(sizeof(message)); i++)
				off += snprintf(message + off, sizeof(message) - off, "%02x",
						rx_buf[i]);
			printInfo(message);
		}

		/* Decode shift-register encoded TDO bits into rx.
		 * Each group of up to 32 bits occupies a 16 or 32-bit little-endian
		 * shift register: bit_k is at position (reg_size - group + k).
		 */
		uint32_t buf_off = 0;
		uint32_t remaining = _nb_tdo_bit;
		uint32_t bit_idx = 0;
		while (remaining > 0) {
			const uint32_t group = (remaining > 32) ? 32 : remaining;
			const uint32_t reg_size = (group > 16) ? 32 : 16;
			const uint32_t shift = reg_size - group;
			uint32_t reg = 0;
			for (uint32_t b = 0; b < reg_size / 8; b++)
				reg |= (uint32_t)rx_buf[buf_off + b] << (b * 8);
			const uint32_t base = rx_offset + bit_idx;
			if ((base & 7u) == 0 && (group & 7u) == 0) {
				uint8_t *out = rx + (base >> 3);
				for (uint32_t b = 0; b < group / 8; b++)
					out[b] = static_cast<uint8_t>((reg >> (shift + b * 8)) & 0xFF);
			} else {
				for (uint32_t k = 0; k < group; k++) {
					const uint32_t out_bit = base + k;
					if ((reg >> (shift + k)) & 1)
						rx[out_bit >> 3] |= (1 << (out_bit & 7));
					else
						rx[out_bit >> 3] &= ~(1 << (out_bit & 7));
				}
			}
			buf_off += reg_size / 8;
			bit_idx += group;
			remaining -= group;
		}
	}

	_nb_bit = 0;
	_nb_tdo_bit = 0;
	return 0;
}

bool XilinxPlatformCableUSB::selectAlternateTdoMask()
{
	if (_tdo_mask_explicit || !_use_control_bitbang)
		return false;

	const uint8_t alternate = (_primary_tdo_mask == 0x01) ? 0x02 : 0x01;
	if (_tdo_mask == alternate)
		return false;

	_tdo_mask = alternate;
	printWarn("Retrying XPCU control-transfer JTAG with alternate TDO mask " +
		endpointToHex(_tdo_mask));
	return true;
}

void XilinxPlatformCableUSB::restorePrimaryTdoMask()
{
	_tdo_mask = _primary_tdo_mask;
}

bool XilinxPlatformCableUSB::enableDevice(bool enable)
{
	if (!xpcuWriteCtrlRetry(fx2.get(), XPCU_BREQUEST,
				(enable ? XPCU_CMD_ENABLE : XPCU_CMD_DISABLE), nullptr, 0,
				0, enable ? "enable device" : "disable device")) {
		char mess[64];
		snprintf(mess, sizeof(mess), "Unable to %s device",
				(enable ? "enable" : "disable"));
		printError(mess);
		return false;
	}
	return true;
}

uint16_t XilinxPlatformCableUSB::displayCableVersion()
{
	uint8_t buf[2];

	if (!xpcuReadCtrlRetry(fx2.get(), XPCU_BREQUEST, XPCU_CMD_RETURN_CONSTANT,
			buf, 2, 0, "constant"))
		throw std::runtime_error("Unable to read constant.");
	const uint16_t const0 = ((uint16_t)buf[1] << 8) | buf[0];

	if (!xpcuReadCtrlRetry(fx2.get(), XPCU_BREQUEST, XPCU_CMD_GET_VERSION,
			buf, 2, 0, "firmware version"))
		throw std::runtime_error("Unable to read firmware version.");
	const uint16_t fx2_firmware = ((uint16_t)buf[1] << 8) | buf[0];

	if (!xpcuReadCtrlRetry(fx2.get(), XPCU_BREQUEST, XPCU_CMD_GET_VERSION,
			buf, 2, XPCU_VERSION_CPLD, "CPLD version"))
		throw std::runtime_error("Unable to read CPLD version.");
	const uint16_t cpld_firmware = ((uint16_t)buf[1] << 8) | buf[0];

	if (!xpcuReadCtrlRetry(fx2.get(), XPCU_BREQUEST, XPCU_CMD_GET_VERSION,
			buf, 2, XPCU_VERSION_CONST1, "const 1"))
		throw std::runtime_error("Unable to read const 1.");
	const uint16_t const1 = ((uint16_t)buf[1] << 8) | buf[0];

	if (!xpcuReadCtrlRetry(fx2.get(), XPCU_BREQUEST, XPCU_CMD_GET_VERSION,
			buf, 2, XPCU_VERSION_CONST2, "const 2"))
		throw std::runtime_error("Unable to read const 2.");
	const uint16_t const2 = ((uint16_t)buf[1] << 8) | buf[0];

	printf("FX2 version:    %04x\n", fx2_firmware);
	printf("CPLD version:   %04x\n", cpld_firmware);
	printf("Const0 version: %04x\n", const0);
	printf("Const1 version: %04x\n", const1);
	printf("Const2 version: %04x\n", const2);
	return fx2_firmware;
}
