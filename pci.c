/*
 * Copyright (c) 2008, Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU Lesser General Public License,
 * version 2.1, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 *
 */

#include "utils.h"
#include "adapt_impl.h"
#include <linux/pci_regs.h>
#include <pciaccess.h>
#include <byteswap.h>

static void
get_device_serial_number(struct pci_device *dev, struct hba_info *hba_info)
{
	pciaddr_t offset;
	u_int32_t pcie_cap_header;
	u_int16_t pcie_cap_id;
	u_int16_t status;
	u_int8_t cap_ptr;
	u_int32_t dword_low = 0;
	u_int32_t dword_high = 0;
	int rc;

	/*
	 * Read the Status Register in the PCIe configuration
	 * header space to see if the PCI Capability List is
	 * supported by this device.
	 */
	rc = pci_device_cfg_read_u16(dev, &status, PCI_STATUS);
	if (rc) {
		fprintf(stderr, "Failed reading PCI Status Register\n");
		return;
	}
	if (!(status & PCI_STATUS_CAP_LIST)) {
		printf("PCI capabilities are not supported\n");
		return;
	}

	/*
	 * Read the offset (cap_ptr) of first entry in the capability list in
	 * the PCI configuration space.
	 */
	rc = pci_device_cfg_read_u8(dev, &cap_ptr, PCI_CAPABILITY_LIST);
	if (rc) {
		fprintf(stderr,
			"Failed reading PCI Capability List Register\n");
		return;
	}
	offset = cap_ptr;

	/* Search for the PCIe capability */
	while (offset) {
		u_int8_t cap_id;
		u_int8_t next_cap;

		rc = pci_device_cfg_read_u8(dev, &cap_id,
					    offset + PCI_CAP_LIST_ID);
		if (rc) {
#if defined(__x86_64__)
			fprintf(stderr,
				"Failed reading capability ID at 0x%lx\n",
				offset + PCI_CAP_LIST_ID);
#elif defined(__i386__)
			fprintf(stderr,
				"Failed reading capability ID at 0x%llx\n",
				offset + PCI_CAP_LIST_ID);
#endif
			return;
		}

		if (cap_id != PCI_CAP_ID_EXP) {
			rc = pci_device_cfg_read_u8(dev, &next_cap,
						    offset + PCI_CAP_LIST_NEXT);
			if (rc) {
#if defined(__x86_64__)
				fprintf(stderr, "Failed reading next capability "
					"offset at 0x%lx\n",
					offset + PCI_CAP_LIST_NEXT);
#elif defined(__i386__)
				fprintf(stderr, "Failed reading next capability "
					"offset at 0x%llx\n",
					offset + PCI_CAP_LIST_NEXT);
#endif
				return;
			}
			offset = (pciaddr_t)next_cap;
			continue;
		}

		/*
		 * PCIe Capability Structure exists!
		 */

		/*
		 * The first PCIe extended capability is located at
		 * offset 0x100 in the device configuration space.
		 */
		offset = 0x100;

		do {
			rc = pci_device_cfg_read_u32(dev, &pcie_cap_header,
						     offset);
			if (rc) {
				fprintf(stderr,
					"Failed reading PCIe config header\n");
				return;
			}

			/* Get the PCIe Extended Capability ID */
			pcie_cap_id = pcie_cap_header & 0xffff;

			if (pcie_cap_id != PCI_EXT_CAP_ID_DSN) {
				/* Get the offset of the next capability */
				offset = (pciaddr_t)pcie_cap_header >> 20;
				continue;
			}

			/*
			 * Found the serial number register!
			 */

			rc = pci_device_cfg_read_u32(dev,
						     &dword_low, offset + 4);
			rc = pci_device_cfg_read_u32(dev,
						     &dword_high, offset + 8);
			snprintf(hba_info->SerialNumber,
				 sizeof(hba_info->SerialNumber),
				 "%02X%02X%02X%02X%02X%02X\n",
				 dword_high >> 24, (dword_high >> 16) & 0xff,
				 (dword_high >> 8) & 0xff, (dword_low >> 16) & 0xff,
				 (dword_low >> 8) & 0xff, dword_low & 0xff);
			break;
		} while (offset);
		break;
	}
}

static void
get_pci_device_info(struct pci_device *dev, struct hba_info *hba_info)
{
	const char *name;
	u_int8_t revision;
	char *unknown = "Unknown";

	name = pci_device_get_vendor_name(dev);
	if (!name)
		name = unknown;
	sa_strncpy_safe(hba_info->Manufacturer,
			sizeof(hba_info->Manufacturer),
			name, sizeof(hba_info->Manufacturer));

	name = pci_device_get_device_name(dev);
	if (!name)
		name = unknown;
	sa_strncpy_safe(hba_info->ModelDescription,
			sizeof(hba_info->ModelDescription),
			name, sizeof(hba_info->ModelDescription));

	/*
	 * Reading hardware revision from PCIe
	 * configuration header space.
	 */
	pci_device_cfg_read_u8(dev, &revision, PCI_REVISION_ID);
	snprintf(hba_info->HardwareVersion,
		 sizeof(hba_info->HardwareVersion),
		 "%02x", revision);

	hba_info->NumberOfPorts = 1;

	/*
	 * Searching for serial number in PCIe extended
	 * capabilities space
	 */
	get_device_serial_number(dev, hba_info);
}

HBA_STATUS
find_pci_device(struct hba_info *hba_info)
{
	struct pci_device_iterator *iterator;
	struct pci_device *dev;
	struct pci_slot_match match;

	int rc;

	rc = pci_system_init();
	if (rc) {
		fprintf(stderr, "pci_system_init failed\n");
		return HBA_STATUS_ERROR;
	}

	match.domain = hba_info->domain;
	match.bus = hba_info->bus;
	match.dev = hba_info->dev;
	match.func = hba_info->func;

	iterator = pci_slot_match_iterator_create(&match);

	for (;;) {
		dev = pci_device_next(iterator);
		if (!dev)
			break;
		get_pci_device_info(dev, hba_info);
	}

	pci_system_cleanup();
	return HBA_STATUS_OK;
}

