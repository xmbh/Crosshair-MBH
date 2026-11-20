#ifndef UNICODE
#define UNICODE
#endif
#define _UNICODE
#define _WIN32_WINNT 0x0a00
#define WINVER 0x0a00
#include <windows.h>
#include <windowsx.h>
#include <shellapi.h>
#include <commctrl.h>
#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>
#include <string.h>

enum { FORM=101, COLORBOX, SIZEBAR, WEIGHTBAR, GAPBAR, OUTLINE, MONITOR, TOGGLE,
       PREVIEW, SIZEVAL, WEIGHTVAL, GAPVAL, HINT, QUIT, OPEN };
enum { TRAY=WM_APP+1, REOPEN, SIDE=100 };
static const WCHAR title[]=L"MBH Crosshair", hubClass[]=L"MBH.Crosshair.Lite.Controller";
static const WCHAR panelClass[]=L"MBH.Lite.Panel", overlayClass[]=L"MBH.Lite.Overlay";
static const WCHAR *shapes[]={L"Croce",L"Punto",L"Anello"};
static const WCHAR *names[]={L"Lime",L"Ciano",L"Bianco",L"Corallo",L"Lilla"};
static const DWORD colors[]={0xb4f884,0x5ce9ef,0xfffdf6,0xff818c,0xe3a8ff};
static struct Settings {int shape,color,size,weight,gap,outline; WCHAR monitor[CCHDEVICENAME];} cfg;
static WCHAR path[1024];
static HINSTANCE instance;
static HWND hub,panel,overlay;
static HFONT font;
static HICON icon;
static HDC surface;
static HBITMAP bitmap;
static HGDIOBJ original;
static DWORD *pixels,*previewPixels;
static BITMAPINFO info;
static MONITORINFOEXW monitors[64];
static int count,selected,dpi;
static BOOL visible,dirty,registered,exiting,notified;
static UINT taskbar;
static NOTIFYICONDATAW tray;

static int bounded(int n,int low,int high){return n<low?low:n>high?high:n;}
static void report(HWND owner, const WCHAR *stage, DWORD error){
    WCHAR message[384],reason[192]=L"";
    if(error)FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM|FORMAT_MESSAGE_IGNORE_INSERTS,NULL,error,0,reason,192,NULL);
    if(error)swprintf(message,384,L"%ls\n\nWindows %lu · %ls",stage,(unsigned long)error,reason);
    else swprintf(message,384,L"%ls",stage);
    MessageBoxW(owner,message,title,MB_OK|MB_ICONERROR);
}
static int parse(const char *text,int fallback,int lo,int hi){
    char *end;long n=strtol(text,&end,10);
    if(end==text || *end)return fallback;
    return bounded((int)n,lo,hi);
}
static void load(void){
    cfg=(struct Settings){.size=8,.weight=2,.gap=3,.outline=1};
    DWORD n=GetModuleFileNameW(NULL,path,1000);
    if(!n || n>=1000){path[0]=0;return;}
    WCHAR *slash=wcsrchr(path,L'\\'); if(!slash){path[0]=0;return;}
    wcscpy(slash+1,L"MBH-Crosshair.ini");
    FILE *f=_wfopen(path,L"rb");if(!f)return;
    char line[256];
    while(fgets(line,sizeof(line),f)){
        line[strcspn(line,"\r\n")]=0;char *value=strchr(line,'=');if(!value)continue;
        *value++=0;
        if(!strcmp(line,"shape"))cfg.shape=parse(value,cfg.shape,0,2);
        else if(!strcmp(line,"color"))cfg.color=parse(value,cfg.color,0,4);
        else if(!strcmp(line,"size"))cfg.size=parse(value,cfg.size,1,32);
        else if(!strcmp(line,"weight"))cfg.weight=parse(value,cfg.weight,1,8);
        else if(!strcmp(line,"gap"))cfg.gap=parse(value,cfg.gap,0,14);
        else if(!strcmp(line,"outline"))cfg.outline=parse(value,cfg.outline,0,1);
        else if(!strcmp(line,"monitor"))MultiByteToWideChar(CP_UTF8,0,value,-1,cfg.monitor,CCHDEVICENAME);
    }
    fclose(f);
}
static void save(void){
    if(!dirty)return;
    WCHAR temp[1040];char monitor[128]="";
    swprintf(temp,1040,L"%ls.tmp",path);WideCharToMultiByte(CP_UTF8,0,cfg.monitor,-1,monitor,sizeof(monitor),NULL,NULL);
    FILE *f=path[0]?_wfopen(temp,L"wb"):NULL;
    if(f){
        int n=fprintf(f,"shape=%d\r\ncolor=%d\r\nsize=%d\r\nweight=%d\r\ngap=%d\r\noutline=%d\r\nmonitor=%s\r\n",
           cfg.shape,cfg.color,cfg.size,cfg.weight,cfg.gap,cfg.outline,monitor);
        int closed=fclose(f);
        if(n>0 && !closed && MoveFileExW(temp,path,MOVEFILE_REPLACE_EXISTING)){dirty=FALSE;return;}
        DeleteFileW(temp);
    }
    report(panel,L"Impossibile salvare le impostazioni nella cartella del programma.",0);
}
static BOOL CALLBACK collect(HMONITOR m,HDC dc,LPRECT r,LPARAM l){
    (void)dc;(void)r;(void)l;if(count>=64)return FALSE;
    monitors[count].cbSize=sizeof(MONITORINFOEXW);
    if(GetMonitorInfoW(m,(MONITORINFO*)&monitors[count]))count++;
    return TRUE;
}
static void findMonitors(void){
    count=0;selected=0;EnumDisplayMonitors(NULL,NULL,collect,0);
    for(int i=0;i<count;i++)if(monitors[i].dwFlags&MONITORINFOF_PRIMARY)selected=i;
    for(int i=0;i<count;i++)if(!lstrcmpiW(cfg.monitor,monitors[i].szDevice)){selected=i;break;}
}
static void draw(void){
    int halfPixel=(cfg.shape==0 && (cfg.weight&1))?0:1, inner=bounded(cfg.size-cfg.weight,0,cfg.size);
    DWORD ink=0xff000000u|colors[cfg.color];
    for(int y=0;y<SIDE;y++)for(int x=0;x<SIDE;x++){
        int dx=abs(2*x-SIDE+halfPixel),dy=abs(2*y-SIDE+halfPixel),end=2*(cfg.size+cfg.gap);
        int distance=dx*dx+dy*dy;
        BOOL on=cfg.shape==0 ? ((dy<cfg.weight && dx>=2*cfg.gap && dx<end)||(dx<cfg.weight && dy>=2*cfg.gap && dy<end))
            : distance<4*cfg.size*cfg.size && (cfg.shape==1||distance>=4*inner*inner);
        pixels[y*SIDE+x]=on?ink:0;
    }
    if(cfg.outline)for(int i=SIDE+1;i<SIDE*(SIDE-1)-1;i++){
        if(pixels[i])continue;
        for(int row=-1;row<=1;row++){
            DWORD *p=pixels+i+row*SIDE;
            if(p[-1]==ink || *p==ink || p[1]==ink){pixels[i]=0xff000000u;break;}
        }
    }
}
static BOOL display(void){
    if(!visible || !count){ShowWindow(overlay,SW_HIDE);return TRUE;}
    RECT r=monitors[selected].rcMonitor;
    POINT dst={r.left+(r.right-r.left)/2-SIDE/2,r.top+(r.bottom-r.top)/2-SIDE/2},src={0,0};
    SIZE size={SIDE,SIDE};BLENDFUNCTION blend={AC_SRC_OVER,0,255,AC_SRC_ALPHA};
    if(!UpdateLayeredWindow(overlay,NULL,&dst,&size,surface,&src,0,&blend,ULW_ALPHA))return FALSE;
    return SetWindowPos(overlay,HWND_TOPMOST,0,0,0,0,SWP_SHOWWINDOW|SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE);
}
static HWND ctl(int id){return GetDlgItem(panel,id);}
static int px(int v){return MulDiv(v,dpi,96);}
static HWND add(const WCHAR *cls,const WCHAR *text,DWORD style,int id,int x,int y,int w,int h){
    HWND c=CreateWindowExW(0,cls,text,WS_CHILD|WS_VISIBLE|style,px(x),px(y),px(w),px(h),panel,(HMENU)(INT_PTR)id,instance,NULL);
    if(c)SendMessageW(c,WM_SETFONT,(WPARAM)font,TRUE);
    return c;
}
static void toggle(void){
    visible=!visible;display();
    if(panel)SetWindowTextW(ctl(TOGGLE),visible?L"Nascondi mirino":L"Mostra mirino");
}
static void quit(void){
    if(exiting)return;
    exiting=TRUE;save();
    if(notified)Shell_NotifyIconW(NIM_DELETE,&tray);
    if(registered)UnregisterHotKey(hub,1);
    if(panel)DestroyWindow(panel);
    if(overlay)DestroyWindow(overlay);
    if(hub)DestroyWindow(hub);
    PostQuitMessage(0);
}
static void combo(int id,const WCHAR **items,int n,int value,int x,int y,int width){
    HWND c=add(WC_COMBOBOXW,L"",CBS_DROPDOWNLIST|WS_VSCROLL|WS_TABSTOP,id,x,y,width,170);
    for(int i=0;i<n;i++)SendMessageW(c,CB_ADDSTRING,0,(LPARAM)items[i]);
    SendMessageW(c,CB_SETCURSEL,value,0);
}
static void screenChoices(void){
    HWND c=ctl(MONITOR);SendMessageW(c,CB_RESETCONTENT,0,0);
    for(int i=0;i<count;i++){
        WCHAR buf[80];RECT r=monitors[i].rcMonitor;
        swprintf(buf,80,L"Schermo %d · %ld × %ld",i+1,r.right-r.left,r.bottom-r.top);
        SendMessageW(c,CB_ADDSTRING,0,(LPARAM)buf);
    }
    SendMessageW(c,CB_SETCURSEL,selected,0);EnableWindow(c,count>1);
}
static void slider(int id,const WCHAR *label,int y,int value,int lo,int hi){
    add(WC_STATICW,label,0,0,230,y,128,19);
    add(WC_STATICW,L"",SS_RIGHT,SIZEVAL+id-SIZEBAR,360,y,63,19);
    HWND c=add(TRACKBAR_CLASSW,L"",TBS_NOTICKS|WS_TABSTOP,id,225,y+19,200,23);
    SendMessageW(c,TBM_SETRANGE,FALSE,MAKELPARAM(lo,hi));SendMessageW(c,TBM_SETPOS,TRUE,value);
}
static void values(void){
    int v[]={cfg.size,cfg.weight,cfg.gap};
    for(int i=0;i<3;i++){
        BOOL enabled=i==0 || (i==1 && cfg.shape!=1) || (i==2 && cfg.shape==0);
        WCHAR text[24];swprintf(text,24,L"%d px",v[i]);SetWindowTextW(ctl(SIZEVAL+i),enabled?text:L"—");
        EnableWindow(ctl(SIZEBAR+i),enabled);
    }
}
static void controls(void){
    font=CreateFontW(-px(14),0,0,0,FW_NORMAL,0,0,0,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Segoe UI");
    add(WC_STATICW,L"Anteprima  ·  2×",0,0,16,14,185,20);
    add(WC_STATICW,L"",SS_OWNERDRAW,PREVIEW,16,40,194,192);
    add(WC_STATICW,L"Monitor",0,0,16,245,170,18);combo(MONITOR,NULL,0,0,16,267,194);screenChoices();
    add(WC_STATICW,L"Forma",0,0,230,14,82,19);add(WC_STATICW,L"Colore",0,0,332,14,86,19);
    combo(FORM,shapes,3,cfg.shape,230,39,92);combo(COLORBOX,names,5,cfg.color,332,39,91);
    slider(SIZEBAR,L"Dimensione",84,cfg.size,1,32);slider(WEIGHTBAR,L"Spessore",134,cfg.weight,1,8);slider(GAPBAR,L"Spazio centrale",184,cfg.gap,0,14);
    add(WC_BUTTONW,L"Contorno nero",BS_AUTOCHECKBOX|WS_TABSTOP,OUTLINE,230,237,190,26);CheckDlgButton(panel,OUTLINE,cfg.outline?BST_CHECKED:BST_UNCHECKED);
    add(WC_BUTTONW,visible?L"Nascondi mirino":L"Mostra mirino",WS_TABSTOP,TOGGLE,230,267,193,28);
    add(WC_STATICW,registered?L"Ctrl + Alt + X   ·   La X chiude il pannello.":L"La X chiude il pannello. Riaprilo dall’icona vicino all’orologio.",0,HINT,16,309,335,39);
    add(WC_BUTTONW,L"Esci",WS_TABSTOP,QUIT,363,309,60,28);values();
}
static const DWORD panelStyle=WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX;
static SIZE panelSize(UINT d){
    RECT r={0,0,MulDiv(440,d,96),MulDiv(353,d,96)};AdjustWindowRectExForDpi(&r,panelStyle,FALSE,0,d);
    return (SIZE){r.right-r.left,r.bottom-r.top};
}
static void openPanel(void){
    if(panel){ShowWindow(panel,SW_RESTORE);SetForegroundWindow(panel);return;}
    SIZE s=panelSize(GetDpiForSystem());
    HWND h=CreateWindowExW(0,panelClass,title,panelStyle,CW_USEDEFAULT,CW_USEDEFAULT,s.cx,s.cy,NULL,NULL,instance,NULL);
    if(h){ShowWindow(h,SW_SHOW);SetForegroundWindow(h);}else report(NULL,L"Impossibile aprire le impostazioni.",GetLastError());
}
static void changed(void){dirty=TRUE;draw();display();values();InvalidateRect(ctl(PREVIEW),NULL,FALSE);}
static LRESULT CALLBACK panelProc(HWND h,UINT m,WPARAM w,LPARAM l){
    switch(m){
    case WM_CREATE:{
        panel=h;dpi=(int)GetDpiForWindow(h);if(!dpi)dpi=96;
        SIZE s=panelSize(dpi);SetWindowPos(h,NULL,0,0,s.cx,s.cy,SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE);
        controls();return 0;}
    case WM_COMMAND:{
        int id=LOWORD(w),event=HIWORD(w);
        if(event==CBN_SELCHANGE && (id==FORM || id==COLORBOX || id==MONITOR)){
            int index=(int)SendMessageW((HWND)l,CB_GETCURSEL,0,0);if(index<0)return 0;
            if(id==FORM && index<3)cfg.shape=index;
            if(id==COLORBOX && index<5)cfg.color=index;
            if(id==MONITOR && index<count){selected=index;lstrcpynW(cfg.monitor,monitors[index].szDevice,CCHDEVICENAME);}
            changed();return 0;
        }
        if(event==BN_CLICKED && id==OUTLINE){cfg.outline=IsDlgButtonChecked(h,OUTLINE)==BST_CHECKED;changed();return 0;}
        if(event==BN_CLICKED && id==TOGGLE){toggle();return 0;}
        if(event==BN_CLICKED && id==QUIT){quit();return 0;}break;}
    case WM_HSCROLL:{
        HWND c=(HWND)l;int v=(int)SendMessageW(c,TBM_GETPOS,0,0);
        if(c==ctl(SIZEBAR))cfg.size=v;else if(c==ctl(WEIGHTBAR))cfg.weight=v;else if(c==ctl(GAPBAR))cfg.gap=v;else break;
        changed();return 0;}
    case WM_DRAWITEM:{
        DRAWITEMSTRUCT *d=(DRAWITEMSTRUCT*)l;if(d->CtlID!=PREVIEW)break;
        if(!previewPixels)previewPixels=malloc(SIDE*SIDE*sizeof(DWORD));
        if(previewPixels){
            for(int y=0;y<SIDE;y++)for(int x=0;x<SIDE;x++){int i=y*SIDE+x;
                previewPixels[i]=pixels[i]?pixels[i]:((x%6==2 && y%6==2)?0x394348:0x181e21);}
            RECT r=d->rcItem;StretchDIBits(d->hDC,r.left,r.top,r.right-r.left,r.bottom-r.top,0,0,SIDE,SIDE,previewPixels,&info,DIB_RGB_COLORS,SRCCOPY);
        }return TRUE;}
    case WM_DPICHANGED:{
        dpi=HIWORD(w);RECT dest=*(RECT*)l;HWND child;
        while((child=GetWindow(h,GW_CHILD)))DestroyWindow(child);
        if(font){DeleteObject(font);font=NULL;}
        SIZE s=panelSize(dpi);SetWindowPos(h,NULL,dest.left,dest.top,s.cx,s.cy,SWP_NOZORDER|SWP_NOACTIVATE);controls();return 0;}
    case WM_CLOSE:save();DestroyWindow(h);return 0;
    case WM_NCDESTROY: panel=NULL;free(previewPixels);previewPixels=NULL;if(font){DeleteObject(font);font=NULL;}break;
    }
    return DefWindowProcW(h,m,w,l);
}
static void trayMenu(void){
    POINT p;GetCursorPos(&p);HMENU menu=CreatePopupMenu();if(!menu)return;
    AppendMenuW(menu,MF_STRING,OPEN,L"Impostazioni");AppendMenuW(menu,MF_STRING,TOGGLE,visible?L"Nascondi mirino":L"Mostra mirino");
    AppendMenuW(menu,MF_SEPARATOR,0,NULL);AppendMenuW(menu,MF_STRING,QUIT,L"Esci");
    SetForegroundWindow(hub);UINT cmd=TrackPopupMenu(menu,TPM_RETURNCMD|TPM_RIGHTBUTTON,p.x,p.y,0,hub,NULL);
    DestroyMenu(menu);PostMessageW(hub,WM_NULL,0,0);if(cmd==OPEN)openPanel();else if(cmd==TOGGLE)toggle();else if(cmd==QUIT)quit();
}
static LRESULT CALLBACK hubProc(HWND h,UINT m,WPARAM w,LPARAM l){
    if(taskbar && m==taskbar){notified=Shell_NotifyIconW(NIM_ADD,&tray);return 0;}
    switch(m){
    case TRAY:if(l==WM_LBUTTONUP)openPanel();else if(l==WM_RBUTTONUP || l==WM_CONTEXTMENU)trayMenu();return 0;
    case REOPEN:openPanel();return 0;
    case WM_HOTKEY:if(w==1)toggle();return 0;
    case WM_DISPLAYCHANGE:if(pixels){findMonitors();display();if(panel)screenChoices();}return 0;
    case WM_QUERYENDSESSION:return TRUE;
    case WM_ENDSESSION:if(w)quit();return 0;
    case WM_CLOSE:quit();return 0;
    }
    return DefWindowProcW(h,m,w,l);
}
static LRESULT CALLBACK overlayProc(HWND h,UINT m,WPARAM w,LPARAM l){
    if(m==WM_NCHITTEST)return HTTRANSPARENT;
    if(m==WM_MOUSEACTIVATE)return MA_NOACTIVATE;
    return DefWindowProcW(h,m,w,l);
}
static BOOL reg(const WCHAR *name,WNDPROC proc,HBRUSH brush){
    WNDCLASSEXW c={.cbSize=sizeof(c),.lpfnWndProc=proc,.hInstance=instance,.hIcon=icon,.hIconSm=icon,
       .hCursor=LoadCursorW(NULL,IDC_ARROW),.hbrBackground=brush,.lpszClassName=name};
    return RegisterClassExW(&c)!=0;
}
static BOOL start(void){
    const WCHAR *stage=L"Impossibile registrare le finestre del mirino.";
    if(!reg(hubClass,hubProc,NULL)||!reg(overlayClass,overlayProc,NULL)||!reg(panelClass,panelProc,GetSysColorBrush(COLOR_BTNFACE)))goto failed;
    stage=L"Impossibile aprire il controller del mirino.";
    hub=CreateWindowExW(WS_EX_TOOLWINDOW,hubClass,L"",WS_POPUP,0,0,0,0,NULL,NULL,instance,NULL);if(!hub)goto failed;
    stage=L"Impossibile creare la finestra trasparente.";
    overlay=CreateWindowExW(WS_EX_LAYERED|WS_EX_TRANSPARENT|WS_EX_NOACTIVATE|WS_EX_TOOLWINDOW|WS_EX_TOPMOST,
       overlayClass,L"",WS_POPUP,0,0,SIDE,SIDE,NULL,NULL,instance,NULL);if(!overlay)goto failed;
    stage=L"Impossibile creare la superficie grafica del mirino.";
    surface=CreateCompatibleDC(NULL);if(!surface)goto failed;
    info.bmiHeader=(BITMAPINFOHEADER){.biSize=sizeof(BITMAPINFOHEADER),.biWidth=SIDE,.biHeight=-SIDE,.biPlanes=1,.biBitCount=32,.biCompression=BI_RGB};
    bitmap=CreateDIBSection(surface,&info,DIB_RGB_COLORS,(void**)&pixels,NULL,0);if(!bitmap||!pixels)goto failed;
    original=SelectObject(surface,bitmap);if(!original || original==HGDI_ERROR)goto failed;
    load();findMonitors();draw();visible=TRUE;stage=L"Impossibile mostrare il mirino sul monitor.";
    if(!display())goto failed;
    return TRUE;
failed:report(NULL,stage,GetLastError());return FALSE;
}
int WINAPI wWinMain(HINSTANCE inst,HINSTANCE previous,LPWSTR args,int mode){
    (void)previous;(void)args;(void)mode;instance=inst;
    HANDLE mutex=CreateMutexW(NULL,FALSE,L"Local\\MBH.Crosshair.Lite.2");
    if(mutex && GetLastError()==ERROR_ALREADY_EXISTS){
        HWND h=FindWindowW(hubClass,NULL);if(h)PostMessageW(h,REOPEN,0,0);CloseHandle(mutex);return 0;}
    icon=LoadIconW(inst,MAKEINTRESOURCEW(1));if(!icon)icon=LoadIconW(NULL,IDI_APPLICATION);
    INITCOMMONCONTROLSEX common={sizeof(common),ICC_BAR_CLASSES};InitCommonControlsEx(&common);
    if(!start())return 1;
    registered=RegisterHotKey(hub,1,MOD_NOREPEAT|MOD_CONTROL|MOD_ALT,'X');
    taskbar=RegisterWindowMessageW(L"TaskbarCreated");tray.cbSize=sizeof(tray);tray.hWnd=hub;tray.uID=1;tray.uFlags=NIF_ICON|NIF_TIP|NIF_MESSAGE;
    tray.uCallbackMessage=TRAY;tray.hIcon=icon;lstrcpynW(tray.szTip,title,ARRAYSIZE(tray.szTip));notified=Shell_NotifyIconW(NIM_ADD,&tray);
    openPanel();MSG msg;int got;
    while((got=GetMessageW(&msg,NULL,0,0))>0){
        if(panel && !registered && (msg.message==WM_KEYDOWN||msg.message==WM_SYSKEYDOWN) && msg.wParam=='X' && GetKeyState(VK_CONTROL)<0 && GetKeyState(VK_MENU)<0){
            if(!(msg.lParam & (1L<<30)))toggle();
            continue;
        }
        if(panel && IsDialogMessageW(panel,&msg))continue;
        TranslateMessage(&msg);DispatchMessageW(&msg);
    }
    SelectObject(surface,original);DeleteObject(bitmap);DeleteDC(surface);if(mutex)CloseHandle(mutex);
    return got<0?1:0;
}
