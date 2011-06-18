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
#include "bind_impl.h"

/*
 * Binding capabilities we understand.
 */
#define BINDING_CAPABILITIES   (HBA_CAN_BIND_TO_D_ID | \
				HBA_CAN_BIND_TO_WWPN | \
				HBA_CAN_BIND_TO_WWNN)
#define SYSFS_BIND		"tgtid_bind_type"

/*
 * Name-value strings for kernel bindings.
 * The first word of the strings must exactly match those in
 * Linux's drivers/scsi/scsi_transport_fc
 */
static struct sa_nameval binding_types_table[] = {
	{ "none",                           0 },
	{ "wwpn (World Wide Port Name)",    HBA_CAN_BIND_TO_WWPN },
	{ "wwnn (World Wide Node Name)",    HBA_CAN_BIND_TO_WWNN },
	{ "port_id (FC Address)",           HBA_CAN_BIND_TO_D_ID },
	{ NULL,                             0 }
};

/*
 * Context for LUN binding reader.
 */
struct binding_context {
	HBA_HANDLE            oc_handle;
	int                   oc_kern_hba;  /* kernel HBA number */
	int                   oc_port;
	int                   oc_target;
	int                   oc_lun;
	u_int32_t             oc_count;
	u_int32_t             oc_limit;
	u_int32_t             oc_ver;
	void                  *oc_entries;
	HBA_STATUS            oc_status;
	char                  oc_sg[32];    /* SCSI-generic dev name */
	HBA_SCSIID            *oc_scp;      /* place for OS device name */
	struct port_info      *oc_rport;    /* target remote port, if known */
	char                  oc_path[256]; /* parent dir save area */
};

/*
 * Get binding capability.
 * We currently don't have a way to get this from the driver.
 * Instead, we hardcode what we know about Linux's capabilities.
 * We don't care which HBA is specified, except to return the correct error.
 */
HBA_STATUS
get_binding_capability(HBA_HANDLE handle, HBA_WWN wwn, HBA_BIND_CAPABILITY *cp)
{
	struct port_info *pp;
	int count = 0;

	pp = adapter_get_port_by_wwn(handle, wwn, &count);
	if (count > 1)
		return HBA_STATUS_ERROR_AMBIGUOUS_WWN;
	else if (pp == NULL)
		return HBA_STATUS_ERROR_ILLEGAL_WWN;
	*cp = BINDING_CAPABILITIES;
	return HBA_STATUS_OK;
}

/*
 * Get binding support.
 */
HBA_STATUS
get_binding_support(HBA_HANDLE handle, HBA_WWN wwn, HBA_BIND_CAPABILITY *cp)
{
	struct port_info *pp;
	char dir[50];
	char bind[50];
	int count = 0;

	pp = adapter_get_port_by_wwn(handle, wwn, &count);
	if (count > 1)
		return HBA_STATUS_ERROR_AMBIGUOUS_WWN;
	else if (pp == NULL)
		return HBA_STATUS_ERROR_ILLEGAL_WWN;
	snprintf(dir, sizeof(dir), SYSFS_HOST_DIR "/host%u", pp->ap_kern_hba);
	if (sa_sys_read_line(dir, SYSFS_BIND, bind, sizeof(bind)) == 0)
		sa_enum_encode(binding_types_table, bind, cp);
	return HBA_STATUS_OK;
}

/*
 * Set binding support.
 */
HBA_STATUS
set_binding_support(HBA_HANDLE handle, HBA_WWN wwn, HBA_BIND_CAPABILITY flags)
{
	struct port_info *pp;
	int count = 0;
	char dir[50];
	char buf[50];
	const char *bind;

	pp = adapter_get_port_by_wwn(handle, wwn, &count);
	if (count > 1)
		return HBA_STATUS_ERROR_AMBIGUOUS_WWN;
	if (pp == NULL)
		return HBA_STATUS_ERROR_ILLEGAL_WWN;
	if ((flags & BINDING_CAPABILITIES) != flags)
		return HBA_STATUS_ERROR_NOT_SUPPORTED;
	bind = sa_enum_decode(buf, sizeof(buf), binding_types_table, flags);
	snprintf(dir, sizeof(dir), SYSFS_HOST_DIR "/host%u", pp->ap_kern_hba);
	if (strstr(bind, "Unknown") != NULL)
		return HBA_STATUS_ERROR_NOT_SUPPORTED;
	if (sa_sys_write_line(dir, SYSFS_BIND, bind) == 0)
		return HBA_STATUS_ERROR_INCAPABLE;
	return HBA_STATUS_OK;
}

static int
get_deprecated_device_name(struct dirent *dp, void *arg)
{
	struct binding_context *cp = arg;

	if (strstr(cp->oc_path, "block"))
		snprintf(cp->oc_scp->OSDeviceName,
			 sizeof(cp->oc_scp->OSDeviceName),
			 "/dev/%s", dp->d_name);
	if (strstr(cp->oc_path, "scsi_generic"))
		snprintf(cp->oc_sg, sizeof(cp->oc_sg),
			 "/dev/%s", dp->d_name);
	return 0;
}

static int
get_binding_os_names(struct dirent *dp, void *arg)
{
	struct binding_context *cp = arg;
	char *name = dp->d_name;
	char *sep;
	char buf[sizeof(cp->oc_scp->OSDeviceName)];

	sep = strchr(name, ':');
	if (dp->d_type == DT_LNK && sep != NULL) {
		*sep = '\0';                /* replace colon */
		if (strcmp(name, "block") == 0) {
			snprintf(cp->oc_scp->OSDeviceName,
				 sizeof(cp->oc_scp->OSDeviceName),
				 "/dev/%s", sep + 1);
		} else if (strcmp(name, "scsi_generic") == 0) {
			snprintf(cp->oc_sg,
				 sizeof(cp->oc_sg),
				 "/dev/%s", sep + 1);
		}
		*sep = ':';                 /* not really needed */
	} else if (dp->d_type == DT_DIR && sep == NULL) {
		if ((!strcmp(name, "block")) ||
		    (!strcmp(name, "scsi_generic"))) {
			/* save the original path */
			sa_strncpy_safe(buf, sizeof(buf),
					cp->oc_path, sizeof(cp->oc_path));

			snprintf(cp->oc_path, sizeof(cp->oc_path),
				"%s/%s", buf, name);
			sa_dir_read(cp->oc_path,
				get_deprecated_device_name, cp);

			/* restore the original path */
			sa_strncpy_safe(cp->oc_path, sizeof(cp->oc_path),
					buf, sizeof(buf));
		}
	}
	return 0;
}

static int
get_binding_target_mapping(struct dirent *dp, void *ctxt_arg)
{
	struct binding_context *cp = ctxt_arg;
	struct port_info *pp;
	HBA_FCPSCSIENTRY *sp;
	HBA_FCPSCSIENTRYV2 *s2p;
	HBA_SCSIID *scp = NULL;
	HBA_FCPID *fcp = NULL;
	HBA_LUID *luid = NULL;
	char name[50];
	u_int32_t hba = -1;
	u_int32_t port = -1;
	u_int32_t tgt = -1;
	u_int32_t lun = -1;

	/*
	 * Parse directory entry name to see if it matches
	 * <hba>:<port>:<target>:<lun>.
	 */
	if (sscanf(dp->d_name, "%u:%u:%u:%u", &hba, &port, &tgt, &lun) != 4)
		return 0;

	if (hba != cp->oc_kern_hba ||
	    (port != cp->oc_port && cp->oc_port != -1) ||
	    (tgt != cp->oc_target && cp->oc_target != -1) ||
	    (lun != cp->oc_lun && cp->oc_lun != -1)) {
		return 0;
	}

	/*
	 * Name matches.  Add to count and to mapping list if there's room.
	 */
	if (cp->oc_count < cp->oc_limit) {

		switch (cp->oc_ver) {
		case 1:
			sp = &((HBA_FCPSCSIENTRY *)
				cp->oc_entries)[cp->oc_count];
			scp = &sp->ScsiId;
			fcp = &sp->FcpId;
			luid = NULL;
			break;
		case 2:
			s2p = &((HBA_FCPSCSIENTRYV2 *)
				cp->oc_entries)[cp->oc_count];
			scp = &s2p->ScsiId;
			fcp = &s2p->FcpId;
			luid = &s2p->LUID;
			break;
		default:
			fprintf(stderr, "*** Fatal! ***\n");
			break;
		}
		pp = cp->oc_rport;
		if (pp == NULL)
			pp = adapter_get_rport_target(cp->oc_handle,
							port, tgt);
		if (pp != NULL) {
			fcp->FcId = pp->ap_attr.PortFcId;
			fcp->NodeWWN = pp->ap_attr.NodeWWN;
			fcp->PortWWN = pp->ap_attr.PortWWN;
			fcp->FcpLun = (HBA_UINT64) lun;
		}

		/*
		 * Find OS device name by searching for symlink block:<device>
		 * and SG name by searching for scsi_generic:<name>
		 * in device subdirectory.
		 */
		snprintf(name, sizeof(name),
			 SYSFS_LUN_DIR "/%s/device", dp->d_name);
		cp->oc_sg[0] = '\0';
		cp->oc_scp = scp;
		scp->OSDeviceName[0] = '\0';
		/* save a copy of the dir name */
		sa_strncpy_safe(cp->oc_path, sizeof(cp->oc_path),
				name, sizeof(name));
		sa_dir_read(name, get_binding_os_names, cp);
		scp->ScsiBusNumber = hba;
		scp->ScsiTargetNumber = tgt;
		scp->ScsiOSLun = lun;

		/*
		 * find the LUN ID information by using scsi_generic I/O.
		 */
		if (luid != NULL && cp->oc_sg[0] != '\0')
			sg_get_dev_id(cp->oc_sg, luid->buffer,
					  sizeof(luid->buffer));
	}
	cp->oc_count++;
	return 0;
}

/*
 * Get FCP target mapping.
 */
HBA_STATUS
get_binding_target_mapping_v1(HBA_HANDLE handle, HBA_FCPTARGETMAPPING *map)
{
	struct binding_context ctxt;
	struct adapter_info *ap;

	ap = adapter_open_handle(handle);
	if (ap == NULL)
		return HBA_STATUS_ERROR_INVALID_HANDLE;
	memset(&ctxt, 0, sizeof(ctxt));
	ctxt.oc_handle = handle;
	ctxt.oc_kern_hba = ap->ad_kern_index;
	ctxt.oc_port = -1;
	ctxt.oc_target = -1;
	ctxt.oc_lun = -1;
	ctxt.oc_limit = map->NumberOfEntries;
	ctxt.oc_ver = 1;
	ctxt.oc_entries = map->entry;
	ctxt.oc_status = HBA_STATUS_OK;
	memset(map->entry, 0, sizeof(map->entry[0]) * ctxt.oc_limit);
	sa_dir_read(SYSFS_LUN_DIR, get_binding_target_mapping, &ctxt);
	map->NumberOfEntries = ctxt.oc_count;
	if (ctxt.oc_status == HBA_STATUS_OK && ctxt.oc_count > ctxt.oc_limit)
		ctxt.oc_status = HBA_STATUS_ERROR_MORE_DATA;
	return ctxt.oc_status;
}

/*
 * Get FCP target mapping.
 */
HBA_STATUS
get_binding_target_mapping_v2(HBA_HANDLE handle, HBA_WWN wwn,
			       HBA_FCPTARGETMAPPINGV2 *map)
{
	struct binding_context ctxt;
	struct adapter_info *ap;
	struct port_info *pp;

	pp = adapter_get_port_by_wwn(handle, wwn, NULL);
	if (pp == NULL)
		return HBA_STATUS_ERROR_INVALID_HANDLE;
	ap = pp->ap_adapt;
	if (ap == NULL)
		return HBA_STATUS_ERROR_INVALID_HANDLE;
	memset(&ctxt, 0, sizeof(ctxt));
	ctxt.oc_handle = handle;
	ctxt.oc_kern_hba = ap->ad_kern_index;
	ctxt.oc_port = pp->ap_index;
	ctxt.oc_target = -1;
	ctxt.oc_lun = -1;
	ctxt.oc_limit = map->NumberOfEntries;
	ctxt.oc_ver = 2;
	ctxt.oc_entries = map->entry;
	ctxt.oc_status = HBA_STATUS_OK;
	memset(map->entry, 0, sizeof(map->entry[0]) * ctxt.oc_limit);
	sa_dir_read(SYSFS_LUN_DIR, get_binding_target_mapping, &ctxt);
	map->NumberOfEntries = ctxt.oc_count;
	if (ctxt.oc_status == HBA_STATUS_OK && ctxt.oc_count > ctxt.oc_limit)
		ctxt.oc_status = HBA_STATUS_ERROR_MORE_DATA;
	return ctxt.oc_status;
}

/*
 * Get LUN scsi-generic device name.
 */
int
get_binding_sg_name(struct port_info *lp, HBA_WWN disc_wwpn,
		     HBA_UINT64 fc_lun, char *buf, size_t len)
{
	struct binding_context ctxt;
	struct port_info *rp;
	HBA_FCPSCSIENTRYV2 entry;

	/*
	 * find discovered (remote) port.
	 */
	rp = adapter_get_rport_by_wwn(lp, disc_wwpn);
	if (rp == NULL)
		return HBA_STATUS_ERROR_ILLEGAL_WWN;

	memset(&ctxt, 0, sizeof(ctxt));
	memset(&entry, 0, sizeof(entry));
	ctxt.oc_rport = rp;
	ctxt.oc_kern_hba = rp->ap_kern_hba;
	ctxt.oc_port = rp->ap_index;
	ctxt.oc_target = rp->ap_scsi_target;
	if (ctxt.oc_target == -1)
		return ENOENT;
	ctxt.oc_lun = (int) fc_lun;
	ctxt.oc_limit = 1;
	ctxt.oc_ver = 1;
	ctxt.oc_entries = &entry;
	sa_dir_read(SYSFS_LUN_DIR, get_binding_target_mapping, &ctxt);
	if (ctxt.oc_count != 1)
		return ENOENT;
	sa_strncpy_safe(buf, len, ctxt.oc_sg, sizeof(ctxt.oc_sg));
	return 0;
}
