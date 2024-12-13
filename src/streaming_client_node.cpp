#include "proto/pointcloud_stream.grpc.pb.h"
#include <grpcpp/grpcpp.h>
#include <iostream>
#include <rclcpp/rclcpp.hpp>
#include <sensor_msgs/msg/point_cloud2.hpp>
#include <thread>
#include <vector>


std::shared_ptr<rclcpp::Node> node;
rclcpp::Publisher<sensor_msgs::msg::PointCloud2>::SharedPtr publisher;

#define SERVER_ADDRESS "localhost:50051"

class StreamClient {

    int m_messageCount = 0;

    sensor_msgs::msg::PointCloud2 convertToROS2PointCloud2(const pointcloud_stream::PointCloud2& grpc_msg) {
        sensor_msgs::msg::PointCloud2 ros_msg;

        // Fill the header
        ros_msg.header.frame_id = grpc_msg.frame_id();
        ros_msg.header.stamp = node->get_clock()->now();
        // ros_msg.header.stamp.sec = static_cast<int32_t>(grpc_msg.timestamp());
        // ros_msg.header.stamp.nanosec = static_cast<uint32_t>((grpc_msg.timestamp() - ros_msg.header.stamp.sec) * 1e9);

        // Fill the data fields
        ros_msg.data = std::vector<uint8_t>(grpc_msg.data().begin(), grpc_msg.data().end());
        ros_msg.width = grpc_msg.width();
        ros_msg.height = grpc_msg.height();

        auto fieldsSize = grpc_msg.fields().size();
        ros_msg.fields.resize(fieldsSize);
        for (int i = 0; i < fieldsSize; ++i) {
            ros_msg.fields[i].name = grpc_msg.fields()[i].name();
            ros_msg.fields[i].offset = grpc_msg.fields()[i].offset();
            ros_msg.fields[i].datatype = grpc_msg.fields()[i].datatype();
            ros_msg.fields[i].count = grpc_msg.fields()[i].count();
        }

        ros_msg.is_dense = grpc_msg.is_dense();
        ros_msg.is_bigendian = grpc_msg.is_bigendian();
        ros_msg.point_step = grpc_msg.point_step();
        ros_msg.row_step = grpc_msg.row_step();

        return ros_msg;
    }

public:
    StreamClient() {}

    ~StreamClient() {
        std::cout << "Received " << m_messageCount << " messages" << std::endl;
    }

    void StreamPointCloudFromServer() {

        // Initialize gRPC
        grpc::ChannelArguments args;

        // Step 1: Create a gRPC channel to the server
        auto channel = grpc::CreateChannel(SERVER_ADDRESS, grpc::InsecureChannelCredentials());
        auto stub = pointcloud_stream::PointCloudStreamer::NewStub(channel);

        // Step 2: Set up the request (empty message, since it's not used in this case)
        google::protobuf::Empty request;

        // Step 3: Prepare the stream to receive the PointCloud2 messages
        grpc::ClientContext context;
        std::unique_ptr<grpc::ClientReader<pointcloud_stream::PointCloud2>> reader(
            stub->StreamPointCloud(&context, request)
        );

        // Step 4: Read the streamed PointCloud2 messages
        pointcloud_stream::PointCloud2 pointcloud_data;
        while (rclcpp::ok()) {
            // Process the received PointCloud2 data
            // std::cout << "Received PointCloud2 message:" << std::endl;
            // std::cout << "Width: " << pointcloud_data.width() << std::endl;
            // std::cout << "Height: " << pointcloud_data.height() << std::endl;
            // std::cout << "Frame ID: " << pointcloud_data.frame_id() << std::endl;
            // std::cout << "Timestamp: " << pointcloud_data.timestamp() << std::endl;
            // std::cout << "Data size: " << pointcloud_data.data().size() << " bytes" << std::endl;

            bool ok = reader->Read(&pointcloud_data);

            if (!ok || pointcloud_data.data().size() == 0) {
                std::cout << "NO DATA" << std::endl;
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                continue;
            }

            auto msg = convertToROS2PointCloud2(pointcloud_data);

            publisher->publish(msg);

            m_messageCount++;

        }

        // Step 5: Handle any errors from the server
        grpc::Status status = reader->Finish();
        if (!status.ok()) {
            std::cerr << "RPC failed: " << status.error_message() << std::endl;
        } else {
            std::cout << "Stream finished successfully." << std::endl;
        }
    }

};

int main(int argc, char** argv) {

    rclcpp::init(argc, argv);

    node = std::make_shared<rclcpp::Node>("pointcloud_streamer_client");

    publisher = node->create_publisher<sensor_msgs::msg::PointCloud2>("stream_comming", 10);


    StreamClient client;

    std::thread clientThread(&StreamClient::StreamPointCloudFromServer, &client);


    rclcpp::spin(node);
    rclcpp::shutdown();

    return 0;
}
