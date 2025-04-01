#include <ros/ros.h>
#include <sensor_msgs/Imu.h>
#include <sensor_msgs/NavSatFix.h>
#include <serial/serial.h>
#include <vector>
#include <atomic>
#include <cmath>
#include <thread>
#include <iomanip>  // 用于字符串解析
#include <queue>
#include <algorithm>

#define BAUDRATE 921600  // 串口波特率
#define MAX_QUEUE_SIZE 100 // 限制IMU数据队列大小

std::atomic_bool thread_running;

class SensorDriverNode
{
public:
    SensorDriverNode(ros::NodeHandle& nh)
    {
        // 获取串口名称
        nh.param("port_name", port_name_, std::string("/dev/ttyUSB2"));

        // 发布IMU和GPS数据
        imu_publisher_ = nh.advertise<sensor_msgs::Imu>("/imu_raw", 50);
        // gps_publisher_ = nh.advertise<sensor_msgs::NavSatFix>("/fix", 10);

        // 启动解析线程
        thread_running.store(true);
        parser_thread_ = std::thread(&SensorDriverNode::parserThread, this);
        processing_thread_ = std::thread(&SensorDriverNode::processIMUData, this);
    }

    ~SensorDriverNode()
    {
        thread_running.store(false);
        if (parser_thread_.joinable())
        {
            parser_thread_.join();
        }
        if (processing_thread_.joinable())
        {
            processing_thread_.join();
        }
    }

private:
    void parserThread()
    {
        serial::Serial serial_port;
        try
        {
            serial_port.setPort(port_name_);
            serial_port.setBaudrate(BAUDRATE);
            serial::Timeout timeout = serial::Timeout::simpleTimeout(500);
            serial_port.setTimeout(timeout);
            serial_port.open();
            ROS_INFO("Serial port opened successfully...");
        }
        catch (const serial::IOException& e)
        {
            ROS_ERROR("Failed to open the serial port.");
            return;
        }

        serial_port.flush();
        ros::Rate loop_rate(200);  // 数据处理频率

        std::string buffer;
        while (ros::ok() && thread_running.load())
        {
            if (serial_port.available() > 0)
            {
                std::string data = serial_port.read(serial_port.available());
                buffer += data;

                // 解析完整行
                size_t pos;
                while ((pos = buffer.find("\n")) != std::string::npos)
                {
                    std::string line = buffer.substr(0, pos);
                    buffer.erase(0, pos + 1);

                    if (line.rfind("$RAWIMU", 0) == 0)  // IMU数据
                    {
                        std::lock_guard<std::mutex> lock(imu_mutex);
                        imu_data_queue.push(line);

                        // 丢弃过旧的数据
                        while (imu_data_queue.size() > MAX_QUEUE_SIZE)
                            imu_data_queue.pop();
                    }
                }
            }
            loop_rate.sleep();
        }

        serial_port.close();
    }

    void processIMUData()
    {
        ros::Rate processing_rate(200);

        while (ros::ok() && thread_running.load())
        {
            std::string imu_line;
            {
                std::lock_guard<std::mutex> lock(imu_mutex);
                if (!imu_data_queue.empty())
                {
                    imu_line = imu_data_queue.front();
                    imu_data_queue.pop();
                }
            }

            if (!imu_line.empty())
            {
                parseIMUData(imu_line);
            }

            processing_rate.sleep();
        }
    }

    void parseIMUData(const std::string& line)
    {
        sensor_msgs::Imu imu_msg;
        imu_msg.header.stamp = ros::Time::now();
        // imu_msg.header.stamp = ros::Time::from(std::stod(tokens[1]));
        imu_msg.header.frame_id = "base_link";

        // 解析IMU数据
        std::vector<std::string> tokens;
        tokenize(line, tokens, ',');

        if (tokens.size() >= 10)  // 数据完整性检查
        {
            // 从IMU报文中提取时间戳
            // std::string utc_time_str = tokens[1];
            // imu_msg.header.stamp = parseUtcTimeToRosTime(utc_time_str);
            try{
                imu_msg.linear_acceleration.x = std::stod(tokens[3]) * 9.8 / 1000.0;
                imu_msg.linear_acceleration.y = std::stod(tokens[2]) * 9.8 / 1000.0;
                imu_msg.linear_acceleration.z = -std::stod(tokens[4]) * 9.8 / 1000.0;

                imu_msg.angular_velocity.x = std::stod(tokens[6]) * M_PI / (180 * 1000.0);
                imu_msg.angular_velocity.y = std::stod(tokens[5]) * M_PI / (180 * 1000.0);
                imu_msg.angular_velocity.z = -std::stod(tokens[7]) * M_PI / (180 * 1000.0);
            }
            catch (const std::invalid_argument& e) 
            {
                ROS_ERROR("Invalid IMU data: %s. Skipping this data.", e.what());
                return;  // 如果遇到无效数据，跳过当前数据
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

            // 发布IMU数据
            imu_publisher_.publish(imu_msg);
        }
    }

    // void parseGPGGAData(const std::string& line)
    // {
    //     sensor_msgs::NavSatFix navsat_msg;
    //     navsat_msg.header.stamp = ros::Time::now();
    //     navsat_msg.header.frame_id = "gps";

    //     // 解析GPGGA数据
    //     std::vector<std::string> tokens;
    //     tokenize(line, tokens, ',');

    //     if (tokens.size() >= 15)  // 数据完整性检查
    //     {
    //         // 从GPGGA报文中提取时间戳
    //         // std::string utc_time_str = tokens[1];
    //         // navsat_msg.header.stamp = parseUtcTimeToRosTime(utc_time_str);
    // 	    try{
    //         	double latitude = std::stod(tokens[2].substr(0, 2)) +
    //                              std::stod(tokens[2].substr(2)) / 60.0;
    //         	if (tokens[3] == "S") latitude = -latitude;
    //         	navsat_msg.latitude = latitude;

    //         	double longitude = std::stod(tokens[4].substr(0, 3)) +
    //                              std::stod(tokens[4].substr(3)) / 60.0;
    //         	if (tokens[5] == "W") longitude = -longitude;
    //         	navsat_msg.longitude = longitude;

    //         	navsat_msg.altitude = std::stod(tokens[9]);

    //         	navsat_msg.status.status = (std::stoi(tokens[6]) >= 1) ? 
    //                                     	sensor_msgs::NavSatStatus::STATUS_FIX : 
    //                                     	sensor_msgs::NavSatStatus::STATUS_NO_FIX;
    //         	navsat_msg.status.service = sensor_msgs::NavSatStatus::SERVICE_GPS;

    //         	navsat_msg.position_covariance[0] = std::stod(tokens[8]);
    //         	navsat_msg.position_covariance[4] = std::stod(tokens[8]);
    //         	navsat_msg.position_covariance[8] = std::stod(tokens[8]) * 2.0;
    //         	navsat_msg.position_covariance_type = sensor_msgs::NavSatFix::COVARIANCE_TYPE_APPROXIMATED;
	//     }
    //         catch (const std::invalid_argument& e) {
    //             ROS_ERROR("Invalid GPS data: %s. Skipping this data.", e.what());
    //             return;  // 如果遇到无效数据，跳过当前数据
    //         }
    //         // 发布GPS数据
    //         gps_publisher_.publish(navsat_msg);
    //     }
    // }

    // // UTC时间解析函数
    // ros::Time parseUtcTimeToRosTime(const std::string& utc_time_str)
    // {
    //     if (utc_time_str.size() < 6)
    //     {
    //         ROS_ERROR("Invalid UTC time string: %s", utc_time_str.c_str());
    //         return ros::Time(0);  // 返回无效时间
    //     }

    //     // 提取小时、分钟、秒
    //     int hours = std::stoi(utc_time_str.substr(0, 2));
    //     int minutes = std::stoi(utc_time_str.substr(2, 2));
    //     double seconds = std::stod(utc_time_str.substr(4));

    //     // 转换为秒数
    //     double total_seconds = hours * 3600 + minutes * 60 + seconds;

    //     // 当前日期的起始时间（假设是同一天的UTC时间）
    //     ros::Time current_day = ros::Time::now();
    //     current_day.sec = (current_day.sec / 86400) * 86400;  // 去掉当天的时间偏移，只保留日期

    //     // 加上UTC时间戳的秒数
    //     ros::Time gps_time = current_day + ros::Duration(total_seconds);

    //     // 处理跨午夜情况
    //     if (gps_time < current_day) {
    //         gps_time += ros::Duration(86400);  // 补偿到第二天
    //     }

    //     return gps_time;
    // }

    // 字符串分割函数
    void tokenize(const std::string& str, std::vector<std::string>& tokens, char delimiter)
    {
        size_t start = 0;
        size_t end = str.find(delimiter);

        while (end != std::string::npos)
        {
            tokens.push_back(str.substr(start, end - start));
            start = end + 1;
            end = str.find(delimiter, start);
        }

        tokens.push_back(str.substr(start, end));
    }

    std::string port_name_;
    ros::Publisher imu_publisher_;
    // ros::Publisher gps_publisher_;
    std::thread parser_thread_;
    std::thread processing_thread_;

    std::mutex imu_mutex;
    std::queue<std::string> imu_data_queue;
};

int main(int argc, char** argv)
{
    ros::init(argc, argv, "sensor_driver_node");
    ros::NodeHandle nh;

    thread_running.store(true);

    SensorDriverNode sensor_node(nh);

    ros::spin();

    return 0;
}
