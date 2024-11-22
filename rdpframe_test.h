/**
 * 构建两个控件，一个输入窗口，一个输出窗口
 * monitor_window: 程序会扫描窗口位图数据，并写入缓存数据
 * output_window: 从缓存数据中获取数据，并将其绘制更新至该窗口
 * 
 * 
 * 位图缓存与局部更新的策略
 * 1、获取基准位图，以此为基准进行更新
 * 2、同步机制，两边需要维护一个同步序列，以保证更新一致
 * 3、缓存更新的局部位置比较及计算
 * 4、局部更新数据的更新与重建访问（格式）
 * 
 * 0.基准位图缓存
 * 1.基准位图传输
 * 2.采集新图
 * 3.CMP，DIFF
 * 4.DIFF transmit 传输 （压缩）
 * 5.DIFF 重建新图 （解压）
 * 6.重建完成，通知采集
 * 
 * 
 * 数学题，关于数据交互的流量问题：
 * 1.已知分辨率 1920 × 1080 = 2025 Kb 约 2M ，如果是24位图， 约6M ,单幅数据量6M
 * 2.假设每秒刷新 30 次 （30~35ms）， fps 为30 
 *      数据量 为 180M/S , 这是未处理情况下需要达到的网络流量（不计算payload)
 * 3.使用压缩算法，有损压缩的压缩比可能高一点，压缩比在10~ 20 倍之间， 假定为10 ， 降为 18M/s
 * 4.将图像进行缩放，缩放为 1000*600 显示框，约为 28.9% , 降为 5.2M/s
 * 5.采用块匹配算法，比如 将 像素 分割为 8 × 8 的 块 ， 被分割为 9375 块， 假设更新率为 50% ，降为 2.6M/s , 我们不知道，实际上更新率应该是更低
 *      这里的逻辑是，块越小，理论上更新率越低，需要更新的越少，因为大块必然包含的尚未发生变化的区域，如果更新率为10%,则可以降至 520Kb/s
 * 
 * 至此，网络速率依然过高，需要寻找一个继续压降10倍的突破口 ????????? 这个突破口在哪里？
 * -----------------------------------------------------------------------------------------------------------------------------------
 * 
 * 
 * 
 * 
 * 目标需要将吞吐压降至90kb/s以内。
 * 微软的rdp能达到50kb/s以内，考虑到我是个菜鸡，不可能与之相提并论，放水一倍。
 * 
 * 数学题2，关于处理速度问题，即源端的处理能力
 * 需要在200ms内完成 采集、压缩、CMP、DIFF 等问题 ，cpu的使用是否会飙升？
 * 
 * 假设有4个采集线程
 * A开始计时，采集。。
 * +----------35ms之后-----B开始计时，采集。。。。
 *                         +--------------------------------C开始计时，采集
 *                                                          +-----------------。。。。
 * +-----------------------+--------------------------------+------------------------------------+-----------------------...
 * A                       B                                C                                    D                      ....
 * 每个开始时点，检查A/B/C/D是否完成任务空闲，如果找到空闲的，唤醒它干活...
 * 这样是否可以解决单个任务30ms无法完成的问题？
 * 
 * 这能否展现多线程的魅力？
 *
 *
 * 以下是chatgpt推荐的精度计时器，属于WindowsAPI Windows Multimedia Timer 
#include <windows.h>
#include <mmsystem.h>
#include <stdio.h>

void CALLBACK TimerCallback(UINT uTimerID, UINT uMsg, DWORD_PTR dwUser, DWORD_PTR dw1, DWORD_PTR dw2) {
    // 每次触发定时器时执行的代码
    printf("Timer Triggered\n");
}

int main() {
    MMRESULT result = timeSetEvent(10, 1, TimerCallback, 0, TIME_PERIODIC); // 每10ms触发一次
    if (result != TIMERR_NOERROR) {
        printf("Failed to set timer\n");
    }
    // 等待一段时间来观察定时器事件
    Sleep(1000); // 休眠1秒

    // 清理定时器
    timeKillEvent(result);
    return 0;
}
 * 
 *
 * 以下是gdiplus 将位图转化为jpeg格式的字节流
 #include <windows.h>
#include <gdiplus.h>
#include <iostream>
#include <vector>

using namespace Gdiplus;
using namespace std;

// 获取指定格式的编码器 CLSID（例如 JPEG）
int GetEncoderClsid(const wchar_t* format, CLSID* pClsid) {
    UINT num;
    UINT size;
    ImageCodecInfo* pImageCodecInfo;

    GetImageEncoders(&num, &size, NULL);
    pImageCodecInfo = (ImageCodecInfo*)(malloc(size));
    if (pImageCodecInfo == NULL) {
        return -1;
    }

    GetImageEncoders(num, size, pImageCodecInfo);

    for (UINT j = 0; j < num; ++j) {
        if (wcscmp(pImageCodecInfo[j].MimeType, format) == 0) {
            *pClsid = pImageCodecInfo[j].Clsid;
            free(pImageCodecInfo);
            return j;  // 返回编码器的索引
        }
    }

    free(pImageCodecInfo);
    return -1;  // 未找到编码器
}

// 将 HBITMAP 转换为 JPEG 格式并保存为内存字节流
bool CompressBitmapToMemory(HBITMAP hBitmap, vector<BYTE>& outMemory) {
    // 初始化 GDI+
    ULONG_PTR gdiplusToken;
    GdiplusStartupInput gdiplusStartupInput;
    GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);

    // 将 HBITMAP 转换为 Gdiplus::Bitmap 对象
    Bitmap* pBitmap = Bitmap::FromHBITMAP(hBitmap, NULL);
    if (pBitmap->GetLastStatus() != Ok) {
        cerr << "Failed to convert HBITMAP to GDI+ Bitmap." << endl;
        GdiplusShutdown(gdiplusToken);
        return false;
    }

    // 设置 JPEG 压缩质量（0 到 100，100 为无损）
    EncoderParameters encoderParams;
    encoderParams.Count = 1;
    encoderParams.Parameter[0].Guid = EncoderParameterValueTypeL;
    encoderParams.Parameter[0].Type = EncoderParameterValueTypeLong;
    encoderParams.Parameter[0].Value = (void*)80;  // 设置为 80，表示压缩质量适中

    // 获取 JPEG 编码器 CLSID
    CLSID jpgClsid;
    if (GetEncoderClsid(L"image/jpeg", &jpgClsid) == -1) {
        cerr << "Failed to get JPEG encoder CLSID." << endl;
        delete pBitmap;
        GdiplusShutdown(gdiplusToken);
        return false;
    }

    // 创建内存流
    IStream* pStream = NULL;
    HRESULT hr = CreateStreamOnHGlobal(NULL, TRUE, &pStream);
    if (FAILED(hr)) {
        cerr << "Failed to create memory stream." << endl;
        delete pBitmap;
        GdiplusShutdown(gdiplusToken);
        return false;
    }

    // 将 Bitmap 保存为内存中的 JPEG
    pBitmap->Save(pStream, &jpgClsid, &encoderParams);

    // 获取内存流的大小并提取数据
    STATSTG stat;
    pStream->Stat(&stat, STATFLAG_NONAME);
    ULONG size = stat.cbSize.LowPart;

    // 将内存流中的数据拷贝到 vector 中
    BYTE* pBuffer = new BYTE[size];
    LARGE_INTEGER liZero = {};
    pStream->Seek(liZero, STREAM_SEEK_SET, NULL);
    pStream->Read(pBuffer, size, NULL);
    
    outMemory.assign(pBuffer, pBuffer + size);  // 将字节流拷贝到 vector 中

    // 清理资源
    delete[] pBuffer;
    pStream->Release();
    delete pBitmap;
    GdiplusShutdown(gdiplusToken);

    return true;
}

int main() {
    // 假设你已经有了一个 HBITMAP 对象
    HBITMAP hBitmap = ???// 获取的 HBITMAP ;
    
    // 压缩并获取内存字节流
    vector<BYTE> jpgData;
    if (CompressBitmapToMemory(hBitmap, jpgData)) {
        // 成功，jpgData 中存储了 JPEG 图像的字节流
        cout << "JPEG data size: " << jpgData.size() << " bytes." << endl;
    } else {
        cout << "Failed to compress bitmap to memory." << endl;
    }

    return 0;
}
 * 
 *
 * 以下是 将bitmap字节流加载进入HBITMAP结构
#include <windows.h>
#include <iostream>

HBITMAP LoadBitmapFromMemory(const BYTE* pData, size_t dataSize) {
    BITMAPFILEHEADER* pFileHeader = (BITMAPFILEHEADER*)pData;
    BITMAPINFOHEADER* pInfoHeader = (BITMAPINFOHEADER*)(pData + sizeof(BITMAPFILEHEADER));

    // 确保数据有效
    if (pFileHeader->bfType != 0x4D42) {
        std::cerr << "Invalid bitmap file!" << std::endl;
        return NULL;
    }

    // 获取图像数据（像素数据）
    BYTE* pImageData = (BYTE*)(pData + pFileHeader->bfOffBits);

    // 创建位图
    HBITMAP hBitmap = CreateDIBitmap(GetDC(NULL), pInfoHeader, CBM_INIT, pImageData, (BITMAPINFO*)pInfoHeader, DIB_RGB_COLORS);
    
    return hBitmap;
}

int main() {
    // 假设 pData 是从文件中读取的二进制数据（可以通过读取文件并存储到内存中）
    const BYTE* pData = // ... your binary data ...;
    size_t dataSize = // size of your binary data;

    HBITMAP hBitmap = LoadBitmapFromMemory(pData, dataSize);
    if (hBitmap) {
        // 使用 hBitmap 做进一步操作
    }

    return 0;
}
 *
 *
 * 
 * 以下是获取屏幕位图数据
#include <opencv2/opencv.hpp>
#include <windows.h>

int main() {
    // 截取屏幕图像
    cv::Mat screenshot = cv::Mat::zeros(1080, 1920, CV_8UC3);
    HDC hdcScreen = GetDC(NULL);
    HDC hdcMemDC = CreateCompatibleDC(hdcScreen);
    HBITMAP hBitmap = CreateCompatibleBitmap(hdcScreen, 1920, 1080);
    SelectObject(hdcMemDC, hBitmap);
    BitBlt(hdcMemDC, 0, 0, 1920, 1080, hdcScreen, 0, 0, SRCCOPY);
    // 处理或保存图像
    // ...

    DeleteObject(hBitmap);
    DeleteDC(hdcMemDC);
    ReleaseDC(NULL, hdcScreen);
}

 * 
 */

/**
 * 开发计划：
 * 分期步骤：
 * 1、先不考虑和实现网络模块，在一台机器上检查数据的采集、压缩、解压、CMP/DIFF、重建，用于试验性质的检测关于传图的一些细节
 *    主要包括，测试采集的频次，耗时，压缩的算法、参数，重建的效果，并行采集的效率、延迟，CPU的占用情况等。
 *    要达到的目标是确立数据采集、压缩、分块、重建、访问等细节的处理流程、算法参数，以及配套的程序结构。
 * 2、确立网络模块，确定要采用的结构、库，确定吞吐，确定交互协议
 * 3、后期开发时进一步明确。
 * 
 * 独立分开的做以下几件事：
 * 1、将bmp文件转jpg，看看实际的压缩比，同时对比探索 WebP、HEIF/HEIC、AVIF、JPEG2000、BPG
 *    初步设想的方案是，如果计算机的算力够，即如果CPU不是太高，比如20%~30%,通过牺牲时间来保证图像品质，再借助并行线程来弥补吞吐量和网络流量，以延迟作为代价。
 *    注意通盘考虑对比在分块模式下的压缩效率和压缩比
 * 2、实现、测试并验证分块、重建算法
 *    特别的，实现一个便于观测和验证的控件，记录分块、观察重建、统计更新率
 * 
 * 
 * We engineers just want to make things work. If not , do it for what ?
 */

#include <windows.h>

/**
 * 2024年11月22日：已基本实现，分块传输
 */
