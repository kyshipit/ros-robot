# SPDX-License-Identifier: MIT
# SPDX-FileCopyrightText: 2026 KY (kyshipit)

# EAI 检测结果消息。
# 每个槽位每帧发布一条 DetectionResult，包含该槽位的所有检测框。

# 帧编号（对应 InferenceTask::frame_id）。
int32 frame_id

# 槽位名："yolo" 或 "scrfd"。
string slot

# 该帧是否检测到人（来自 AdapterSignals）。
bool person_present

# 该帧是否检测到人脸（来自 AdapterSignals）。
bool face_detected

# 所有检测框。
Box[] boxes