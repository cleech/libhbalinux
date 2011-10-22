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

#include <sys/ioctl.h>
#include "utils.h"
#include "api_lib.h"
#include "adapt_impl.h"
#include "fc_scsi.h"

/*
 * Perform INQUIRY of SCSI-generic device.
 */
HBA_STATUS
sg_issue_inquiry(const char *file, HBA_UINT8 cdb_byte1,
	       HBA_UINT8 cdb_byte2, void *buf, HBA_UINT32 *lenp,
	       HBA_UINT8 *statp, void *sense, HBA_UINT32 *sense_lenp)
{
	struct sg_io_hdr hdr;
	struct scsi_inquiry cmd;
	size_t len;
	HBA_UINT32 slen;
	int fd;
	int rc;

	len = *lenp;
	slen = *sense_lenp;
	if (slen > 255)
		slen = 255;   /* must fit in an 8-bit field */
	if (len > 255)
		len = 255;    /* sometimes must fit in 8-byte field */
	*lenp = 0;
	*statp = 0;
	fd = open(file, O_RDWR);
	if (fd < 0) {
		fprintf(stderr, "%s: open of %s failed, errno=0x%x\n",
			__func__, file, errno);
		return HBA_STATUS_ERROR;
	}
	memset(&hdr, 0, sizeof(hdr));
	memset(&cmd, 0, sizeof(cmd));
	memset(buf, 0, len);

	cmd.in_op = SCSI_OP_INQUIRY;
	cmd.in_flags = cdb_byte1;
	cmd.in_page_code = cdb_byte2;
	ua_net16_put(&cmd.in_alloc_len, len); /* field may actually be 8 bits */

	hdr.interface_id = 'S';
	hdr.dxfer_direction = SG_DXFER_FROM_DEV;
	hdr.cmd_len = sizeof(cmd);
	hdr.mx_sb_len = slen;
	hdr.dxfer_len = len;
	hdr.dxferp = (unsigned char *) buf;
	hdr.cmdp = (unsigned char *) &cmd;
	hdr.sbp = (unsigned char *) sense;
	hdr.timeout = 3000;                     /* mS to wait for result */

	rc = ioctl(fd, SG_IO, &hdr);
	if (rc < 0) {
		rc = errno;
		fprintf(stderr, "%s: SG_IO error. file %s, errno=0x%x\n",
			__func__, file, errno);
		close(fd);
		return HBA_STATUS_ERROR;
	}
	close(fd);
	*lenp = len - hdr.resid;
	*sense_lenp = hdr.sb_len_wr;
	*statp = hdr.status;
	return HBA_STATUS_OK;
}

static inline unsigned int
sg_get_id_type(struct scsi_inquiry_desc *dp)
{
	return dp->id_type_flags & SCSI_INQT_TYPE_MASK;
}

/*
 * Get device ID information for HBA-API.
 * See the spec.  We get the "best" information and leave the rest.
 * The buffer is left empty if nothing is gotten.
 */
void
sg_get_dev_id(const char *name, char *buf, size_t result_len)
{
	struct scsi_inquiry_dev_id *idp;
	struct scsi_inquiry_desc *dp;
	struct scsi_inquiry_desc *best = NULL;
	char sense[252];
	HBA_UINT32 len;
	HBA_UINT32 slen;
	u_char scsi_stat;
	size_t rlen;
	size_t dlen;
	unsigned int type;

	memset(buf, 0, result_len);
	len = result_len;
	slen = sizeof(sense);
	idp = (struct scsi_inquiry_dev_id *) buf;
	sg_issue_inquiry(name, SCSI_INQF_EVPD, SCSI_INQP_DEV_ID,
			buf, &len, &scsi_stat, sense, &slen);
	if (len < sizeof(*idp))
		return;
	if (idp->is_page_code != SCSI_INQP_DEV_ID)
		return;
	len -= sizeof(*idp);
	rlen = net16_get(&idp->is_page_len);
	if (rlen > len)
		rlen = len;
	dp = (struct scsi_inquiry_desc *) (idp + 1);
	for (; rlen >= sizeof(*dp);
	     rlen -= dlen,
	     dp = (struct scsi_inquiry_desc *) ((char *) dp + dlen)) {
		dlen = dp->id_designator_len + sizeof(*dp) -
		       sizeof(dp->id_designator[0]);
		if (dlen > rlen)
			break;
		type = sg_get_id_type(dp);
		if (type > SCSI_DTYPE_NAA)
			continue;
		if (best == NULL)
			best = dp;
		else if (type == sg_get_id_type(best) &&
			   (dp->id_designator_len < best->id_designator_len ||
			   (dp->id_designator_len == best->id_designator_len &&
			   memcmp(dp->id_designator, best->id_designator,
			   best->id_designator_len) < 0))) {
			best = dp;
		} else if (type > sg_get_id_type(best))
			best = dp;
	}
	if (best) {
		dp = best;
		dlen = dp->id_designator_len + sizeof(*dp) -
			sizeof(dp->id_designator[0]);
		if (dlen > result_len)
			dlen = 0;                       /* can't happen */
		else
			memmove(buf, dp, dlen);         /* areas may overlap */
		memset(buf +  dlen, 0, result_len - dlen);
	}
}

/*
 * Read Capacity for HBA-API.
 */
HBA_STATUS
sg_issue_read_capacity(const char *file, void *resp, HBA_UINT32 *resp_lenp,
		HBA_UINT8 *statp, void *sense, HBA_UINT32 *sense_lenp)
{
	struct sg_io_hdr hdr;
	struct scsi_rcap10 cmd;
	struct scsi_rcap16 cmd_16;
	size_t len;
	int fd;
	int rc;

	len = *resp_lenp;
	*resp_lenp = 0;
	fd = open(file, O_RDWR);
	if (fd < 0) {
		fprintf(stderr, "%s: open of %s failed, errno=0x%x\n",
			__func__, file, errno);
		return errno;
	}
	memset(&hdr, 0, sizeof(hdr));

	/* If the response buffer size is enough to
	 * accomodate READ CAPACITY(16) response issue
	 * SCSI READ CAPACITY(16) else issue
	 * SCSI READ CAPACITY(10)
	 */
	if (len >= sizeof(struct scsi_rcap16_resp)) {
		memset(&cmd_16, 0, sizeof(cmd_16));
		cmd_16.rc_op = SCSI_OP_SA_IN_16;
		cmd_16.rc_sa = SCSI_SA_READ_CAP16;
		ua_net32_put(&cmd_16.rc_alloc_len, len);
		hdr.cmd_len = sizeof(cmd_16);
		hdr.cmdp = (unsigned char *) &cmd_16;
	} else {
		memset(&cmd, 0, sizeof(cmd));
		cmd.rc_op = SCSI_OP_READ_CAP10;
		hdr.cmd_len = sizeof(cmd);
		hdr.cmdp = (unsigned char *) &cmd;
	}

	hdr.interface_id = 'S';
	hdr.dxfer_direction = SG_DXFER_FROM_DEV;
	hdr.mx_sb_len = *sense_lenp;
	hdr.dxfer_len = len;
	hdr.dxferp = (unsigned char *) resp;
	hdr.sbp = (unsigned char *) sense;
	hdr.timeout = UINT_MAX;
	hdr.timeout = 3000;                     /* mS to wait for result */

	rc = ioctl(fd, SG_IO, &hdr);
	if (rc < 0) {
		rc = errno;
		fprintf(stderr, "%s: SG_IO error. file %s, errno=0x%x\n",
			__func__, file, errno);
		close(fd);
		return HBA_STATUS_ERROR;
	}
	close(fd);
	*resp_lenp = len - hdr.resid;
	*sense_lenp = hdr.sb_len_wr;
	*statp = hdr.status;
	return HBA_STATUS_OK;
}

/*
 * Report LUNs for HBA-API.
 */
HBA_STATUS
sg_issue_report_luns(const char *file, void *resp, HBA_UINT32 *resp_lenp,
		   HBA_UINT8 *statp, void *sense, HBA_UINT32 *sense_lenp)
{
	struct sg_io_hdr hdr;
	struct scsi_report_luns cmd;
	size_t len;
	int fd;
	int rc;

	len = *resp_lenp;
	*resp_lenp = 0;
	fd = open(file, O_RDWR);
	if (fd < 0) {
		fprintf(stderr, "%s: open of %s failed, errno=0x%x\n",
			__func__, file, errno);
		return errno;
	}
	memset(&hdr, 0, sizeof(hdr));
	memset(&cmd, 0, sizeof(cmd));

	cmd.rl_op = SCSI_OP_REPORT_LUNS;
	ua_net32_put(&cmd.rl_alloc_len, len);

	hdr.interface_id = 'S';
	hdr.dxfer_direction = SG_DXFER_FROM_DEV;
	hdr.cmd_len = sizeof(cmd);
	hdr.mx_sb_len = *sense_lenp;
	hdr.dxfer_len = len;
	hdr.dxferp = (unsigned char *) resp;
	hdr.cmdp = (unsigned char *) &cmd;
	hdr.sbp = (unsigned char *) sense;
	hdr.timeout = UINT_MAX;
	hdr.timeout = 3000;                     /* mS to wait for result */

	rc = ioctl(fd, SG_IO, &hdr);
	if (rc < 0) {
		rc = errno;
		fprintf(stderr, "%s: SG_IO error. file %s, errno=0x%x\n",
			__func__, file, errno);
		close(fd);
		return HBA_STATUS_ERROR;
	}
	close(fd);
	*resp_lenp = len - hdr.resid;
	*sense_lenp = hdr.sb_len_wr;
	*statp = hdr.status;
	return HBA_STATUS_OK;
}

