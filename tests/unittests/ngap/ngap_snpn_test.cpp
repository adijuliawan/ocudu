// SPDX-FileCopyrightText: Copyright (C) 2021-2026 Software Radio Systems Limited
// SPDX-License-Identifier: BSD-3-Clause-Open-MPI
// Portions of this file may implement 3GPP specifications, which may be subject to additional licensing requirements.

#include "lib/ngap/ngap_asn1_converters.h"
#include "lib/ngap/ngap_asn1_helpers.h"
#include "ocudu/asn1/ngap/common.h"
#include "ocudu/asn1/ngap/ngap.h"
#include <gtest/gtest.h>

using namespace ocudu;
using namespace ocucp;

namespace {

/// Fills an NG Setup Request for the given tracking areas, and returns it as decoded by the AMF.
asn1::ngap::ng_setup_request_s snpn_pack_and_unpack_ng_setup_request(std::vector<supported_tracking_area> tas)
{
  ngap_context_t ctxt = {.gnb_id = {411, 22}, .ran_node_name = "tstgnb01", .supported_tas = std::move(tas)};

  asn1::ngap::ngap_pdu_c pdu;
  pdu.set_init_msg().load_info_obj(ASN1_NGAP_ID_NG_SETUP);
  fill_asn1_ng_setup_request(pdu.init_msg().value.ng_setup_request(), ctxt);

  byte_buffer buf;
  {
    asn1::bit_ref bref{buf};
    EXPECT_EQ(pdu.pack(bref), asn1::OCUDUASN_SUCCESS);
  }
  asn1::ngap::ngap_pdu_c decoded;
  {
    asn1::cbit_ref bref{buf};
    EXPECT_EQ(decoded.unpack(bref), asn1::OCUDUASN_SUCCESS);
  }
  return decoded.init_msg().value.ng_setup_request();
}

const network_identifier lab_nid = network_identifier::parse("000007ed9d5").value();

} // namespace

TEST(ngap_snpn_test, ng_setup_names_the_plmn_and_the_snpn_in_their_own_tracking_areas)
{
  // PLMN 00101 on TAC 1; SNPN 00101 + NID 000007ed9d5 on TAC 3 with slice 1-000001.
  const s_nssai_t   slice{slice_service_type{1}, slice_differentiator::create(0x000001).value()};
  plmn_item         snpn_item{plmn_identity::test_value(), {slice}};
  snpn_item.nid = lab_nid;
  std::vector<supported_tracking_area> tas = {{1, {{plmn_identity::test_value(), {slice}}}, std::nullopt},
                                              {3, {snpn_item}, std::nullopt}};

  asn1::ngap::ng_setup_request_s req = snpn_pack_and_unpack_ng_setup_request(tas);

  ASSERT_EQ(req->supported_ta_list.size(), 2);
  const auto& plmn_ta = req->supported_ta_list[0];
  EXPECT_EQ(plmn_ta.tac.to_number(), 1U);
  ASSERT_EQ(plmn_ta.broadcast_plmn_list.size(), 1);
  EXPECT_FALSE(plmn_ta.broadcast_plmn_list[0].ie_exts_present) << "a PLMN item carries no NPN Support";

  const auto& snpn_ta = req->supported_ta_list[1];
  EXPECT_EQ(snpn_ta.tac.to_number(), 3U);
  ASSERT_EQ(snpn_ta.broadcast_plmn_list.size(), 1);
  const auto& item = snpn_ta.broadcast_plmn_list[0];
  EXPECT_EQ(plmn_identity::from_bytes(item.plmn_id.to_bytes()).value(), plmn_identity::test_value());
  ASSERT_TRUE(item.ie_exts_present);
  ASSERT_TRUE(item.ie_exts.npn_support_present);
  ASSERT_EQ(item.ie_exts.npn_support.type().value, asn1::ngap::npn_support_c::types_opts::sn_pn);
  EXPECT_EQ(item.ie_exts.npn_support.sn_pn().to_number(), 0x000007ed9d5U);
  ASSERT_EQ(item.tai_slice_support_list.size(), 1);
  EXPECT_EQ(item.tai_slice_support_list[0].s_nssai.sst.to_number(), 1U);
}

TEST(ngap_snpn_test, uli_of_a_plmn_ue_carries_no_nid)
{
  cu_cp_user_location_info_nr uli;
  uli.nr_cgi = {plmn_identity::test_value(), nr_cell_identity::create(0x66c000).value()};
  uli.tai    = {plmn_identity::test_value(), 1};

  asn1::ngap::user_location_info_nr_s asn1_uli = cu_cp_user_location_info_to_asn1(uli);

  EXPECT_FALSE(asn1_uli.ie_exts_present and asn1_uli.ie_exts.n_id_present);
}

TEST(ngap_snpn_test, uli_of_an_snpn_ue_carries_its_nid)
{
  cu_cp_user_location_info_nr uli;
  uli.nr_cgi = {plmn_identity::test_value(), nr_cell_identity::create(0x66c000).value()};
  uli.tai    = {plmn_identity::test_value(), 3};
  uli.nid    = lab_nid;

  asn1::ngap::user_location_info_nr_s asn1_uli = cu_cp_user_location_info_to_asn1(uli);

  ASSERT_TRUE(asn1_uli.ie_exts_present);
  ASSERT_TRUE(asn1_uli.ie_exts.n_id_present);
  EXPECT_EQ(asn1_uli.ie_exts.n_id.to_number(), 0x000007ed9d5U);
  EXPECT_EQ(asn1_uli.tai.tac.to_number(), 3U);
  EXPECT_FALSE(asn1_uli.ie_exts.nr_ntn_tai_info_present);

  // The IE survives encoding, as the AMF reads it (id-NID in UserLocationInformationNR-ExtIEs).
  byte_buffer buf;
  {
    asn1::bit_ref bref{buf};
    ASSERT_EQ(asn1_uli.pack(bref), asn1::OCUDUASN_SUCCESS);
  }
  asn1::ngap::user_location_info_nr_s decoded;
  {
    asn1::cbit_ref bref{buf};
    ASSERT_EQ(decoded.unpack(bref), asn1::OCUDUASN_SUCCESS);
  }
  ASSERT_TRUE(decoded.ie_exts_present and decoded.ie_exts.n_id_present);
  EXPECT_EQ(decoded.ie_exts.n_id.to_number(), 0x000007ed9d5U);
}
