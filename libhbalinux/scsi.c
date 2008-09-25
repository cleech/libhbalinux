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
#include "fc_scsi.h"

/*
 * Inquiry V1.
 */
HBA_STATUS
scsi_inquiry_v1(HBA_HANDLE handle, HBA_WWN disc_wwpn, HBA_UINT64 fc_lun,
		    HBA_UINT8 evpd, HBA_UINT32 page_code, void *resp,
		    HBA_UINT32 resp_len, void *sense, HBA_UINT32 sense_len)
{
	struct port_info *pp;
	char sg_name[50];
	HBA_UINT8 stat;
	HBA_STATUS status;

	/*
	 * Find port.
	 */
	pp = adapter_get_port(handle, 0);
	if (pp == NULL)
		return HBA_STATUS_ERROR_INVALID_HANDLE;

	if (get_binding_sg_name(
	    pp, disc_wwpn, fc_lun, sg_name, sizeof(sg_name)) != 0)
		return HBA_STATUS_ERROR_TARGET_LUN;

	status = sg_issue_inquiry(sg_name, evpd ? SCSI_INQF_EVPD : 0, page_code,
				resp, &resp_len, &stat, sense, &sense_len);
	if (status == HBA_STATUS_OK && stat == SCSI_ST_CHECK)
		status = HBA_STATUS_SCSI_CHECK_CONDITION;

	return status;
}

/*
 * Inquiry V2.
 */
HBA_STATUS
scsi_inquiry_v2(HBA_HANDLE handle, HBA_WWN wwpn, HBA_WWN disc_wwpn,
		    HBA_UINT64 fc_lun, HBA_UINT8 cdb_byte1,
		    HBA_UINT8 cdb_byte2, void *resp, HBA_UINT32 *resp_lenp,
		    HBA_UINT8 *statp, void *sense, HBA_UINT32 *sense_lenp)
{
	struct port_info *pp;
	char sg_name[50];

	/*
	 * Find port.
	 */
	pp = adapter_get_port_by_wwn(handle, wwpn, NULL);
	if (pp == NULL)
		return HBA_STATUS_ERROR_ILLEGAL_WWN;

	if (get_binding_sg_name(
	    pp, disc_wwpn, fc_lun, sg_name, sizeof(sg_name)) != 0)
		return HBA_STATUS_ERROR_TARGET_LUN;

	return sg_issue_inquiry(sg_name, cdb_byte1, cdb_byte2,
			       resp, resp_lenp, statp, sense, sense_lenp);
}

/*
 * Read capacity V1.
 */
HBA_STATUS
scsi_read_capacity_v1(HBA_HANDLE handle, HBA_WWN disc_wwpn,
			  HBA_UINT64 fc_lun, void *resp,
			  HBA_UINT32 resp_len, void *sense,
			  HBA_UINT32 sense_len)
{
	struct port_info *pp;
	char sg_name[50];
	HBA_UINT8 stat;
	HBA_STATUS status;

	/*
	 * Find port.
	 */
	pp = adapter_get_port(handle, 0);
	if (pp == NULL)
		return HBA_STATUS_ERROR_INVALID_HANDLE;

	if (get_binding_sg_name(
	    pp, disc_wwpn, fc_lun, sg_name, sizeof(sg_name)) != 0)
		return HBA_STATUS_ERROR_TARGET_LUN;

	status = sg_issue_read_capacity(sg_name, resp, &resp_len,
				 &stat, sense, &sense_len);
	if (status == HBA_STATUS_OK && stat == SCSI_ST_CHECK)
		status = HBA_STATUS_SCSI_CHECK_CONDITION;

	return status;
}

/*
 * Read capacity V2.
 */
HBA_STATUS
scsi_read_capacity_v2(HBA_HANDLE handle, HBA_WWN wwpn,
			  HBA_WWN disc_wwpn, HBA_UINT64 fc_lun,
			  void *resp, HBA_UINT32 *resp_lenp,
			  HBA_UINT8 *statp, void *sense,
			  HBA_UINT32 *sense_lenp)
{
	struct port_info *pp;
	char sg_name[50];

	/*
	 * Find port.
	 */
	pp = adapter_get_port_by_wwn(handle, wwpn, NULL);
	if (pp == NULL)
		return HBA_STATUS_ERROR_ILLEGAL_WWN;

	if (get_binding_sg_name(
	    pp, disc_wwpn, fc_lun, sg_name, sizeof(sg_name)) != 0)
		return HBA_STATUS_ERROR_TARGET_LUN;

	return sg_issue_read_capacity(sg_name, resp, resp_lenp,
				statp, sense, sense_lenp);
}

/*
 * Report LUNS V1.
 */
HBA_STATUS
scsi_report_luns_v1(HBA_HANDLE handle, HBA_WWN disc_wwpn,
			void *resp, HBA_UINT32 resp_len,
			void *sense, HBA_UINT32 sense_len)
{
	struct port_info *pp;
	char sg_name[50];
	HBA_UINT8 stat;
	HBA_STATUS status;

	/*
	 * Find port.
	 */
	pp = adapter_get_port(handle, 0);
	if (pp == NULL)
		return HBA_STATUS_ERROR_INVALID_HANDLE;

	if (get_binding_sg_name(
	    pp, disc_wwpn, 0, sg_name, sizeof(sg_name)) != 0)
		return HBA_STATUS_ERROR_TARGET_PORT_WWN;

	status = sg_issue_report_luns(sg_name, resp, &resp_len,
				    &stat, sense, &sense_len);
	if (status == HBA_STATUS_OK && stat == SCSI_ST_CHECK)
		status = HBA_STATUS_SCSI_CHECK_CONDITION;

	return status;
}

/*
 * Report LUNS V2.
 */
HBA_STATUS
scsi_report_luns_v2(HBA_HANDLE handle, HBA_WWN wwpn,
			HBA_WWN disc_wwpn, void *resp,
			HBA_UINT32 *resp_lenp, HBA_UINT8 *statp,
			void *sense, HBA_UINT32 *sense_lenp)
{
	struct port_info *pp;
	char sg_name[50];

	/*
	 * Find port.
	 */
	pp = adapter_get_port_by_wwn(handle, wwpn, NULL);
	if (pp == NULL)
		return HBA_STATUS_ERROR_ILLEGAL_WWN;

	if (get_binding_sg_name(
	    pp, disc_wwpn, 0, sg_name, sizeof(sg_name)) != 0)
		return HBA_STATUS_ERROR_TARGET_PORT_WWN;

	return sg_issue_report_luns(sg_name, resp, resp_lenp,
				  statp, sense, sense_lenp);
}

