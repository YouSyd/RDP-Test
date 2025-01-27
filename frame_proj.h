#include "netio.h"
#include "frame.h"

typedef struct __rs_frameproj_test__ {
    int cx;
    int cy;
    char title[256];
    HINSTANCE inst;
    HICON icon_app;
    WNDPROC wrapped_proc;
    
    impl_frameinst impl;//界面初始化函数
    
    void* extra;    
}fp_test,*pfp_test;

typedef struct proj_param {
    HWND monitor;
}fp_param,*pfp_param;

extern pfp_test f;

pfp_test initframemodule(HINSTANCE inst);
pfp_test getframemodule();
void clearframemodule(pfp_test p);

int FrameProj_Test(HWND hwnd);

LRESULT CALLBACK TestProc(HWND hwnd,UINT msg,WPARAM wParam,LPARAM lParam);
