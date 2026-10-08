// SPDX-FileCopyrightText: Copyright (C) 2021-2026 Software Radio Systems Limited
// SPDX-License-Identifier: BSD-3-Clause-Open-MPI
// Portions of this file may implement 3GPP specifications, which may be subject to additional licensing requirements.

#include "ngap_test_helpers.h"
#include "tests/test_doubles/utils/test_rng.h"
#include "ocudu/adt/format.h"
#include "ocudu/asn1/ngap/ngap_pdu_contents.h"
#include "ocudu/ran/cu_types.h"
#include "ocudu/ran/rb_id.h"
#include <gtest/gtest.h>

using namespace ocudu;
using namespace ocucp;

class ngap_pdu_session_resource_modify_procedure_test : public ngap_test
{
protected:
  cu_cp_ue_index_t start_procedure(pdu_session_id_t pdu_session_id)
  {
    cu_cp_ue_index_t ue_index = create_ue();

    // Inject DL NAS transport message from AMF
    run_dl_nas_transport(ue_index);

    // Inject UL NAS transport message from RRC
    run_ul_nas_transport(ue_index);

    // Inject Initial Context Setup Request
    run_initial_context_setup(ue_index);

    // Inject PDU Session Resource Setup Request
    run_pdu_session_resource_setup(ue_index, pdu_session_id);
    add_pdu_session_to_up_manager(
        ue_index,
        pdu_session_id,
        pdu_session_type_t::ipv4,
        up_transport_layer_info{transport_layer_address::create_from_string("127.0.0.1"), int_to_gtpu_teid(1)},
        uint_to_drb_id(1),
        uint_to_qos_flow_id(1));

    return ue_index;
  }

  bool was_conversion_successful(ngap_message     pdu_session_resource_modify_request,
                                 pdu_session_id_t pdu_session_id) const
  {
    bool test_1 = pdu_session_resource_modify_request.pdu.init_msg()
                      .value.pdu_session_res_modify_request()
                      ->pdu_session_res_modify_list_mod_req.size() ==
                  cu_cp_notifier.last_modify_request.pdu_session_res_modify_items.size();

    bool test_2 = cu_cp_notifier.last_modify_request.pdu_session_res_modify_items[pdu_session_id].pdu_session_id ==
                  pdu_session_id;

    return test_1 && test_2;
  }

  bool was_pdu_session_resource_modify_request_valid() const
  {
    // Check that AMF notifier was called with right type
    bool test_1 = n2_gw.last_ngap_msgs.back().pdu.successful_outcome().value.type() ==
                  asn1::ngap::ngap_elem_procs_o::successful_outcome_c::types_opts::pdu_session_res_modify_resp;

    // Check that response contains PDU Session Resource Modify List
    bool test_2 = n2_gw.last_ngap_msgs.back()
                      .pdu.successful_outcome()
                      .value.pdu_session_res_modify_resp()
                      ->pdu_session_res_modify_list_mod_res_present;

    return test_1 && test_2;
  }

  bool was_pdu_session_resource_modify_request_invalid() const
  {
    // Check that a UE release was requested from the AMF
    bool test_1 = n2_gw.last_ngap_msgs.back().pdu.init_msg().value.type() ==
                  asn1::ngap::ngap_elem_procs_o::init_msg_c::types_opts::ue_context_release_request;

    // Check that AMF notifier was called with right type
    bool test_2 = n2_gw.last_ngap_msgs.end()[-2].pdu.successful_outcome().value.type() ==
                  asn1::ngap::ngap_elem_procs_o::successful_outcome_c::types_opts::pdu_session_res_modify_resp;

    // Check that response doesn't contain PDU Session Resource Modify List
    bool test_3 = !n2_gw.last_ngap_msgs.end()[-2]
                       .pdu.successful_outcome()
                       .value.pdu_session_res_modify_resp()
                       ->pdu_session_res_modify_list_mod_res_present;

    // Check that response contains PDU Session Resource Failed to Modify List
    bool test_4 = n2_gw.last_ngap_msgs.end()[-2]
                      .pdu.successful_outcome()
                      .value.pdu_session_res_modify_resp()
                      ->pdu_session_res_failed_to_modify_list_mod_res_present;

    return test_1 && test_2 && test_3 && test_4;
  }

  bool was_error_indication_sent() const
  {
    return n2_gw.last_ngap_msgs.back().pdu.init_msg().value.type() ==
           asn1::ngap::ngap_elem_procs_o::init_msg_c::types_opts::error_ind;
  }
};

/// Test valid PDU Session Resource Modify Request
TEST_F(ngap_pdu_session_resource_modify_procedure_test,
       when_valid_pdu_session_resource_modify_request_received_then_pdu_session_modification_succeeds)
{
  // Test preamble
  pdu_session_id_t pdu_session_id = uint_to_pdu_session_id(
      test_rng::uniform_int<uint16_t>(to_underlying(pdu_session_id_t::min), to_underlying(pdu_session_id_t::max)));
  cu_cp_ue_index_t ue_index = this->start_procedure(pdu_session_id);
  auto&            ue       = test_ues.at(ue_index);

  // Inject PDU Session Resource Modify Request
  ngap_message pdu_session_resource_modify_request = generate_valid_pdu_session_resource_modify_request_message(
      ue.amf_ue_id.value(), ue.ran_ue_id.value(), pdu_session_id);
  ngap->handle_message(pdu_session_resource_modify_request);

  // Check conversion in adapter
  ASSERT_TRUE(was_conversion_successful(pdu_session_resource_modify_request, pdu_session_id));

  // Check that PDU Session Resource Modify Request was valid
  ASSERT_TRUE(was_pdu_session_resource_modify_request_valid());
}

/// Test that the GBR QoS Flow Information of a QoS flow added by a PDU Session Resource Modify Request is forwarded.
TEST_F(ngap_pdu_session_resource_modify_procedure_test,
       when_pdu_session_resource_modify_request_adds_gbr_qos_flow_then_gbr_qos_flow_information_is_forwarded)
{
  // Test preamble
  pdu_session_id_t pdu_session_id = uint_to_pdu_session_id(
      test_rng::uniform_int<uint16_t>(to_underlying(pdu_session_id_t::min), to_underlying(pdu_session_id_t::max)));
  cu_cp_ue_index_t ue_index = this->start_procedure(pdu_session_id);
  auto&            ue       = test_ues.at(ue_index);

  // Inject PDU Session Resource Modify Request adding a GBR QoS flow
  qos_flow_id_t qos_flow_id                         = uint_to_qos_flow_id(2);
  ngap_message  pdu_session_resource_modify_request =
      generate_valid_pdu_session_resource_modify_request_with_gbr_qos_flow_message(
          ue.amf_ue_id.value(), ue.ran_ue_id.value(), pdu_session_id, qos_flow_id);
  ngap->handle_message(pdu_session_resource_modify_request);

  // Check conversion in adapter
  ASSERT_TRUE(was_conversion_successful(pdu_session_resource_modify_request, pdu_session_id));

  // Check that the QoS flow level QoS parameters, including the GBR QoS Flow Information, were forwarded
  const auto& modify_item = cu_cp_notifier.last_modify_request.pdu_session_res_modify_items[pdu_session_id];
  ASSERT_TRUE(modify_item.transfer.qos_flow_add_or_modify_request_list.contains(qos_flow_id));
  const qos_flow_level_qos_parameters& qos_params =
      modify_item.transfer.qos_flow_add_or_modify_request_list[qos_flow_id].qos_flow_level_qos_params;
  ASSERT_FALSE(qos_params.qos_desc.is_dyn_5qi());
  ASSERT_EQ(qos_params.qos_desc.get_5qi(), uint_to_five_qi(1));
  ASSERT_EQ(qos_params.alloc_retention_prio.prio_level_arp, 1);
  ASSERT_TRUE(qos_params.gbr_qos_info.has_value());
  ASSERT_EQ(qos_params.gbr_qos_info->max_br_dl, 41000);
  ASSERT_EQ(qos_params.gbr_qos_info->max_br_ul, 49000);
  ASSERT_EQ(qos_params.gbr_qos_info->gbr_dl, 41000);
  ASSERT_EQ(qos_params.gbr_qos_info->gbr_ul, 49000);

  // Check that PDU Session Resource Modify Request was valid
  ASSERT_TRUE(was_pdu_session_resource_modify_request_valid());
}

/// Test invalid PDU Session Resource Modify Request
TEST_F(ngap_pdu_session_resource_modify_procedure_test,
       when_invalid_pdu_session_resource_modify_request_received_then_pdu_session_modification_failed)
{
  // Test preamble
  pdu_session_id_t pdu_session_id = uint_to_pdu_session_id(
      test_rng::uniform_int<uint16_t>(to_underlying(pdu_session_id_t::min), to_underlying(pdu_session_id_t::max)));
  cu_cp_ue_index_t ue_index = this->start_procedure(pdu_session_id);
  auto&            ue       = test_ues.at(ue_index);

  // Inject invalid PDU Session Resource Modify Request
  ngap_message pdu_session_resource_modify_request = generate_invalid_pdu_session_resource_modify_request_message(
      ue.amf_ue_id.value(), ue.ran_ue_id.value(), pdu_session_id);
  ngap->handle_message(pdu_session_resource_modify_request);

  // Check that PDU Session Resource Modify Request was invalid
  ASSERT_TRUE(was_pdu_session_resource_modify_request_invalid());
}

/// Test valid PDU Session Resource Modify Request
TEST_F(ngap_pdu_session_resource_modify_procedure_test,
       when_valid_pdu_session_resource_modify_request_received_twice_then_error_indication_is_send)
{
  // Test preamble
  pdu_session_id_t pdu_session_id = uint_to_pdu_session_id(
      test_rng::uniform_int<uint16_t>(to_underlying(pdu_session_id_t::min), to_underlying(pdu_session_id_t::max)));
  cu_cp_ue_index_t ue_index = this->start_procedure(pdu_session_id);
  auto&            ue       = test_ues.at(ue_index);

  // Inject PDU Session Resource Modify Request
  ngap_message pdu_session_resource_modify_request = generate_valid_pdu_session_resource_modify_request_message(
      ue.amf_ue_id.value(), ue.ran_ue_id.value(), pdu_session_id);
  ngap->handle_message(pdu_session_resource_modify_request);

  // Check conversion in adapter
  ASSERT_TRUE(was_conversion_successful(pdu_session_resource_modify_request, pdu_session_id));

  // Check that PDU Session Resource Modify Request was valid
  ASSERT_TRUE(was_pdu_session_resource_modify_request_valid());

  // Inject same PDU Session Resource Modify Request again
  ngap->handle_message(pdu_session_resource_modify_request);

  ASSERT_TRUE(was_error_indication_sent());
}
