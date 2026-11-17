// SPDX-FileCopyrightText: Copyright (C) 2021-2026 Software Radio Systems Limited
// SPDX-License-Identifier: BSD-3-Clause-Open-MPI
// Portions of this file may implement 3GPP specifications, which may be subject to additional licensing requirements.

#pragma once

#include "rrc_ue_logger.h"
#include "ocudu/asn1/rrc_nr/ul_dcch_msg_ies.h"
#include "ocudu/ran/five_g_s_tmsi.h"
#include "ocudu/ran/plmn_identity.h"
#include "ocudu/rrc/coarse_ue_location.h"
#include "ocudu/rrc/rrc_cell_context.h"
#include "ocudu/rrc/rrc_du.h"
#include "ocudu/rrc/rrc_ue.h"
#include "ocudu/rrc/rrc_ue_capabilities.h"
#include "ocudu/rrc/rrc_ue_config.h"
#include <array>

namespace ocudu::ocucp {

/// Holds the RRC UE context used by the UE object and all its procedures.
class rrc_ue_context_t
{
public:
  rrc_ue_context_t(cu_cp_ue_index_t                       ue_index_,
                   rnti_t                                 c_rnti_,
                   const rrc_cell_context&                cell_,
                   const rrc_ue_cfg_t&                    cfg_,
                   std::optional<rrc_ue_transfer_context> rrc_context_,
                   std::optional<rrc_resume_context_t>    remote_resume_context_,
                   rrc_ue_srb_pdcp_manager&               pdcp_manager_,
                   rrc_ue_logger&                         logger_);

  const cu_cp_ue_index_t         ue_index; // UE index assigned by the DU processor
  rnti_t                         c_rnti;   // current C-RNTI
  rrc_cell_context               cell;     // current cell
  const rrc_ue_cfg_t             cfg;
  plmn_identity                  plmn_id = plmn_identity::test_value(); // PLMN identity of the UE
  /// SNPN the UE selected in RRCSetupComplete, with the TAC the cell broadcasts for it. Empty for a PLMN UE.
  std::optional<snpn_broadcast_info> selected_snpn;
  rrc_state                      state   = rrc_state::idle;
  std::optional<rrc_meas_cfg>    meas_cfg;
  std::optional<uint8_t>         serving_cell_mo;
  std::optional<five_g_s_tmsi_t> five_g_s_tmsi;
  std::variant<uint64_t, asn1::fixed_bitstring<39>>
                                                  setup_ue_id; ///< this is either a random value or the 5G-S-TMSI-PART1
  establishment_cause_t                           connection_cause;
  rrc_ue_srb_pdcp_manager&                        pdcp_manager;
  std::array<rrc_ue_pdcp_notifier*, MAX_NOF_SRBS> pdcp_notifiers = {};

  rrc_ue_pdcp_notifier* get_pdcp_notifier(srb_id_t srb_id) const
  {
    unsigned idx = srb_id_to_uint(srb_id);
    return idx < MAX_NOF_SRBS ? pdcp_notifiers[idx] : nullptr;
  }
  std::optional<asn1::rrc_nr::ue_cap_rat_container_list_l> capabilities_list;
  rrc_ue_capabilities_t                                    capabilities;
  std::optional<rrc_ue_transfer_context> transfer_context; // Context of old UE when created through mobility.
  // Resume identity of a UE that resumed here without this node holding its context, so the context has to be
  // retrieved from the peer NG-RAN node that allocated the I-RNTI.
  std::optional<rrc_resume_context_t> remote_resume_context;
  byte_buffer                         cell_group_config;
  bool                                reestablishment_ongoing = false;
  /// Last coarse UE location reported by the UE.
  std::optional<coarse_ue_location> coarse_location;
  // NAS messages received while UE is in RRC Inactive, to be sent after successful resume.
  std::vector<byte_buffer> pending_dl_nas_transport_messages;
  rrc_ue_logger&           logger;
};

/// \brief Reports a UE served in an SNPN by its SNPN: the TAC the cell broadcasts for the SNPN and its NID, TS 38.413,
/// sec. 9.3.1.16. Leaves the location of a PLMN UE untouched.
inline void apply_selected_snpn(const rrc_ue_context_t& context, cu_cp_user_location_info_nr& user_location_info)
{
  if (not context.selected_snpn.has_value()) {
    return;
  }
  // TS 38.413 sec. 9.3.1.16: the TAI and NR CGI of an SNPN UE name the SNPN's PLMN ID, which may differ from the
  // PLMN of the cell, e.g. MCC 999.
  user_location_info.tai.plmn_id    = context.selected_snpn->snpn.plmn;
  user_location_info.nr_cgi.plmn_id = context.selected_snpn->snpn.plmn;
  user_location_info.tai.tac        = context.selected_snpn->tac;
  // The broadcast TAC list belongs to the PLMN entry of the cell (NTN), not to the SNPN.
  user_location_info.tac_list.clear();
  user_location_info.nid = context.selected_snpn->snpn.nid;
}

} // namespace ocudu::ocucp
