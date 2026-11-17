// SPDX-FileCopyrightText: Copyright (C) 2021-2026 Software Radio Systems Limited
// SPDX-License-Identifier: BSD-3-Clause-Open-MPI
// Portions of this file may implement 3GPP specifications, which may be subject to additional licensing requirements.

#include "lib/rrc/rrc_sib1_npn.h"
#include <gtest/gtest.h>

using namespace ocudu;
using namespace ocucp;
using namespace asn1::rrc_nr;

namespace {

npn_id_info_r16_s make_snpn_entry(const std::vector<uint64_t>& nids, unsigned tac)
{
  npn_id_info_r16_s info;
  npn_id_r16_c      id;
  auto&             snpn = id.set_snpn_r16();
  snpn.plmn_id_r16.mcc_present = true;
  snpn.plmn_id_r16.mcc         = {0, 0, 1};
  snpn.plmn_id_r16.mnc.resize(2);
  snpn.plmn_id_r16.mnc[0] = 0;
  snpn.plmn_id_r16.mnc[1] = 1;
  for (uint64_t nid : nids) {
    asn1::fixed_bitstring<44> asn1_nid;
    asn1_nid.from_number(nid);
    snpn.nid_list_r16.push_back(asn1_nid);
  }
  info.npn_id_list_r16.push_back(id);
  info.tac_r16.from_number(tac);
  info.cell_id_r16.from_number(0x66c000);
  return info;
}

npn_id_info_r16_s make_cag_entry(unsigned nof_cag_ids, unsigned tac)
{
  npn_id_info_r16_s info;
  npn_id_r16_c      id;
  auto&             pni = id.set_pni_npn_r16();
  pni.plmn_id_r16.mcc_present = true;
  pni.plmn_id_r16.mcc         = {0, 0, 1};
  pni.plmn_id_r16.mnc.resize(2);
  pni.plmn_id_r16.mnc[0] = 0;
  pni.plmn_id_r16.mnc[1] = 1;
  for (unsigned i = 0; i != nof_cag_ids; ++i) {
    cag_id_info_r16_s cag;
    cag.cag_id_r16.from_number(i + 1);
    pni.cag_id_list_r16.push_back(cag);
  }
  info.npn_id_list_r16.push_back(id);
  info.tac_r16.from_number(tac);
  info.cell_id_r16.from_number(0x66c000);
  return info;
}

class rrc_sib1_npn_test : public ::testing::Test
{
protected:
  static void SetUpTestSuite() { ocudulog::init(); }

  rrc_sib1_npn_info extract() { return extract_sib1_npn_info(sib1, nr_cell_identity::create(0x66c000).value(), logger); }

  sib1_s                  sib1;
  ocudulog::basic_logger& logger = ocudulog::fetch_basic_logger("RRC");
};

} // namespace

TEST_F(rrc_sib1_npn_test, cell_without_npn_list_has_no_npn)
{
  const rrc_sib1_npn_info info = extract();
  EXPECT_TRUE(info.npn_list.empty());
  EXPECT_FALSE(info.npn_only);
}

TEST_F(rrc_sib1_npn_test, shared_cell_is_not_npn_only)
{
  sib1.cell_access_related_info.npn_id_info_list_r16.set_present();
  sib1.cell_access_related_info.npn_id_info_list_r16->push_back(make_snpn_entry({0x000007ed9d5U}, 3));

  const rrc_sib1_npn_info info = extract();
  ASSERT_EQ(info.npn_list.size(), 1);
  ASSERT_TRUE(info.npn_list[0].has_value());
  EXPECT_EQ(info.npn_list[0]->snpn.nid.value(), 0x000007ed9d5U);
  EXPECT_EQ(info.npn_list[0]->tac, 3U);
  EXPECT_FALSE(info.npn_only);
}

TEST_F(rrc_sib1_npn_test, reserved_cell_with_npn_list_is_npn_only)
{
  sib1.cell_access_related_info.cell_reserved_for_other_use_present = true;
  sib1.cell_access_related_info.npn_id_info_list_r16.set_present();
  sib1.cell_access_related_info.npn_id_info_list_r16->push_back(make_snpn_entry({0x000007ed9d5U}, 3));

  EXPECT_TRUE(extract().npn_only);
}

TEST_F(rrc_sib1_npn_test, reserved_cell_without_npn_list_is_not_npn_only)
{
  sib1.cell_access_related_info.cell_reserved_for_other_use_present = true;
  EXPECT_FALSE(extract().npn_only);
}

TEST_F(rrc_sib1_npn_test, each_nid_takes_an_index_but_a_cag_entry_takes_one)
{
  // TS 38.331, npn-IdentityInfoList: e(i) is i for the NIDs of an SNPN entry, 1 for a PNI-NPN entry.
  sib1.cell_access_related_info.npn_id_info_list_r16.set_present();
  sib1.cell_access_related_info.npn_id_info_list_r16->push_back(make_cag_entry(3, 5));
  sib1.cell_access_related_info.npn_id_info_list_r16->push_back(make_snpn_entry({0x1U, 0x2U}, 3));

  const rrc_sib1_npn_info info = extract();
  ASSERT_EQ(info.npn_list.size(), 3);
  EXPECT_FALSE(info.npn_list[0].has_value()) << "the CAG entry is one unsupported index value";
  ASSERT_TRUE(info.npn_list[1].has_value());
  EXPECT_EQ(info.npn_list[1]->snpn.nid.value(), 0x1U);
  ASSERT_TRUE(info.npn_list[2].has_value());
  EXPECT_EQ(info.npn_list[2]->snpn.nid.value(), 0x2U);
}
