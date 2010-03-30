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

static int sys_read_port_state(const char *, const char *, u_int32_t *);
static int sys_read_classes(const char *, const char *, u_int32_t *);

static struct sa_table rports_table;          /* table of discovered ports */

/*
 * Handle a single remote port from the /sys directory entry.
 * The return value is 0 unless an error is detected which should stop the
 * directory read.
 */
static int
sysfs_get_rport(struct dirent *dp, void *arg)
{
	struct port_info *rp;
	HBA_PORTATTRIBUTES *rpa;
	int rc;
	u_int32_t hba;
	u_int32_t port;
	u_int32_t rp_index;
	char *rport_dir;
	char buf[256];

	/*
	 * Parse name into bus number, channel number, and remote port number.
	 */
	hba = ~0;
	port = ~0;
	rp_index = ~0;
	rc = sscanf(dp->d_name, SYSFS_RPORT_DIR, &hba, &port, &rp_index);
	if (rc != 3) {
		fprintf(stderr,
			"%s: remote port %s didn't parse."
			" rc %d h 0x%x p 0x%x rp 0x%x\n", __func__,
			dp->d_name, rc, hba, port, rp_index);
		return 0;
	}

	/*
	 * Allocate a remote port.
	 */
	rp = malloc(sizeof(*rp));
	if (rp == NULL) {
		fprintf(stderr, "%s: malloc for remote port %s failed,"
			" errno=0x%x\n", __func__, dp->d_name, errno);
		return ENOMEM;
	}
	memset(rp, 0, sizeof(*rp));
	rp->ap_kern_hba = hba;
	rp->ap_index = port;
	rp->ap_disc_index = rp_index;
	rpa = &rp->ap_attr;

	snprintf(rpa->OSDeviceName, sizeof(rpa->OSDeviceName), "%s/%s",
		SYSFS_RPORT_ROOT, dp->d_name);
	rport_dir = rpa->OSDeviceName;
	rc = 0;
	rc |= sys_read_wwn(rport_dir, "node_name", &rpa->NodeWWN);
	rc |= sys_read_wwn(rport_dir, "port_name", &rpa->PortWWN);
	rc |= sa_sys_read_u32(rport_dir, "port_id", &rpa->PortFcId);
	rc |= sa_sys_read_u32(rport_dir, "scsi_target_id", &rp->ap_scsi_target);
	sa_sys_read_line(rport_dir, "maxframe_size", buf, sizeof(buf));
	sscanf(buf, "%d", &rpa->PortMaxFrameSize);
	rc |= sys_read_port_state(rport_dir, "port_state", &rpa->PortState);
	rc |= sys_read_classes(rport_dir, "supported_classes",
				   &rpa->PortSupportedClassofService);
	if (rc != 0 || sa_table_append(&rports_table, rp) < 0) {
		if (rc != 0)
			fprintf(stderr,
				"%s: errors (%x) from /sys reads in %s\n",
				__func__, rc, dp->d_name);
		else
			fprintf(stderr,
				"%s: sa_table_append error on rport %s\n",
				__func__, dp->d_name);
		free(rp);
	}
	return 0;
}

/*
 * Get remote port information from /sys.
 */
static void
sysfs_find_rports(void)
{
	sa_dir_read(SYSFS_RPORT_ROOT, sysfs_get_rport, NULL);
}

/*
 * Read port state as formatted by scsi_transport_fc.c in the linux kernel.
 */
static int
sys_read_port_state(const char *dir, const char *file, u_int32_t *statep)
{
	char buf[256];
	int rc;

	rc = sa_sys_read_line(dir, file, buf, sizeof(buf));
	if (rc == 0) {
		rc = sa_enum_encode(port_states_table, buf, statep);
		if (rc != 0)
			fprintf(stderr,
				"%s: parse error. file %s/%s line '%s'\n",
				__func__, dir, file, buf);
	}
	return rc;
}

/*
 * Read class list as formatted by scsi_transport_fc.c in the linux kernel.
 * Format is expected to be "Class 3[, Class 4]..."
 * Actually accepts "[Class ]3[,[ ][Class ]4]..." (i.e., "Class" and spaces
 * are optional).
 */
static int
sys_read_classes(const char *dir, const char *file, u_int32_t *classp)
{
	char buf[256];
	int rc;
	u_int32_t val;
	char *cp;
	char *ep;

	*classp = 0;
	rc = sa_sys_read_line(dir, file, buf, sizeof(buf));
	if (rc == 0 && strstr(buf, "unspecified") == NULL) {
		for (cp = buf; *cp != '\0'; cp = ep) {
			if (strncmp(cp, "Class ", 6) == 0)
				cp += 6;
			val = strtoul(cp, &ep, 10);
			*classp |= 1 << val;
			if (*ep == '\0')
				break;
			if (*ep == ',') {
				ep++;
				if (*ep == ' ')
					ep++;
			} else {
				fprintf(stderr, "%s: parse error. file %s/%s "
				       "line '%s' ep '%c'\n", __func__,
					dir, file, buf, *ep);
				rc = -1;
				break;
			}
		}
	}
	return rc;
}

/*
 * Get all discovered ports for a particular port using /sys.
 */
void
get_rport_info(struct port_info *pp)
{
	struct port_info *rp;
	int rp_count = 0;
	int ri;

	if (rports_table.st_size == 0)
		sysfs_find_rports();

	for (ri = 0; ri < rports_table.st_size; ri++) {
		rp = sa_table_lookup(&rports_table, ri);
		if (rp != NULL) {
			if (rp->ap_kern_hba == pp->ap_kern_hba &&
			    rp->ap_index == pp->ap_index &&
			    rp->ap_adapt == NULL) {
				rp->ap_adapt = pp->ap_adapt;
				if (sa_table_lookup(&pp->ap_rports,
						    rp->ap_disc_index)) {
					fprintf(stderr,
						"%s: discovered port exists. "
					       "hba %x port %x rport %x\n",
					       __func__, pp->ap_kern_hba,
					       pp->ap_index, rp->ap_disc_index);
				}
				sa_table_insert(&pp->ap_rports,
						rp->ap_disc_index, rp);
			}
			rp_count++;
		}
	}
}

