#include <windows.h>
#include <stdio.h>

// APC 回调函数
void CALLBACK TimerAPCProc(LPVOID lpArg, DWORD dwTimerLowValue, DWORD dwTimerHighValue) {
    printf("%d:Timer triggered. Argument: %s\n", GetCurrentThreadId(), (char *)lpArg);
}

int main() {
    HANDLE hTimer;
    LARGE_INTEGER liDueTime;

    // 创建可等待的定时器对象
    hTimer = CreateWaitableTimer(NULL, FALSE, NULL);
    if (hTimer == NULL) {
        printf("Failed to create timer. Error: %lu\n", GetLastError());
        return 1;
    }

    // 设置初次触发时间为 2 秒后
    liDueTime.QuadPart = -20000000LL; // 2 秒

    // 设置定时器为每隔 1 秒触发一次
    LONG lPeriod = 1000; // 1 秒（以毫秒为单位）

    if (!SetWaitableTimer(hTimer, &liDueTime, lPeriod, TimerAPCProc, "Hello, Periodic Timer!", FALSE)) {
        printf("Failed to set timer. Error: %lu\n", GetLastError());
        CloseHandle(hTimer);
        return 1;
    }

    printf("%d:Periodic timer set. Waiting for it to trigger...\n",GetCurrentThreadId());

    // 模拟等待回调触发
    // 使用 SleepEx 让主线程进入可警醒状态以接收回调
    for (int i = 0; i < 10; ++i) {
        SleepEx(INFINITE, TRUE); // 每次等待直到一个 APC 调用被执行
    }

    printf("Finished waiting. Cleaning up.\n");

    // 关闭定时器
    CancelWaitableTimer(hTimer);
    CloseHandle(hTimer);

    return 0;
}
