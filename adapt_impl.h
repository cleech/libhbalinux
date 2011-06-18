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

#ifndef _ADAPT_IMPL_H_
#define _ADAPT_IMPL_H_

#define SYSFS_HOST_DIR     "/sys/class/fc_host"
#define SYSFS_HBA_DIR      "/sys/class/net"
#define SYSFS_LUN_DIR      "/sys/class/scsi_device"
#define SYSFS_MODULE       "/driver/module"
#define SYSFS_MODULE_VER   "driver/module/version"
#define SYSFS_RPORT_ROOT       "/sys/class/fc_remote_ports"
#define SYSFS_RPORT_DIR        "rport-%u:%u-%u" /* host, chan, rport */

struct hba_info {
	u_int32_t	domain;
	u_int32_t	bus;
	u_int32_t	dev;
	u_int32_t	func;
	u_int32_t	vendor_id;
	u_int32_t	subsystem_vendor_id;
	u_int32_t	subsystem_device_id;
	u_int32_t	device_id;
	u_int32_t	device_class;
	u_int32_t	irq;
	char		Manufacturer[64];
	char		SerialNumber[64];
	char		Model[256];
	char		ModelDescription[256];
	char		HardwareVersion[256];
	char		OptionROMVersion[256];
	char		FirmwareVersion[256];
	u_int32_t	VendorSpecificID;
	u_int32_t	NumberOfPorts;
};

#define MAX_DRIVER_NAME_LEN	20
#define ARRAY_SIZE(a)		(sizeof(a)/sizeof((a)[0]))

HBA_STATUS sysfs_get_port_stats(char *dir, HBA_PORTSTATISTICS *sp);
HBA_STATUS sysfs_get_port_fc4stats(char *dir, HBA_FC4STATISTICS *fc4sp);

extern struct sa_nameval port_states_table[];
extern struct sa_nameval port_speeds_table[];
extern void adapter_scan(void);
extern int sys_read_wwn(const char *, const char *, HBA_WWN *);
extern HBA_STATUS find_pci_device(struct hba_info *);

/*
 * per-adapter interface.
 */

/*
 * Information about a particular adapter.
 */
struct adapter_info {
    u_int32_t               ad_index;       /* adapter's library index */
    u_int32_t               ad_kern_index;  /* adapter's kernel index */
    const char              *ad_name;       /* adapter driver name */
    struct sa_table         ad_ports;       /* table of ports */
    u_int32_t               ad_port_count;  /* adapter's number of ports */
    HBA_ADAPTERATTRIBUTES   ad_attr;        /* HBA-API attributes */
};

/*
 * Information about a port on an adapter or a discovered remote port.
 */
struct port_info {
    struct adapter_info     *ap_adapt;
    u_int32_t               ap_index;
    u_int32_t               ap_disc_index;  /* discovered port index */
    u_int32_t               ap_scsi_target; /* SCSI target index (rports) */
    u_int32_t               ap_kern_hba;    /* kernel HBA index (rports) */
    struct sa_table         ap_rports;      /* discovered ports */
    HBA_PORTATTRIBUTES      ap_attr;        /* HBA-API port attributes */
    char                    host_dir[80];   /* sysfs directory save area */
};

/*
 * Internal functions.
 */
HBA_UINT32 adapter_get_count(void);
HBA_STATUS adapter_get_name(HBA_UINT32 index, char *);
struct port_info *adapter_get_port_by_wwn(HBA_HANDLE, HBA_WWN, int *countp);
HBA_STATUS adapter_create(struct adapter_info *);
void adapter_destroy(struct adapter_info *);
void adapter_destroy_all(void);
struct adapter_info *adapter_open_handle(HBA_HANDLE);
struct port_info *adapter_get_port(HBA_HANDLE, HBA_UINT32 port);
struct port_info *adapter_get_rport(HBA_HANDLE, HBA_UINT32, HBA_UINT32);
struct port_info *adapter_get_rport_n(HBA_HANDLE, HBA_UINT32, HBA_UINT32);
struct port_info *adapter_get_rport_target(HBA_HANDLE, HBA_UINT32, HBA_UINT32);
struct port_info *adapter_get_rport_by_wwn(struct port_info *, HBA_WWN);
struct port_info *adapter_get_rport_by_fcid(struct port_info *, fc_fid_t);
void get_rport_info(struct port_info *);
void sg_get_dev_id(const char *name, char *buf, size_t result_len);
void copy_wwn(HBA_WWN *dest, fc_wwn_t src);
int is_wwn_nonzero(HBA_WWN *wwn);
HBA_STATUS sg_issue_read_capacity(const char *, void *, HBA_UINT32 *,
			HBA_UINT8 *, void *, HBA_UINT32 *);
HBA_STATUS sg_issue_report_luns(const char *, void *, HBA_UINT32 *,
			HBA_UINT8 *, void *, HBA_UINT32 *);

/*
 * Library functions.
 */
HBA_HANDLE adapter_open(char *name);
HBA_STATUS adapter_open_by_wwn(HBA_HANDLE *, HBA_WWN);
void adapter_close(HBA_HANDLE);
HBA_STATUS adapter_get_attr(HBA_HANDLE, HBA_ADAPTERATTRIBUTES *);
HBA_STATUS adapter_get_port_attr(HBA_HANDLE, HBA_UINT32 port,
				HBA_PORTATTRIBUTES *);
HBA_STATUS adapter_get_port_attr_by_wwn(HBA_HANDLE, HBA_WWN,
				HBA_PORTATTRIBUTES *);
HBA_STATUS adapter_get_rport_attr(HBA_HANDLE, HBA_UINT32 port,
				HBA_UINT32 rport, HBA_PORTATTRIBUTES *);
HBA_STATUS get_port_statistics(HBA_HANDLE, HBA_UINT32 port,
				HBA_PORTSTATISTICS *);
HBA_STATUS get_port_fc4_statistics(HBA_HANDLE, HBA_WWN,
				HBA_UINT8 fc4_type, HBA_FC4STATISTICS *);
HBA_STATUS scsi_read_capacity_v1(HBA_HANDLE, HBA_WWN, HBA_UINT64,
				void *, HBA_UINT32, void *, HBA_UINT32);
HBA_STATUS scsi_read_capacity_v2(HBA_HANDLE, HBA_WWN, HBA_WWN,
			HBA_UINT64, void *, HBA_UINT32 *, HBA_UINT8 *,
			void *, HBA_UINT32 *);
HBA_STATUS scsi_inquiry_v1(HBA_HANDLE, HBA_WWN, HBA_UINT64, HBA_UINT8,
			HBA_UINT32, void *, HBA_UINT32, void *, HBA_UINT32);
HBA_STATUS scsi_inquiry_v2(HBA_HANDLE, HBA_WWN, HBA_WWN, HBA_UINT64,
			HBA_UINT8, HBA_UINT8, void *, HBA_UINT32 *,
			HBA_UINT8 *, void *, HBA_UINT32 *);
HBA_STATUS scsi_report_luns_v1(HBA_HANDLE, HBA_WWN,
			void *, HBA_UINT32, void *, HBA_UINT32);
HBA_STATUS scsi_report_luns_v2(HBA_HANDLE, HBA_WWN, HBA_WWN,
			void *, HBA_UINT32 *, HBA_UINT8 *,
			void *, HBA_UINT32 *);
HBA_STATUS sg_issue_inquiry(const char *, HBA_UINT8, HBA_UINT8,
		void *, HBA_UINT32 *, HBA_UINT8 *, void *, HBA_UINT32 *);

void adapter_init(void);
void adapter_shutdown(void);

/* struct port_stats; */

#endif /* _ADAPT_IMPL_H_ */
