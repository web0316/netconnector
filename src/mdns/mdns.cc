// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <limits>
#include <unordered_set>

#include "apps/netconnector/src/mdns/mdns.h"

#include "apps/netconnector/src/mdns/address_responder.h"
#include "apps/netconnector/src/mdns/dns_formatting.h"
#include "apps/netconnector/src/mdns/host_name_resolver.h"
#include "apps/netconnector/src/mdns/instance_publisher.h"
#include "apps/netconnector/src/mdns/instance_subscriber.h"
#include "apps/netconnector/src/mdns/mdns_addresses.h"
#include "apps/netconnector/src/mdns/mdns_names.h"
#include "apps/netconnector/src/mdns/resource_renewer.h"
#include "lib/ftl/logging.h"
#include "lib/ftl/time/time_delta.h"
#include "lib/mtl/tasks/message_loop.h"

namespace netconnector {
namespace mdns {
namespace {

static constexpr uint32_t kCancelTimeToLive =
    std::numeric_limits<uint32_t>::max();

static const ftl::TimeDelta kMessageAggregationWindowSize =
    ftl::TimeDelta::FromMilliseconds(100);

}  // namespace

Mdns::Mdns() : task_runner_(mtl::MessageLoop::GetCurrent()->task_runner()) {}

Mdns::~Mdns() {}

void Mdns::EnableInterface(const std::string& name, sa_family_t family) {
  transceiver_.EnableInterface(name, family);
}

void Mdns::SetVerbose(bool verbose) {
  verbose_ = verbose;
}

bool Mdns::Start(const std::string& host_name) {
  host_full_name_ = MdnsNames::LocalHostFullName(host_name);

  address_placeholder_ =
      std::make_shared<DnsResource>(host_full_name_, DnsType::kA);

  // Create an address responder agent to respond to simple address queries.
  AddAgent(AddressResponder::kName,
           std::make_shared<AddressResponder>(this, host_full_name_));

  // Create a resource renewer agent to keep resources alive.
  resource_renewer_ = std::make_shared<ResourceRenewer>(this);

  started_ = transceiver_.Start(
      host_full_name_,
      [this](std::unique_ptr<DnsMessage> message,
             const SocketAddress& source_address, uint32_t interface_index) {
        if (verbose_) {
          FTL_LOG(INFO) << "Inbound message from " << source_address
                        << " through interface " << interface_index << ":"
                        << *message;
        }

        for (auto& question : message->questions_) {
          ReceiveQuestion(*question);
        }

        for (auto& resource : message->answers_) {
          ReceiveResource(*resource, MdnsResourceSection::kAnswer);
        }

        for (auto& resource : message->authorities_) {
          ReceiveResource(*resource, MdnsResourceSection::kAuthority);
        }

        for (auto& resource : message->additionals_) {
          ReceiveResource(*resource, MdnsResourceSection::kAdditional);
        }

        resource_renewer_->EndOfMessage();
        for (auto& pair : agents_by_name_) {
          pair.second->EndOfMessage();
        }

        SendMessage();
        PostTask();
      });

  if (started_) {
    for (auto pair : agents_by_name_) {
      pair.second->Start();
    }

    SendMessage();
    PostTask();
  }

  return started_;
}

void Mdns::Stop() {
  transceiver_.Stop();
  started_ = false;
}

void Mdns::ResolveHostName(const std::string& host_name,
                           ftl::TimePoint timeout,
                           const ResolveHostNameCallback& callback) {
  FTL_DCHECK(callback);

  std::string host_full_name = MdnsNames::LocalHostFullName(host_name);

  AddAgent(host_full_name,
           std::make_shared<HostNameResolver>(this, host_name, host_full_name,
                                              timeout, callback));
}

void Mdns::SubscribeToService(const std::string& service_name,
                              const ServiceInstanceCallback& callback) {
  FTL_DCHECK(MdnsNames::IsValidServiceName(service_name));
  FTL_DCHECK(callback);

  std::string service_full_name = MdnsNames::LocalServiceFullName(service_name);

  AddAgent(service_full_name,
           std::make_shared<InstanceSubscriber>(this, service_name,
                                                service_full_name, callback));
}

void Mdns::UnsubscribeToService(const std::string& service_name) {
  FTL_DCHECK(MdnsNames::IsValidServiceName(service_name));

  TellAgentToQuit(MdnsNames::LocalServiceFullName(service_name));
}

void Mdns::PublishServiceInstance(const std::string& service_name,
                                  const std::string& instance_name,
                                  IpPort port,
                                  const std::vector<std::string>& text) {
  FTL_DCHECK(MdnsNames::IsValidServiceName(service_name));

  std::string instance_full_name =
      MdnsNames::LocalInstanceFullName(instance_name, service_name);

  std::string service_full_name = MdnsNames::LocalServiceFullName(service_name);

  AddAgent(instance_full_name, std::make_shared<InstancePublisher>(
                                   this, host_full_name_, instance_full_name,
                                   service_full_name, port, text));
}

void Mdns::UnpublishServiceInstance(const std::string& instance_name,
                                    const std::string& service_name) {
  FTL_DCHECK(MdnsNames::IsValidServiceName(service_name));
  TellAgentToQuit(
      MdnsNames::LocalInstanceFullName(instance_name, service_name));
}

void Mdns::WakeAt(std::shared_ptr<MdnsAgent> agent, ftl::TimePoint when) {
  FTL_DCHECK(agent);
  wake_queue_.emplace(when, agent);
}

void Mdns::SendQuestion(std::shared_ptr<DnsQuestion> question,
                        ftl::TimePoint when) {
  FTL_DCHECK(question);
  question_queue_.emplace(when, question);
}

void Mdns::SendResource(std::shared_ptr<DnsResource> resource,
                        MdnsResourceSection section,
                        ftl::TimePoint when) {
  FTL_DCHECK(resource);

  if (section == MdnsResourceSection::kExpired) {
    // Expirations are distributed to local agents.
    for (auto& pair : agents_by_name_) {
      pair.second->ReceiveResource(*resource, MdnsResourceSection::kExpired);
    }

    return;
  }

  resource_queue_.emplace(when, resource, section);
}

void Mdns::SendAddresses(MdnsResourceSection section, ftl::TimePoint when) {
  // Placeholder for address resource record.
  resource_queue_.emplace(when, address_placeholder_, section);
}

void Mdns::Renew(const DnsResource& resource) {
  resource_renewer_->Renew(resource);
}

void Mdns::RemoveAgent(const std::string& name) {
  agents_by_name_.erase(name);
}

void Mdns::AddAgent(const std::string& name, std::shared_ptr<MdnsAgent> agent) {
  agents_by_name_.emplace(name, agent);
  if (started_) {
    agent->Start();
    SendMessage();
    PostTask();
  }
}

void Mdns::SendMessage() {
  // It's acceptable to send records a bit early, and this provides two
  // advantages:
  // 1) We get more records per message, which is more efficient.
  // 2) Agents can schedule records in short sequences if sequence is important.
  ftl::TimePoint now = ftl::TimePoint::Now() + kMessageAggregationWindowSize;

  DnsMessage message;

  bool empty = true;

  while (!question_queue_.empty() && question_queue_.top().time_ <= now) {
    message.questions_.push_back(question_queue_.top().question_);
    question_queue_.pop();
    empty = false;
  }

  // TODO(dalesat): Fully implement traffic mitigation.
  // For now, we just make sure we don't send the same record instance twice.
  std::unordered_set<DnsResource*> resources_added;

  while (!resource_queue_.empty() && resource_queue_.top().time_ <= now) {
    if (resource_queue_.top().resource_->time_to_live_ == kCancelTimeToLive) {
      // Cancelled while in the queue.
      resource_queue_.pop();
      continue;
    }

    if (resources_added.count(resource_queue_.top().resource_.get()) != 0) {
      // Already added to this message.
      resource_queue_.pop();
      continue;
    }

    resources_added.insert(resource_queue_.top().resource_.get());

    switch (resource_queue_.top().section_) {
      case MdnsResourceSection::kAnswer:
        message.answers_.push_back(resource_queue_.top().resource_);
        break;
      case MdnsResourceSection::kAuthority:
        message.authorities_.push_back(resource_queue_.top().resource_);
        break;
      case MdnsResourceSection::kAdditional:
        message.additionals_.push_back(resource_queue_.top().resource_);
        break;
      case MdnsResourceSection::kExpired:
        FTL_DCHECK(false);
        break;
    }

    resource_queue_.pop();
    empty = false;
  }

  if (empty) {
    return;
  }

  message.UpdateCounts();

  if (message.questions_.empty()) {
    message.header_.SetResponse(true);
    message.header_.SetAuthoritativeAnswer(true);
  }

  if (verbose_) {
    FTL_LOG(INFO) << "Outbound message: " << message;
  }

  // V6 interface transceivers will treat this as |kV6Multicast|.
  transceiver_.SendMessage(&message, MdnsAddresses::kV4Multicast, 0);

  for (auto resource : message.answers_) {
    if (resource->time_to_live_ == 0) {
      resource->time_to_live_ = kCancelTimeToLive;
    }
  }

  for (auto resource : message.authorities_) {
    if (resource->time_to_live_ == 0) {
      resource->time_to_live_ = kCancelTimeToLive;
    }
  }

  for (auto resource : message.additionals_) {
    if (resource->time_to_live_ == 0) {
      resource->time_to_live_ = kCancelTimeToLive;
    }
  }
}

void Mdns::ReceiveQuestion(const DnsQuestion& question) {
  // Renewer doesn't need questions.
  for (auto& pair : agents_by_name_) {
    pair.second->ReceiveQuestion(question);
  }
}

void Mdns::ReceiveResource(const DnsResource& resource,
                           MdnsResourceSection section) {
  // Renewer is always first.
  resource_renewer_->ReceiveResource(resource, section);
  for (auto& pair : agents_by_name_) {
    pair.second->ReceiveResource(resource, section);
  }
}

void Mdns::PostTask() {
  ftl::TimePoint when = ftl::TimePoint::Max();

  if (!wake_queue_.empty()) {
    when = wake_queue_.top().time_;
  }

  if (!question_queue_.empty() && when > question_queue_.top().time_) {
    when = question_queue_.top().time_;
  }

  if (!resource_queue_.empty() && when > resource_queue_.top().time_) {
    when = resource_queue_.top().time_;
  }

  if (when == ftl::TimePoint::Max()) {
    return;
  }

  if (!post_task_queue_.empty() && post_task_queue_.top() <= when) {
    // We're already scheduled to wake up by |when|.
    return;
  }

  post_task_queue_.push(when);

  task_runner_->PostTaskForTime(
      [this, when]() {
        while (!post_task_queue_.empty() && post_task_queue_.top() <= when) {
          post_task_queue_.pop();
        }

        ftl::TimePoint now = ftl::TimePoint::Now();

        while (!wake_queue_.empty() && wake_queue_.top().time_ <= now) {
          std::shared_ptr<MdnsAgent> agent = wake_queue_.top().agent_;
          wake_queue_.pop();
          agent->Wake();
        }

        SendMessage();
        PostTask();
      },
      when);
}

void Mdns::TellAgentToQuit(const std::string& name) {
  auto iter = agents_by_name_.find(name);

  if (iter != agents_by_name_.end()) {
    iter->second->Quit();
  }
}

}  // namespace mdns
}  // namespace netconnector
