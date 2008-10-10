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

#ifndef _BIND_IMPL_H_
#define _BIND_IMPL_H_

HBA_STATUS get_binding_capability(HBA_HANDLE, HBA_WWN, HBA_BIND_CAPABILITY *);
HBA_STATUS get_binding_support(HBA_HANDLE, HBA_WWN, HBA_BIND_CAPABILITY *);
HBA_STATUS set_binding_support(HBA_HANDLE, HBA_WWN, HBA_BIND_CAPABILITY);
HBA_STATUS get_binding_target_mapping_v1(HBA_HANDLE, HBA_FCPTARGETMAPPING *);
HBA_STATUS get_binding_target_mapping_v2(HBA_HANDLE, HBA_WWN,
					  HBA_FCPTARGETMAPPINGV2 *);
int get_binding_sg_name(struct port_info *,
			HBA_WWN, HBA_UINT64, char *, size_t);

#endif /* _BIND_IMPL_H_ */
