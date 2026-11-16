// SPDX-FileCopyrightText: Copyright (C) 2021-2026 Software Radio Systems Limited
// SPDX-License-Identifier: BSD-3-Clause-Open-MPI
// Portions of this file may implement 3GPP specifications, which may be subject to additional licensing requirements.

#include "rrc_ue_test_helpers.h"
#include "ocudu/adt/format.h"
#include <gtest/gtest.h>

using namespace ocudu;
using namespace ocucp;

/// Fixture class RRC Setup tests preparation.
class rrc_ue_setup : public rrc_ue_test_helper, public ::testing::Test
{
protected:
  static void SetUpTestSuite() { ocudulog::init(); }

  void SetUp() override { init(); }

  void TearDown() override
  {
    // Flush logger after each test.
    ocudulog::flush();
  }
};

/// Test the RRC setup with disconnected AMF.
TEST_F(rrc_ue_setup, when_amf_disconnected_then_rrc_reject_sent)
{
  this->rrc_ue_cu_cp_notifier.next_ue_setup_response = false;

  receive_setup_request();

  check_ue_release_requested();
}

/// Test the RRC setup with connected AMF.
TEST_F(rrc_ue_setup, when_amf_connected_then_rrc_setup_sent)
{
  receive_setup_request();

  // Check if the RRC setup message was generated.
  ASSERT_EQ(get_srb0_pdu_type(), asn1::rrc_nr::dl_ccch_msg_type_c::c1_c_::types::rrc_setup);

  // Check if SRB1 was created.
  check_srb1_exists();

  receive_setup_complete();
}

/// Test the RRC setup with disconnected AMF.
TEST_F(rrc_ue_setup, when_rrc_setup_invalid_then_rrc_reject_sent)
{
  receive_invalid_setup_request();

  check_ue_release_requested();
}

/// Test the correct handling of missing RRC setup complete message.
TEST_F(rrc_ue_setup, when_setup_complete_timeout_then_ue_deleted)
{
  receive_setup_request();

  // Check that UE has been created and was not requested to be released.
  check_ue_release_not_requested();

  // Tick timer until RRC setup complete timer fires.
  tick_timer();

  // Verify that RRC requested UE context release.
  check_ue_release_requested();
}

TEST_F(rrc_ue_setup, when_setup_complete_received_initial_ue_message_sent)
{
  receive_setup_request();

  // Check if the RRC setup message was generated.
  ASSERT_EQ(get_srb0_pdu_type(), asn1::rrc_nr::dl_ccch_msg_type_c::c1_c_::types::rrc_setup);

  // Check if SRB1 was created.
  check_srb1_exists();

  receive_setup_complete();

  check_initial_ue_message_sent();
}

/// Test the correct handling of corrupted RRC setup complete message.
TEST_F(rrc_ue_setup, when_integrity_failure_detected_then_ue_deleted)
{
  receive_setup_request();

  // Check if the RRC setup message was generated.
  ASSERT_EQ(get_srb0_pdu_type(), asn1::rrc_nr::dl_ccch_msg_type_c::c1_c_::types::rrc_setup);

  // Check if SRB1 was created.
  check_srb1_exists();

  receive_corrupted_setup_complete();

  // Tick timer until RRC setup complete timer fires.
  tick_timer();

  // Verify that RRC requested UE context release.
  check_ue_release_requested();
}

/// Test the correct handling of corrupted RRC setup complete message.
TEST_F(rrc_ue_setup, when_malformed_setup_complete_received_then_ue_deleted)
{
  receive_setup_request();

  // Check if the RRC setup message was generated.
  ASSERT_EQ(get_srb0_pdu_type(), asn1::rrc_nr::dl_ccch_msg_type_c::c1_c_::types::rrc_setup);

  // Check if SRB1 was created.
  check_srb1_exists();

  // UL-DCCH with malformed RRC setup complete message.
  std::array<uint8_t, 28> malformed_rrc_setup_complete_pdu = {
      0x00, 0x00, 0x18, 0x00, 0x05, 0xdf, 0x80, 0x10, 0x5e, 0x40, 0x03, 0x40, 0x40, 0x3c,
      0x44, 0x00, 0x00, 0x00, 0x00, 0x04, 0x0c, 0x95, 0x1d, 0x82, 0x0b, 0x80, 0xbc, 0x1c};

  // Inject malformed RRC setup complete.
  pdcp_ctx->handle_ul_dcch_pdu(srb_id_t::srb1, byte_buffer::create(malformed_rrc_setup_complete_pdu).value());
  // Tick timer until RRC setup complete timer fires.
  tick_timer();

  // Verify that RRC requested UE context release.
  check_ue_release_requested();
}

/// RRC UE on a shared cell: PLMN 00101 (TAC 7) in plmn-IdentityInfoList, SNPN 00101 + NID 000007ed9d5 (TAC 3) in
/// npn-IdentityInfoList, then a PNI-NPN (CAG) entry, which is not supported.
class rrc_ue_setup_snpn : public rrc_ue_test_helper, public ::testing::Test
{
protected:
  static void SetUpTestSuite() { ocudulog::init(); }

  void SetUp() override
  {
    rrc_ue_test_cell_params params;
    params.tac = 7;
    params.npn_list.emplace_back(snpn_broadcast_info{
        {plmn_identity::test_value(), network_identifier::parse("000007ed9d5").value()}, 3});
    params.npn_list.emplace_back(std::nullopt);
    init(params);

    receive_setup_request();
    ASSERT_EQ(get_srb0_pdu_type(), asn1::rrc_nr::dl_ccch_msg_type_c::c1_c_::types::rrc_setup);
  }

  void TearDown() override { ocudulog::flush(); }
};

TEST_F(rrc_ue_setup_snpn, when_ue_selects_the_plmn_of_a_shared_cell_then_uli_has_no_nid)
{
  receive_setup_complete_with_selected_plmn(1);

  check_initial_ue_message_sent();
  const auto& uli = rrc_ue_ngap_notifier.last_init_ue_uli;
  ASSERT_TRUE(uli.has_value());
  EXPECT_EQ(uli->tai.plmn_id, plmn_identity::test_value());
  EXPECT_EQ(uli->tai.tac, 7U);
  EXPECT_FALSE(uli->nid.has_value());
}

TEST_F(rrc_ue_setup_snpn, when_ue_selects_the_snpn_then_uli_names_the_snpn_tac_and_nid)
{
  // TS 38.331: the NPN index counts on from the single PLMN index value, so the SNPN is 2.
  receive_setup_complete_with_selected_plmn(2);

  check_initial_ue_message_sent();
  const auto& uli = rrc_ue_ngap_notifier.last_init_ue_uli;
  ASSERT_TRUE(uli.has_value());
  EXPECT_EQ(uli->tai.plmn_id, plmn_identity::test_value());
  EXPECT_EQ(uli->nr_cgi.plmn_id, plmn_identity::test_value());
  EXPECT_EQ(uli->tai.tac, 3U) << "an SNPN UE is reported in the TA the cell broadcasts for the SNPN";
  ASSERT_TRUE(uli->nid.has_value());
  EXPECT_EQ(uli->nid->value(), 0x000007ed9d5U);

  // Later ULIs of the UE, e.g. UL NAS Transport, go through the same hook.
  cu_cp_user_location_info_nr later_uli;
  later_uli.tai = {plmn_identity::test_value(), 7};
  rrc_ue->fill_ue_derived_location(later_uli);
  EXPECT_EQ(later_uli.tai.tac, 3U);
  ASSERT_TRUE(later_uli.nid.has_value());
  EXPECT_EQ(later_uli.nid->value(), 0x000007ed9d5U);
}

TEST_F(rrc_ue_setup_snpn, when_ue_selects_an_unsupported_cag_entry_then_ue_is_released)
{
  receive_setup_complete_with_selected_plmn(3);

  ASSERT_FALSE(rrc_ue_ngap_notifier.initial_ue_msg_received);
  check_ue_release_requested();
}

TEST_F(rrc_ue_setup_snpn, when_ue_selects_an_index_past_every_npn_then_ue_is_released)
{
  receive_setup_complete_with_selected_plmn(4);

  ASSERT_FALSE(rrc_ue_ngap_notifier.initial_ue_msg_received);
  check_ue_release_requested();
}
