#include <stdio.h>
#include <stdlib.h>
#include <xxhash.h>

int main() {
    // 创建哈希状态对象
    XXH64_state_t* state = XXH64_createState();
    if (state == NULL) {
        fprintf(stderr, "Failed to create state\n");
        return 1;
    }

    // 初始化状态对象（使用种子 0）
    if (XXH64_reset(state, 0) == XXH_ERROR) {
        fprintf(stderr, "Failed to reset state\n");
        XXH64_freeState(state);
        return 1;
    }

    // 模拟多个非连续数据块
    const char* data1 = "Hello, ";
    const char* data2 = "World!";
    const char* data3 = " xxHash is fast.";

    // 分段更新状态
    XXH64_update(state, data1, strlen(data1));
    XXH64_update(state, data2, strlen(data2));
    XXH64_update(state, data3, strlen(data3));

    // 计算最终哈希值
    uint64_t hash = XXH64_digest(state);

    // 打印结果
    printf("Hash: %llu\n", (unsigned long long)hash);

    // 释放状态对象
    XXH64_freeState(state);

    return 0;
}
