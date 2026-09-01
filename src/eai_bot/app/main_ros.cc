// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 KY (kyshipit)

/*
 * app/main_ros.cc — ROS2 集成入口。
 *
 * 与 app/main.cc 装配逻辑完全一致，差异仅在于：
 *   1. 开头初始化 ROS2（通过 RosBridge）。
 *   2. PostTaskHandler 中额外调用 bridge.PublishTask(task) 发布 ROS 消息。
 *   3. 不再使用 ConsoleUi（ROS2 节点通过 ros2 topic/ros2 service 交互）。
 *
 * 所有 engine/、platform/、adapters/ 代码不动。
 */
#include <ament_index_cpp/get_package_share_directory.hpp>
#include <iostream>
#include <memory>
#include <string>
#include <vector>
#include <atomic>
#include <algorithm>
#include <cstdlib>

#include "engine/pipeline.h"
#include "platform/logging.h"
#include "platform/model_coordinator.h"
#include "capture/camera_source.h"
#include "capture/frame_transform.h"
#include "display/display_sink.h"
#include "display/display_layout.h"
#include "display/result_overlay.h"
#include "adapters/yolo/yolo_adapter.h"
#include "adapters/scrfd/scrfd_adapter.h"
#include "adapters/llm/llm_worker.h"
#include "adapters/melotts/melotts_session.h"
#include "voice/tts_planner.h"
#include "voice/tts_worker.h"
#include "voice/voice_reply_bridge.h"
#include "app/config_parser.h"
#include "ros_bridge/ros_bridge.h"

#include <signal.h>
#include <execinfo.h>
#include <unistd.h>

static std::atomic<bool> g_stop_requested{false};
static std::atomic<int> g_sigint_count{0};

static void segv_handler(int sig) {
    void* bt[20];
    int bt_size = backtrace(bt, 20);
    LogFatal("signal %d, backtrace:", sig);
    fprintf(stderr, "Fatal signal %d, backtrace:\n", sig);
    backtrace_symbols_fd(bt, bt_size, STDERR_FILENO);
    fsync(STDERR_FILENO);
    _exit(128 + sig);
}

static void stop_handler(int sig) {
    (void)sig;
    const int n = g_sigint_count.fetch_add(1) + 1;
    g_stop_requested.store(true);
    fprintf(stderr, "\nSignal received, request stop (workers will exit)...\n");
    if (n >= 2) {
        fprintf(stderr, "Second interrupt, forcing exit.\n");
        _exit(130);
    }
}

int main(int argc, char** argv) {
    // 获取包的 share 目录绝对路径
    std::string package_share_dir = ament_index_cpp::get_package_share_directory("eai_bot");
    std::string config_path = package_share_dir + "/config/default.yaml";

    // 如果命令行传入了配置文件路径，则覆盖默认值
    if (argc == 2) {
        config_path = argv[1];
    }

    ConfigParser cfg;
    if (!cfg.LoadFromFile(config_path)) {
        std::cerr << "Failed to load config: " << config_path << std::endl;
        return -1;
    }

    // 定义相对路径转换函数
    auto resolve_path = [&](const std::string& path) -> std::string {
        if (path.rfind("./", 0) == 0) {
            return package_share_dir + "/" + path.substr(2);
        }
        return path;
    };

    // 读取配置并直接转换所有路径
    std::string model_type = cfg.GetString("model.type");
    std::string yolo_model_path = resolve_path(cfg.GetString("model.yolo.path"));
    std::string scrfd_model_path = resolve_path(cfg.GetString("model.scrfd.path"));

    //std::string model_type = cfg.GetString("model.type");
    //std::string yolo_model_path = cfg.GetString("model.yolo.path");
    //std::string scrfd_model_path = cfg.GetString("model.scrfd.path");
    float scrfd_conf_th = static_cast<float>(cfg.GetInt("model.scrfd.conf_threshold_percent")) / 100.0f;
    float scrfd_nms_th = static_cast<float>(cfg.GetInt("model.scrfd.nms_threshold_percent")) / 100.0f;
    int infer_threads = cfg.GetInt("system.infer_threads");
    bool yolo_always_on = cfg.GetBool("system.slots.yolo_always_on");
    int scene_dwell_frames = cfg.GetInt("system.slots.scene_dwell_frames");
    int switch_present_threshold = cfg.GetInt("system.switch.present_threshold");
    int switch_absent_threshold = cfg.GetInt("system.switch.absent_threshold");
    bool single_thread = cfg.GetBool("system.switch.single_thread");
    std::string log_level = cfg.GetString("system.log_level");
    std::vector<int> npu_cores = cfg.GetIntArray("system.npu_cores");
    std::string input_source = cfg.GetString("input.source");
    int input_width = cfg.GetInt("input.width");
    int input_height = cfg.GetInt("input.height");
    std::string input_rotate = cfg.GetString("input.rotate");
    int yolo_person_threshold_percent = cfg.GetInt("model.yolo.person_threshold_percent");
    bool show_window = cfg.GetBool("input.show_window");
    int display_screen_w = cfg.GetInt("input.display.screen_width");
    int display_screen_h = cfg.GetInt("input.display.screen_height");
    int display_max_ratio_percent = cfg.GetInt("input.display.max_screen_ratio_percent");
    bool display_fullscreen = cfg.GetBool("input.display.fullscreen");
    int display_title_reserve = cfg.GetInt("input.display.title_bar_reserve_px");
    bool llm_enabled = cfg.GetBool("model.llm.enabled");
    LogInfo("MainROS: config %s model.llm.enabled=%s", config_path.c_str(),
            llm_enabled ? "true" : "false");
    //std::string llm_model_path = cfg.GetString("model.llm.path");
    // 对于 llm_model_path 也进行转换
    std::string llm_model_path = resolve_path(cfg.GetString("model.llm.path"));
    int llm_max_new_tokens = cfg.GetInt("model.llm.max_new_tokens");
    int llm_max_context_len = cfg.GetInt("model.llm.max_context_len");
    std::string llm_system_prompt = cfg.GetString("model.llm.system_prompt");
    int llm_face_stable_frames = cfg.GetInt("model.llm.face_stable_frames");
    int llm_face_absent_frames = cfg.GetInt("model.llm.face_absent_frames");
    int llm_grace_timeout_ms = cfg.GetInt("model.llm.grace_timeout_ms");
    int llm_idle_timeout_ms = cfg.GetInt("model.llm.idle_timeout_ms");
    bool llm_preload_on_scrfd = cfg.GetBool("model.llm.preload_on_scrfd");
    bool llm_preload_on_startup = cfg.GetBool("model.llm.preload_on_startup");
    std::string llm_auto_greeting_text = cfg.GetString("model.llm.auto_greeting_text");
    bool llm_tts_enabled = cfg.GetBool("model.tts.enabled");
    bool llm_tts_skip_greeting = cfg.GetBool("model.tts.skip_static_greeting");
    int llm_tts_max_chars = cfg.GetInt("model.tts.max_speak_chars");
    int llm_tts_split_min_chars = cfg.GetInt("model.tts.split_min_chars", 4);
    int llm_tts_single_shot_max = cfg.GetInt("model.tts.single_shot_max_chars", 96);
    int llm_tts_planner_zh_min = cfg.GetInt("model.tts.planner.zh_min_chars", 2);
    int llm_tts_planner_zh_max = cfg.GetInt("model.tts.planner.zh_max_chars", 48);
    int llm_tts_planner_en_min = cfg.GetInt("model.tts.planner.en_min_words", 4);
    int llm_tts_planner_en_max = cfg.GetInt("model.tts.planner.en_max_words", 8);
    int llm_tts_planner_fallback_ms = cfg.GetInt("model.tts.planner.fallback_timeout_ms", 150);
    int llm_tts_planner_short_max = cfg.GetInt("model.tts.planner.short_answer_max_chars", 96);
    bool llm_tts_preload = cfg.GetBool("model.tts.preload_on_startup");
    float llm_tts_speed = static_cast<float>(std::atof(cfg.GetString("model.tts.speed", "1.0").c_str()));
    if (llm_tts_speed <= 0.0f) {
        llm_tts_speed = 1.0f;
    }
    bool llm_tts_visual_throttle = cfg.GetBool("model.tts.qos.enable_visual_throttle", true);
    int llm_tts_low_watermark_chunks = cfg.GetInt("model.tts.qos.low_watermark_chunks", 1);
    int llm_tts_high_watermark_chunks = cfg.GetInt("model.tts.qos.high_watermark_chunks", 3);
    int llm_tts_yolo_stride = cfg.GetInt("model.tts.qos.yolo_infer_stride", 3);

    std::shared_ptr<IModelAdapter> base_adapter;
    if (model_type == "yolo") {
        auto yolo = std::make_shared<YoloAdapter>();
        yolo->SetPersonScoreThreshold(yolo_person_threshold_percent / 100.0f);
        base_adapter = yolo;
    } else {
        std::cerr << "Unsupported model type: " << model_type << std::endl;
        return -1;
    }

    try {
        SetLogLevelByName(log_level);
        setvbuf(stdout, NULL, _IONBF, 0);
        setvbuf(stderr, NULL, _IONBF, 0);
        signal(SIGSEGV, segv_handler);
        signal(SIGINT, stop_handler);
        signal(SIGTERM, stop_handler);

        // ==============================================================
        // ★ 唯一新增：ROS2 初始化
        // ==============================================================
        eai::ros_bridge::RosBridge ros_bridge(argc, argv, "eai_detector");

        ModelCoordinator coordinator;
        coordinator.SetSlotOptions(yolo_always_on);
        coordinator.SetSceneDwellFrames(scene_dwell_frames);
        coordinator.SetTtsVisualThrottleEnabled(llm_tts_visual_throttle);
        coordinator.SetYoloInferStride(llm_tts_yolo_stride);

        std::shared_ptr<LlmWorker> llm_worker;
        std::shared_ptr<TtsWorker> tts_worker;
        VoiceReplyBridge voice_bridge;
        coordinator.GetLlmGreeting().SetTriggerThreshold(llm_face_stable_frames);
        coordinator.GetLlmGreeting().SetFaceAbsentThreshold(llm_face_absent_frames);
        coordinator.GetLlmGreeting().SetGraceTimeoutMs(llm_grace_timeout_ms);
        coordinator.GetLlmGreeting().SetIdleTimeoutMs(llm_idle_timeout_ms);
        coordinator.GetLlmGreeting().SetPreloadOnScrfd(llm_preload_on_scrfd);
        coordinator.GetLlmGreeting().SetAutoGreetingText(llm_auto_greeting_text);
        if (llm_enabled) {
            llm_worker = std::make_shared<LlmWorker>();
            llm_worker->Configure(llm_model_path, llm_max_new_tokens, llm_max_context_len,
                                  llm_system_prompt);
            coordinator.GetLlmGreeting().SetLlmWorker(llm_worker.get());
            if (llm_preload_on_startup) {
                llm_worker->RequestInitializeAsync();
            }
            if (llm_tts_enabled) {
                /*
                MeloTtsConfig tts_cfg;
                tts_cfg.encoder_path = cfg.GetString("model.tts.encoder_path");
                tts_cfg.decoder_path = cfg.GetString("model.tts.decoder_path");
                tts_cfg.lexicon_path = cfg.GetString("model.tts.lexicon_path");
                tts_cfg.tokens_path = cfg.GetString("model.tts.tokens_path"); 
                */
                // 在 TTS 配置中转换路径
                MeloTtsConfig tts_cfg;
                tts_cfg.encoder_path = resolve_path(cfg.GetString("model.tts.encoder_path"));
                tts_cfg.decoder_path = resolve_path(cfg.GetString("model.tts.decoder_path"));
                tts_cfg.lexicon_path = resolve_path(cfg.GetString("model.tts.lexicon_path"));
                tts_cfg.tokens_path = resolve_path(cfg.GetString("model.tts.tokens_path"));
                tts_cfg.language = cfg.GetString("model.tts.language");
                tts_cfg.speak_id = cfg.GetInt("model.tts.speak_id");
                tts_cfg.speed = llm_tts_speed;
                tts_cfg.disable_bert = cfg.GetBool("model.tts.disable_bert");
                tts_cfg.split_min_chars = llm_tts_split_min_chars;
                tts_cfg.single_shot_max_chars = std::max(0, llm_tts_single_shot_max);
                tts_worker = std::make_shared<TtsWorker>();
                tts_worker->Configure(tts_cfg, llm_tts_max_chars);
                tts_worker->SetPlaybackProtectionThresholds(
                    static_cast<size_t>(llm_tts_low_watermark_chunks),
                    static_cast<size_t>(llm_tts_high_watermark_chunks));
                voice_bridge.SetTtsWorker(tts_worker.get());
                voice_bridge.SetEnabled(true);
                llm_worker->SetVoiceReplyBridge(&voice_bridge);
                TtsPlannerConfig planner_cfg;
                planner_cfg.zh_min_chars = static_cast<size_t>(std::max(1, llm_tts_planner_zh_min));
                planner_cfg.zh_max_chars = static_cast<size_t>(std::max(1, llm_tts_planner_zh_max));
                planner_cfg.en_min_words = static_cast<size_t>(std::max(1, llm_tts_planner_en_min));
                planner_cfg.en_max_words = static_cast<size_t>(std::max(1, llm_tts_planner_en_max));
                planner_cfg.fallback_timeout_ms = std::max(100, llm_tts_planner_fallback_ms);
                planner_cfg.short_answer_max_chars =
                    static_cast<size_t>(std::max(1, llm_tts_planner_short_max));
                voice_bridge.ConfigurePlanner(planner_cfg);
                coordinator.GetLlmGreeting().SetVoiceOutput(tts_worker.get(),
                                                          llm_tts_skip_greeting);
                if (llm_tts_preload) {
                    tts_worker->RequestInitializeAsync();
                }
                LogInfo("MainROS: TTS enabled encoder=%s", tts_cfg.encoder_path.c_str());
            }
            LogInfo("MainROS: LLM configured path=%s preload_on_startup=%d preload_on_scrfd=%d",
                    llm_model_path.c_str(), llm_preload_on_startup ? 1 : 0,
                    llm_preload_on_scrfd ? 1 : 0);
        } else {
            LogInfo("MainROS: LLM disabled (model.llm.enabled=false)");
        }

        CameraSource camera(input_source, input_width, input_height);
        if (!camera.Open()) {
            LogError("MainROS: failed to open input source '%s'", input_source.c_str());
            std::cerr << "Failed to open input: " << input_source << std::endl;
            return -1;
        }

        FrameTransform frame_transform(input_rotate);
        ResultOverlay overlay;

        // 显示窗口：仅在 show_window=true 时创建。
        std::unique_ptr<IDisplaySink> display;
        if (show_window) {
            DisplayWindowConfig display_cfg;
            display_cfg.enabled = true;
            display_cfg.screen_width = display_screen_w;
            display_cfg.screen_height = display_screen_h;
            display_cfg.max_screen_ratio =
                std::max(10, std::min(100, display_max_ratio_percent)) / 100.0f;
            display_cfg.fullscreen = display_fullscreen;
            display_cfg.title_bar_reserve_px = display_title_reserve;
            display = CreateOpenCVDisplaySink(display_cfg);
            display->Prepare();
        }

        if (!coordinator.Init("yolo", base_adapter, yolo_model_path, npu_cores, infer_threads)) {
            LogError("MainROS: ModelCoordinator init failed for yolo");
            return -1;
        }

        Pipeline pipeline(coordinator, infer_threads, single_thread);
        pipeline.SetExternalStopFlag(&g_stop_requested);

        pipeline.RegisterFactory("scrfd",
                                 [scrfd_conf_th, scrfd_nms_th]() {
                                     auto adapter = std::make_shared<ScrfdAdapter>();
                                     adapter->SetThresholds(scrfd_conf_th, scrfd_nms_th);
                                     return adapter;
                                 },
                                 scrfd_model_path);
        pipeline.SetSwitchDebounceThresholds(switch_present_threshold, switch_absent_threshold);

        pipeline.SetFrameIO(
            [&camera](cv::Mat& frame) { return camera.ReadFrame(frame, &g_stop_requested); },
            [&frame_transform](cv::Mat& frame) { frame_transform.Apply(frame); },
            [&frame_transform](const cv::Mat& frame, int frame_id) {
                return frame_transform.Validate(frame, frame_id);
            });

        // ==============================================================
        // ★ PostTaskHandler：coordinator 更新 + ROS 发布 + 可选显示
        // ==============================================================
        pipeline.SetPostTaskHandler([&](InferenceTask& task) -> bool {
            if (task.frame_id == -1) {
                return false;
            }

            // 1. 更新场景机（与 main.cc 一致）。
            coordinator.UpdateAfterFrame(task.merged_signals, task.original_frame);

            // 2. ★ ROS 发布（唯一新增行）。
            ros_bridge.PublishTask(task);

            // 3. 可选 OpenCV 显示。
            if (show_window && display && !task.original_frame.empty()) {
                const bool suppress_yolo_person = coordinator.ShouldSuppressYoloPersonDraw();
                for (const auto& slot_result : task.slot_results) {
                    const bool suppress = (slot_result.slot == "yolo") && suppress_yolo_person;
                    overlay.Apply(task.original_frame, slot_result.result_json, suppress);
                }
                overlay.DrawModelBadge(task.original_frame, coordinator.GetEnabledSlotsBadge());

                cv::Mat display_frame = task.original_frame.isContinuous()
                    ? task.original_frame : task.original_frame.clone();
                display->Show(display_frame);
            }

            return true;
        });

        pipeline.SetIdleHandler([&]() {
            if (show_window && display) {
                const int key = display->PollKey(1);
                if (key == 27) {  // ESC → 退出。
                    g_stop_requested.store(true);
                }
            }
        });

        pipeline.SetOnStop([&coordinator]() {
            coordinator.GetLlmGreeting().AbortActiveGeneration();
        });

        LogInfo("MainROS: warming up scrfd slot...");
        coordinator.WarmupSlot("scrfd");

        LogInfo("MainROS: yolo=%s scrfd=%s infer_threads=%d llm=%s",
                yolo_model_path.c_str(), scrfd_model_path.c_str(), infer_threads,
                llm_enabled ? "on" : "off");

        pipeline.Run();

        // 清理。
        if (show_window && display) {
            display->Shutdown();
        }
        camera.Release();
        if (tts_worker) {
            tts_worker->Shutdown();
        }
        if (llm_worker) {
            llm_worker->Shutdown();
        }
    } catch (const std::exception& e) {
        LogError("Pipeline failed: %s", e.what());
        std::cerr << "Pipeline failed: " << e.what() << std::endl;
        return -1;
    }

    return 0;
}