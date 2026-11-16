// SPDX-FileCopyrightText: Copyright (C) 2021-2026 Software Radio Systems Limited
// SPDX-License-Identifier: BSD-3-Clause-Open-MPI
// Portions of this file may implement 3GPP specifications, which may be subject to additional licensing requirements.

#include "lib/du/du_high/du_manager/converters/asn1_sys_info_packer.h"
#include "ocudu/asn1/rrc_nr/sys_info.h"
#include "ocudu/du/du_cell_config_helpers.h"
#include "ocudu/pcap/mac_pcap.h"
#include "ocudu/ran/snpn_identity.h"
#include <gtest/gtest.h>

using namespace ocudu;
using namespace odu;

// Global variables for PCAP support, required by du_manager_converters.
bool             g_enable_pcap = false;
ocudu::mac_pcap* g_pcap        = nullptr;

static void pack_and_unpack_sib1(const du_cell_config& cell_cfg, asn1::rrc_nr::sib1_s& sib1)
{
  byte_buffer    buf = asn1_packer::pack_sib1(cell_cfg);
  asn1::cbit_ref bref{buf};
  ASSERT_EQ(sib1.unpack(bref), asn1::OCUDUASN_SUCCESS);
}

/// The lab SNPN: 001/01 with the coordinated NID 000007ed9d5, broadcast on TAC 3.
static snpn_broadcast_info make_lab_snpn()
{
  return snpn_broadcast_info{
      {plmn_identity::parse("00101").value(), network_identifier::parse("000007ed9d5").value()}, 3};
}

TEST(network_identifier_test, parses_and_prints_eleven_hex_digits)
{
  auto nid = network_identifier::parse("000007ed9d5");
  ASSERT_TRUE(nid.has_value());
  EXPECT_EQ(nid->value(), 0x000007ed9d5U);
  EXPECT_EQ(nid->to_string(), "000007ed9d5");
  EXPECT_EQ(fmt::format("{}", *nid), "000007ed9d5");
  EXPECT_EQ(network_identifier::parse("FFFFFFFFFFF")->value(), network_identifier::max_value);

  EXPECT_FALSE(network_identifier::parse("7ed9d5").has_value()) << "a NID has exactly 11 digits";
  EXPECT_FALSE(network_identifier::parse("000007ed9d50").has_value());
  EXPECT_FALSE(network_identifier::parse("000007ed9dg").has_value());
  EXPECT_FALSE(network_identifier::from_number(network_identifier::max_value + 1).has_value());
}

TEST(asn1_sib1_snpn_test, plmn_only_cell_broadcasts_no_npn_list)
{
  du_cell_config cell_cfg = config_helpers::make_default_du_cell_config();

  asn1::rrc_nr::sib1_s sib1;
  pack_and_unpack_sib1(cell_cfg, sib1);

  EXPECT_FALSE(sib1.cell_access_related_info.npn_id_info_list_r16.is_present());
}

TEST(asn1_sib1_snpn_test, shared_cell_broadcasts_plmn_and_snpn)
{
  du_cell_config cell_cfg = config_helpers::make_default_du_cell_config();
  cell_cfg.tac            = 1;
  cell_cfg.si.cell_acc_rel_info.snpn_list.push_back(make_lab_snpn());

  asn1::rrc_nr::sib1_s sib1;
  pack_and_unpack_sib1(cell_cfg, sib1);
  const auto& access_info = sib1.cell_access_related_info;

  // The PLMN entry is unchanged and the cell is not reserved: other UEs keep using it.
  ASSERT_EQ(access_info.plmn_id_info_list.size(), 1);
  EXPECT_TRUE(access_info.plmn_id_info_list[0].tac_present);
  EXPECT_EQ(access_info.plmn_id_info_list[0].tac.to_number(), 1U);
  EXPECT_FALSE(access_info.cell_reserved_for_other_use_present);

  ASSERT_TRUE(access_info.npn_id_info_list_r16.is_present()) << "npn-IdentityInfoList did not survive the round trip";
  ASSERT_EQ(access_info.npn_id_info_list_r16->size(), 1);
  const auto& npn_info = (*access_info.npn_id_info_list_r16)[0];
  EXPECT_EQ(npn_info.tac_r16.to_number(), 3U);
  EXPECT_EQ(npn_info.cell_id_r16.to_number(), cell_cfg.nr_cgi.nci.value());
  EXPECT_EQ(npn_info.cell_reserved_for_oper_r16.value,
            asn1::rrc_nr::npn_id_info_r16_s::cell_reserved_for_oper_r16_opts::not_reserved);

  ASSERT_EQ(npn_info.npn_id_list_r16.size(), 1);
  ASSERT_EQ(npn_info.npn_id_list_r16[0].type().value, asn1::rrc_nr::npn_id_r16_c::types_opts::snpn_r16);
  const auto& snpn = npn_info.npn_id_list_r16[0].snpn_r16();
  // Qualcomm modems refuse an NPN PLMN without its MCC ("NPN PLMN mccPresent == 0").
  ASSERT_TRUE(snpn.plmn_id_r16.mcc_present);
  EXPECT_EQ(plmn_identity(mobile_country_code::from_bytes(snpn.plmn_id_r16.mcc).value(),
                          mobile_network_code::from_bytes(snpn.plmn_id_r16.mnc).value()),
            plmn_identity::parse("00101").value());
  ASSERT_EQ(snpn.nid_list_r16.size(), 1);
  EXPECT_EQ(snpn.nid_list_r16[0].to_number(), 0x000007ed9d5U);
}

TEST(asn1_sib1_snpn_test, each_snpn_gets_its_own_entry)
{
  du_cell_config cell_cfg = config_helpers::make_default_du_cell_config();
  cell_cfg.si.cell_acc_rel_info.snpn_list.push_back(make_lab_snpn());
  snpn_broadcast_info second = make_lab_snpn();
  second.snpn.nid            = network_identifier::parse("000007ed9d6").value();
  second.tac                 = 4;
  cell_cfg.si.cell_acc_rel_info.snpn_list.push_back(second);

  asn1::rrc_nr::sib1_s sib1;
  pack_and_unpack_sib1(cell_cfg, sib1);

  ASSERT_TRUE(sib1.cell_access_related_info.npn_id_info_list_r16.is_present());
  const auto& npn_list = *sib1.cell_access_related_info.npn_id_info_list_r16;
  ASSERT_EQ(npn_list.size(), 2);
  EXPECT_EQ(npn_list[0].npn_id_list_r16[0].snpn_r16().nid_list_r16[0].to_number(), 0x000007ed9d5U);
  EXPECT_EQ(npn_list[1].npn_id_list_r16[0].snpn_r16().nid_list_r16[0].to_number(), 0x000007ed9d6U);
  EXPECT_EQ(npn_list[1].tac_r16.to_number(), 4U);
}
