#include <ros/ros.h>
#include <sensor_msgs/Imu.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <cstring>
#include <vector>
#include <sstream>
#include <queue>
#include <mutex>
#include <thread>
#include <condition_variable>
#include <cctype>
#include <sys/select.h>

#define BUFFER_SIZE 1024
#define MAX_QUEUE_SIZE 100  // 限制队列长度，防止积累数据过多

std::queue<std::string> imu_data_queue;
std::mutex queue_mutex;
std::condition_variable queue_cv;
bool running = true;

void tokenize(const std::string& str, std::vector<std::string>& tokens, char delimiter) {
    std::stringstream ss(str);
    std::string token;
    while (std::getline(ss, token, delimiter)) {
        tokens.push_back(token);
    }
}

bool isValidNumber(const std::string& s) {
    return !s.empty() && (std::isdigit(s[0]) || s[0] == '-' || s[0] == '+');
}

void parseIMUData(ros::Publisher& imu_pub) {
    ros::Rate rate(200);  // 设定发布频率 200Hz
    while (running) {
        std::unique_lock<std::mutex> lock(queue_mutex);
        queue_cv.wait(lock, [] { return !imu_data_queue.empty() || !running; });
        if (!running) break;

        std::string line = imu_data_queue.front();
        imu_data_queue.pop();
        lock.unlock();

        sensor_msgs::Imu imu_msg;
        imu_msg.header.stamp = ros::Time::now();
        imu_msg.header.frame_id = "imu_link";

        std::vector<std::string> tokens;
        tokens.reserve(12);  // 预分配空间，提高解析速度
        tokenize(line, tokens, ',');

        if (tokens.size() < 10) {
            continue;  // 直接跳过，不打印日志
        }

        try {
            if (!isValidNumber(tokens[2]) || !isValidNumber(tokens[3]) || !isValidNumber(tokens[4]) ||
                !isValidNumber(tokens[5]) || !isValidNumber(tokens[6]) || !isValidNumber(tokens[7])) {
                continue;
            }

            imu_msg.linear_acceleration.x = std::stod(tokens[3]) * 9.8 / 1000.0;
            imu_msg.linear_acceleration.y = std::stod(tokens[2]) * 9.8 / 1000.0;
            imu_msg.linear_acceleration.z = -std::stod(tokens[4]) * 9.8 / 1000.0;

            imu_msg.angular_velocity.x = std::stod(tokens[6]) * M_PI / (180 * 1000.0);
            imu_msg.angular_velocity.y = std::stod(tokens[5]) * M_PI / (180 * 1000.0);
            imu_msg.angular_velocity.z = -std::stod(tokens[7]) * M_PI / (180 * 1000.0);
        } catch (const std::invalid_argument& e) {
            continue;  // 直接跳过错误数据
        }

        imu_msg.orientation.x = 0.0;
        imu_msg.orientation.y = 0.0;
        imu_msg.orientation.z = 0.0;
        imu_msg.orientation.w = 1.0;

        imu_msg.linear_acceleration_covariance[0] = 0.01;
        imu_msg.linear_acceleration_covariance[4] = 0.01;
        imu_msg.linear_acceleration_covariance[8] = 0.01;

        imu_msg.angular_velocity_covariance[0] = 0.001;
        imu_msg.angular_velocity_covariance[4] = 0.001;
        imu_msg.angular_velocity_covariance[8] = 0.001;

        imu_msg.orientation_covariance[0] = -1;

        imu_pub.publish(imu_msg);
        rate.sleep();
    }
}

void receiveIMUData(int sockfd) {
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    char buffer[BUFFER_SIZE];

    fd_set read_fds;
    struct timeval timeout;

    while (running) {
        FD_ZERO(&read_fds);
        FD_SET(sockfd, &read_fds);

        timeout.tv_sec = 0;
        timeout.tv_usec = 5000;  // 5ms 超时，避免长时间阻塞

        int retval = select(sockfd + 1, &read_fds, NULL, NULL, &timeout);
        if (retval > 0 && FD_ISSET(sockfd, &read_fds)) {
            ssize_t received = recvfrom(sockfd, buffer, BUFFER_SIZE, 0, (struct sockaddr*)&client_addr, &client_len);
            if (received > 0) {
                std::string imu_data(buffer, received);
                std::lock_guard<std::mutex> lock(queue_mutex);
                if (imu_data_queue.size() > MAX_QUEUE_SIZE) {
                    imu_data_queue.pop();  // 丢弃旧数据，防止积累
                }
                imu_data_queue.push(imu_data);
                queue_cv.notify_one();
            }
        }
    }
}

int main(int argc, char** argv) {
    ros::init(argc, argv, "imu_gps_receiver_node");
    ros::NodeHandle nh;
    ros::Publisher imu_pub = nh.advertise<sensor_msgs::Imu>("/imu_raw", 1000);

    std::string imu_ip;
    int imu_port;
    nh.param<std::string>("imu_ip", imu_ip, "192.168.0.100");
    nh.param<int>("imu_port", imu_port, 1258);

    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        ROS_ERROR("Failed to create socket");
        return -1;
    }

    // 增大 UDP 接收缓冲区，减少丢包风险
    int rcvbuf_size = 65536;  // 64 KB
    setsockopt(sockfd, SOL_SOCKET, SO_RCVBUF, &rcvbuf_size, sizeof(rcvbuf_size));

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(imu_ip.c_str());
    server_addr.sin_port = htons(imu_port);

    if (bind(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        ROS_ERROR("Failed to bind socket");
        close(sockfd);
        return -1;
    }

    ROS_INFO("IMU receiver node started on %s:%d", imu_ip.c_str(), imu_port);

    std::thread recvThread(receiveIMUData, sockfd);
    std::thread processThread(parseIMUData, std::ref(imu_pub));

    ros::spin();

    running = false;
    queue_cv.notify_all();
    recvThread.join();
    processThread.join();
    close(sockfd);

    return 0;
}
