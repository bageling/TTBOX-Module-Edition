#include <rknn_api.h>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <thread>
#include <vector>

static constexpr int NUM_CORES = 3;
static std::atomic<long> g_total_runs{0};
static std::atomic<bool> g_stop{false};

static unsigned char *load_model(const char *path, int *out_size) {
    FILE *fp = fopen(path, "rb");
    if (!fp) return nullptr;
    fseek(fp, 0, SEEK_END);
    int sz = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    auto *buf = (unsigned char *)malloc(sz);
    if (!buf) { fclose(fp); return nullptr; }
    fread(buf, 1, sz, fp);
    fclose(fp);
    *out_size = sz;
    return buf;
}

static void worker_loop(rknn_context ctx, const uint8_t *input, size_t bytes) {
    (void)input; (void)bytes;
    while (!g_stop.load()) {
        int ret = rknn_run(ctx, nullptr);
        if (ret < 0) break;
        g_total_runs.fetch_add(1);
    }
}

int main(int argc, char **argv) {
    if (argc < 4) {
        printf("Usage: %s <rknn_model> <num_threads> <seconds>\n", argv[0]);
        return 1;
    }
    const char *model_path = argv[1];
    int num_threads = atoi(argv[2]);
    int seconds = atoi(argv[3]);
    int num_ctxs = 3;

    int model_size = 0;
    unsigned char *model_data = load_model(model_path, &model_size);
    if (!model_data) { printf("FAIL: load model\n"); return 1; }

    std::vector<rknn_context> ctxs(num_ctxs);
    std::vector<rknn_tensor_mem *> input_mems(num_ctxs);

    for (int i = 0; i < num_ctxs; ++i) {
        rknn_context ctx = 0;
        int ret = rknn_init(&ctx, model_data, model_size, 0, NULL);
        if (ret < 0) { printf("FAIL: rknn_init ctx %d ret=%d\n", i, ret); return 1; }
        ctxs[i] = ctx;

        rknn_input_output_num io_num;
        rknn_query(ctx, RKNN_QUERY_IN_OUT_NUM, &io_num, sizeof(io_num));

        rknn_tensor_attr input_attr;
        memset(&input_attr, 0, sizeof(input_attr));
        input_attr.index = 0;
        rknn_query(ctx, RKNN_QUERY_INPUT_ATTR, &input_attr, sizeof(input_attr));

        input_attr.type = RKNN_TENSOR_INT8;
        input_attr.fmt = RKNN_TENSOR_NHWC;
        input_attr.pass_through = 1;
        input_mems[i] = rknn_create_mem(ctx, input_attr.size_with_stride);
        rknn_set_io_mem(ctx, input_mems[i], &input_attr);

        rknn_core_mask mask = (rknn_core_mask)((i == 0) ? 1 : (i == 1) ? 2 : 4);
        rknn_set_core_mask(ctx, mask);

        memset(input_mems[i]->virt_addr, 0, input_attr.size_with_stride);
    }

    std::vector<std::thread> workers;
    auto t_start = std::chrono::steady_clock::now();

    for (int i = 0; i < num_threads; ++i) {
        int cidx = i % num_ctxs;
        workers.emplace_back(worker_loop, ctxs[cidx],
            (const uint8_t *)input_mems[cidx]->virt_addr, 0);
    }

    std::this_thread::sleep_for(std::chrono::seconds(seconds));
    g_stop.store(true);
    for (auto &t : workers) t.join();
    auto t_end = std::chrono::steady_clock::now();

    double elapsed = std::chrono::duration<double>(t_end - t_start).count();
    long total = g_total_runs.load();
    double fps = total / elapsed;
    double per_core = (1000.0 * num_threads) / (fps * NUM_CORES);

    printf("\n===== NPU Benchmark =====\n");
    printf("  threads:  %d\n", num_threads);
    printf("  ctxs:     %d\n", num_ctxs);
    printf("  seconds:  %d\n", seconds);
    printf("  total:    %ld\n", total);
    printf("  elapsed:  %.3f s\n", elapsed);
    printf("  FPS:      %.2f\n", fps);
    printf("  per-core: %.2f ms\n", per_core);
    printf("=========================\n");

    for (int i = 0; i < num_ctxs; ++i) {
        rknn_destroy_mem(ctxs[i], input_mems[i]);
        rknn_destroy(ctxs[i]);
    }
    free(model_data);
    return 0;
}
