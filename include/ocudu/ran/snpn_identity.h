// SPDX-FileCopyrightText: Copyright (C) 2021-2026 Software Radio Systems Limited
// SPDX-License-Identifier: BSD-3-Clause-Open-MPI
// Portions of this file may implement 3GPP specifications, which may be subject to additional licensing requirements.

#pragma once

#include "ocudu/adt/expected.h"
#include "ocudu/ran/plmn_identity.h"
#include "ocudu/ran/tac.h"
#include "fmt/format.h"
#include <cstdint>
#include <optional>
#include <string>

namespace ocudu {

/// \brief Network Identifier (NID) of a Stand-alone Non-Public Network (SNPN), TS 23.003, clause 12.7.
///
/// The NID is 44 bits long: 11 hexadecimal digits, the first being the assignment mode.
class network_identifier
{
public:
  /// Largest value a 44-bit NID can take.
  static constexpr uint64_t max_value = (uint64_t{1} << 44U) - 1U;

  network_identifier() = delete;

  /// Builds a NID from its 44-bit value. Returns an error if the value does not fit in 44 bits.
  static expected<network_identifier> from_number(uint64_t value)
  {
    if (value > max_value) {
      return make_unexpected(default_error_t{});
    }
    return network_identifier{value};
  }

  /// Parses the 11 hexadecimal digits of a NID, e.g. "000007ed9d5" (TS 23.003, clause 12.7.1).
  static expected<network_identifier> parse(const std::string& hex)
  {
    if (hex.size() != 11) {
      return make_unexpected(default_error_t{});
    }
    uint64_t value = 0;
    for (char c : hex) {
      unsigned digit;
      if (c >= '0' and c <= '9') {
        digit = c - '0';
      } else if (c >= 'a' and c <= 'f') {
        digit = c - 'a' + 10;
      } else if (c >= 'A' and c <= 'F') {
        digit = c - 'A' + 10;
      } else {
        return make_unexpected(default_error_t{});
      }
      value = (value << 4U) | digit;
    }
    return network_identifier{value};
  }

  uint64_t value() const { return val; }

  /// The 11 lower-case hexadecimal digits of the NID.
  std::string to_string() const { return fmt::format("{:011x}", val); }

  bool operator==(const network_identifier& rhs) const { return val == rhs.val; }
  bool operator!=(const network_identifier& rhs) const { return val != rhs.val; }

private:
  explicit network_identifier(uint64_t value) : val(value) {}

  uint64_t val;
};

/// Identity of an SNPN: PLMN ID and NID, TS 23.501, clause 5.30.2.1.
struct snpn_identity {
  plmn_identity      plmn;
  network_identifier nid;

  bool operator==(const snpn_identity& rhs) const { return plmn == rhs.plmn and nid == rhs.nid; }
  bool operator!=(const snpn_identity& rhs) const { return not(*this == rhs); }
};

/// An SNPN broadcast by a cell, with the Tracking Area Code the cell broadcasts for it (\c NPN-IdentityInfo,
/// TS 38.331, SIB1).
struct snpn_broadcast_info {
  snpn_identity snpn;
  tac_t         tac;
};

} // namespace ocudu

namespace fmt {

template <>
struct formatter<ocudu::network_identifier> {
  template <typename ParseContext>
  auto parse(ParseContext& ctx)
  {
    return ctx.begin();
  }

  template <typename FormatContext>
  auto format(const ocudu::network_identifier& val, FormatContext& ctx) const
  {
    return format_to(ctx.out(), "{:011x}", val.value());
  }
};

template <>
struct formatter<ocudu::snpn_identity> {
  template <typename ParseContext>
  auto parse(ParseContext& ctx)
  {
    return ctx.begin();
  }

  template <typename FormatContext>
  auto format(const ocudu::snpn_identity& val, FormatContext& ctx) const
  {
    return format_to(ctx.out(), "{}-{}", val.plmn, val.nid);
  }
};

} // namespace fmt
