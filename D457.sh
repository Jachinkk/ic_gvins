#!/bin/bash
echo "Hello, welcome to IC-GVINS !"

# 进入指定文件夹
echo "进入目录 /home/fury/kk/ic_gvins_ws ..."
cd /home/fury/kk/ic_gvins_ws || { echo "无法进入目录 /home/fury/kk/ic_gvins_ws"; exit 1;}
sleep 1

# 修改/dev/ttyUSB0 的权限
# echo "修改 /dev/ttyUSB0 权限 ..."
echo "修改 /dev/ttyUSB1/2 权限 ..."
# sudo chmod 666 /dev/ttyUSB0
sudo chmod 666 /dev/ttyUSB1
# sudo chmod 666 /dev/ttyUSB2
sleep 1

# 启动 IC-GVINS 节点
echo "启动 IC-GVINS 节点 ..."
gnome-terminal -- bash -c "source /opt/ros/noetic/setup.bash; source /home/fury/kk/ic_gvins_ws/devel/setup.bash; roslaunch ic_gvins ic_gvins_d457.launch configfile:=/home/fury/kk/ic_gvins_ws/K3105C2.0/IC-GVINS/gvins.yaml; exec bash"
sleep 5

# 启动 zed 相机节点
echo "启动 d457 相机节点 ..."
gnome-terminal -- bash -c "source /opt/ros/noetic/setup.bash; source /home/fury/realsense_ws/devel/setup.bash; roslaunch realsense2_camera camera.launch; exec bash"
sleep 5

# 启动 NMEA 驱动节点
echo "启动 NMEA 驱动节点 ..."
gnome-terminal -- bash -c "source /opt/ros/noetic/setup.bash; source /home/fury/kk/ic_gvins_ws/devel/setup.bash; roslaunch nmea_navsat_driver nmea_serial_driver.launch port:=/dev/ttyUSB1 baud:=9600; exec bash"
sleep 3

# 启动 ether 网口节点
echo "启动 ether 网口节点 ..."
gnome-terminal -- bash -c "source /opt/ros/noetic/setup.bash; source /home/fury/kk/ic_gvins_ws/devel/setup.bash; roslaunch imu_ethernet_receiver imu_ethernet.launch; exec bash"

# 启动 sensor 驱动节点
# echo "启动 sensor 驱动节点 ..."
# gnome-terminal -- bash -c "source /opt/ros/noetic/setup.bash; source /home/fury/kk/ic_gvins_ws/devel/setup.bash; roslaunch sensor_driver sensor_driver.launch; exec bash"

echo "所有节点已启动完成！"



