/*
 * CodeSignature/Entitlements.h - PureDarwin stub. Apple's CodeSignature library
 * is not open source; xnu 12377's bsd/net/necp_client.c needs this constant.
 *
 * The real entitlement string is unknown to us, so this deliberately cannot
 * match: IOTaskHasEntitlement() returns false and the caller falls through to
 * its priv_check_cred() path. That is the restrictive direction.
 */

#ifndef PD_CODESIGNATURE_ENTITLEMENTS_H
#define PD_CODESIGNATURE_ENTITLEMENTS_H

#define kCSWebBrowserNetworkEntitlement    "sh.kido.unmatchable-entitlement"
#define kCSWebBrowserHostEntitlement       "sh.kido.unmatchable-entitlement"
#define kCSWebBrowserGPUEntitlement        "sh.kido.unmatchable-entitlement"
#define kCSWebBrowserWebContentEntitlement "sh.kido.unmatchable-entitlement"

#endif /* PD_CODESIGNATURE_ENTITLEMENTS_H */
