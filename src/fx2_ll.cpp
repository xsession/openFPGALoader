// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright (c) 2021 Gwenhael Goavec-Merou
 */

#include <unistd.h>

#include <cstdlib>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

#include <libusb.h>

#include "display.hpp"
#include "fx2_ll.hpp"
#include "ihexParser.hpp"
#include "progressBar.hpp"

#define FX2_FIRM_LOAD 0xA0
#define FX2_GCR_CPUCS 0xe600
#define FX2_GCR_CPUCS_8051_RES (1 << 0)

namespace {

bool fx2VerboseUsbErrors()
{
	const char *value = std::getenv("OPENFPGALOADER_FX2_VERBOSE_USB_ERRORS");
	return value != nullptr && value[0] != '\0' && std::string(value) != "0";
}

unsigned int fx2ControlTimeoutMs()
{
	const char *value = std::getenv("OPENFPGALOADER_FX2_CTRL_TIMEOUT_MS");
	if (value == nullptr || value[0] == '\0')
		return 2000;

	char *end = nullptr;
	const unsigned long parsed = std::strtoul(value, &end, 0);
	if (end == value || *end != '\0' || parsed < 100 || parsed > 30000)
		return 2000;

	return static_cast<unsigned int>(parsed);
}

} // namespace

FX2_ll::FX2_ll(uint16_t uninit_vid, uint16_t uninit_pid, uint16_t vid,
		uint16_t pid, const std::string &firmware_path) :
	dev_handle(nullptr), usb_ctx(nullptr)
{
	int ret;
	bool reenum = false;

	if (libusb_init(&usb_ctx) < 0) {
		throw std::runtime_error("libusb init failed");
	}

	/* try to open uninitialized device */
	if (uninit_vid != 0 && uninit_pid != 0) {
		dev_handle = libusb_open_device_with_vid_pid(usb_ctx, uninit_vid, uninit_pid);
		if (dev_handle) {
			ret = libusb_claim_interface(dev_handle, 0);
			if (ret) {
				printError("claim interface failed: " +
						std::string(libusb_error_name(ret)));
				libusb_close(dev_handle);
				libusb_exit(usb_ctx);
				throw std::runtime_error("claim interface failed");
			}

			/* load firmware */
			if (!load_firmware(firmware_path)) {
				libusb_close(dev_handle);
				libusb_exit(usb_ctx);
				throw std::runtime_error("fx2 firmware load failed");
			}
			close();
			reenum = true;
		}
	}

	/* try to open an already initialized device.
	 * Since FX2 may be not immediately ready, retry with a delay.
	 */
	ProgressBar progress("Waiting reload", 100, 50, false);
	int timeout = 100;
	do {
		dev_handle = libusb_open_device_with_vid_pid(usb_ctx, vid, pid);
		timeout--;
		progress.display(100 - timeout);
		if (!dev_handle)
			sleep(1);
	} while (!dev_handle && timeout > 0 && reenum);

	if (!dev_handle) {
		progress.fail();
		throw std::runtime_error("FX2: fail to open device");
	}
	progress.done();

	ret = libusb_claim_interface(dev_handle, 0);
	if (ret) {
		printError("claim interface failed: " +
				std::string(libusb_error_name(ret)));
		libusb_close(dev_handle);
		libusb_exit(usb_ctx);
		throw std::runtime_error("claim interface failed");
	}
}

bool FX2_ll::set_interface_alt_setting(const int if_num, const int alt_setting)
{
	const int ret = libusb_set_interface_alt_setting(dev_handle, if_num, alt_setting);
	if (ret && ret != LIBUSB_ERROR_NOT_FOUND) {
		printWarn("unable to select alternate interface " + std::to_string(alt_setting) +
				": " + std::string(libusb_error_name(ret)));
		return false;
	}
	return true;
}

bool FX2_ll::select_bulk_endpoints(const int if_num, uint8_t &out_endpoint,
		uint8_t &in_endpoint, const int preferred_alt_setting)
{
	libusb_device *dev = libusb_get_device(dev_handle);
	libusb_config_descriptor *config = nullptr;

	if (libusb_get_active_config_descriptor(dev, &config) != LIBUSB_SUCCESS ||
			config == nullptr) {
		printWarn("unable to read active USB configuration descriptor");
		return false;
	}

	struct candidate_t {
		int alt_setting;
		uint8_t out_ep;
		uint8_t in_ep;
		bool preferred;
	};

	std::vector<candidate_t> candidates;

	for (uint8_t i = 0; i < config->bNumInterfaces; i++) {
		const libusb_interface &interface = config->interface[i];
		for (int a = 0; a < interface.num_altsetting; a++) {
			const libusb_interface_descriptor &alt = interface.altsetting[a];
			if (alt.bInterfaceNumber != if_num)
				continue;

			uint8_t out_ep = 0;
			uint8_t in_ep = 0;

			for (uint8_t e = 0; e < alt.bNumEndpoints; e++) {
				const libusb_endpoint_descriptor &ep = alt.endpoint[e];
				const uint8_t attrs = ep.bmAttributes & LIBUSB_TRANSFER_TYPE_MASK;
				if (attrs != LIBUSB_TRANSFER_TYPE_BULK)
					continue;

				if ((ep.bEndpointAddress & LIBUSB_ENDPOINT_DIR_MASK) == LIBUSB_ENDPOINT_IN) {
					if (in_ep == 0)
						in_ep = ep.bEndpointAddress;
				} else {
					if (out_ep == 0)
						out_ep = ep.bEndpointAddress;
				}
			}

			if (out_ep != 0 && in_ep != 0) {
				candidates.push_back({alt.bAlternateSetting, out_ep, in_ep,
						alt.bAlternateSetting == preferred_alt_setting});
			}
		}
	}

	libusb_free_config_descriptor(config);

	/* Try preferred alternate setting first, then any usable pair. */
	for (int pass = 0; pass < 2; pass++) {
		for (const candidate_t &candidate: candidates) {
			if ((pass == 0) != candidate.preferred)
				continue;

			const bool alt_ok = set_interface_alt_setting(if_num, candidate.alt_setting);
			if (!alt_ok) {
				printWarn("bulk endpoint candidate alt setting " +
						std::to_string(candidate.alt_setting) + " is not selectable");
				continue;
			}

			out_endpoint = candidate.out_ep;
			in_endpoint = candidate.in_ep;
			return true;
		}
	}

	return false;
}

/* destructor: close current device and destroy context */
FX2_ll::~FX2_ll()
{
	close();
	libusb_exit(usb_ctx);
}

/* close device after releasing interface */
bool FX2_ll::close()
{
	bool success = true;
	if (dev_handle) {
		int ret;
		ret = libusb_release_interface(dev_handle, 0);
		if (ret != LIBUSB_SUCCESS && ret != LIBUSB_ERROR_NO_DEVICE) {
			printError("Error: Fail to release interface: " +
					std::string(libusb_error_name(ret)));
			success = false;
		}
		/* Always close the handle, including failed-release and disconnected
		 * cases, so a later process can claim the WinUSB interface. */
		libusb_close(dev_handle);
		dev_handle = nullptr;
	}
	return success;
}

/* write len byte in bulk using endpoint */
int FX2_ll::write(uint8_t endpoint, uint8_t *buff, uint16_t len)
{
	int ret, actual_length;
	ret = libusb_bulk_transfer(dev_handle, LIBUSB_ENDPOINT_OUT | endpoint,
			buff, len, &actual_length, 1000);
	if (ret != LIBUSB_SUCCESS) {
		printError("FX2 write error: " + std::string(libusb_error_name(ret)));
		return -1;
	}
	return actual_length;
}

/* read len byte in bulk using endpoint */
int FX2_ll::read(uint8_t endpoint, uint8_t *buff, uint16_t len)
{
	int ret, actual_length;
	ret = libusb_bulk_transfer(dev_handle, LIBUSB_ENDPOINT_IN | endpoint,
			buff, len, &actual_length, 1000);
	if (ret != LIBUSB_SUCCESS) {
		printError("FX2 read error: " + std::string(libusb_error_name(ret)));
		return -1;
	}
	return actual_length;
}

/* write len data using control */
int FX2_ll::write_ctrl(uint8_t bRequest, uint16_t wValue, uint8_t *buff,
		uint16_t len, uint16_t wIndex)
{
	int ret = libusb_control_transfer(dev_handle,
			LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_ENDPOINT_OUT,
			bRequest, wValue, wIndex, buff, len, fx2ControlTimeoutMs());
	if (ret < 0) {
		if (fx2VerboseUsbErrors())
			printError("Unable to send control request: " + std::string(libusb_error_name(ret)));
		return false;
	}
	return true;
}

/* read len data using control */
int FX2_ll::read_ctrl(uint8_t bRequest, uint16_t wValue, uint8_t *buff,
		uint16_t len, uint16_t wIndex)
{
	int ret = libusb_control_transfer(dev_handle,
			LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_ENDPOINT_IN,
			bRequest, wValue, wIndex, buff, len, fx2ControlTimeoutMs());
	if (ret < 0) {
		if (fx2VerboseUsbErrors())
			printError("Unable to read control request: " + std::string(libusb_error_name(ret)));
		return false;
	}
	return true;
}

bool FX2_ll::clear_halt(uint8_t endpoint)
{
	const int ret = libusb_clear_halt(dev_handle, endpoint);
	if (ret != LIBUSB_SUCCESS) {
		printWarn("FX2 clear halt failed on endpoint " + std::to_string(endpoint) +
				": " + std::string(libusb_error_name(ret)));
		return false;
	}
	return true;
}

bool FX2_ll::reset_device()
{
	const int ret = libusb_reset_device(dev_handle);
	if (ret != LIBUSB_SUCCESS) {
		printWarn("FX2 USB reset failed: " + std::string(libusb_error_name(ret)));
		return false;
	}
	return true;
}

/* load firmware section by section and 64B by 64B.
 * Set CPU in reset state before and restart after.
 */
bool FX2_ll::load_firmware(std::string firmware_path)
{
	IhexParser ihex(firmware_path, false, true);
	ihex.parse();

	/* reset */
	if (!reset(1))
		return false;

	/* load */
	std::vector<IhexParser::data_line_t> data = ihex.getDataArray();
	ProgressBar progress("Loading firmware", data.size(), 50, false);
	for (size_t i = 0; i < data.size(); i++) {
		IhexParser::data_line_t data_line = data[i];
		uint16_t toSend = data_line.length;
		uint8_t *tx_buff = data_line.line_data.data();
		uint16_t addr = data_line.addr;

		while (toSend > 0) {
			uint16_t xfer_len = (toSend > 64) ? 64 : toSend;
			if (!write_ctrl(FX2_FIRM_LOAD, addr, tx_buff, xfer_len)) {
				progress.fail();
				return false;
			}
			toSend -= xfer_len;
			tx_buff += xfer_len;
			addr += xfer_len;
		}
		progress.display(i);
	}
	progress.done();

	/* unset reset */
	if (!reset(0))
		return false;

	return true;
}

/* set or unset 8051RES in CPUCS register */
bool FX2_ll::reset(uint8_t res8051)
{
	unsigned char buf[1];
	int ret;

	buf[0] = res8051;
	if (!(ret = write_ctrl(FX2_FIRM_LOAD, FX2_GCR_CPUCS, buf, 1))) {
		printError("Unable to send control request: " + std::string(libusb_error_name(ret)));
		return false;
	}
	return true;
}
