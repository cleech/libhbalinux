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

static struct sa_table adapter_table;
static const u_int32_t adapter_handle_offset = 0x100;

#define HBA_SHORT_NAME_LIMIT    64

/*
 * Support for adapter information.
 */
HBA_UINT32
adapter_get_count(void)
{
	return adapter_table.st_limit;
}

/*
 * Get adapter name.
 */
HBA_STATUS
adapter_get_name(HBA_UINT32 index, char *buf)
{
	HBA_STATUS status;
	struct adapter_info *ap;

	status = HBA_STATUS_ERROR_ILLEGAL_INDEX;
	ap = sa_table_lookup(&adapter_table, index);
	if (ap != NULL) {
		snprintf(buf, HBA_SHORT_NAME_LIMIT,
			"%s-%u", ap->ad_name, index);
		status = HBA_STATUS_OK;
	}
	return status;
}

/*
 * Add an adapter to the table.
 */
HBA_STATUS
adapter_create(struct adapter_info *ap)
{
	int index;

	index = sa_table_append(&adapter_table, ap);
	if (index < 0)
		return HBA_STATUS_ERROR;
	ap->ad_index = index;
	return HBA_STATUS_OK;
}

void
adapter_destroy(struct adapter_info *ap)
{
	sa_table_destroy_all(&ap->ad_ports);
	free(ap);
}

void
adapter_destroy_all(void)
{
	struct adapter_info *ap;
	int i;

	for (i = 0; i < adapter_table.st_limit; i++) {
		ap = adapter_table.st_table[i];
		if (ap) {
			adapter_table.st_table[i] = NULL;
			adapter_destroy(ap);
		}
	}
	sa_table_destroy(&adapter_table);
}

struct adapter_info *
adapter_open_handle(HBA_HANDLE handle)
{
	return sa_table_lookup(&adapter_table, handle -
			       adapter_handle_offset);
}

struct port_info *
adapter_get_port(HBA_HANDLE handle, HBA_UINT32 port)
{
	struct adapter_info *ap;
	struct port_info *pp = NULL;

	ap = adapter_open_handle(handle);
	if (ap)
		pp = sa_table_lookup(&ap->ad_ports, port);
	return pp;
}

struct port_info *
adapter_get_rport(HBA_HANDLE handle, HBA_UINT32 port, HBA_UINT32 rport)
{
	struct port_info *pp;
	struct port_info *rp = NULL;

	pp = adapter_get_port(handle, port);
	if (pp) {
		get_rport_info(pp);
		rp = sa_table_lookup(&pp->ap_rports, rport);
	}
	return rp;
}

/*
 * Get the Nth discovered port information.
 */
struct port_info *
adapter_get_rport_n(HBA_HANDLE handle, HBA_UINT32 port, HBA_UINT32 n)
{
	struct port_info *pp;
	struct port_info *rp = NULL;

	pp = adapter_get_port(handle, port);
	if (pp) {
		get_rport_info(pp);
		rp = sa_table_lookup_n(&pp->ap_rports, n);
	}
	return rp;
}

static void *
adapter_target_match(void *rp_arg, void *target_arg)
{
	struct port_info *rp = rp_arg;

	if (rp->ap_scsi_target != *(u_int32_t *)target_arg)
		rp_arg = NULL;
	return rp_arg;
}

/*
 * Get the rport by scsi_target number.
 */
struct port_info *
adapter_get_rport_target(HBA_HANDLE handle, HBA_UINT32 port, HBA_UINT32 n)
{
	struct port_info *pp;
	struct port_info *rp = NULL;

	pp = adapter_get_port(handle, port);
	if (pp) {
		get_rport_info(pp);
		rp = sa_table_search(&pp->ap_rports,
				     adapter_target_match, &n);
	}
	return rp;
}

static void *
adapter_wwpn_match(void *rp_arg, void *wwpn_arg)
{
	struct port_info *rp = rp_arg;

	if (memcmp(&rp->ap_attr.PortWWN, wwpn_arg, sizeof(HBA_WWN)) != 0)
		rp_arg = NULL;
	return rp_arg;
}

struct port_info *
adapter_get_rport_by_wwn(struct port_info *pp, HBA_WWN wwpn)
{
	struct port_info *rp;

	get_rport_info(pp);
	rp = sa_table_search(&pp->ap_rports, adapter_wwpn_match, &wwpn);
	return rp;
}

static void *
adapter_fcid_match(void *rp_arg, void *fcid_arg)
{
	struct port_info *rp = rp_arg;

	if (rp->ap_attr.PortFcId != *(fc_fid_t *)fcid_arg)
		rp_arg = NULL;
	return rp_arg;
}

struct port_info *
adapter_get_rport_by_fcid(struct port_info *pp, fc_fid_t fcid)
{
	struct port_info *rp;

	get_rport_info(pp);
	rp = sa_table_search(&pp->ap_rports, adapter_fcid_match, &fcid);
	return rp;
}

/*
 * Open adapter by name.
 */
HBA_HANDLE
adapter_open(char *name)
{
	char buf[256];
	HBA_HANDLE i;
	HBA_STATUS status;

	for (i = 0; i < adapter_table.st_limit; i++) {
		status = adapter_get_name(i, buf);
		if (status != HBA_STATUS_OK)
			return 0;
		if (!strcmp(buf, name))
			return adapter_handle_offset + i;
	}
	return 0;
}

/*
 * Get port by WWPN.
 * Returns NULL if WWN not unique.
 * If countp is non-NULL, the int it points to will be set to the
 * number found so that the caller can tell if the WWN was ambiguous.
 */
struct port_info *
adapter_get_port_by_wwn(HBA_HANDLE handle, HBA_WWN wwn, int *countp)
{
	struct adapter_info *ap;
	struct port_info *pp_found = NULL;
	struct port_info *pp;
	int count = 0;
	int p;

	ap = adapter_open_handle(handle);
	if (ap != NULL) {
		for (p = 0; p < ap->ad_ports.st_limit; p++) {
			pp = ap->ad_ports.st_table[p];
			if (pp &&
			    !memcmp(&pp->ap_attr.PortWWN, &wwn, sizeof(wwn))) {
				count++;
				pp_found = pp;
			}
		}
	}
	if (count > 1)
		pp_found = NULL;
	if (countp != NULL)
		*countp = count;
	return pp_found;
}

/*
 * Open adapter by WWN.
 */
HBA_STATUS
adapter_open_by_wwn(HBA_HANDLE *phandle, HBA_WWN wwn)
{
	struct adapter_info *ap;
	struct port_info *pp;
	HBA_HANDLE found_handle = 0;
	int count = 0;
	HBA_STATUS status;
	int i;
	int p;

	for (i = 0; i < adapter_table.st_limit; i++) {
		ap = adapter_table.st_table[i];
		if (!ap)
			continue;
		if (memcmp(&ap->ad_attr.NodeWWN, &wwn, sizeof(wwn)) == 0) {
			count++;
			found_handle = ap->ad_index + adapter_handle_offset;
		} else {
			for (p = 0; p < ap->ad_ports.st_limit; p++) {
				pp = ap->ad_ports.st_table[p];
				if (!pp)
					continue;
				if (memcmp(&pp->ap_attr.PortWWN,
					   &wwn, sizeof(wwn)) == 0) {
					count++;
					found_handle = ap->ad_index +
						       adapter_handle_offset;
				}
			}
		}
	}

	*phandle = HBA_HANDLE_INVALID;
	if (count == 1) {
		status = HBA_STATUS_OK;
		*phandle = found_handle;
	} else if (count > 1) {
		status = HBA_STATUS_ERROR_AMBIGUOUS_WWN;
	} else {
		status = HBA_STATUS_ERROR_ILLEGAL_WWN;
	}
	return status;
}

/*
 * Close adapter.
 */
void
adapter_close(HBA_HANDLE handle)
{
}

/*
 * Get adapter attributes.
 */
HBA_STATUS
adapter_get_attr(HBA_HANDLE handle, HBA_ADAPTERATTRIBUTES *pattr)
{
	struct adapter_info *ap;

	ap = adapter_open_handle(handle);
	if (ap) {
		*pattr = ap->ad_attr;       /* struct copy */
		return HBA_STATUS_OK;
	}
	return HBA_STATUS_ERROR;
}

/*
 * Get adapter port attributes.
 */
HBA_STATUS
adapter_get_port_attr(HBA_HANDLE handle, HBA_UINT32 port,
			HBA_PORTATTRIBUTES *pattr)
{
	struct port_info *pp;

	pp = adapter_get_port(handle, port);
	if (pp) {
		*pattr = pp->ap_attr;       /* struct copy */
		return HBA_STATUS_OK;
	}
	return HBA_STATUS_ERROR;
}

/*
 * Get discovered (remote) port attributes.
 */
HBA_STATUS
adapter_get_rport_attr(HBA_HANDLE handle, HBA_UINT32 port, HBA_UINT32 rport,
			 HBA_PORTATTRIBUTES *pattr)
{
	struct port_info *rp;

	rp = adapter_get_rport_n(handle, port, rport);
	if (rp) {
		*pattr = rp->ap_attr;       /* struct copy */
		return HBA_STATUS_OK;
	}
	return HBA_STATUS_ERROR;
}

/*
 * Get adapter port attributes.
 */
HBA_STATUS
adapter_get_port_attr_by_wwn(HBA_HANDLE handle, HBA_WWN wwn,
			       HBA_PORTATTRIBUTES *pattr)
{
	struct adapter_info *ap;
	struct port_info *pp;
	struct port_info *pp_found = NULL;
	u_int32_t p;
	int count = 0;
	HBA_STATUS status;

	ap = adapter_open_handle(handle);
	if (ap != NULL) {
		for (p = 0; p < ap->ad_ports.st_limit; p++) {
			pp = ap->ad_ports.st_table[p];
			if (pp == NULL)
				continue;
			if (!memcmp(&pp->ap_attr.PortWWN, &wwn, sizeof(wwn))) {
				count++;
				pp_found = pp;
			}
			pp = sa_table_search(&pp->ap_rports,
					     adapter_wwpn_match,
					     &wwn);
			if (pp) {
				count++;
				pp_found = pp;
			}
		}
	}
	pp = pp_found;
	if (pp != NULL) {
		if (count > 1) {
			status = HBA_STATUS_ERROR_AMBIGUOUS_WWN;
		} else {
			*pattr = pp->ap_attr;       /* struct copy */
			status = HBA_STATUS_OK;
		}
	} else {
		status = HBA_STATUS_ERROR_ILLEGAL_WWN;
	}
	return status;
}

