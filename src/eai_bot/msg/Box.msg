# SPDX-License-Identifier: MIT
# SPDX-FileCopyrightText: 2026 KY (kyshipit)

# 单个检测框，含可选关键点（SCRFD 人脸）。

# 类别标签（如 "person"、"face"）。
string label

# 边界框像素坐标（原始帧分辨率）。
int32 x1
int32 y1
int32 x2
int32 y2

# 置信度 [0.0, 1.0]。
float32 score

# 5 个人脸关键点（仅 SCRFD 填充，YOLO 为空）。
Point2D[<=5] keypoints