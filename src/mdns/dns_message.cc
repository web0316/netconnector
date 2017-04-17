// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "apps/netconnector/src/mdns/dns_message.h"

namespace netconnector {
namespace mdns {

void DnsHeader::SetResponse(bool value) {
  if (value) {
    flags_ |= kQueryResponseMask;
  } else {
    flags_ &= ~kQueryResponseMask;
  }
}

void DnsHeader::SetOpCode(DnsOpCode op_code) {
  flags_ &= ~kOpCodeMask;
  flags_ |= static_cast<uint16_t>(op_code) << kOpCodeShift;
}

void DnsHeader::SetTruncated(bool value) {
  if (value) {
    flags_ |= kTruncationMask;
  } else {
    flags_ &= ~kTruncationMask;
  }
}

void DnsHeader::SetRecursionDesired(bool value) {
  if (value) {
    flags_ |= kRecursionDesiredMask;
  } else {
    flags_ &= ~kRecursionDesiredMask;
  }
}

void DnsHeader::SetRecursionAvailable(bool value) {
  if (value) {
    flags_ |= kRecursionAvailableMask;
  } else {
    flags_ &= ~kRecursionAvailableMask;
  }
}

void DnsHeader::SetResponseCode(DnsResponseCode response_code) {
  flags_ &= ~kResponseCodeMask;
  flags_ |= static_cast<uint16_t>(response_code);
}

DnsResource::DnsResource(){};

DnsResource::DnsResource(const DnsResource& other) {
  name_ = other.name_;
  type_ = other.type_;
  class_ = other.class_;
  cache_flush_ = other.cache_flush_;
  time_to_live_ = other.time_to_live_;

  switch (type_) {
    case DnsType::kA:
      a_ = other.a_;
      break;
    case DnsType::kNs:
      ns_ = other.ns_;
      break;
    case DnsType::kCName:
      cname_ = other.cname_;
      break;
    case DnsType::kPtr:
      ptr_ = other.ptr_;
      break;
    case DnsType::kTxt:
      txt_ = other.txt_;
      break;
    case DnsType::kAaaa:
      aaaa_ = other.aaaa_;
      break;
    case DnsType::kSrv:
      srv_ = other.srv_;
      break;
    case DnsType::kNSec:
      nsec_ = other.nsec_;
      break;
    default:
      break;
  }
};

DnsResource::~DnsResource() {
  switch (type_) {
    case DnsType::kA:
      a_.~DnsResourceDataA();
      break;
    case DnsType::kNs:
      ns_.~DnsResourceDataNs();
      break;
    case DnsType::kCName:
      cname_.~DnsResourceDataCName();
      break;
    case DnsType::kPtr:
      ptr_.~DnsResourceDataPtr();
      break;
    case DnsType::kTxt:
      txt_.~DnsResourceDataTxt();
      break;
    case DnsType::kAaaa:
      aaaa_.~DnsResourceDataAaaa();
      break;
    case DnsType::kSrv:
      srv_.~DnsResourceDataSrv();
      break;
    case DnsType::kNSec:
      nsec_.~DnsResourceDataNSec();
      break;
    default:
      break;
  }
};

}  // namespace mdns
}  // namespace netconnector
