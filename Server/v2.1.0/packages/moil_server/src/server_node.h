#pragma once

#include <memory>
#include <string>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

#include <rclcpp/rclcpp.hpp>
#include <rclcpp_action/rclcpp_action.hpp>

struct ServerContext;

// Common ground for the eight nodes: the context, the callback group, and the
// two registration helpers.
//
// WHAT THIS REPLACES, AND WHY IT IS WORTH A BASE CLASS.
//
// Every node used to declare one member per service --
//
//     rclcpp::Service<ShowPattern>::SharedPtr srvShow_;
//
// -- thirty-seven of them across the eight nodes, and not one was ever read
// again. They existed solely because create_service returns a handle that must
// outlive the call, and dropping it silently unadvertises the service: the node
// still appears in `ros2 node list` and the service simply is not there. So the
// members were load-bearing, invisible, and indistinguishable from state.
// handles_ below keeps them alive in one place, which is what they were for.
//
// Each registration also repeated the QoS and the callback group at the call
// site. Both must be the same for every service on a node -- the group is what
// makes the multi-threaded executor able to serve a Stop while a drive-to-limit
// is running -- and repeating a value that must not vary is an invitation to
// vary it. They are set once, here.
//
// THE CALLBACKS TAKE REFERENCES, NOT SHARED POINTERS.
//
// rclcpp hands over std::shared_ptr<Request> and std::shared_ptr<Response>, and
// spelling those out made a two-line handler into a five-line one. Neither
// pointer can be null and neither is stored past the call, so the pointer buys
// nothing a reference does not give more plainly. addService unwraps them.

class ServerNode : public rclcpp::Node {
protected:
    // `type` is Reentrant for six of the eight nodes: several callbacks may be in
    // flight at once, and what makes that safe is that the device classes were
    // already built for it -- see the threading note on ServerContext.
    //
    // The two that pass MutuallyExclusive are not being cautious, they are keeping
    // what they had. The supervisor's timers must not overlap: tick() reads every
    // subsystem and publishes one status, and two of those interleaved publish a
    // row from one sweep beside a row from another. The axis put its services on
    // the node's DEFAULT group, which rclcpp makes mutually exclusive -- its own
    // reentrant group is jobGroup_, and only the actions and timers are on it.
    ServerNode(const char *name, ServerContext &ctx, const rclcpp::NodeOptions &options,
               rclcpp::CallbackGroupType type = rclcpp::CallbackGroupType::Reentrant)
        : rclcpp::Node(name, options),
          ctx_(ctx),
          group_(create_callback_group(type)) {}

    // Advertise a service. `handler` is either a lambda or a pointer to a method
    // of the derived node, and takes (const Srv::Request &, Srv::Response &):
    //
    //     addService<Capture>("/camera/capture", &CameraNode::onCapture);
    //
    //     addService<ClosePattern>("/monitor/close_pattern",
    //                              [this](const auto &req, auto &res) { ... });
    //
    // The method-pointer form is the one to reach for when the body is long
    // enough to want a name; both end up in the same place.
    template <typename Srv, typename Handler>
    void addService(const std::string &name, Handler handler) {
        using Request = typename Srv::Request;
        using Response = typename Srv::Response;
        handles_.push_back(create_service<Srv>(
            name,
            // Explicitly typed parameters, not `auto`: rclcpp matches the
            // callback by inspecting its signature, and a generic lambda has
            // none to inspect -- it fails to compile deep inside
            // AnyServiceCallback rather than here.
            [this, handler](const std::shared_ptr<Request> req,
                            std::shared_ptr<Response> res) {
                if constexpr (std::is_member_function_pointer_v<Handler>) {
                    // `this` is a ServerNode*; the method belongs to the derived
                    // node, which is what actually constructed us.
                    using Owner = typename MemberOwner<Handler>::type;
                    (static_cast<Owner *>(this)->*handler)(*req, *res);
                } else {
                    handler(*req, *res);
                }
            },
            rclcpp::ServicesQoS(), group_));
    }

    // Advertise an action. `group` overrides the node's default group, which the
    // axis needs: its long-running goals get a group of their own so a Stop is
    // never queued behind the drive-to-limit it is meant to end.
    template <typename Action, typename GoalFn, typename CancelFn, typename AcceptedFn>
    void addAction(const std::string &name, GoalFn goal, CancelFn cancel, AcceptedFn accepted,
                   rclcpp::CallbackGroup::SharedPtr group = nullptr) {
        handles_.push_back(rclcpp_action::create_server<Action>(
            this, name, std::move(goal), std::move(cancel), std::move(accepted),
            rcl_action_server_get_default_options(), group ? group : group_));
    }

    // The shape all six actions on this server actually have: accept every goal,
    // accept every cancellation, and run the goal on a detached thread.
    //
    //     addDetachedAction<CaliJob>("/compute/cali_job", &ComputeNode::executeCaliJob);
    //
    // The two accept-everything lambdas were written out six times, identically,
    // and being identical is the point rather than a coincidence: a goal is
    // rejected by the executor body reporting why, not by refusing to start, and
    // a cancel must always be accepted because the thing being cancelled is a
    // stage in motion. An action that genuinely needs to refuse a goal should use
    // addAction and say so.
    //
    // The detached thread is what keeps the executor free. Running the goal on the
    // callback would hold an executor thread for the length of a drive-to-limit --
    // and the one command that must not queue behind a drive-to-limit is the one
    // that ends it.
    template <typename Action, typename Execute>
    void addDetachedAction(const std::string &name, Execute execute,
                           rclcpp::CallbackGroup::SharedPtr group = nullptr) {
        using GoalHandle = rclcpp_action::ServerGoalHandle<Action>;
        addAction<Action>(
            name,
            [](const rclcpp_action::GoalUUID &, std::shared_ptr<const typename Action::Goal>) {
                return rclcpp_action::GoalResponse::ACCEPT_AND_EXECUTE;
            },
            [](const std::shared_ptr<GoalHandle>) {
                return rclcpp_action::CancelResponse::ACCEPT;
            },
            [this, execute](const std::shared_ptr<GoalHandle> gh) {
                std::thread{[this, execute, gh] {
                    if constexpr (std::is_member_function_pointer_v<Execute>) {
                        using Owner = typename MemberOwner<Execute>::type;
                        (static_cast<Owner *>(this)->*execute)(gh);
                    } else {
                        execute(gh);
                    }
                }}.detach();
            },
            std::move(group));
    }

    ServerContext &ctx_;
    rclcpp::CallbackGroup::SharedPtr group_;

private:
    template <typename T>
    struct MemberOwner;
    template <typename C, typename R, typename... A>
    struct MemberOwner<R (C::*)(A...)> {
        using type = C;
    };

    // shared_ptr<void> because a service handle and an action-server handle have
    // no common base worth naming. Nothing reads this: it exists so the handles
    // outlive the constructor, which is the whole of what they are for.
    std::vector<std::shared_ptr<void>> handles_;
};
