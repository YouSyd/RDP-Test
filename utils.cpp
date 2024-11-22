#include "utils.h"
#include <windows.h>

long long get_current_timestamp_ms() {
    FILETIME ft;
    GetSystemTimeAsFileTime(&ft);

    ULARGE_INTEGER ull;
    ull.LowPart = ft.dwLowDateTime;
    ull.HighPart = ft.dwHighDateTime;

    // 将 100 纳秒单位转换为毫秒，并减去到 Unix 纪元的偏移
    return (ull.QuadPart / 10000) - EPOCH_DIFFERENCE;
}