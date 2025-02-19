// Copyright 2021 The Ray Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//  http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "ray/gcs/gcs_server/pubsub_handler.h"

namespace ray {
namespace gcs {

// Needs to use rpc::GcsSubscriberPollRequest and rpc::GcsSubscriberPollReply here,
// and convert the reply to rpc::PubsubLongPollingReply because GCS RPC services are
// required to have the `status` field in replies.
void InternalPubSubHandler::HandleGcsSubscriberPoll(
    const rpc::GcsSubscriberPollRequest &request, rpc::GcsSubscriberPollReply *reply,
    rpc::SendReplyCallback send_reply_callback) {
  const auto subscriber_id = UniqueID::FromBinary(request.subscriber_id());
  auto pubsub_reply = std::make_shared<rpc::PubsubLongPollingReply>();
  auto pubsub_reply_ptr = pubsub_reply.get();
  gcs_publisher_->GetPublisher()->ConnectToSubscriber(
      subscriber_id, pubsub_reply_ptr,
      [reply, reply_cb = std::move(send_reply_callback),
       pubsub_reply = std::move(pubsub_reply)](ray::Status status,
                                               std::function<void()> success_cb,
                                               std::function<void()> failure_cb) {
        reply->mutable_pub_messages()->Swap(pubsub_reply->mutable_pub_messages());
        reply_cb(std::move(status), std::move(success_cb), std::move(failure_cb));
      });
}

// Similar for HandleGcsSubscriberPoll() above, needs to use
// rpc::GcsSubscriberCommandBatchReply as reply type instead of using
// rpc::GcsSubscriberCommandBatchReply directly.
void InternalPubSubHandler::HandleGcsSubscriberCommandBatch(
    const rpc::GcsSubscriberCommandBatchRequest &request,
    rpc::GcsSubscriberCommandBatchReply *reply,
    rpc::SendReplyCallback send_reply_callback) {
  const auto subscriber_id = UniqueID::FromBinary(request.subscriber_id());
  for (const auto &command : request.commands()) {
    if (command.has_unsubscribe_message()) {
      gcs_publisher_->GetPublisher()->UnregisterSubscription(
          command.channel_type(), subscriber_id,
          command.key_id().empty() ? std::nullopt : std::make_optional(command.key_id()));
    } else if (command.has_subscribe_message()) {
      gcs_publisher_->GetPublisher()->RegisterSubscription(
          command.channel_type(), subscriber_id,
          command.key_id().empty() ? std::nullopt : std::make_optional(command.key_id()));
    } else {
      RAY_LOG(FATAL) << "Invalid command has received, "
                     << static_cast<int>(command.command_message_one_of_case())
                     << ". If you see this message, please file an issue to Ray Github.";
    }
  }
  send_reply_callback(Status::OK(), nullptr, nullptr);
}

}  // namespace gcs
}  // namespace ray
