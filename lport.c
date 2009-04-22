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
#include "api_lib.h"
#include "adapt_impl.h"

#ifndef HBA_STATUS_ERROR_ILLEGAL_FCID
#define HBA_STATUS_ERROR_ILLEGAL_FCID 33	/* defined after HBA-API 2.2 */
#endif
#define SEND_CT_TIMEOUT		(3 * 1000)	/* timeout in milliseconds */

/*
 * The following are temporary settings until we can find a way to
 * collect these information.
 */
#define HBA_MODEL               "(Unknown)"
#define HBA_ROM_VERSION         ""
#define HBA_FW_VERSION          ""
#define HBA_VENDOR_SPECIFIC_ID  0

/*
 * Convert fc_port_type values to ascii string name.
 * (This table is copied from scsi_transport_fc.c).
 */
static struct {
	enum fc_port_type	value;
	char			*name;
} port_types_table[] = {
    { FC_PORTTYPE_UNKNOWN,     "Unknown" },
    { FC_PORTTYPE_OTHER,       "Other" },
    { FC_PORTTYPE_NOTPRESENT,  "Not Present" },
    { FC_PORTTYPE_NPORT,       "NPort (fabric via point-to-point)" },
    { FC_PORTTYPE_NLPORT,      "NLPort (fabric via loop)" },
    { FC_PORTTYPE_LPORT,       "LPort (private loop)" },
    { FC_PORTTYPE_PTP,         "Point-To-Point (direct nport connection)" },
    { FC_PORTTYPE_NPIV,        "NPIV VPORT" },
};
fc_enum_name_search(port_type, fc_port_type, port_types_table)
#define FC_PORTTYPE_MAX_NAMELEN         50

/*
 * table of /sys port state strings to HBA-API values.
 */
struct sa_nameval port_states_table[] = {
	{ "Not Present",    HBA_PORTSTATE_UNKNOWN },
	{ "Online",         HBA_PORTSTATE_ONLINE },
	{ "Offline",        HBA_PORTSTATE_OFFLINE },
	{ "Blocked",        HBA_PORTSTATE_UNKNOWN },
	{ "Bypassed",       HBA_PORTSTATE_BYPASSED },
	{ "Diagnostics",    HBA_PORTSTATE_DIAGNOSTICS },
	{ "Linkdown",       HBA_PORTSTATE_LINKDOWN },
	{ "Error",          HBA_PORTSTATE_ERROR },
	{ "Loopback",       HBA_PORTSTATE_LOOPBACK },
	{ "Deleted",        HBA_PORTSTATE_UNKNOWN },
	{ NULL, 0 }
};

/*
 * table of /sys port speed strings to HBA-API values.
 */
struct sa_nameval port_speeds_table[] = {
	{ "10 Gbit",        HBA_PORTSPEED_10GBIT },
	{ "2 Gbit",         HBA_PORTSPEED_2GBIT },
	{ "1 Gbit",         HBA_PORTSPEED_1GBIT },
	{ "Not Negotiated", HBA_PORTSPEED_NOT_NEGOTIATED },
	{ "Unknown",        HBA_PORTSPEED_UNKNOWN },
	{ NULL, 0 }
};

/*
 * Code for OpenFC-supported adapters.
 */

static int
counting_rports(struct dirent *dp, void *arg)
{
	int *count = (int *)arg;

	if (!strstr(dp->d_name, "rport-"))
		return HBA_STATUS_OK;
	(*count)++;
	return HBA_STATUS_OK;
}

static int
sysfs_scan(struct dirent *dp, void *arg)
{
	HBA_ADAPTERATTRIBUTES *atp;
	HBA_PORTATTRIBUTES *pap;
	HBA_WWN wwnn;
	struct hba_info hba_info;
	struct adapter_info *ap;
	struct port_info *pp;
	char host_dir[80], hba_dir[80], drv_dir[80];
	char ifname[20], buf[256];
	char *driverName;
	int data[32], rc, i;
	char *cp;

	memset(&hba_info, 0, sizeof(hba_info));

	/*
	 * Create a new HBA entry (ap) for the local port
	 * We will create a new HBA entry for each local port.
	 */
	ap = malloc(sizeof(*ap));
	if (!ap) {
		fprintf(stderr, "%s: malloc failed, errno=0x%x\n",
			__func__, errno);
		return HBA_STATUS_ERROR;
	}
	memset(ap, 0, sizeof(*ap));
	ap->ad_kern_index = atoi(dp->d_name + sizeof("host") - 1);
	ap->ad_port_count = 1;

	/* atp points to the HBA attributes structure */
	atp = &ap->ad_attr;

	/*
	 * Create a new local port entry
	 */
	pp = malloc(sizeof(*pp));
	if (pp == NULL) {
		fprintf(stderr,
			"%s: malloc for local port %d failed,"
			" errno=0x%x\n", __func__,
			ap->ad_port_count - 1, errno);
		free(ap);
		return 0;
	}

	memset(pp, 0, sizeof(*pp));
	pp->ap_adapt = ap;
	pp->ap_index = ap->ad_port_count - 1;
	pp->ap_kern_hba = atoi(dp->d_name + sizeof("host") - 1);

	/* pap points to the local port attributes structure */
	pap = &pp->ap_attr;

	/* Construct the host directory name from the input name */
	snprintf(host_dir, sizeof(host_dir),
		SYSFS_HOST_DIR "/%s", dp->d_name);

	rc = sa_sys_read_line(host_dir, "symbolic_name", buf, sizeof(buf));

	/* Get PortSymbolicName */
	sa_strncpy_safe(pap->PortSymbolicName, sizeof(pap->PortSymbolicName),
			buf, sizeof(buf));

	/* Skip the HBA if it isn't OpenFC */
	cp = strstr(pap->PortSymbolicName, " over ");
	if (!cp)
		goto skip;

	/*
	 * See if host_dir is a PCI device directory
	 * If not, try it as a net device.
	 */
	i = readlink(host_dir, buf, sizeof(buf) - 1);
	if (i < 0)
		goto skip;
	buf[i] = '\0';

	if (strstr(buf, "devices/pci")) {
		snprintf(hba_dir, sizeof(hba_dir), "%s/device/..", host_dir);
	} else {
		/* assume a net device */
		cp += 6;
		sa_strncpy_safe(ifname, sizeof(ifname), cp, strlen(cp));
		snprintf(hba_dir, sizeof(hba_dir), SYSFS_HBA_DIR "/%s/device",
			ifname);
		i = readlink(hba_dir, buf, sizeof(buf) - 1);
		if (i < 0) {
			printf("readlink %s failed\n", hba_dir);
			goto skip;
		}
		buf[i] = '\0';
	}

	/*
	 * Assume a PCI symlink value is in buf.
	 * Back up to the last path component that looks like a PCI element.
	 * A sample link value is like:
	 * ../devices/pci*.../0000:00:07.0/0000:06:00.4/host2/fc_host/host2
	 */
	rc = 0;
	do {
		cp = strrchr(buf, '/');
		if (!cp)
			break;
		rc = sscanf(cp + 1, "%x:%x:%x.%x",
			    &hba_info.domain, &hba_info.bus,
			    &hba_info.dev, &hba_info.func);
		if (rc == 4)
			break;
		*cp = '\0';
	} while (cp && cp > buf);

	if (rc != 4)
		goto skip;

	/*
	 * Save the host directory and the hba directory
	 * in local port structure
	 */
	sa_strncpy_safe(pp->host_dir, sizeof(pp->host_dir),
			host_dir, sizeof(host_dir));

	/* Get NodeWWN */
	rc = sys_read_wwn(pp->host_dir, "node_name", &wwnn);
	memcpy(&pap->NodeWWN, &wwnn, sizeof(wwnn));

	/* Get PortWWN */
	rc = sys_read_wwn(pp->host_dir, "port_name", &pap->PortWWN);

	/* Get PortFcId */
	rc = sa_sys_read_u32(pp->host_dir, "port_id", &pap->PortFcId);

	/* Get PortType */
	rc = sa_sys_read_line(pp->host_dir, "port_type", buf, sizeof(buf));
	pap->PortType = get_fc_port_type_value(buf);

	/* Get PortState */
	rc = sa_sys_read_line(pp->host_dir, "port_state", buf, sizeof(buf));
	rc = sa_enum_encode(port_states_table, buf, &pap->PortState);

	/* Get PortSpeed */
	rc = sa_sys_read_line(pp->host_dir, "speed", buf, sizeof(buf));
	rc = sa_enum_encode(port_speeds_table, buf, &pap->PortSpeed);

	/* Get PortSupportedSpeed */
	rc = sa_sys_read_line(pp->host_dir, "supported_speed",
				buf, sizeof(buf));
	rc = sa_enum_encode(port_speeds_table, buf, &pap->PortSupportedSpeed);

	/* Get PortMaxFrameSize */
	rc = sa_sys_read_line(pp->host_dir, "maxframe_size", buf, sizeof(buf));
	sscanf(buf, "%d", &pap->PortMaxFrameSize);

	/* Get PortSupportedFc4Types */
	rc = sa_sys_read_line(pp->host_dir, "supported_fc4s", buf, sizeof(buf));
	sscanf(buf, "0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x "
		    "0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x "
		    "0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x "
		    "0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x",
		&data[0], &data[1], &data[2], &data[3], &data[4], &data[5],
		&data[6], &data[7], &data[8], &data[9], &data[10], &data[11],
		&data[12], &data[13], &data[14], &data[15], &data[16],
		&data[17], &data[18], &data[19], &data[20], &data[21],
		&data[22], &data[23], &data[24], &data[25], &data[26],
		&data[27], &data[28], &data[29], &data[30], &data[31]);
	for (i = 0; i < 32; i++)
		pap->PortSupportedFc4Types.bits[i] = data[i];

	/* Get PortActiveFc4Types */
	rc = sa_sys_read_line(pp->host_dir, "active_fc4s", buf, sizeof(buf));
	sscanf(buf, "0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x "
		    "0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x "
		    "0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x "
		    "0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x",
		&data[0], &data[1], &data[2], &data[3], &data[4], &data[5],
		&data[6], &data[7], &data[8], &data[9], &data[10], &data[11],
		&data[12], &data[13], &data[14], &data[15], &data[16],
		&data[17], &data[18], &data[19], &data[20], &data[21],
		&data[22], &data[23], &data[24], &data[25], &data[26],
		&data[27], &data[28], &data[29], &data[30], &data[31]);
	for (i = 0; i < 32; i++)
		pap->PortActiveFc4Types.bits[i] = data[i];

	/* Get FabricName */
	rc = sys_read_wwn(pp->host_dir, "fabric_name", &pap->FabricName);

	/* Get PortSupportedClassofService */
	rc = sa_sys_read_line(pp->host_dir, "supported_classes",
				buf, sizeof(buf));
	pap->PortSupportedClassofService = *(strstr(buf, "Class") + 6) - '0';

	/* Get OSDeviceName */
	sa_strncpy_safe(pap->OSDeviceName, sizeof(pap->OSDeviceName),
			dp->d_name, sizeof(dp->d_name));

	/* Get NumberofDiscoveredPorts */
	snprintf(buf, sizeof(buf), "%s/device", pp->host_dir);
	sa_dir_read(buf, counting_rports, &pap->NumberofDiscoveredPorts);

	/*
	 * Add the local port structure into local port table within
	 * the HBA structure.
	 */
	if (sa_table_insert(&ap->ad_ports, pp->ap_index, pp) < 0) {
		fprintf(stderr,
			"%s: insert of HBA %d port %d failed\n",
			__func__, ap->ad_kern_index, pp->ap_index);
		goto skip;
	}

	/* Create adapter name */
	snprintf(buf, sizeof(buf), "fcoe:%s", ifname);
	ap->ad_name = strdup(buf);

	/* Get vendor_id */
	rc = sa_sys_read_u32(hba_dir, "vendor", &hba_info.vendor_id);

	/* Get device_id */
	rc = sa_sys_read_u32(hba_dir, "device", &hba_info.device_id);

	/* Get subsystem_vendor_id */
	rc = sa_sys_read_u32(hba_dir, "subsystem_vendor",
				&hba_info.subsystem_vendor_id);

	/* Get subsystem_device_id */
	rc = sa_sys_read_u32(hba_dir, "subsystem_device",
				&hba_info.subsystem_device_id);

	/* Get device_class */
	rc = sa_sys_read_u32(hba_dir, "class", &hba_info.device_class);
	hba_info.device_class = hba_info.device_class>>8;

	/*
	 * Get Hardware Information via PCI Library
	 */
	(void) find_pci_device(&hba_info);

	/* Get Number of Ports */
	atp->NumberOfPorts = hba_info.NumberOfPorts;

	/* Get Manufacturer */
	sa_strncpy_safe(atp->Manufacturer, sizeof(atp->Manufacturer),
			hba_info.Manufacturer, sizeof(hba_info.Manufacturer));

	/* Get SerialNumber */
	sa_strncpy_safe(atp->SerialNumber, sizeof(atp->SerialNumber),
			hba_info.SerialNumber, sizeof(hba_info.SerialNumber));

	/* Get Model (TODO) */
	sa_strncpy_safe(atp->Model, sizeof(atp->Model),
			HBA_MODEL, sizeof(HBA_MODEL));

	/* Get ModelDescription */
	sa_strncpy_safe(atp->ModelDescription, sizeof(atp->ModelDescription),
			hba_info.ModelDescription,
			sizeof(hba_info.ModelDescription));

	/* Get HardwareVersion */
	sa_strncpy_safe(atp->HardwareVersion, sizeof(atp->HardwareVersion),
			hba_info.HardwareVersion,
			sizeof(hba_info.HardwareVersion));

	/* Get OptionROMVersion (TODO) */
	sa_strncpy_safe(atp->OptionROMVersion, sizeof(atp->OptionROMVersion),
			HBA_ROM_VERSION, sizeof(HBA_ROM_VERSION));

	/* Get FirmwareVersion (TODO) */
	sa_strncpy_safe(atp->FirmwareVersion, sizeof(atp->FirmwareVersion),
			HBA_FW_VERSION, sizeof(HBA_FW_VERSION));

	/* Get VendorSpecificID (TODO) */
	atp->VendorSpecificID = HBA_VENDOR_SPECIFIC_ID;

	/* Get DriverVersion */
	rc = sa_sys_read_line(hba_dir, SYSFS_MODULE_VER,
			atp->DriverVersion, sizeof(atp->DriverVersion));

	/* Get NodeSymbolicName */
	sa_strncpy_safe(atp->NodeSymbolicName, sizeof(atp->NodeSymbolicName),
			ap->ad_name, sizeof(atp->NodeSymbolicName));

	/* Get NodeWWN - The NodeWWN is the same as
	 *               the NodeWWN of the local port.
	 */
	memcpy((char *)&atp->NodeWWN, (char *)&pap->NodeWWN,
		sizeof(pap->NodeWWN));

	/* Get DriverName */
	snprintf(drv_dir, sizeof(drv_dir), "%s" SYSFS_MODULE , hba_dir);
	i = readlink(drv_dir, buf, sizeof(buf));
	if (i < 0)
		i = 0;
	buf[i] = '\0';
	if (!strstr(buf, "module")) {
		/*
		 * Does not find "module" in the string.
		 * This should not happen. In this case, set
		 * the driver name to "Unknown".
		 */
		driverName = "Unknown";
	} else
		driverName = strstr(buf, "module") + 7;
	sa_strncpy_safe(atp->DriverName, sizeof(atp->DriverName),
			driverName, sizeof(atp->DriverName));

	/*
	 * Give HBA to library
	 */
	rc = adapter_create(ap);
	if (rc != HBA_STATUS_OK) {
		fprintf(stderr, "%s: adapter_create failed, status=%d\n",
			__func__, rc);
		adapter_destroy(ap);      /* free adapter and ports */
	}

	return 0;

skip:
	free(pp);
	free(ap);
	return 0;
}

void
copy_wwn(HBA_WWN *dest, fc_wwn_t src)
{
	dest->wwn[0] = (u_char) (src >> 56);
	dest->wwn[1] = (u_char) (src >> 48);
	dest->wwn[2] = (u_char) (src >> 40);
	dest->wwn[3] = (u_char) (src >> 32);
	dest->wwn[4] = (u_char) (src >> 24);
	dest->wwn[5] = (u_char) (src >> 16);
	dest->wwn[6] = (u_char) (src >> 8);
	dest->wwn[7] = (u_char) src;
}

/* Test for a non-zero WWN */
int
is_wwn_nonzero(HBA_WWN *wwn)
{
	return (wwn->wwn[0] | wwn->wwn[1] | wwn->wwn[2] | wwn->wwn[3] |
		wwn->wwn[4] | wwn->wwn[5] | wwn->wwn[6] | wwn->wwn[7]) != 0;
}

int
sys_read_wwn(const char *dir, const char *file, HBA_WWN *wwn)
{
	int rc;
	u_int64_t val;

	rc = sa_sys_read_u64(dir, file, &val);
	if (rc == 0)
		copy_wwn(wwn, val);
	return rc;
}

/* Port Statistics */
HBA_STATUS
sysfs_get_port_stats(char *dir, HBA_PORTSTATISTICS *sp)
{
	int rc;

	rc  = sa_sys_read_u64(dir, "seconds_since_last_reset",
				(u_int64_t *)&sp->SecondsSinceLastReset);
	rc |= sa_sys_read_u64(dir, "tx_frames", (u_int64_t *)&sp->TxFrames);
	rc |= sa_sys_read_u64(dir, "tx_words", (u_int64_t *)&sp->TxWords);
	rc |= sa_sys_read_u64(dir, "rx_frames", (u_int64_t *)&sp->RxFrames);
	rc |= sa_sys_read_u64(dir, "rx_words", (u_int64_t *)&sp->RxWords);
	rc |= sa_sys_read_u64(dir, "lip_count", (u_int64_t *)&sp->LIPCount);
	rc |= sa_sys_read_u64(dir, "nos_count", (u_int64_t *)&sp->NOSCount);
	rc |= sa_sys_read_u64(dir, "error_frames",
				(u_int64_t *)&sp->ErrorFrames);
	rc |= sa_sys_read_u64(dir, "dumped_frames",
				(u_int64_t *)&sp->DumpedFrames);
	rc |= sa_sys_read_u64(dir, "link_failure_count",
				(u_int64_t *)&sp->LinkFailureCount);
	rc |= sa_sys_read_u64(dir, "loss_of_sync_count",
				(u_int64_t *)&sp->LossOfSyncCount);
	rc |= sa_sys_read_u64(dir, "loss_of_signal_count",
				(u_int64_t *)&sp->LossOfSignalCount);
	rc |= sa_sys_read_u64(dir, "prim_seq_protocol_err_count",
				(u_int64_t *)&sp->PrimitiveSeqProtocolErrCount);
	rc |= sa_sys_read_u64(dir, "invalid_tx_word_count",
				(u_int64_t *)&sp->InvalidTxWordCount);
	rc |= sa_sys_read_u64(dir, "invalid_crc_count",
				(u_int64_t *)&sp->InvalidCRCCount);

	return rc;
}

/* Port FC-4 Statistics */
HBA_STATUS
sysfs_get_port_fc4stats(char *dir, HBA_FC4STATISTICS *fc4sp)
{
	int rc;

	rc  = sa_sys_read_u64(dir, "fcp_input_requests",
				(u_int64_t *)&fc4sp->InputRequests);
	rc |= sa_sys_read_u64(dir, "fcp_output_requests",
				(u_int64_t *)&fc4sp->OutputRequests);
	rc |= sa_sys_read_u64(dir, "fcp_control_requests",
				(u_int64_t *)&fc4sp->ControlRequests);
	rc |= sa_sys_read_u64(dir, "fcp_input_megabytes",
				(u_int64_t *)&fc4sp->InputMegabytes);
	rc |= sa_sys_read_u64(dir, "fcp_output_megabytes",
				(u_int64_t *)&fc4sp->OutputMegabytes);

	return rc;
}
/*
 * Open device and read adapter info if available.
 */
void
adapter_init(void)
{
	sa_dir_read(SYSFS_HOST_DIR, sysfs_scan, NULL);
}

void
adapter_shutdown(void)
{
}

HBA_STATUS
get_port_statistics(HBA_HANDLE handle, HBA_UINT32 port, HBA_PORTSTATISTICS *sp)
{
	struct port_info *pp;
	char dir[80];
	int rc;

	memset(sp, 0xff, sizeof(*sp)); /* unsupported statistics give -1 */
	pp = adapter_get_port(handle, port);
	if (pp == NULL) {
		fprintf(stderr, "%s: lookup failed. handle 0x%x port 0x%x\n",
			__func__, handle, port);
		return HBA_STATUS_ERROR;
	}

	snprintf(dir, sizeof(dir), "%s/statistics", pp->host_dir);
	rc = sysfs_get_port_stats(dir, sp);
	if (rc != 0) {
		fprintf(stderr, "%s: sysfs_get_port_stats() failed,"
			" hba index=%d port index=%d, -rc=0x%x\n",
			__func__, pp->ap_adapt->ad_kern_index,
			pp->ap_index, -rc);
		return HBA_STATUS_ERROR;
	}
	return HBA_STATUS_OK;
}

/*
 * Get FC4 statistics.
 */
HBA_STATUS
get_port_fc4_statistics(HBA_HANDLE handle, HBA_WWN wwn,
		       HBA_UINT8 fc4_type, HBA_FC4STATISTICS *sp)
{
	struct port_info *pp;
	char dir[80];
	int count;
	int rc;

	memset(sp, 0xff, sizeof(*sp)); /* unsupported statistics give -1 */

	pp = adapter_get_port_by_wwn(handle, wwn, &count);
	if (count > 1)
		return HBA_STATUS_ERROR_AMBIGUOUS_WWN;
	else if (pp == NULL)
		return HBA_STATUS_ERROR_ILLEGAL_WWN;

	snprintf(dir, sizeof(dir), "%s/statistics", pp->host_dir);
	rc = sysfs_get_port_fc4stats(dir, sp);
	if (rc != 0) {
		fprintf(stderr, "%s: sysfs_get_port_fc4stats() failed,"
			" hba index=%d port index=%d, -rc=0x%x\n",
			__func__, pp->ap_adapt->ad_kern_index,
			pp->ap_index, -rc);
		return HBA_STATUS_ERROR;
	}
	return HBA_STATUS_OK;
}

