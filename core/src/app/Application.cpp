// Application.cpp — 应用生命周期实现
#include "app/Application.hpp"

#include <atomic>
#include <chrono>
#include <cmath>
#include <cstring>
#include <fstream>
#include <filesystem>
#include <sstream>
#include <thread>

#include "common/Logger.hpp"
#include "common/CpuAffinity.hpp"
#include "model/ModelManagement.hpp"
#include "model/ModelAdapter.hpp"
#ifdef TTBOX_CORE_HAS_RKNN
#include "rknn/RKNNEngine.hpp"
#endif
#include "output/AiboxHidOutput.hpp"
#include "output/FifoHidOutput.hpp"
#include "output/OutputBackend.hpp"
#include "ttbox/core/version.hpp"

#include <cstdio>

namespace ttbox::core {

namespace {

std::atomic<bool> g_shutdown_requested{false};
std::atomic<bool>& shutdown_flag() { return g_shutdown_requested; }

#ifndef TTBOX_PROJECT_ROOT
#define TTBOX_PROJECT_ROOT "."
#endif
const char* kDefaultConfigPath = TTBOX_PROJECT_ROOT "/config/default.json";
const char* kSystemLicenseFile = "/etc/ttbox/license.key";

double now_ms() {
    using clock = std::chrono::steady_clock;
    return static_cast<double>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            clock::now().time_since_epoch())
            .count());
}

LogLevel parse_log_level(const std::string& s) {
    if (s == "debug") return LogLevel::kDebug;
    if (s == "warn") return LogLevel::kWarn;
    if (s == "error") return LogLevel::kError;
    if (s == "off") return LogLevel::kOff;
    return LogLevel::kInfo;
}

int parse_color_order(const std::string& s) {
    if (s == "rgb") return 1;
    return 0;
}

std::vector<int> parse_worker_cores(const std::string& s) {
    // RK3588 生产配置固定三核；保留参数仅用于兼容旧配置读取。
    (void)s;
    return {1, 2, 4};
}

std::string strip(const std::string& s) {
    size_t a = 0, b = s.size();
    while (a < b && std::isspace(static_cast<unsigned char>(s[a]))) ++a;
    while (b > a && (std::isspace(static_cast<unsigned char>(s[b-1])) ||
                     s[b-1] == '\n' || s[b-1] == '\r')) --b;
    return s.substr(a, b - a);
}

}  // namespace

Application::~Application() {
    if (running_.load() || initialized_) {
        try { shutdown(); } catch (...) {}
    }
}

std::string Application::resolve_license_card(const std::string& cli_license) const {
    if (!cli_license.empty()) return cli_license;

    // 系统路径 /etc/ttbox/license.key：与原 aibox /etc/aibox/ 1:1 对齐语义
    {
        std::ifstream f(kSystemLicenseFile);
        if (f) {
            std::string s;
            std::getline(f, s);
            s = strip(s);
            if (!s.empty()) return s;
        }
    }

    // 配置 fallback（开发期）
    const std::string cfg = config_.get_string("license_card_key", "");
    if (!cfg.empty()) return cfg;
    return {};
}

bool Application::build_runtime_params(CoreRuntime::Params& out_params,
                                       std::string* error) {
    (void)error;
    out_params.capture.device =
        config_.get_string("capture_device", "/dev/video0");
    out_params.capture.num_buffers =
        static_cast<uint32_t>(config_.get_int("capture_buffers", 4));
    out_params.capture.poll_timeout_ms =
        config_.get_int("capture_poll_timeout_ms", 1000);

    out_params.workers.model_path = config_.get_string("model_path", "");
    if (model_management_) {
        const std::string selected = model_management_->registry().active_model();
        if (selected.empty()) {
            if (error) *error = "MODEL_NOT_SELECTED";
            return false;
        }
        const auto records = model_management_->registry().records();
        const ModelRecord* selected_record = nullptr;
        for (const auto& record : records) {
            if (record.model_id == selected) {
                selected_record = &record;
                break;
            }
        }
        if (selected_record == nullptr) {
            if (error) *error = "MODEL_NOT_FOUND: " + selected;
            return false;
        }
        if (selected_record->status == ModelStatus::kInvalid ||
            selected_record->status == ModelStatus::kFailed ||
            selected_record->path.empty()) {
            if (error) {
                *error = selected_record->failure_code.empty()
                             ? "MODEL_METADATA_INVALID: " + selected
                             : selected_record->failure_code + ": " + selected_record->failure_message;
            }
            return false;
        }
        out_params.workers.model_path = selected_record->path;
        if (selected_record->manifest.input_width > 0) {
            out_params.workers.out_w = selected_record->manifest.input_width;
        }
        if (selected_record->manifest.input_height > 0) {
            out_params.workers.out_h = selected_record->manifest.input_height;
        }
    }
    if (out_params.workers.model_path.empty()) {
        if (error) *error = "MODEL_NOT_FOUND: model_path 为空";
        return false;
    }
    out_params.workers.worker_cores =
        parse_worker_cores(config_.get_string("worker_cores", ""));
    if (out_params.workers.out_w == 0) {
        out_params.workers.out_w =
            static_cast<uint32_t>(config_.get_int("model_input_width", 640));
    }
    if (out_params.workers.out_h == 0) {
        out_params.workers.out_h =
            static_cast<uint32_t>(config_.get_int("model_input_height", 640));
    }
    out_params.preview.fps = static_cast<int>(config_.get_int("preview_fps", 60));
    out_params.preview.crop_width = static_cast<uint32_t>(config_.get_int("crop_width", 640));
    out_params.preview.crop_height = static_cast<uint32_t>(config_.get_int("crop_height", 640));
    out_params.preview.jpeg_quality = static_cast<int>(config_.get_int("preview_quality", 60));
    out_params.preview.draw_detections = config_.get_bool("preview_draw_detections", false);
    out_params.workers.conf_thres =
        static_cast<float>(config_.get_double("conf", 0.25));
    out_params.workers.iou_thres =
        static_cast<float>(config_.get_double("nms", 0.45));
    out_params.workers.color_order =
        parse_color_order(config_.get_string("model_color_order", "bgr"));
    out_params.workers.pass_through =
        config_.get_bool("model_pass_through", true);
    // 第13阶段：链路诊断开关（默认关闭；开启后每 N 帧输出一次完整链路）
    out_params.pipeline_debug_enabled =
        config_.get_bool("pipeline_debug_enabled", false);
    out_params.pipeline_debug_interval =
        static_cast<uint32_t>(config_.get_int("pipeline_debug_interval", 60));
    // 第13阶段：PID Trace 采集（默认关闭；开启后逐帧写 CSV，只记录不改变行为）
    out_params.pid_trace_enabled = config_.get_bool("pid_trace_enabled", false);
    out_params.pid_trace_path = config_.get_string("pid_trace_path", "/tmp/pid_trace.csv");
    // 第15阶段：目标预测时域（秒；0=关闭预测，保持原行为）
    out_params.prediction_time_s =
        static_cast<float>(config_.get_double("prediction_time_s", 0.0));

    const std::string output_kind = config_.get_string("output_backend", "aibox");
    bool enabled = config_.get_bool("output_enabled", false);
    if (output_kind == "fifo") {
        const std::string fifo_path =
            config_.get_string("output_fifo_path", "/tmp/ttbox_hid.fifo");
        hid_output_ = std::make_shared<output::FifoHidOutput>(fifo_path);
    } else if (output_kind == "local_hid" || output_kind == "usb_proxy" ||
               output_kind == "kmboxnet" || output_kind == "makcu" || output_kind == "ferrum" ||
               output_kind == "kmboxb") {
        // 统一 OutputBackend：按 kind 选择后端，行为与 AiboxHidOutput 完全一致
        // （local_hid 即原 aibox 逻辑迁移；kmboxnet/makcu/ferrum/kmboxb 后续接入）。
        auto backend = std::make_shared<output::OutputBackend>();
        output::OutputBackend::Params bp;
        bp.kind = output_kind;
        bp.hidg_path = config_.get_string("output_hidg_path", "/dev/hidg0");
        bp.proxy_socket_path = config_.get_string("output_proxy_socket", "/run/orangepi-mouse-passthrough/cmd.sock");
        bp.enabled = enabled;
        bp.runtime_config = &runtime_config_;
        // button_source 由 Application::start 阶段绑定（见 add_hid_button_source 处）
        std::string berr;
        if (!backend->configure(bp, &berr)) {
            TTBOX_LOG_WARN("OutputBackend 配置失败（回退 aibox）: " + berr);
        } else {
            hid_output_ = std::move(backend);
        }
    }
    if (!hid_output_) {
        const std::string hidg_path =
            config_.get_string("output_hidg_path", "/dev/hidg0");
        auto output = std::make_shared<output::AiboxHidOutput>(hidg_path);
        // output_enabled 是后端静态总闸（不写配置时默认关闭，fail-closed）。
        // mouse.enabled 由 AimThread 与输出后端每周期实时读取 RuntimeConfig，
        // 不在此快照 —— 用户改配置后无需重启即生效。
        output->set_enabled(enabled);
        output->set_config_source(&runtime_config_);
        hid_output_ = std::move(output);
    }
    out_params.output = hid_output_;
    out_params.runtime_config = &runtime_config_;
    return true;
}

int Application::initialize(int argc, char** argv) {
    std::string cli_license;
    std::string cli_secret;
    bool verify_only = false;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        auto consume_value = [&](const char* name) -> bool {
            if (i + 1 >= argc) {
                TTBOX_LOG_ERROR(std::string("缺少参数值: ") + name);
                return false;
            }
            ++i;
            return true;
        };
        if (arg == "--config") {
            if (!consume_value("--config")) return 1;
            config_path_ = argv[i];
        } else if (arg == "--ipc") {
            if (!consume_value("--ipc")) return 1;
            ipc_path_ = argv[i];
        } else if (arg == "--log-level") {
            if (!consume_value("--log-level")) return 1;
            Logger::instance().set_level(parse_log_level(argv[i]));
        } else if (arg == "--license") {
            if (!consume_value("--license")) return 1;
            cli_license = argv[i];
        } else if (arg == "--license-server-secret") {
            if (!consume_value("--license-server-secret")) return 1;
            cli_secret = argv[i];
        } else if (arg == "--debug-license-pro-endpoint") {
            if (!consume_value("--debug-license-pro-endpoint")) return 1;
            license_override_pro_endpoint_ = argv[i];
        } else if (arg == "--debug-license-normal-endpoint") {
            if (!consume_value("--debug-license-normal-endpoint")) return 1;
            license_override_normal_endpoint_ = argv[i];
        } else if (arg == "--verify-only") {
            verify_only = true;
        } else if (arg == "--help" || arg == "-h") {
            TTBOX_LOG_INFO(
                "用法: ttbox_core [--config <path>] [--ipc <path>]\n"
                "                [--log-level debug|info|warn|error|off]\n"
                "                [--license <card>] [--license-server-secret <secret>]\n"
                "                [--verify-only]\n"
                "                [--debug-license-pro-endpoint <host>]\n"
                "                [--debug-license-normal-endpoint <host>]");
            return 1;
        } else {
            TTBOX_LOG_WARN(std::string("忽略未知参数: ") + arg);
        }
    }

    Logger::instance().add_sink(std::make_shared<ConsoleSink>());
    TTBOX_LOG_INFO("=== " + std::string(kAppName) + " v" +
                   std::string(kVersion) + " 启动 ===");

    // ---- 风扇满转（fan_control min_pwm=100）：防热节流拖慢 NPU ----
    {
        std::ofstream pwm("/sys/class/hwmon/hwmon8/pwm1");
        if (pwm) {
            pwm << 255;
            TTBOX_LOG_INFO("风扇已设满转（防热节流）");
        } else {
            TTBOX_LOG_WARN("风扇控制不可用（hwmon8/pwm1）");
        }
    }


    if (config_path_.empty()) config_path_ = kDefaultConfigPath;
    std::string cfg_error;
    if (!config_.load(config_path_, &cfg_error)) {
        TTBOX_LOG_ERROR(cfg_error);
        TTBOX_LOG_ERROR("配置加载失败，拒绝启动");
        return 1;
    }
    TTBOX_LOG_INFO("配置已加载: " + config_path_);

    // ---- CPU 调频策略（实测：CPU 占用仅 15%，NPU 才是主力且频率独立）----
    // governor=schedutil（动态调频：忙时自动满频，闲时降频降温）+ min 下限 50% 防深睡。
    // 实测与 performance+锁死 性能完全一致（e2e/fps 无差异），温度更低。
    {
        for (const char* pol : {"policy0", "policy4", "policy6"}) {
            std::ofstream g(std::string("/sys/devices/system/cpu/cpufreq/") + pol + "/scaling_governor");
            if (g) {
                g << "schedutil";
                if (!g.good()) TTBOX_LOG_WARN(std::string("governor 切换失败: ") + pol);
            }
        }
        const int pct = static_cast<int>(config_.get_int("cpu_min_freq_percent", 50));
        auto fr = CpuAffinity::lock_min_freq_percent(pct);
        if (fr.freq_ok) {
            TTBOX_LOG_INFO("CPU 调频策略完成（schedutil+min" + std::to_string(pct) + "%）: " + fr.detail);
        } else {
            TTBOX_LOG_WARN("CPU 调频策略部分失败: " + fr.detail);
        }
    }

    // ---- 3. 授权层初始化（等价 cardVerifyThreadFunc）----
    license_client_ = std::make_unique<auth::AiboxLicenseClient>();
    if (!license_override_pro_endpoint_.empty() ||
        !license_override_normal_endpoint_.empty()) {
        license_client_->override_endpoint(license_override_pro_endpoint_,
                                            license_override_normal_endpoint_);
    }
    license_server_secret_ = cli_secret.empty()
                                 ? config_.get_string("license_server_secret", "")
                                 : cli_secret;
    license_daemon_ = std::make_unique<auth::LicenseDaemon>(*license_client_);
    const std::string card = resolve_license_card(cli_license);
    if (!card.empty()) {
        license_daemon_->set_card(card);
        TTBOX_LOG_INFO("授权卡号已加载 (prefix: " +
                       card.substr(0, std::min<size_t>(8, card.size())) + "...)");
    }
    // 开发模式：没有卡号时允许 --license-server-secret 为空，后续 --verify-only 可快速失败
    verify_only_ = verify_only;
    if (!license_daemon_->start()) {
        TTBOX_LOG_ERROR("授权线程启动失败");
        return 1;
    }
    // --verify-only 模式：立即同步触发一次，打印结果后退出，不进入推理
    if (verify_only_) {
        std::string err;
        bool ok = license_daemon_->verify_now_blocking(&err);
        auto st = license_daemon_->status_snapshot();
        TTBOX_LOG_INFO(std::string("verify-only: ok=") + (ok ? "true" : "false") +
                       " state=" + std::to_string(static_cast<int>(st.state)) +
                       " is_pro=" + (st.is_pro ? "true" : "false") +
                       " error=" + (st.last_error.empty() ? err : st.last_error));
        // verify-only 模式：无论结果如何，打印后直接退出
        license_daemon_->stop();
        return (ok && (st.state == auth::LicenseState::kValid ||
                       st.state == auth::LicenseState::kFallback))
                   ? 0
                   : 2;
    }

    // ---- 4. 加载 RuntimeProfile：让配置文件真正进入 Worker/AimThread ----
    if (const JsonValue* profile_json = config_.root().find("runtime_profile")) {
        RuntimeProfile profile = RuntimeProfile::from_json(*profile_json);
        std::string profile_error;
        if (!profile.validate(&profile_error)) {
            TTBOX_LOG_ERROR("RuntimeProfile 校验失败: " + profile_error);
            return 1;
        }
        runtime_config_.update(profile);
        TTBOX_LOG_INFO("RuntimeProfile 已加载");
    }

    // ---- 模型管理（v0.3）：先初始化 Registry，再构建 Runtime 参数 ----
    {
        const std::string reg_root = config_.get_string("model_registry_root", "");
        model_management_ = std::make_unique<ModelManagement>(
            ModelRegistryOptions{reg_root, true});
        std::string mm_error;
        if (!model_management_->init(&mm_error)) {
            TTBOX_LOG_ERROR("ModelRegistry 初始化失败: " + mm_error);
            model_management_.reset();
            return 1;
        }
#ifdef TTBOX_CORE_HAS_RKNN
        model_management_->set_validator([](const std::string& rknn_path, JsonValue* meta_out,
                                            std::string* error) -> bool {
            std::error_code fec;
            if (!std::filesystem::exists(rknn_path, fec)) {
                if (error) *error = "模型文件不存在: " + rknn_path;
                return false;
            }
            RKNNEngine probe;
            RKNNEngine::Params pp;
            pp.model_path = rknn_path;
            pp.core_mask = 0;
            std::string perr;
            if (!probe.init(pp, &perr)) {
                if (error) *error = "RKNN 探测加载失败: " + perr;
                return false;
            }
            ModelAdapter adapter;
            ModelAdapterConfig adapter_cfg;
            std::string adapter_error;
            if (!adapter.analyze(probe.info(), adapter_cfg, &adapter_error)) {
                if (error) *error = "ModelAdapter 分析失败: " + adapter_error;
                return false;
            }
            const ModelMetadata& metadata = adapter.metadata();
            if (metadata.class_count == 0) {
                if (error) *error = "METADATA_INVALID: RKNN 输出无法确定 class_count";
                return false;
            }
            // ---- 真实冒烟推理（入库硬门槛）----
            // 用合成输入（中性灰 128）真实跑一次 NPU 推理 + Decode：
            //  - 推理失败 → SMOKE_INFERENCE_FAILED（NPU 不兼容/内存不足等在此暴露）
            //  - Decode 崩溃或输出 NaN/Inf → DECODE_INVALID（解码器与输出结构不匹配在此暴露）
            // 冒烟通过不要求检出目标（合成图没有真目标），只要求"跑得动、解得动、数值合法"。
            {
                const uint32_t in_bytes = probe.info().input_size > 0
                                              ? probe.info().input_size
                                              : static_cast<uint32_t>(metadata.input_width *
                                                                      metadata.input_height * 3);
                std::vector<uint8_t> smoke_input(in_bytes, 128);  // 中性灰，避免极端激活
                std::string smoke_error;
                // 与生产路径完全一致：set_input（原生字节）→ run → get_raw_outputs（原生输出）
                if (!probe.set_input(smoke_input.data(), smoke_input.size(), &smoke_error) ||
                    !probe.run(&smoke_error)) {
                    if (error) *error = "SMOKE_INFERENCE_FAILED: " + smoke_error;
                    probe.destroy();
                    return false;
                }
                const uint32_t n_out = probe.info().n_outputs;
                std::vector<std::vector<uint8_t>> raw_bufs;
                std::vector<void*> out_ptrs(n_out, nullptr);
                std::vector<size_t> out_sizes(n_out, 0);
                for (uint32_t i = 0; i < n_out; ++i) {
                    const uint32_t sz = probe.info().output_sizes[i];
                    raw_bufs.emplace_back(sz, 0);
                    out_ptrs[i] = raw_bufs.back().data();
                    out_sizes[i] = sz;
                }
                if (!probe.get_raw_outputs(out_ptrs.data(), out_sizes.data(), &smoke_error)) {
                    if (error) *error = "SMOKE_INFERENCE_FAILED: " + smoke_error;
                    probe.destroy();
                    return false;
                }
                // Decode 合法性：用与生产一致的 Decoder 跑冒烟输出
                auto decoder = adapter.create_decoder(&adapter_error);
                if (!decoder) {
                    if (error) *error = "UNSUPPORTED_OUTPUT: " + adapter_error;
                    probe.destroy();
                    return false;
                }
                std::vector<DetectionBox> smoke_dets;
                if (!decoder->process(probe.info(), out_ptrs.data(), &smoke_dets,
                                      &adapter_error)) {
                    if (error) *error = "DECODE_INVALID: " + adapter_error;
                    probe.destroy();
                    return false;
                }
                // 数值合法性：不允许 NaN/Inf 混入解码结果
                for (const auto& d : smoke_dets) {
                    const float v[4] = {d.x1, d.y1, d.x2, d.y2};
                    for (float x : v) {
                        if (!std::isfinite(x)) {
                            if (error) *error = "DECODE_INVALID: 检测框含 NaN/Inf";
                            probe.destroy();
                            return false;
                        }
                    }
                    if (!std::isfinite(d.score) || d.score < 0.0f || d.score > 1.0f) {
                        if (error) *error = "DECODE_INVALID: 置信度越界";
                        probe.destroy();
                        return false;
                    }
                }
                if (meta_out) {
                    meta_out->set("smoke_inference", JsonValue::boolean(true));
                }
            }
            if (meta_out) {
                JsonValue obj = JsonValue::object();
                obj.set("input_width", JsonValue::number(static_cast<double>(metadata.input_width)));
                obj.set("input_height", JsonValue::number(static_cast<double>(metadata.input_height)));
                obj.set("input_channels", JsonValue::number(static_cast<double>(metadata.input_channels)));
                obj.set("input_dtype", JsonValue::number(static_cast<double>(metadata.input_dtype)));
                obj.set("input_layout", JsonValue::number(static_cast<double>(metadata.input_layout)));
                obj.set("quantization", JsonValue::number(static_cast<double>(static_cast<int>(metadata.quantization_type))));
                obj.set("output_count", JsonValue::number(static_cast<double>(metadata.output_count)));
                obj.set("class_count", JsonValue::number(static_cast<double>(metadata.class_count)));
                obj.set("decode_type", JsonValue::string(ModelAdapter::decode_type_name(metadata.decode_type)));
                obj.set("objectness", JsonValue::boolean(metadata.objectness));
                obj.set("dfl", JsonValue::boolean(metadata.dfl));
                JsonValue strides = JsonValue::array();
                for (uint32_t stride : metadata.strides) strides.push_back(JsonValue::number(static_cast<double>(stride)));
                obj.set("strides", std::move(strides));
                JsonValue output_shapes = JsonValue::array();
                for (const auto& shape : metadata.output_shapes) {
                    JsonValue dims = JsonValue::array();
                    for (uint32_t dim : shape) dims.push_back(JsonValue::number(static_cast<double>(dim)));
                    output_shapes.push_back(std::move(dims));
                }
                obj.set("output_shapes", std::move(output_shapes));
                *meta_out = std::move(obj);
            }
            probe.destroy();
            return true;
        });
#else
        model_management_->set_validator(ModelManagement::file_level_validator);
#endif
        if (!model_management_->registry().refresh(&mm_error)) {
            TTBOX_LOG_ERROR("ModelRegistry 刷新失败: " + mm_error);
            model_management_.reset();
            return 1;
        }
        TTBOX_LOG_INFO("ModelRegistry 已就绪: " + model_management_->registry().root_dir());
    }

    // ---- 5. 构建 CoreRuntime 参数 & 初始化 ----
    core_runtime_ = std::make_unique<CoreRuntime>();
    CoreRuntime::Params rt_params{};
    std::string rt_error;
    if (!build_runtime_params(rt_params, &rt_error)) {
        TTBOX_LOG_ERROR("CoreRuntime 参数构建失败: " + rt_error);
        return 1;
    }
    if (!core_runtime_->initialize(rt_params, &rt_error)) {
        TTBOX_LOG_ERROR("CoreRuntime 初始化失败: " + rt_error);
        return 1;
    }
    TTBOX_LOG_INFO("CoreRuntime 初始化完成 (workers=" +
                   std::to_string(rt_params.workers.worker_cores.size()) + ")");

    // ---- 5. 启动 IPC 服务 ----
    ipc_.set_status_provider([this] { return status_provider(); });
    ipc_.set_preview_provider([this](std::vector<uint8_t>* out, uint64_t* seq) {
        const bool ok = core_runtime_ && core_runtime_->preview() && core_runtime_->preview()->running()
                   ? core_runtime_->preview()->snapshot(out)
                   : false;
        if (ok && seq) {
            *seq = core_runtime_->preview()->metrics().frames.load();
        }
        return ok;
    });
    ipc_.set_config_provider([this] { return config_provider(); });
    ipc_.set_config_update_handler(
        [this](const JsonValue& profile_json, std::string* error, bool* persisted) {
            return handle_config_update(profile_json, error, persisted);
        });
    ipc_.set_runtime_control_handler(
        [this](const std::string& action, std::string* error) {
            return handle_runtime_control(action, error);
        });

    if (model_management_) {
        ipc_.set_model_list_handler([this] { return handle_model_list(); });
        ipc_.set_model_import_handler(
            [this](const std::string& src, const std::string& id,
                   const std::string& label, const std::string& source_format,
                   const std::string& sha256, std::string* error) {
                return handle_model_import(src, id, label, source_format, sha256, error);
            });
        ipc_.set_model_validate_handler(
            [this](const std::string& id, std::string* error) {
                return handle_model_validate(id, error);
            });
        ipc_.set_model_install_handler(
            [this](const std::string& id, std::string* error) {
                return handle_model_install(id, error);
            });
        ipc_.set_model_activate_handler(
            [this](const std::string& id, std::string* error) {
                return handle_model_activate(id, error);
            });
        ipc_.set_model_remove_handler(
            [this](const std::string& id, std::string* error) {
                return handle_model_remove(id, error);
            });
    }
    std::string ipc_error;
    if (!ipc_.start(ipc_path_, &ipc_error)) {
        TTBOX_LOG_ERROR("IPC 启动失败: " + ipc_error);
        return 1;
    }

    start_time_ms_ = now_ms();
    initialized_ = true;
    return 0;
}

void Application::run() {
    if (!initialized_) {
        TTBOX_LOG_ERROR("Application 未初始化，拒绝 run()");
        return;
    }
    running_.store(true);

    // 授权门控：先等待一次立即验卡；允许的状态：kValid 或 Fallback
    // 超过 60s 仍未通过 → 打印但仍然继续（开发期离线）
    {
        std::string err;
        (void)license_daemon_->verify_now_blocking(&err);
        int waited = 0;
        while (!license_daemon_->allow_run() && waited < 60) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            ++waited;
        }
        auto st = license_daemon_->status_snapshot();
        if (!license_daemon_->allow_run()) {
            TTBOX_LOG_WARN("授权未通过，但继续启动（离线开发模式/未填卡）；推理结果仍会按配置过滤");
        } else {
            TTBOX_LOG_INFO(std::string("授权通过 state=") +
                           std::to_string(static_cast<int>(st.state)) +
                           " is_pro=" + (st.is_pro ? "true" : "false"));
        }
    }

    // ---- 自动启动 AI 流水线 ----
    // 语义：want_runtime_running_=true 时，尽力保持 runtime 运行。
    //   1) 首次启动：先立即尝试一次；失败则进入后台重试（HDMI 未锁定 / V4L2 CMA 碎片
    //      是板端常见瞬时故障，几秒后即可恢复）。
    //   2) 运行中崩溃/退出：主循环每 tick 检测到 runtime 停但 want=true 时自动重启。
    // 用户 /api/control/stop 会把 want 置 false，此后不再自动拉起。
    std::string rt_error;
    if (want_runtime_running_.load()) {
        if (core_runtime_ && core_runtime_->start(&rt_error)) {
            runtime_started_ = true;
            model_failure_code_.clear();
            TTBOX_LOG_INFO("CoreRuntime 已启动，等待首帧真实 RKNN 推理与 Decode");
        } else {
            model_failure_code_ = rt_error.empty() ? "RKNN_INIT_FAILED" : rt_error;
            TTBOX_LOG_WARN("CoreRuntime 首次启动失败，进入后台自动重试: " + model_failure_code_);
            runtime_started_ = false;
        }
    } else {
        runtime_started_ = false;
    }

    constexpr auto kTickMs = std::chrono::milliseconds(50);
    constexpr auto kHeartbeatSec = std::chrono::seconds(10);
    auto last_heartbeat = std::chrono::steady_clock::now();
    // 启动失败重试间隔（与 heartbeat 解耦：重试更激进，HDMI 恢复后 ~2s 内拉起）
    constexpr auto kRetryInterval = std::chrono::seconds(2);
    auto last_retry = std::chrono::steady_clock::now();
    while (!shutdown_flag().load()) {
        std::this_thread::sleep_for(kTickMs);
        // 授权失效时仍保持进程存活（通过 supervisor recover() 重启恢复），不主动自杀
        const auto now = std::chrono::steady_clock::now();
        // 自动启停核心：want=true 但 runtime 没在跑 → 每 2s 重试一次（首次失败自恢复/崩溃自拉起）
        if (want_runtime_running_.load() && core_runtime_ && !core_runtime_->running()) {
            if (now - last_retry >= kRetryInterval) {
                last_retry = now;
                std::string retry_error;
                if (core_runtime_->start(&retry_error)) {
                    runtime_started_ = true;
                    model_failure_code_.clear();
                    TTBOX_LOG_INFO("CoreRuntime 自动重试成功，等待真实模型首帧");
                } else if (!retry_error.empty()) {
                    model_failure_code_ = retry_error;
                    TTBOX_LOG_DEBUG("CoreRuntime 自动重试中: " + retry_error);
                }
            }
        }
        if (core_runtime_ && core_runtime_->model_ready()) {
            const std::string selected = model_management_ ? model_management_->registry().active_model() : "";
            if (!selected.empty() && running_model_id_ != selected) {
                running_model_id_ = selected;
                model_failure_code_.clear();
                // 模型实际就绪（含启动恢复/自动重试）时，同步 runtime_profile.model_id，
                // 消除「配置显示 B 实际跑 A」——active.json 与 runtime_profile 永远一致。
                sync_model_id_to_profile(selected);
                TTBOX_LOG_INFO("真实模型已就绪: running_model_id=" + running_model_id_ +
                               " inference+decode=PASS");
            }
        } else if (core_runtime_ && core_runtime_->running() && core_runtime_->model_errors() > 0) {
            if (model_failure_code_.empty()) model_failure_code_ = "INFERENCE_FAILED_OR_OUTPUT_INVALID";
        }
        if (now - last_heartbeat >= kHeartbeatSec) {
            last_heartbeat = now;
            const bool rt_ok = core_runtime_ ? core_runtime_->running() : false;
            auto st = license_daemon_->status_snapshot();
            TTBOX_LOG_DEBUG(std::string("heartbeat: runtime=") +
                            (rt_ok ? "running" : "stopped") +
                            " license_state=" +
                            std::to_string(static_cast<int>(st.state)));
        }
    }
    TTBOX_LOG_INFO("收到退出请求，正在停止...");
}

void Application::shutdown() {
    const bool was_running = running_.exchange(false);
    if (was_running) TTBOX_LOG_INFO("Application shutdown() 开始");

    if (runtime_started_ && core_runtime_) {
        TTBOX_LOG_INFO("停止 CoreRuntime...");
        core_runtime_->stop();
        runtime_started_ = false;
    }
    core_runtime_.reset();
    hid_output_.reset();

    // 授权线程停在 IPC 之后（让 IPC 最后一个响应仍能拿到授权快照）
    if (license_daemon_) {
        license_daemon_->stop();
        license_daemon_.reset();
    }
    license_client_.reset();

    ipc_.stop();
    initialized_ = false;
    TTBOX_LOG_INFO("=== " + std::string(kAppName) + " 已退出 ===");
}

void Application::request_shutdown() {
    shutdown_flag().store(true);
}

SystemStatus Application::status() const { return status_provider(); }

auth::LicenseStatus Application::license_status_snapshot() const {
    return license_daemon_ ? license_daemon_->status_snapshot()
                           : auth::LicenseStatus{};
}
bool Application::license_allow_run() const {
    return license_daemon_ && license_daemon_->allow_run();
}
bool Application::license_is_pro() const {
    return license_daemon_ && license_daemon_->is_pro();
}

SystemStatus Application::status_provider() const {
    SystemStatus st;
    st.running = running_.load();
    st.app_name = kAppName;
    st.version = kVersion;
    if (start_time_ms_ > 0.0) st.uptime_ms = now_ms() - start_time_ms_;
    st.ipc_socket = ipc_.socket_path();
    st.config_file = config_.path();
    // 当前模型：优先实际运行中的模型，其次激活模型（active.json 真相源）
    if (model_management_) {
        st.current_model_id = model_management_->registry().active_model();
    }
    st.runtime_running = core_runtime_ ? core_runtime_->running() : false;
    // G1：真实流水线指标（runtime 未运行时保持全 0 = unavailable）
    if (core_runtime_) {
        core_runtime_->collect_metrics(&st.metrics);
    }
    return st;
}

JsonValue Application::config_provider() const {
    // G4 契约：Web 需要 RuntimeProfile 结构（前端 ConfigContext 深拷贝改字段 → 全量 PUT）。
    // 数据源优先级（唯一真源 = RuntimeConfig 内存 canonical）：
    //   1) runtime_config_ 内存快照（SET_CONFIG 热更新后的最新值）
    //   2) 回退到宿主配置文件的 runtime_profile 键（启动时来源）
    JsonValue data = JsonValue::object();
    JsonValue prof = JsonValue::object();
    if (auto snap = runtime_config_.snapshot()) {
        prof = snap->to_json();
    } else if (config_.loaded()) {
        const JsonValue* p = config_.root().find("runtime_profile");
        if (p != nullptr && p->is_object()) {
            prof = *p;
        }
    }
    data.set("runtime_profile", prof);
    data.set("config_file", JsonValue::string(config_.path()));
    return data;
}

// SET_CONFIG 原子更新：
//   JSON 解析 → RuntimeProfile::validate → RuntimeConfig.update（内存原子替换）→ 持久化。
// 任何一步失败都直接返回 false；内存与磁盘均保证不被污染。
bool Application::handle_config_update(const JsonValue& profile_json, std::string* error,
                                       bool* persisted) {
    if (persisted) *persisted = false;
    if (!profile_json.is_object()) {
        if (error) *error = "profile 必须是 JSON 对象";
        return false;
    }

    // 1) 解析（严格：非法字段/类型错误 → 失败）
    RuntimeProfile profile = RuntimeProfile::from_json(profile_json);
    std::string validate_error;
    if (!profile.validate(&validate_error)) {
        if (error) *error = validate_error.empty() ? "profile 校验失败" : ("profile 校验失败: " + validate_error);
        return false;
    }

    // 2) 内存热更新（原子替换 shared_ptr；AimThread/Worker 下个周期即读到新配置）
    runtime_config_.update(profile);

    // 3) 持久化：读回宿主配置文件（config_ 的 root），仅替换 runtime_profile 键，
    //    其余键（app/conf/nms/...）原样保留；保存失败不撤销内存更新（热更新已生效），
    //    以 persisted=false 告知调用方“内存已应用但落盘失败”。
    bool saved = false;
    if (!config_path_.empty() && config_.loaded()) {
        JsonValue merged = config_.root();  // 深拷贝宿主 JSON
        merged.set("runtime_profile", profile_json);
        const std::string text = merged.dump();
        FILE* f = std::fopen(config_path_.c_str(), "w");
        if (f) {
            const bool ok = std::fwrite(text.data(), 1, text.size(), f) == text.size();
            if (std::fclose(f) != 0 && ok) {
                saved = false;
            } else {
                saved = ok;
            }
        }
        if (!saved && error) {
            *error = "内存配置已生效，但写入配置文件失败: " + config_path_;
        }
    }
    if (persisted) *persisted = saved;
    return true;
}

// RUNTIME_CONTROL：start / stop / restart。
// 直接复用 CoreRuntime start/stop（幂等），不触碰平台 RuntimeController 状态机。
bool Application::handle_runtime_control(const std::string& action, std::string* error) {
    if (!core_runtime_) {
        if (error) *error = "core_runtime 未初始化";
        return false;
    }
    if (action == "start") {
        if (core_runtime_->running()) return true;  // 幂等
        want_runtime_running_.store(true);
        if (!core_runtime_->start(error)) {
            if (error && error->empty()) *error = "CoreRuntime 启动失败";
            // 启动失败仍保留 want=true：主循环每 2s 自动重试，直到 HDMI/模型就绪
            return false;
        }
        runtime_started_ = true;
        return true;
    }
    if (action == "stop") {
        want_runtime_running_.store(false);
        core_runtime_->stop();
        runtime_started_ = false;
        running_model_id_.clear();
        model_failure_code_.clear();
        return true;
    }
    if (action == "restart") {
        want_runtime_running_.store(true);
        core_runtime_->stop();
        if (!core_runtime_->start(error)) {
            if (error && error->empty()) *error = "CoreRuntime 重启失败";
            runtime_started_ = false;
            return false;
        }
        runtime_started_ = true;
        return true;
    }
    if (error) *error = "未知 action: " + action;
    return false;
}

// ---- 模型管理（v0.3）实现 ----
// 收件目录：<registry_root>/_incoming。Gateway 上传端点先把文件写到这里，
// 再发 MODEL_IMPORT 引用路径。import 只允许引用收件目录内的文件（防任意路径读取）。
static std::string incoming_dir_of(const ModelRegistry& reg) {
    return reg.root_dir() + "/_incoming";
}

bool Application::handle_model_import(const std::string& src_path, const std::string& model_id,
                                      const std::string& label, const std::string& source_format,
                                      const std::string& sha256, std::string* error) {
    if (!model_management_) {
        if (error) *error = "模型仓库不可用（初始化失败）";
        return false;
    }
    ModelRegistry& reg = model_management_->registry();
    // 路径安全：src_path 必须位于收件目录内（防 ../ 任意文件读取）。
    // Windows 下 fs 返回反斜杠路径，统一归一化成 '/' 再比较。
    auto normalize = [](std::string s) {
        for (char& c : s) if (c == '\\') c = '/';
        return s;
    };
    const std::string incoming = normalize(incoming_dir_of(reg));
    const std::string src_norm = normalize(src_path);
    if (src_norm.rfind(incoming, 0) != 0) {
        if (error) *error = "模型文件必须先上传到收件目录（" + incoming + "）";
        return false;
    }
    ModelManifest manifest;
    manifest.label = label.empty() ? model_id : label;
    manifest.origin = "local";
    // 来源格式：直接上传=rknn（默认）；ONNX 转换产物=onnx。落 manifest 供前端/排查。
    manifest.source_format = (source_format == "onnx") ? "onnx" : "rknn";
    if (!sha256.empty()) manifest.sha256 = sha256;  // Web 层 hashlib 计算（标准实现），Core 只存不算
    if (!reg.import(src_path, model_id, manifest, error)) return false;
    TTBOX_LOG_INFO("模型已导入 staging: " + model_id);
    return true;
}

JsonValue Application::handle_model_list() {
    JsonValue data = JsonValue::object();
    if (!model_management_) {
        data.set("models", JsonValue::array());
        data.set("selected_model_id", JsonValue::string(""));
        data.set("running_model_id", JsonValue::string(""));
        data.set("state", JsonValue::string("failed"));
        data.set("available", JsonValue::boolean(false));
        return data;
    }
    const ModelRegistry& reg = model_management_->registry();
    const std::string selected = reg.active_model();
    const std::string running = running_model_id_;
    JsonValue arr = JsonValue::array();
    for (const auto& record : reg.records()) {
        ModelRecord current = record;
        current.selected = (current.model_id == selected);
        current.running = (!running.empty() && current.model_id == running);
        if (current.running) current.status = ModelStatus::kRunning;
        else if (current.selected && current.status == ModelStatus::kReady) current.status = ModelStatus::kSelected;
        arr.push_back(current.to_json());
    }
    data.set("models", std::move(arr));
    data.set("selected_model_id", JsonValue::string(selected));
    data.set("running_model_id", JsonValue::string(running));
    data.set("state", JsonValue::string(selected.empty() ? "stopped" :
                                        (selected == running ? "running" : "switching")));
    data.set("failure_code", JsonValue::string(model_failure_code_));
    data.set("failure_message", JsonValue::string(model_failure_code_));
    data.set("available", JsonValue::boolean(true));
    return data;
}

bool Application::handle_model_validate(const std::string& model_id, std::string* error) {
    if (!model_management_) {
        if (error) *error = "模型仓库不可用";
        return false;
    }
    return model_management_->registry().validate(model_id, error);
}

bool Application::handle_model_install(const std::string& model_id, std::string* error) {
    if (!model_management_) {
        if (error) *error = "模型仓库不可用";
        return false;
    }
    return model_management_->registry().install(model_id, error);
}

bool Application::handle_model_activate(const std::string& model_id, std::string* error) {
    if (!model_management_) {
        if (error) *error = "模型仓库不可用";
        return false;
    }
    // 热切换语义：activate 只改注册表选中态；真正"切过去跑"由 switch_active_model_runtime
    // 完成（stop → 以新模型重建 Worker 参数 → start → 首帧真实推理通过才提交 running）。
    // 旧模型路径：runtime 切换失败时回滚 active.json 并恢复旧模型运行，当前推理不中断窗口由
    // stop→start 间隙承担（~1s，与进程级重启相比无 IPC/端口中断）。
    const std::string previous_active = model_management_->registry().active_model();
    if (previous_active == model_id && core_runtime_ && core_runtime_->running() &&
        running_model_id_ == model_id) {
        sync_model_id_to_profile(model_id);  // 幂等：也同步，防止历史脱节
        return true;  // 幂等：已经是当前运行模型
    }
    if (!model_management_->registry().activate(model_id, error)) {
        return false;  // 激活校验失败：active 未变，无需回滚
    }
    if (!core_runtime_ || !core_runtime_->running()) {
        // runtime 未在跑（如 HDMI 断流后）：同样立即用新模型拉起，
        // 不留"选中未生效"的中间态（用户视角=切换必须真实生效）
        TTBOX_LOG_INFO("模型已激活，runtime 未运行 → 直接以新模型拉起: " + model_id);
        if (!switch_active_model_runtime(model_id, error)) {
            std::string switch_error = error && !error->empty() ? *error : "拉起失败";
            // 拉起失败：回滚 active 到旧模型（主循环会自动重试恢复）
            std::string rb_error;
            if (!previous_active.empty() && previous_active != model_id) {
                model_management_->registry().activate(previous_active, &rb_error);
            }
            if (error) *error = "模型切换失败（runtime 未运行，拉起新模型失败已回滚）: " + switch_error;
            return false;
        }
        TTBOX_LOG_INFO("模型热切换完成（runtime 原本未运行）: running_model_id=" + running_model_id_);
        sync_model_id_to_profile(model_id);
        return true;
    }
    if (!switch_active_model_runtime(model_id, error)) {
        // 切换失败：回滚 active 到旧模型，恢复旧模型运行
        std::string switch_error = error && !error->empty() ? *error : "切换失败";
        std::string rb_error;
        if (!previous_active.empty()) {
            model_management_->registry().activate(previous_active, &rb_error);
        }
        if (switch_active_model_runtime(previous_active, &rb_error)) {
            if (error) *error = "模型切换失败已回滚到 " + previous_active + ": " + switch_error;
        } else {
            // 旧模型也起不来：让主循环 2s 自动重试拉起（want 仍为 true）
            if (error) {
                *error = "模型切换失败且回滚失败: " + switch_error;
            }
        }
        return false;
    }
    TTBOX_LOG_INFO("模型热切换完成: running_model_id=" + running_model_id_);
    sync_model_id_to_profile(model_id);
    return true;
}

void Application::sync_model_id_to_profile(const std::string& model_id) {
    // 1) 内存热更新：复制当前 RuntimeProfile，仅改 model_id 字段。
    RuntimeProfile updated;
    if (auto base = runtime_config_.snapshot()) {
        updated = *base;
    }
    updated.model_id = model_id;
    runtime_config_.update(updated);

    // 2) 落盘：写回宿主配置文件 runtime_profile 键（其余键原样保留）。
    //    失败不影响激活（active.json 才是真相源，重启仍读 active.json），仅记日志。
    if (!config_path_.empty() && config_.loaded()) {
        JsonValue merged = config_.root();
        merged.set("runtime_profile", updated.to_json());
        const std::string text = merged.dump();
        FILE* f = std::fopen(config_path_.c_str(), "w");
        if (f) {
            const bool ok = std::fwrite(text.data(), 1, text.size(), f) == text.size();
            std::fclose(f);
            if (!ok) TTBOX_LOG_WARN("模型切换后同步 model_id 落盘失败: " + config_path_);
        } else {
            TTBOX_LOG_WARN("模型切换后同步 model_id 无法打开配置: " + config_path_);
        }
    }
}

bool Application::switch_active_model_runtime(const std::string& new_model_id,
                                              std::string* error) {
    if (!core_runtime_ || !model_management_) {
        if (error) *error = "core_runtime 或模型仓库未初始化";
        return false;
    }
    // 1) 停止当前流水线（capture/worker/aim/preview 全部停干净，引用与 DMA 全释放）
    want_runtime_running_.store(false);  // 防止主循环 2s 重试在我们重建期间抢跑
    core_runtime_->stop();
    runtime_started_ = false;
    running_model_id_.clear();

    // 2) 以新 active 模型重建 Worker 参数（build_runtime_params 读 registry active）
    CoreRuntime::Params rt_params{};
    std::string rt_error;
    if (!build_runtime_params(rt_params, &rt_error)) {
        if (error) *error = "新模型参数构建失败: " + rt_error;
        want_runtime_running_.store(true);
        return false;
    }
    // 3) 重初始化 CoreRuntime（worker_params_ 仅在 initialize 时拷入，必须重走）
    if (!core_runtime_->initialize(rt_params, &rt_error)) {
        if (error) *error = "CoreRuntime 重初始化失败: " + rt_error;
        want_runtime_running_.store(true);
        return false;
    }
    // 4) 启动新模型
    if (!core_runtime_->start(&rt_error)) {
        if (error) *error = "新模型启动失败: " + rt_error;
        want_runtime_running_.store(true);
        return false;
    }
    runtime_started_ = true;
    want_runtime_running_.store(true);

    // 5) 首帧门槛：等待真实推理+Decode 成功（最多 5s），通过才提交 running_model_id
    constexpr int kWaitFramesMs = 5000;
    for (int waited = 0; waited < kWaitFramesMs; waited += 50) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        if (core_runtime_->model_ready()) {
            running_model_id_ = new_model_id;
            model_failure_code_.clear();
            return true;
        }
        if (!core_runtime_->running()) {
            if (error) *error = "新模型流水线中途退出";
            return false;
        }
    }
    if (error) *error = "新模型 5s 内未完成首次真实推理（首帧门槛未通过）";
    return false;
}

bool Application::handle_model_remove(const std::string& model_id, std::string* error) {
    if (!model_management_) {
        if (error) *error = "模型仓库不可用";
        return false;
    }
    return model_management_->registry().remove(model_id, error);
}

}  // namespace ttbox::core
