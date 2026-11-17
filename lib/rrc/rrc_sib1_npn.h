// SPDX-FileCopyrightText: Copyright (C) 2021-2026 Software Radio Systems Limited
// SPDX-License-Identifier: BSD-3-Clause-Open-MPI
// Portions of this file may implement 3GPP specifications, which may be subject to additional licensing requirements.

#pragma once

#include "ocudu/asn1/rrc_nr/sys_info.h"
#include "ocudu/ran/nr_cell_identity.h"
#include "ocudu/ran/snpn_identity.h"
#include "ocudu/ocudulog/logger.h"
#include <optional>
#include <vector>

namespace ocudu::ocucp {

/// NPNs a cell broadcasts in SIB1, as the RRC needs them to resolve \c selectedPLMN-Identity.
struct rrc_sib1_npn_info {
  /// NPNs of \c npn-IdentityInfoList in NPN index order. An entry is empty for a PNI-NPN (CAG), which is not supported.
  std::vector<std::optional<snpn_broadcast_info>> npn_list;
  /// Whether the cell is an NPN-only cell: \c cellReservedForOtherUse with \c npn-IdentityInfoList (TS 38.304, 5.3.1).
  /// The PLMN entry of such a cell takes no index value, so its NPN index values start at 1.
  bool npn_only = false;
};

/// \brief Reads the NPNs of SIB1 \c npn-IdentityInfoList.
///
/// TS 38.331, \c npn-IdentityInfoList: the NPN index is B+c1+...+c(n-1)+d1+...+d(m-1)+e(i), where B is the index of
/// the last PLMN of \c plmn-IdentityInfoList (0 in an NPN-only cell), c(j) and d(k) the index values of the preceding
/// \c NPN-IdentityInfo and \c NPN-Identity entries, and e(i) is i for the i-th NID of an SNPN entry but 1 for a PNI-NPN
/// entry, whatever the number of its CAG IDs.
inline rrc_sib1_npn_info
extract_sib1_npn_info(const asn1::rrc_nr::sib1_s& sib1, nr_cell_identity nci, ocudulog::basic_logger& logger)
{
  rrc_sib1_npn_info info;
  if (not sib1.cell_access_related_info.npn_id_info_list_r16.is_present()) {
    return info;
  }
  for (const auto& npn_info : *sib1.cell_access_related_info.npn_id_info_list_r16) {
    const tac_t tac = npn_info.tac_r16.to_number();
    for (const auto& npn_id : npn_info.npn_id_list_r16) {
      if (npn_id.type().value == asn1::rrc_nr::npn_id_r16_c::types_opts::pni_npn_r16) {
        logger.warning("nci={}: SIB1 broadcasts a PNI-NPN (CAG), which is not supported", nci);
        info.npn_list.emplace_back(std::nullopt);
        continue;
      }
      const auto&                  snpn = npn_id.snpn_r16();
      std::optional<plmn_identity> plmn;
      if (snpn.plmn_id_r16.mcc_present) {
        expected<mobile_country_code> mcc = mobile_country_code::from_bytes(snpn.plmn_id_r16.mcc);
        expected<mobile_network_code> mnc = mobile_network_code::from_bytes(snpn.plmn_id_r16.mnc);
        if (mcc.has_value() and mnc.has_value()) {
          plmn.emplace(mcc.value(), mnc.value());
        }
      }
      for (const auto& asn1_nid : snpn.nid_list_r16) {
        if (not plmn.has_value()) {
          logger.warning("nci={}: Ignoring an SNPN of SIB1 without a valid MCC and MNC", nci);
          info.npn_list.emplace_back(std::nullopt);
          continue;
        }
        info.npn_list.emplace_back(
            snpn_broadcast_info{{*plmn, network_identifier::from_number(asn1_nid.to_number()).value()}, tac});
        logger.debug("Found SNPN {} with TAC {} in SIB1 RRC container", info.npn_list.back()->snpn, tac);
      }
    }
  }
  info.npn_only = sib1.cell_access_related_info.cell_reserved_for_other_use_present and not info.npn_list.empty();
  return info;
}

} // namespace ocudu::ocucp
