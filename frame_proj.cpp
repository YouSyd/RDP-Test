#include "frame_proj.h"
#include "monitor.h"
#include "utils.h"

#include "dialog.h"

pfp_test f;

pfp_test initframemodule(HINSTANCE inst) {
    pfp_test p=(pfp_test)calloc(sizeof(fp_test),1);
    if(!p) return NULL;
    
    p->icon_app=LoadIcon(inst, MAKEINTRESOURCE(0x0001)/*IDC_ICON_DATAVIEW*/);
    p->wrapped_proc=TestProc;
    p->cx=1000;p->cy=600;
    p->inst=inst;
    p->impl=FrameProj_Test;
    strcpy(p->title,"RDP Frame Test.");
    
    return p;
}

pfp_test getframemodule() {return f;}

void clearframemodule(pfp_test p) {
    if(!p) return;
    if(p->extra) free((pfp_param)f->extra);
    free(p);
}

int WINAPI WinMain(HINSTANCE hinstance,HINSTANCE hpreinst,PSTR szcmdline,int icmdshow) {
    f=initframemodule(hinstance);
    return LAUNCH_FRAME(f->cx,f->cy,f->impl);
}

int FrameProj_Test(HWND hwnd) {
    SetWindowLongPtr(hwnd,GWLP_HINSTANCE,(LONG_PTR)f->inst);
    Frame_InitialSettings(hwnd,f->wrapped_proc);
    //取消WS_HSCROLL
    SetWindowLongPtr(hwnd,GWL_STYLE,(((UINT)GetWindowLongPtr(hwnd,GWL_STYLE))&(~WS_HSCROLL)));
    SetWindowText(hwnd,f->title);
    SendMessage(hwnd,WM_SETICON,(WPARAM)ICON_SMALL,(LPARAM)f->icon_app);
    updatencwindow(hwnd);

    if(!f->extra) {
        pfp_param wfpj=(pfp_param)calloc(sizeof(fp_param),1);
        f->extra=wfpj;
    }

    RECT rcClient;
    GetClientRect(hwnd,&rcClient);
    pfp_param param=(pfp_param)f->extra;
    if(!param) return -1;
    register_window((char*)CTRL_MONITOR,f->inst,(WNDPROC)MonitorViewProc);
    param->monitor=CreateWindow(CTRL_MONITOR,"",
        WS_CHILD|WS_VISIBLE,
        50,50,rcClient.right-rcClient.left-100+1,(rcClient.bottom-rcClient.top)-100,
        hwnd,(HMENU)0x0002,f->inst,NULL);
    Monitor_InitialSettings(param->monitor);
    Monitor_SetSrcSize(param->monitor,1920,1080);
    Monitor_InitReportStamp(param->monitor);

    QUIC_API_TABLE* api=NetIo_InitialAPI();
    pnet_io net=NetIo_Initial();

    char svr_ip[256]="127.0.0.1";
    int svr_port=9537;
    HMODULE cfg=LoadLibrary("MonitorCfg.dll");
    if(cfg) {
        SHOWDIALOGPROC GetSvrAddr=(SHOWDIALOGPROC)GetProcAddress(cfg,"MonitorGetSVRADDR");
        if(GetSvrAddr) {
            char addr[256]="0.0.0.0:0";
            GetSvrAddr((HINSTANCE)cfg,hwnd,(LPARAM)addr);
            sscanf(addr,"%[^:]:%d",svr_ip,&svr_port);
        } else {
            MessageBox(hwnd,"fuck, proc addr not found.","异常",MB_OK|MB_ICONINFORMATION);
        }
        FreeLibrary(cfg);
    } else {
        MessageBox(hwnd,"加载Proj配置失败","异常",MB_OK|MB_ICONINFORMATION);
    }
    NetIo_LaunchClient(net,svr_ip,svr_port,client_connection_callback);
    net->protocol_process=pack_protocol_process;
    NetIo_SetHelloCallback(net,client_connect_first_hello_frameproj);
    
    Monitor_SetNetIo(Monitor_GetSettings(param->monitor),net);

    return 0;
}

LRESULT CALLBACK TestProc(HWND hwnd,UINT msg,WPARAM wParam,LPARAM lParam) {
    LRESULT ret=0;
    pFrameStyle fs=Frame_GetSettings(hwnd);
    
    if(fs&&fs->proc) ret=CallWindowProc(proc_window,hwnd,msg,wParam,lParam);
    else ret=CallWindowProc(DefWindowProc,hwnd,msg,wParam,lParam);
        
    if(msg==WM_DESTROY) {
        clearframemodule(f);
        NetIo_ClearAPI(NetIo_GetAPI());
    } else if(msg==WM_SIZE) {
        pfp_param param=(pfp_param)f->extra;
        if(!param) return ret;

        int cx=LOWORD(lParam);
        int cy=HIWORD(lParam);
        SetWindowPos(param->monitor,NULL,0,0,cx,cy,SWP_NOZORDER);
    }

    return ret;
}