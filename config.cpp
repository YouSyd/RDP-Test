#include "config.h"

char* pMonitorAddress;

INT_PTR WINAPI MonitorCfgHandle(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if(msg==WM_INITDIALOG) {
        /*
         * lParam 包含了外部传入的参数
         */
        pMonitorAddress=(char*)lParam;
        
        WNDPROC pre_proc=(WNDPROC)GetWindowLongPtr(hwnd,GWLP_WNDPROC);
        Frame_InitialSettings(hwnd,DialogWindowProc);
        Frame_GetSettings(hwnd)->pre_proc=pre_proc;
        SetWindowLongPtr(hwnd,GWL_STYLE,~WS_MAXIMIZEBOX&GetWindowLongPtr(hwnd,GWL_STYLE));
        updatencwindow(hwnd);
        
        InitialCtrls(hwnd);
        CenterMsg(hwnd);
        return (INT_PTR)TRUE;
    } else if(msg==WM_CLOSE) {
        EndDialog(hwnd,(INT_PTR)0);
        return (INT_PTR)TRUE;
    } else if(msg==WM_COMMAND) {
        switch(LOWORD(wParam)) {
        case IDC_CONFIRM: {
            Edit_GetValue(GetDlgItem(hwnd,IDC_SERVER),pMonitorAddress);
            SendMessage(hwnd,WM_CLOSE,0,0);
        } break;
        case IDC_CANCEL: {
            SendMessage(hwnd,WM_CLOSE,0,0);
        } break;
        }
    }

    return (INT_PTR)FALSE;
}

int InitialCtrls(HWND hwnd) {
    HWND ctrl_svr=GetDlgItem(hwnd,IDC_SERVER);
    Edit_InitialSettings(ctrl_svr,(char*)"端点地址",NULL);
    Edit_SetInternalStyle(ctrl_svr,0);
    wchar_t svr_buffer[256]={0};
    swprintf(svr_buffer,L"%s",L"源地址(IP:Port)");
    Edit_SetCueBannerText(ctrl_svr,svr_buffer);
    Edit_SetTitleOffset(ctrl_svr,120);
    char svr_text[256]="";
    sprintf(svr_text,"%s",pMonitorAddress);
    Edit_SetText(ctrl_svr,svr_text);
    SetWindowPos(ctrl_svr,NULL,0,0,0,0,SWP_FRAMECHANGED|SWP_NOMOVE|SWP_NOSIZE|SWP_NOZORDER|SWP_SHOWWINDOW);
    
    HWND confirm=GetDlgItem(hwnd,IDC_CONFIRM);
    Btn_InitialSettings(confirm);
    ApplyBtnStyle(confirm,ROUNDEDGE);

    HWND cancel=GetDlgItem(hwnd,IDC_CANCEL);
    Btn_InitialSettings(cancel);
    ApplyBtnStyle(cancel,ROUNDEDGE);
    return 0;
}

INT_PTR CALLBACK MonitorGetSVRADDR(HINSTANCE hInst, HWND hWndParent, LPARAM lParam) {
    /*
     * 文档说 DialogBoxParam 这种是模态，但似乎无法阻止我对父窗口的操作.
     * CreateDialogParam 创建的是非模态
     * 
     */
    return DialogBoxParam(hInst, MAKEINTRESOURCE(IDD_MONITORCFG),hWndParent,MonitorCfgHandle,lParam);
}

