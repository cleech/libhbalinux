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

#define _XOPEN_SOURCE 500        /* for strptime() */
#include "utils.h"
#include "api_lib.h"
#include "adapt_impl.h"
#include "bind_impl.h"

/**
 * Return the version of the SNIA HBA-API supported by this library.
 */
static HBA_UINT32 get_library_version()
{
	return HBA_LIBVERSION;
}

/*
 * When HBA_GetVendorLibraryAttributes() is called,
 * it does not dispatch to the library entry point at
 * .GetVendorLibraryAttributesHandler. Thus this
 * routine can never be entered. -[sma]
 */
#if 0
/**
 * Get the library attributes.
 * @param ap library attributes pointer.
 * @returns 0 or error code.
 */
static HBA_STATUS get_vendor_lib_attrs(HBA_LIBRARYATTRIBUTES *ap)
{
	memset(ap, 0, sizeof(*ap));
	if (strptime(BUILD_DATE, "%Y/%m/%d %T %Z", &ap->build_date) == NULL)
		memset(&ap->build_date, 0, sizeof(ap->build_date));
	strcpy(ap->VName, HBA_API_VENDOR);
	strcpy(ap->VVersion, HBA_API_VERSION);
	return HBA_STATUS_OK;
}
#endif

/*
 * initialize the library after load.
 */
static HBA_STATUS load_library(void)
{
	adapter_init();
	return HBA_STATUS_OK;
}

static HBA_STATUS free_library(void)
{
	adapter_shutdown();
	adapter_destroy_all();
	return HBA_STATUS_OK;
}

static HBA_ENTRYPOINTSV2 vendor_lib_entrypoints = {
    .GetVersionHandler =                       get_library_version,
    .LoadLibraryHandler =                      load_library,
    .FreeLibraryHandler =                      free_library,
    .GetNumberOfAdaptersHandler =              adapter_get_count,
    .GetAdapterNameHandler =                   adapter_get_name,
    .OpenAdapterHandler =                      adapter_open,
    .CloseAdapterHandler =                     adapter_close,
    .GetAdapterAttributesHandler =             adapter_get_attr,
    .GetAdapterPortAttributesHandler =         adapter_get_port_attr,
    .GetPortStatisticsHandler =                get_port_statistics,
    .GetDiscoveredPortAttributesHandler =      adapter_get_rport_attr,

    .GetPortAttributesByWWNHandler =           NULL,
					/* adapter_get_port_attr_by_wwn, */
    /* Next function deprecated but still supported */
    .SendCTPassThruHandler =                   NULL,
    .RefreshInformationHandler =               NULL,
    .ResetStatisticsHandler =                  NULL,
    /* Next function deprecated but still supported */
    .GetFcpTargetMappingHandler =              get_binding_target_mapping_v1,
    /* Next function depricated but still supported */
    .GetFcpPersistentBindingHandler =          NULL,
    .GetEventBufferHandler =                   NULL,
    .SetRNIDMgmtInfoHandler =                  NULL,
    .GetRNIDMgmtInfoHandler =                  NULL,
    /* Next function deprecated but still supported */
    .SendRNIDHandler =                         NULL,
    .ScsiInquiryHandler =                      scsi_inquiry_v1,
    .ReportLUNsHandler =                       scsi_report_luns_v1,
    .ReadCapacityHandler =                     scsi_read_capacity_v1,

    /* V2 handlers */
    .OpenAdapterByWWNHandler =                 NULL,
					/* adapter_open_by_wwn, */
    .GetFcpTargetMappingV2Handler =            get_binding_target_mapping_v2,
    .SendCTPassThruV2Handler =                 NULL,
    .RefreshAdapterConfigurationHandler =      NULL,
    .GetBindingCapabilityHandler =             NULL,
					/* get_binding_capability, */
    .GetBindingSupportHandler =                NULL,
					/* get_binding_support, */
    .SetBindingSupportHandler =                NULL,
					/* set_binding_support, */
    .SetPersistentBindingV2Handler =           NULL,
    .GetPersistentBindingV2Handler =           NULL,
    .RemovePersistentBindingHandler =          NULL,
    .RemoveAllPersistentBindingsHandler =      NULL,
    .SendRNIDV2Handler =                       NULL,
    .ScsiInquiryV2Handler =                    scsi_inquiry_v2,
    .ScsiReportLUNsV2Handler =                 scsi_report_luns_v2,
    .ScsiReadCapacityV2Handler =               scsi_read_capacity_v2,
    .GetVendorLibraryAttributesHandler =       NULL,
					/* get_vendor_lib_attrs, */
    .RemoveCallbackHandler =                   NULL,
    .RegisterForAdapterAddEventsHandler =      NULL,
    .RegisterForAdapterEventsHandler =         NULL,
    .RegisterForAdapterPortEventsHandler =     NULL,
    .RegisterForAdapterPortStatEventsHandler = NULL,
    .RegisterForTargetEventsHandler =          NULL,
    .RegisterForLinkEventsHandler =            NULL,
    .SendRPLHandler =                          NULL,
    .SendRPSHandler =                          NULL,
    .SendSRLHandler =                          NULL,
    .SendLIRRHandler =                         NULL,
    .GetFC4StatisticsHandler =                 get_port_fc4_statistics,
    .GetFCPStatisticsHandler =                 NULL,
    .SendRLSHandler =                          NULL,
};

/**
 * Function called by a version 1 common HBAAPI library to get our entry points.
 *
 * @arg ep pointer to entrypoints structure where we store our
 *  function pointers.
 * @returns HBA_STATUS.
 */
HBA_STATUS HBA_RegisterLibrary(HBA_ENTRYPOINTS *ep)
{
	memcpy(ep, &vendor_lib_entrypoints, sizeof(HBA_ENTRYPOINTS));
	return HBA_STATUS_OK;
}

/**
 * Function called by the common HBAAPI library to get our entry points.
 *
 * @arg ep pointer to entrypoints structure where we store our
 *  function pointers.
 * @returns HBA_STATUS.
 */
HBA_STATUS HBA_RegisterLibraryV2(HBA_ENTRYPOINTSV2 *ep)
{
	*ep = vendor_lib_entrypoints;  /* structure copy */
	return HBA_STATUS_OK;
}

