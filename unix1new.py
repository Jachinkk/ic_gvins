import csv
import math
from datetime import datetime, timezone

# �� GPS 相关参数
GPS_LEAP_SECOND = 18  # GPS 时间相对于 UTC 时间的闰秒数
GPS_EPOCH_OFFSET = 315964800  # Unix 时间 1970-01-01 到 GPS 1980-01-06 00:00:00 的秒数
SECONDS_IN_WEEK = 604800  # 每周的秒数
zero = 0
# ✅ 参考日期
reference_date = "2025-03-15"

# ✅ 文件路径
input_file = "/home/fury/kk/ic_gvins_ws/IE.txt"
output_file = "kc1.txt"

# ✅ 解析数据
data = []
with open(input_file, "r", encoding="utf-8") as file:
    for line in file:
        # ✅ 跳过表头
        if "UTC-Corr" in line or "HMS" in line:
            continue
        
        # ✅ 按空格拆分数据（避免额外空格影响）
        parts = list(filter(None, line.strip().split()))
        
        if len(parts) < 10:
            print(f"⚠️ 数据格式异常，跳过: {line.strip()}")
            continue
        
        # ✅ 解析 UTC 时间
        try:
            utc_time_str = parts[0]  # 形如 7:07:21.000
            utc_time = datetime.strptime(f"{reference_date} {utc_time_str}", "%Y-%m-%d %H:%M:%S.%f").replace(tzinfo=timezone.utc)
            unix_timestamp = utc_time.timestamp()
            seconds = unix_timestamp + GPS_LEAP_SECOND - GPS_EPOCH_OFFSET
            gps_week = int(math.floor(seconds / SECONDS_IN_WEEK))  # 计算 GPS 周数
            gps_sow = round(seconds - gps_week * SECONDS_IN_WEEK, 3)
        except ValueError:
            print(f"⚠️ 解析 UTC 时间失败: {parts[0]}")
            continue

        # ✅ 解析纬度、经度、高度
        try:
            lat_deg = float(parts[1])
            lon_deg = float(parts[2])
            height_ellipsoid = float(parts[3]) + 1.757
        except ValueError:
            print(f"⚠️ 解析经纬度或高度失败: {parts[1:4]}")
            continue
	
	# ✅ 解析北向速度、东向速度、地向速度
        try:
            v_north = float(parts[5])
            v_east = float(parts[4])
            v_down = - float(parts[6])
        except ValueError:
            print(f"⚠️ 解析角度数据失败: {parts[4:7]}")
            continue
            
        # ✅ 解析航向角、俯仰角、横滚角
        try:
            roll = -float(parts[9]) + 1
            pitch = float(parts[8]) + 2.365
            heading = float(parts[7])
        except ValueError:
            print(f"⚠️ 解析角度数据失败: {parts[7:10]}")
            continue

        # ✅ 存储数据（格式化输出）
        data.append(f"{zero:.9f} {gps_sow:.4f} {lat_deg:.9f} {lon_deg:.9f} {height_ellipsoid:.9f} "
                    f"{v_north:.9f} {v_east:.9f} {v_down:.9f} {roll:.9f} {pitch:.9f} {heading:.9f}")

# 写入 TXT
with open(output_file, "w", encoding="utf-8") as txtfile:
    # txtfile.write("Gps_Sow Latitude Longitude Height_Ellipsoid v_north v_east v_down Roll Pitch Heading\n")
    txtfile.write("\n".join(data))

print(f"✅ 数据提取完成，已保存至 {output_file}")

