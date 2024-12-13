#include "proto/pointcloud_stream.grpc.pb.h"
#include <grpcpp/grpcpp.h>
#include <rclcpp/rclcpp.hpp>
#include <sensor_msgs/msg/point_cloud2.hpp>
#include <mutex>

class PointCloudStreamerService final : public pointcloud_stream::PointCloudStreamer::Service {
public:
    ::grpc::Status StreamPointCloud(
    ::grpc::ServerContext* context,
    const ::google::protobuf::Empty* /*request*/,
    ::grpc::ServerWriter< ::pointcloud_stream::PointCloud2>* writer) override {

    while (!context->IsCancelled()) { // Check if client disconnects
        pointcloud_stream::PointCloud2 grpc_pointcloud;

        // Fill PointCloud2 data from the ROS message
        {
            std::lock_guard<std::mutex> lock(pointcloud_mutex_);

            const auto& stamp = pointcloud_msg_.header.stamp;
            grpc_pointcloud.set_stamp_sec(stamp.sec);
            grpc_pointcloud.set_stamp_nanosec(stamp.nanosec);

            grpc_pointcloud.set_data(pointcloud_msg_.data.data(), pointcloud_msg_.data.size());
            grpc_pointcloud.set_width(pointcloud_msg_.width);
            grpc_pointcloud.set_height(pointcloud_msg_.height);
            grpc_pointcloud.set_frame_id(pointcloud_msg_.header.frame_id);

            grpc_pointcloud.set_is_bigendian(pointcloud_msg_.is_bigendian);
            grpc_pointcloud.set_point_step(pointcloud_msg_.point_step);
            grpc_pointcloud.set_row_step(pointcloud_msg_.row_step);
            grpc_pointcloud.set_is_dense(pointcloud_msg_.is_dense);

            // Populate fields
            for (const auto& field : pointcloud_msg_.fields) {
                auto* grpc_field = grpc_pointcloud.add_fields();
                grpc_field->set_name(field.name);
                grpc_field->set_offset(field.offset);
                grpc_field->set_datatype(field.datatype);
                grpc_field->set_count(field.count);
            }

        }
        // Write to client
        if (!writer->Write(grpc_pointcloud)) {
            break; // Stop streaming if client disconnects
        }

        // Optionally, add a sleep to control the stream rate
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    return grpc::Status::OK;
}


    // Update the stored point cloud
    void setPointCloud(const sensor_msgs::msg::PointCloud2& msg) {
        std::lock_guard<std::mutex> lock(pointcloud_mutex_);
        pointcloud_msg_ = msg;
    }

private:
    sensor_msgs::msg::PointCloud2 pointcloud_msg_;
    std::mutex pointcloud_mutex_;
};

int main(int argc, char** argv) {
    rclcpp::init(argc, argv);

    auto node = std::make_shared<rclcpp::Node>("pointcloud_streamer");
    auto service = std::make_shared<PointCloudStreamerService>();

    // Subscribe to the PointCloud2 topic
    auto subscription = node->create_subscription<sensor_msgs::msg::PointCloud2>(
        "/pointcloud", 10,
        [&service](const sensor_msgs::msg::PointCloud2::SharedPtr msg) {
            service->setPointCloud(*msg);
        });

    // Set up the gRPC server
    grpc::ServerBuilder builder;
    builder.AddListeningPort("0.0.0.0:50051", grpc::InsecureServerCredentials());
    builder.RegisterService(service.get());
    auto server = builder.BuildAndStart();

    RCLCPP_INFO(node->get_logger(), "PointCloud streaming server started.");
    rclcpp::spin(node);
    server->Shutdown();
    rclcpp::shutdown();

    return 0;
}
