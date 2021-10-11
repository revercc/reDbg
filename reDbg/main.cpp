#include <Windows.h>
#include <iostream>
#include <iomanip>
#include <TLHELP32.H>
#include <stdio.h>
#include "resource.h"
#include "capstone.h"
#pragma comment(lib,"capstone.lib")

#pragma comment( lib, "comctl32.lib" )
#include <commctrl.h>

#include <Psapi.h>
#pragma comment (lib,"Psapi.lib")

#include <tlhelp32.h>
#include <tchar.h>

using namespace std;
#define	b00000000000000000000000100000000	0x100				//用来设置tf标志
#define	CODE_NUM							0x1000				//每次从目标进程读取机器码的数目

//int3断点结构
typedef struct Int3
{
	BYTE	oldByte;		//原始字节
	DWORD	dwAddress;		//断点地址
	Int3* lpNextINT3;
}INT3;

INT3* lpHeadInt3 = NULL;			//断点链表
//单步步过地址
typedef struct Step
{
	DWORD	dwStepAddress = 0;
	BYTE	oldByte;
};


//内存断点结构
typedef struct Memory
{
	DWORD	dwAddress = 0;				//内存断点地址
	DWORD	dwSize;					//断点大小
	DWORD	oldModel;				//旧的保护模式
};

Memory	stMemory;			//内存断点结构（就一个）
Step	stStep;				//单步步过结构（就一个）




int		flag = 0;			//不同标志意义不同
int		flag1 = 0;			//标志是刚刚是否发生了int3断点。
int		flag2 = 0;			//标记反汇编窗口是否被打开
char	Button;				//存储刚刚按下的按键
DWORD	dwpid;				//被附件进程的PID
DWORD	dModel;				//需要恢复的内存断点的属性
DWORD   dwAddressToEip[0x1000][1];	//eip对应在控件中的索引
HWND	hWinMain;			//主窗口句柄
HWND	hChildWindow;		//子窗口句柄
DWORD	dwItemEsp;			//esp指针在堆栈窗口的索引
BYTE	NewByte = 0xCC;		//断点处将被写入的字节
DWORD	dwEntryPoint = 0;	//被调试程序的入口地址	
DWORD	dwMappingFile;		//可执行文件的映射大小
DWORD	dwEntryPointRVA;	//入口地址RVA

DWORD	dwInstance;			//程序加载基地址
char	ExeName[MAX_PATH] = { 0 };			//可执行文件的路径
HANDLE  hDebugThread;		//调试线程的句柄
HANDLE	hIsDebuggedThread;	//被调试线程句柄
HANDLE  hIsDebuggedProcess	= NULL; //被调试进程句柄
char	szAddress[]			= "地址";
char	szCode[]			= "机器码";
char	szDisassembled[]	= "反汇编代码";
char	szEsegesis[]		= "注释";
char	szRegister[]		= "寄存器";
char	szHex[]				= "HEX数据";
char	szAscii[]			= "ASCII";
char	szData[]			= "数据";
char	szRegisterData[]	= "值";
char	szPID[]				= "进程";
char	szPIDpath[]			= "路径";


BOOL _stdcall _SetRead(HWND hWnd, LVHITTESTINFO lo, DWORD dwSize);
BOOL _stdcall _SetMemoryWrite(DWORD dwData, DWORD dwSize);
BOOL _stdcall _SetMemoryAccess(DWORD dwData, DWORD dwSize);
BOOL _stdcall _SetWrite(HWND hWnd, LVHITTESTINFO lo, DWORD dwSize);
BOOL _stdcall _SetHardware(HWND hWnd, LVHITTESTINFO lo);
int _stdcall _SetInt3(HWND hWnd, DWORD dwItem);



BOOL _stdcall _AddProcess();
BOOL _stdcall _DeleteAccess();
void _stdcall _DealAccess(DEBUG_EVENT devent);
void _stdcall _DealClean();
DWORD	_stdcall _GetEntryPoint(char* lpExeName);
void _stdcall _ShowContext(CONTEXT* lpstContext);
BOOL _stdcall _ShowDataWindow(HINSTANCE);
void _stdcall _ShowPointer();
void _stdcall _UpdateCpu(CONTEXT stContext);
DWORD _stdcall _ToNextAddress(CONTEXT stContext);
int _stdcall _Disassembled(BYTE* lpBuffer, DWORD Address);
int _stdcall _InitList(HWND hWnd, RECT stWindowRect);
int _stdcall _ChangeList(HWND hWnd, RECT stWindowRect);
int _stdcall _OpenFile();
int _stdcall _MyContinueDebugEvent(DEBUG_EVENT devent, PROCESS_INFORMATION pi);
int _stdcall _DealSingle(DEBUG_EVENT devent, PROCESS_INFORMATION pi);
int _stdcall _DealBreakPoint(DEBUG_EVENT devent, CONTEXT& stContext, DWORD dwEntryPoint, PROCESS_INFORMATION pi);
void _stdcall _DealHardware(DEBUG_EVENT devent, CONTEXT stContext);
BOOL CALLBACK _DialogFind(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
BOOL CALLBACK  _DialogMemory(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
BOOL _stdcall _DialogLook(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
BOOL _stdcall _DialogSee(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
BOOL CALLBACK _EnumChildProc(HWND hWnd, LPARAM lParam);
BOOL CALLBACK _DialogAdd(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
BOOL DosPathToNtPath(LPTSTR pszDosPath, LPTSTR pszNtPath);
BOOL CALLBACK _MainDialog(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam);
BOOL CALLBACK _AboutDialog(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
BOOL CALLBACK _DisassembleDialog(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);



//UI主函数
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	MSG		stMsg;
	HACCEL	hAccelerators;
	hWinMain = CreateDialogParam(hInstance, MAKEINTRESOURCE(IDD_DIALOG1), NULL, _MainDialog, NULL);

	//加载加速键
	hAccelerators = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDR_ACCELERATOR1));

	while (GetMessage(&stMsg, NULL, 0, 0))
	{

		if(!TranslateAccelerator(hWinMain, hAccelerators, &stMsg))
		{
			if (stMsg.message == WM_QUIT)
				break;
			TranslateMessage(&stMsg);
			DispatchMessage(&stMsg);
		}
	}

	return stMsg.wParam;
}








//主窗口回调函数
BOOL CALLBACK _MainDialog(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam)
{
	HICON		hIcon;
	CONTEXT		stContext;
	BYTE		lpBuffer[0x1000];
	
	switch (Msg)
	{

	case WM_COMMAND:
		switch (LOWORD(wParam))
		{
		
		case ID_40012:		//单步步过
			Button = '8';
			ResumeThread(hDebugThread);
			break;
		case ID_40011:		//单步步入
			Button = '7';
			ResumeThread(hDebugThread);
			break;
		case ID_Menu:		//运行
			Button = '5';
			ResumeThread(hDebugThread);
			break;

		//查找内存
		case ID_40033:
			DialogBoxParam(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_DIALOG4), NULL, _DialogFind, 0);
			break;

		//查看硬件断点
		case ID_40050:
			DialogBoxParam(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_DIALOG6), NULL, _DialogLook, 0);
			break;

		//删除内存断点
		case ID_40052:
			_DeleteAccess();
			break;
		//查看内存断点
		case ID_40053:
			DialogBoxParam(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_DIALOG7), NULL, _DialogSee, 0);

			break;
		//打开，创建调试线程
		case ID_40001:
			if (dwEntryPoint != 0)
			{
				ResumeThread(hDebugThread);
				Button = '5';	//让程序运行然后结束他
				TerminateProcess(hIsDebuggedProcess, 0);
			}
			hDebugThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)_OpenFile, NULL, NULL, NULL);
			break;
		//打开附加程序窗口
		case ID_40002:
			DialogBoxParam(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_DIALOG9), NULL, _DialogAdd, 0);
			break;
		//结束调试
		case ID_40017:
			ResumeThread(hDebugThread);
			Button = '5';	//让程序运行然后结束他
			TerminateProcess(hIsDebuggedProcess, 0);
			break;
		//打开反汇编窗口
		case ID_40007:
			if (flag2 == 0)
			{
				flag2 = 1;
				CreateDialogParam(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_DIALOG3), hWnd, _DisassembleDialog, 0);
				if (hIsDebuggedProcess != NULL)
				{
					stContext.ContextFlags = CONTEXT_ALL;
					GetThreadContext(hIsDebuggedThread, &stContext);
					ReadProcessMemory(hIsDebuggedProcess, (LPCVOID)(stContext.Eip - (stContext.Eip % 0x1000)), lpBuffer, CODE_NUM, NULL);
					_Disassembled(lpBuffer, stContext.Eip - (stContext.Eip % 0x1000));					//反汇编并显示反汇编窗口
					_ShowContext(&stContext);															//显示寄存器窗口
					_ShowDataWindow((HINSTANCE)(dwEntryPoint - dwEntryPointRVA));						//显示数据窗口
					_ShowPointer();																		//显示堆栈窗口

				}
			}
			
			break;
		case ID_40003:
			SendMessage(hWnd, WM_CLOSE, 0, 0);
			break;
		case ID_40005:
			DialogBoxParam(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_DIALOG2), hWnd, _AboutDialog, 0);
			break;
		}
		break;
	case WM_INITMENU:
		if (dwEntryPoint == 0)
		{
			EnableMenuItem(GetMenu(hWnd), ID_Menu, MF_DISABLED);
			EnableMenuItem(GetMenu(hWnd), ID_40011, MF_DISABLED);
			EnableMenuItem(GetMenu(hWnd), ID_40012, MF_DISABLED);
			EnableMenuItem(GetMenu(hWnd), ID_40017, MF_DISABLED);
		}
		else
		{
			EnableMenuItem(GetMenu(hWnd), ID_Menu, MF_ENABLED);
			EnableMenuItem(GetMenu(hWnd), ID_40011, MF_ENABLED);
			EnableMenuItem(GetMenu(hWnd), ID_40012, MF_ENABLED);
			EnableMenuItem(GetMenu(hWnd), ID_40017, MF_ENABLED);
		}
		break;
	case WM_INITDIALOG:
		//这里不能用hWinMain，因为CreateDialogParam( )函数还没返回，其子在内进行窗口的注册和创建（从而发送WM_INITDIALOG消息）
		hIcon = LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_ICON1));
		SendMessage(hWnd, WM_SETICON, ICON_BIG, LPARAM(hIcon));
		break;
	case WM_CLOSE:
		DestroyWindow(hWnd);
		break;
	case WM_DESTROY:
		PostQuitMessage(NULL);
		break;
	default:
		return FALSE;
	}


	return TRUE;

}




//附加进程窗口回调函数
BOOL CALLBACK _DialogAdd(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	static PROCESSENTRY32 pe32;
	static HANDLE hProcessSnap;

	HANDLE hThreadSnap;
	HANDLE hModuleSnap;
	THREADENTRY32 th32;
	MODULEENTRY32 me32;
	me32.dwSize = sizeof(me32);
	th32.dwSize = sizeof(th32);
	pe32.dwSize = sizeof(pe32);

	int i;
	DWORD		dwData;
	char		szBuffer[256];
	RECT		stWindowRect;
	LVCOLUMN    lvColumn;						//列表头属性
	memset(&lvColumn, 0, sizeof(lvColumn));
	LVITEM       lvItem;					//ListView列表项属性
	switch (message)
	{


	case WM_NOTIFY:							//响应公共控件消息（listcontrol）	
		switch (((LPNMHDR)lParam)->code)
		{
		case NM_DBLCLK:			//有项目被双击
			switch (((LPNMITEMACTIVATE)lParam)->hdr.idFrom)
			{
			case IDC_LIST1:

				ListView_GetItemText(GetDlgItem(hWnd, IDC_LIST1), ((LPNMITEMACTIVATE)lParam)->iItem, 0, szBuffer, sizeof(szBuffer));
				dwData = strtol(szBuffer, NULL, 16);


				//如果已打开程序，则先结束被调试程序
				if (dwEntryPoint != 0)
				{
					ResumeThread(hDebugThread);
					Button = '5';	//让程序运行然后结束他
					TerminateProcess(hIsDebuggedProcess, 0);
				}
				
				hIsDebuggedProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, dwData);
				
				//获得附加进程的路径
				GetProcessImageFileName(hIsDebuggedProcess, ExeName, sizeof(ExeName));				//获得进程Dos路径
				DosPathToNtPath(ExeName, ExeName);													//Dos路径转NT路径


				//线程快照（为了获得进程主线程的句柄）
				hThreadSnap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, NULL);
				Thread32First(hThreadSnap, &th32);
				while (1)
				{
					if (th32.th32OwnerProcessID == dwData)
					{
						hIsDebuggedThread = OpenThread(THREAD_ALL_ACCESS, FALSE, th32.th32ThreadID);
					
					}
					if (Thread32Next(hThreadSnap, &th32) == FALSE)
						break;
				}
				CloseHandle(hThreadSnap);




				//进程模块快照，（获得已运行进程基地址）
				hModuleSnap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, dwData);
				Module32First(hModuleSnap, &me32);
				dwInstance = (DWORD)me32.modBaseAddr;
				CloseHandle(hModuleSnap);



				dwpid = dwData;
				hDebugThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)_AddProcess, NULL, NULL, NULL);
			
				break;
			}
			break;

		}
		break;


	case WM_INITDIALOG:
		

		
		
		GetClientRect(hWnd, &stWindowRect);

		//附加窗口
		SendMessage(GetDlgItem(hWnd, IDC_LIST1), LVM_SETEXTENDEDLISTVIEWSTYLE, 0, (LPARAM)((LVS_EX_FULLROWSELECT)));
		lvColumn.mask = LVCF_TEXT | LVCF_WIDTH;

		lvColumn.cx = (stWindowRect.right / 10) * 3;
		lvColumn.pszText = szPID;
		SendDlgItemMessage(hWnd, IDC_LIST1, LVM_INSERTCOLUMN, 0, (LPARAM)&lvColumn);


		lvColumn.cx = (stWindowRect.right / 10) * 7;
		lvColumn.pszText = szPIDpath;
		SendDlgItemMessage(hWnd, IDC_LIST1, LVM_INSERTCOLUMN, 2, (LPARAM)&lvColumn);


		
		

		hProcessSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
		Process32First(hProcessSnap, &pe32);
		i = 0;
		while(Process32Next(hProcessSnap, &pe32))
		{
			
			memset(&lvItem, 0, sizeof(lvItem));
			lvItem.mask = LVIF_TEXT;
			lvItem.iItem = i;
			SendDlgItemMessage(hWnd, IDC_LIST1, LVM_INSERTITEM, 0, (LPARAM)&lvItem);

			wsprintf(szBuffer, TEXT("%08lX"), pe32.th32ProcessID);
			lvItem.pszText = szBuffer;
			lvItem.iSubItem = 0;
			SendDlgItemMessage(hWnd, IDC_LIST1, LVM_SETITEM, 0, (LPARAM)&lvItem);

			lstrcpy(szBuffer, pe32.szExeFile);
			lvItem.pszText = szBuffer;
			lvItem.iSubItem = 1;
			SendDlgItemMessage(hWnd, IDC_LIST1, LVM_SETITEM, 0, (LPARAM)&lvItem);

			i++;
		
		}
		CloseHandle(hProcessSnap);
		break;
	case WM_CLOSE:
		EndDialog(hWnd,0);
		break;
	default:
		return FALSE;
	}
	return TRUE;

}




//Dos路径转化为NT路径
BOOL DosPathToNtPath(LPTSTR pszDosPath, LPTSTR pszNtPath)
{
	TCHAR            szDriveStr[500];
	TCHAR            szDrive[3];
	TCHAR            szDevName[100];
	INT                iDevName;
	INT                i;

	//检查参数
	if (!pszDosPath || !pszNtPath)
		return FALSE;

	//获取本地磁盘所有盘符,以'\0'分隔,所以下面+4
	if (GetLogicalDriveStrings(sizeof(szDriveStr), szDriveStr))
	{
		for (i = 0; szDriveStr[i]; i += 4)
		{
			if (!lstrcmpi(&(szDriveStr[i]), _T("A:\\")) || !lstrcmpi(&(szDriveStr[i]), _T("B:\\")))
				continue;    //从C盘开始

			//盘符
			szDrive[0] = szDriveStr[i];
			szDrive[1] = szDriveStr[i + 1];
			szDrive[2] = '\0';
			if (!QueryDosDevice(szDrive, szDevName, 100))//查询 Dos 设备名(盘符由NT查询DOS)
				return FALSE;

			iDevName = lstrlen(szDevName);
			if (_tcsnicmp(pszDosPath, szDevName, iDevName) == 0)//是否为此盘
			{
				lstrcpy(pszNtPath, szDrive);//复制驱动器
				lstrcat(pszNtPath, pszDosPath + iDevName);//复制路径

				return TRUE;
			}
		}
	}

	lstrcpy(pszNtPath, pszDosPath);

	return FALSE;
}






//内存断点查看窗口回调函数

BOOL _stdcall _DialogSee(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	char	Buffer[256];
	switch (message)
	{
	
	case WM_CLOSE:
		EndDialog(hWnd, 0);
		break;
	case WM_INITDIALOG:
		if (stMemory.dwAddress != 0)
		{
			wsprintf(Buffer, TEXT("%08X"), stMemory.dwAddress);
			SetWindowText(GetDlgItem(hWnd, IDC_EDIT1), Buffer);
			wsprintf(Buffer, TEXT("%08X"), stMemory.dwSize);
			SetWindowText(GetDlgItem(hWnd, IDC_EDIT3), Buffer);
			wsprintf(Buffer, TEXT("%08X"), stMemory.dwSize);
			SetWindowText(GetDlgItem(hWnd, IDC_EDIT4), Buffer);
		}
		break;
	default:
		return FALSE;
	}
	return TRUE;


}





//硬件断点查看窗口回调函数
BOOL _stdcall _DialogLook(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	char		Buffer[256];
	static		CONTEXT		stContext;
	stContext.ContextFlags = CONTEXT_ALL;
	switch (message)
	{


	case WM_COMMAND:
		
		switch (LOWORD(wParam))
		{
		case IDC_BUTTON1:
			if (stContext.Dr0 != 0)
			{
				stContext.Dr0 = 0;
				stContext.Dr7 = stContext.Dr7 - 1;
			}
			break;
		case IDC_BUTTON2:
			if (stContext.Dr1 != 0)
			{
				stContext.Dr1 = 0;
				stContext.Dr7 = stContext.Dr7 - 4;
			}
			break;
		case IDC_BUTTON3:
			if (stContext.Dr2 != 0)
			{
				stContext.Dr2 = 0;
				stContext.Dr7 = stContext.Dr7 - 16;
			}
			break;
		case IDC_BUTTON4:
			if (stContext.Dr3 != 0)
			{
				stContext.Dr3 = 0;
				stContext.Dr7 = stContext.Dr7 - 64;
			}
			break;
			
		case IDC_BUTTON5:
			SetThreadContext(hIsDebuggedThread, &stContext);
			ResumeThread(hIsDebuggedThread);
			EndDialog(hWnd, 0);
			break;
		}
		break;
	case WM_INITDIALOG:
		SuspendThread(hIsDebuggedThread);
		GetThreadContext(hIsDebuggedThread, &stContext);
		if (stContext.Dr0 != 0)
		{
			wsprintf(Buffer, TEXT("%08X"), stContext.Dr0);
			SetWindowText(GetDlgItem(hWnd, IDC_EDIT1), Buffer);
		}

		if (stContext.Dr1 != 0)
		{
			wsprintf(Buffer, TEXT("%08X"), stContext.Dr1);
			SetWindowText(GetDlgItem(hWnd, IDC_EDIT3), Buffer);
		}

		if (stContext.Dr2 != 0)
		{
			wsprintf(Buffer, TEXT("%08X"), stContext.Dr2);
			SetWindowText(GetDlgItem(hWnd, IDC_EDIT4), Buffer);
		}

		if (stContext.Dr3 != 0)
		{
			wsprintf(Buffer, TEXT("%08X"), stContext.Dr3);
			SetWindowText(GetDlgItem(hWnd, IDC_EDIT5), Buffer);
		}



		break;
	case WM_CLOSE:
		ResumeThread(hIsDebuggedThread);
		EndDialog(hWnd, 0);
		break;
	default:
		return FALSE;
	}

	return TRUE;
	
}






//查找窗口回调函数
BOOL CALLBACK _DialogFind(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	char	Buffer[256] = {0};
	char*	lpChar;
	DWORD	dwData;
	int n;
	switch (message)
	{
	case WM_COMMAND:
		switch (LOWORD(wParam))
		{
		case IDOK:
			GetWindowText(GetDlgItem(hWnd, IDC_EDIT1), Buffer, sizeof(Buffer));
			dwData = strtol(Buffer, &lpChar, 16);
			if ((lpChar[0] == '\0') && dwData != 0)
				_ShowDataWindow((HINSTANCE)dwData);
			break;
		}
		break;
	case WM_CLOSE:
		EndDialog(hWnd, 0);
		break;
	default:
		return FALSE;
	}

	return TRUE;
}





//关于窗口回调函数
BOOL CALLBACK _AboutDialog(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	HICON	hIcon;

	switch (message)
	{
	case WM_CLOSE:
		EndDialog(hWnd, 0);
		break;
	case WM_INITDIALOG:
		hIcon = LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_ICON1));
		SendMessage(hWnd, WM_SETICON, ICON_BIG, LPARAM(hIcon));
		break;
	default:
		return FALSE;
	}
	return TRUE;

}


//反汇编窗口过程
BOOL CALLBACK _DisassembleDialog(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	RECT			stWindowRect;
	POINT			stPos;
	static LVHITTESTINFO	lo;
	static HMENU	hMenu1;
	

	int n;
	switch (message)
	{

	
	case WM_NOTIFY:							//响应公共控件消息（listcontrol）	
		switch (((LPNMHDR)lParam)->code)
		{
		
		/*
		case NM_CUSTOMDRAW:		//有项目正在被绘制
			lplvcd = (LPNMLVCUSTOMDRAW)lParam;

			switch (lplvcd->nmcd.dwDrawStage) 
			{

			case CDDS_PREPAINT:
				
				return CDRF_NOTIFYITEMDRAW;

			case CDDS_ITEMPREPAINT:
		
					return CDRF_NEWFONT;
				//  or return CDRF_NOTIFYSUBITEMDRAW;

			case CDDS_SUBITEM | CDDS_ITEMPREPAINT:
			
					
				return CDRF_NEWFONT;
			}
			break;

			*/



		case NM_DBLCLK:			//有项目被双击
			switch (((LPNMITEMACTIVATE)lParam)->hdr.idFrom)
			{
			case IDC_LIST1:
				if (((LPNMITEMACTIVATE)lParam)->iSubItem == 1)
					_SetInt3(hWnd, ((LPNMITEMACTIVATE)lParam)->iItem);
				break;
			}
			break;
		



		case NM_RCLICK:			//在项目上右击
			switch (((LPNMITEMACTIVATE)lParam)->hdr.idFrom)
			{
			case IDC_LIST1:
				
				GetCursorPos(&stPos);					//弹出窗口
				TrackPopupMenu(GetSubMenu(hMenu1, 0), TPM_LEFTALIGN, stPos.x, stPos.y, NULL, hWnd, NULL);
				lo.pt.x = ((LPNMITEMACTIVATE)lParam)->ptAction.x;
				lo.pt.y = ((LPNMITEMACTIVATE)lParam)->ptAction.y;
				ListView_SubItemHitTest(GetDlgItem(hWnd, IDC_LIST1), &lo);
				break;
			case IDC_LIST2:
				GetCursorPos(&stPos);					//弹出窗口
				TrackPopupMenu(GetSubMenu(hMenu1, 1), TPM_LEFTALIGN, stPos.x, stPos.y, NULL, hWnd, NULL);
				lo.pt.x = ((LPNMITEMACTIVATE)lParam)->ptAction.x;
				lo.pt.y = ((LPNMITEMACTIVATE)lParam)->ptAction.y;
				ListView_SubItemHitTest(GetDlgItem(hWnd, IDC_LIST2), &lo);
				
				break;

			}
			break;
		}
		break;
	case WM_COMMAND:
		switch (LOWORD(wParam))
		{
		case ID_40018:			//设置硬件执行断点
			_SetHardware(hWnd, lo);
			break;
		case ID_40024:			//硬件写（BYTE）
		case ID_40046:
			_SetWrite(hWnd, lo, 0);
			break;

		case ID_40025:			//硬件写（WORD）
		case ID_40047:
			_SetWrite(hWnd, lo, 1);
			break;
		case ID_40026:			//硬件写（DWORD）
		case ID_40048:
			_SetWrite(hWnd, lo, 2);
			break;

		case ID_40028:			//硬件写（DWORD-2）
		case ID_40049:
			_SetWrite(hWnd, lo, 3);
			break;

		case ID_40021:			//硬件读（BYTE）
		case ID_40042:
			_SetRead(hWnd, lo, 0);
			break;
		
		case ID_40022:			//硬件读（WORD）
		case ID_40043:
			_SetRead(hWnd, lo, 1);
			break;

		case ID_40023:			//硬件读（DWORD）
		case ID_40044:
			_SetRead(hWnd, lo, 2);
			break;

		case ID_40027:			//硬件读（DWORD-2）
		case ID_40045:
			_SetRead(hWnd, lo, 3);
			break;



		case ID_40039:			//内存访问
			DialogBoxParam(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_DIALOG5), NULL, _DialogMemory, 0);
			break;
		case ID_40040:			//内存写入
			DialogBoxParam(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_DIALOG5), NULL, _DialogMemory, 1);
			break;
		//case ID_40041:			//条件断点
		//case ID_40031:
		//	break;
		

		}
		break;
	case WM_SIZE:
		GetClientRect(hWnd, &stWindowRect);
		MoveWindow(GetDlgItem(hWnd, IDC_LIST1), 0, 0, (stWindowRect.right / 12) * 9, (stWindowRect.bottom / 3) * 2, TRUE);
		MoveWindow(GetDlgItem(hWnd, IDC_LIST5), (stWindowRect.right / 12) * 9, 0, (stWindowRect.right / 12) * 3, (stWindowRect.bottom / 3) * 2, TRUE);
		MoveWindow(GetDlgItem(hWnd, IDC_LIST2), 0, (stWindowRect.bottom / 3) * 2, (stWindowRect.right / 12) * 9, stWindowRect.bottom / 3, TRUE);
		MoveWindow(GetDlgItem(hWnd, IDC_LIST3), (stWindowRect.right / 12) * 9, (stWindowRect.bottom / 3) * 2, (stWindowRect.right / 12) * 3, stWindowRect.bottom / 3, TRUE);
		_ChangeList(hWnd, stWindowRect);				//改变列表使其自适应
		break;
	case WM_INITDIALOG:
		GetClientRect(hWnd, &stWindowRect);
		_InitList(hWnd, stWindowRect);					//初始化列表	

		hMenu1 = LoadMenu(GetModuleHandle(NULL), MAKEINTRESOURCE(IDR_MENU2));
		
		break;
	case WM_CLOSE:
		flag2 = 0;
		DestroyWindow(hWnd);
		break;
	

	default:
		return FALSE;
	}

	return TRUE;

}




//内存断点设置窗口回调函数
BOOL CALLBACK  _DialogMemory(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	static	int flag;
	char Buffer[256];
	char Buffer1[256];
	char* lpChar;
	DWORD	dwData;
	DWORD	dwSize;

	switch (message)
	{
	case WM_COMMAND:
		switch (LOWORD(wParam))
		{
		case IDOK:
			GetWindowText(GetDlgItem(hWnd, IDC_EDIT1), Buffer, sizeof(Buffer));
			dwData = strtol(Buffer, &lpChar, 16);
			if ((lpChar[0] != '\0') || dwData == 0)
				break;
			GetWindowText(GetDlgItem(hWnd, IDC_EDIT3), Buffer, sizeof(Buffer));
			dwSize = strtol(Buffer, &lpChar, 16);
			if ((lpChar[0] != '\0') || dwSize == 0)
				break;
			if (flag == 0)
				_SetMemoryAccess(dwData, dwSize);			//设置内存访问断点
			else if (flag == 1)
				_SetMemoryWrite(dwData, dwSize);			//设置内存写入断点
		}
		break;
	case WM_CLOSE:
		EndDialog(hWnd, 0);
		break;
	case WM_INITDIALOG:
		flag = lParam;
		break;
	default:
		return FALSE;
	}
	return TRUE;
}







//初始化列表头
int _stdcall _InitList(HWND hWnd, RECT	stWindowRect)
{
	LVCOLUMN    lvColumn;						//列表头属性
	memset(&lvColumn, 0, sizeof(lvColumn));
	
	
	//反汇编窗口
	SendMessage(GetDlgItem(hWnd, IDC_LIST1), LVM_SETEXTENDEDLISTVIEWSTYLE, 0, (LPARAM)((LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER)));
	lvColumn.mask = LVCF_TEXT | LVCF_WIDTH;
	
	lvColumn.cx = (stWindowRect.right / 36) * 3;
	lvColumn.pszText = szAddress;
	SendDlgItemMessage(hWnd, IDC_LIST1, LVM_INSERTCOLUMN, 0, (LPARAM)&lvColumn);

	
	lvColumn.cx = (stWindowRect.right / 36) * 6;
	lvColumn.pszText = szCode;
	SendDlgItemMessage(hWnd, IDC_LIST1, LVM_INSERTCOLUMN, 1, (LPARAM)&lvColumn);

	
	lvColumn.cx = (stWindowRect.right / 36) * 12;
	lvColumn.pszText = szDisassembled;
	SendDlgItemMessage(hWnd, IDC_LIST1, LVM_INSERTCOLUMN, 2, (LPARAM)&lvColumn);

	
	lvColumn.cx = (stWindowRect.right / 36) * 6;
	lvColumn.pszText = szEsegesis;
	SendDlgItemMessage(hWnd, IDC_LIST1, LVM_INSERTCOLUMN, 3, (LPARAM)&lvColumn);

	//寄存器窗口
	SendMessage(GetDlgItem(hWnd, IDC_LIST5), LVM_SETEXTENDEDLISTVIEWSTYLE, 0, (LPARAM)(LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER));
	lvColumn.cx = (stWindowRect.right / 8);
	lvColumn.pszText = szRegister;
	SendDlgItemMessage(hWnd, IDC_LIST5, LVM_INSERTCOLUMN, 0, (LPARAM)&lvColumn);

	SendMessage(GetDlgItem(hWnd, IDC_LIST5), LVM_SETEXTENDEDLISTVIEWSTYLE, 0, (LPARAM)(LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER));
	lvColumn.cx = (stWindowRect.right / 8);
	lvColumn.pszText = szRegisterData;
	SendDlgItemMessage(hWnd, IDC_LIST5, LVM_INSERTCOLUMN, 1, (LPARAM)&lvColumn);
	
	//数据窗口
	SendMessage(GetDlgItem(hWnd, IDC_LIST2), LVM_SETEXTENDEDLISTVIEWSTYLE, 0, (LPARAM)LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER);
	lvColumn.cx = (stWindowRect.right / 36) * 3;
	lvColumn.pszText = szAddress;
	SendDlgItemMessage(hWnd, IDC_LIST2, LVM_INSERTCOLUMN, 0, (LPARAM)&lvColumn);

	lvColumn.cx = (stWindowRect.right / 36) * 18;
	lvColumn.pszText = szHex;
	SendDlgItemMessage(hWnd, IDC_LIST2, LVM_INSERTCOLUMN, 1, (LPARAM)&lvColumn);

	lvColumn.cx = (stWindowRect.right / 36) * 6;
	lvColumn.pszText = szAscii;
	SendDlgItemMessage(hWnd, IDC_LIST2, LVM_INSERTCOLUMN, 2, (LPARAM)&lvColumn);


	//堆栈窗口
	SendMessage(GetDlgItem(hWnd, IDC_LIST3), LVM_SETEXTENDEDLISTVIEWSTYLE, 0, (LPARAM)LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER);
	lvColumn.cx = (stWindowRect.right / 12) * 1;
	lvColumn.pszText = szAddress;
	SendDlgItemMessage(hWnd, IDC_LIST3, LVM_INSERTCOLUMN, 0, (LPARAM)&lvColumn);

	lvColumn.cx = (stWindowRect.right / 12) * 1;
	lvColumn.pszText = szData;
	SendDlgItemMessage(hWnd, IDC_LIST3, LVM_INSERTCOLUMN, 1, (LPARAM)&lvColumn);


	return 0;
}



//改变列表头
int _stdcall _ChangeList(HWND hWnd, RECT stWindowRect)
{
	
	
	LVCOLUMN    lvColumn;						//ListView列表头属性
	memset(&lvColumn, 0, sizeof(lvColumn));

	
	//反汇编窗口
	lvColumn.mask = LVCF_TEXT | LVCF_WIDTH;
	lvColumn.cx = (stWindowRect.right / 36) * 3;
	lvColumn.pszText = szAddress;
	SendDlgItemMessage(hWnd, IDC_LIST1, LVM_SETCOLUMN, 0, (LPARAM)&lvColumn);

	
	lvColumn.cx = (stWindowRect.right / 36) * 6;
	lvColumn.pszText = szCode;
	SendDlgItemMessage(hWnd, IDC_LIST1, LVM_SETCOLUMN, 1, (LPARAM)&lvColumn);

	
	lvColumn.cx = (stWindowRect.right / 36) * 12;
	lvColumn.pszText = szDisassembled;
	SendDlgItemMessage(hWnd, IDC_LIST1, LVM_SETCOLUMN, 2, (LPARAM)&lvColumn);

	
	lvColumn.cx = (stWindowRect.right / 36) * 6;
	lvColumn.pszText = szEsegesis;
	SendDlgItemMessage(hWnd, IDC_LIST1, LVM_SETCOLUMN, 3, (LPARAM)&lvColumn);

	//寄存器窗口
	lvColumn.cx = (stWindowRect.right / 8);
	lvColumn.pszText = szRegister;
	SendDlgItemMessage(hWnd, IDC_LIST5, LVM_SETCOLUMN, 0, (LPARAM)&lvColumn);

	lvColumn.cx = (stWindowRect.right / 8);
	lvColumn.pszText = szRegisterData;
	SendDlgItemMessage(hWnd, IDC_LIST5, LVM_SETCOLUMN, 1, (LPARAM)&lvColumn);

	//数据窗口
	lvColumn.cx = (stWindowRect.right / 36) * 3;
	lvColumn.pszText = szAddress;
	SendDlgItemMessage(hWnd, IDC_LIST2, LVM_SETCOLUMN, 0, (LPARAM)&lvColumn);

	lvColumn.cx = (stWindowRect.right / 36) * 18;
	lvColumn.pszText = szHex;
	SendDlgItemMessage(hWnd, IDC_LIST2, LVM_SETCOLUMN, 1, (LPARAM)&lvColumn);

	lvColumn.cx = (stWindowRect.right / 36) * 6;
	lvColumn.pszText = szAscii;
	SendDlgItemMessage(hWnd, IDC_LIST2, LVM_SETCOLUMN, 2, (LPARAM)&lvColumn);


	//堆栈窗口
	lvColumn.cx = (stWindowRect.right / 12) * 1;
	lvColumn.pszText = szAddress;
	SendDlgItemMessage(hWnd, IDC_LIST3, LVM_SETCOLUMN, 0, (LPARAM)&lvColumn);

	lvColumn.cx = (stWindowRect.right / 12) * 1;
	lvColumn.pszText = szData;
	SendDlgItemMessage(hWnd, IDC_LIST3, LVM_SETCOLUMN, 1, (LPARAM)&lvColumn);

	return 0;
}




//调试线程
int _stdcall _OpenFile()
{
	char ExeName[MAX_PATH] = {0};			//可执行文件的路径

	OPENFILENAME		stOF;
	PROCESS_INFORMATION pi;					//接受新进程的一些有关信息
	STARTUPINFO			si;					//指定新进程的主窗体如何显示
	DEBUG_EVENT			devent;				//消息事件
	CONTEXT				stContext;			//线程信息块
	memset(&stOF, 0, sizeof(stOF));
	memset(&si, 0, sizeof(si));
	si.cb = sizeof(STARTUPINFO);

			
	


	//打开文件
	stOF.lStructSize = sizeof(stOF);
	stOF.hwndOwner = hWinMain;
	stOF.lpstrFilter = TEXT("可执行文件(*.exe,*.dll)\0*.exe;*.dll\0任意文件(*.*)\0*.*\0\0");
	stOF.lpstrFile = ExeName;
	stOF.nMaxFile = MAX_PATH;
	stOF.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;

	if (!GetOpenFileName(&stOF))
	{
		MessageBox(NULL, TEXT("打开文件失败！"), NULL, MB_OK);
		return -1;
	}






	HANDLE	hFile;				//文件句柄
	HANDLE	hMapFile;			//内存映射文件对象句柄
	LPVOID	lpMapFile;			//内存映射文件指针


	hFile = CreateFile(ExeName, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);

	hMapFile = CreateFileMapping(hFile, NULL, PAGE_READWRITE, 0, 0, NULL);
	lpMapFile = MapViewOfFile(hMapFile, FILE_MAP_READ, 0, 0, 0);
	IMAGE_DOS_HEADER* lpDosMZ = (IMAGE_DOS_HEADER*)lpMapFile;
	IMAGE_NT_HEADERS* lpNT = (IMAGE_NT_HEADERS*)lpMapFile;
	lpNT = (IMAGE_NT_HEADERS*)((BYTE*)lpNT + lpDosMZ->e_lfanew);

	dwMappingFile = lpNT->OptionalHeader.SizeOfImage;						//获得程序映射大小
	dwEntryPointRVA = lpNT->OptionalHeader.AddressOfEntryPoint;								//获得入口地址的RVA


	UnmapViewOfFile(lpMapFile);		//撤销映射
	CloseHandle(hMapFile);			//关闭内存映射对象句柄
	CloseHandle(hFile);				//关闭文件

	int n;






	

	//创建被调试程序进程
	CreateProcess(
		ExeName,
		NULL,
		NULL,
		NULL,
		FALSE, // 不可继承
		DEBUG_ONLY_THIS_PROCESS | DEBUG_PROCESS, // 调试模式启动			(DEBUG_ONLY_THIS_PROCESS标志表示其不能调试进程如果被调试的话，此新进程不会成为其调试进程的调试对象)
		NULL,
		NULL,
		&si,
		&pi);

	hIsDebuggedThread	= pi.hThread;
	hIsDebuggedProcess	= pi.hProcess;
		
	//获取入口地址
	stContext.ContextFlags = CONTEXT_ALL;
	GetThreadContext(pi.hThread, &stContext);
	dwEntryPoint = stContext.Eax;



	
	//在程序入口处设下断点，从而使在程序入口处时中断到调试器中
	lpHeadInt3 = new INT3;
	lpHeadInt3->lpNextINT3 = NULL;
	lpHeadInt3->dwAddress = dwEntryPoint;
	ReadProcessMemory(pi.hProcess, (LPCVOID)dwEntryPoint, &lpHeadInt3->oldByte, 1, NULL);
	WriteProcessMemory(pi.hProcess, (LPVOID)dwEntryPoint, &NewByte, 1, NULL);		//将入口点的第一个字节改为0xCC

	while (TRUE)
	{
		WaitForDebugEvent(&devent, INFINITE);	//等待调试事件
		switch (devent.dwDebugEventCode)
		{
		case EXCEPTION_DEBUG_EVENT:				//异常调试事件
			switch (devent.u.Exception.ExceptionRecord.ExceptionCode)
			{
			
			case EXCEPTION_BREAKPOINT:			//断点异常
			
				_DealBreakPoint(devent, stContext, dwEntryPoint, pi);							//处理断点异常
				break;
			case EXCEPTION_SINGLE_STEP:			//单步和硬件异常
				GetThreadContext(hIsDebuggedThread, &stContext);
				if (stContext.Dr6 % 0x10 != 0)													//硬件断点
					_DealHardware(devent, stContext);
				else
					_DealSingle(devent, pi);													//处理单步
				break;
			case EXCEPTION_ACCESS_VIOLATION:	//访问异常
				_DealAccess(devent);														//处理访问异常
				break;
			default:
				break;
			}
			break;
		case EXIT_PROCESS_DEBUG_EVENT:
			_DealClean();
		}

		_MyContinueDebugEvent(devent, pi);

	}
	return 0;
}


//附加进程线程

BOOL _stdcall _AddProcess()
{





	CONTEXT	stContext;
	DEBUG_EVENT			devent = {0};				//消息事件
	PROCESS_INFORMATION pi;					//接受新进程的一些有关信息
	memset(&pi, 0, sizeof(pi));

	pi.hProcess = hIsDebuggedProcess;
	pi.hThread = hIsDebuggedThread;


	
	DebugActiveProcess(dwpid);

	

	//获取入口地址RVA;,与映射大小

	DWORD lpNT;
	ReadProcessMemory(hIsDebuggedProcess, (LPVOID)(dwInstance + 0x3c), &lpNT, 4, NULL);
	lpNT = lpNT + dwInstance;

	ReadProcessMemory(hIsDebuggedProcess, (LPVOID)(lpNT + 0x50), &dwMappingFile, 4, NULL);
	ReadProcessMemory(hIsDebuggedProcess, (LPVOID)(lpNT + 0x28), &dwEntryPointRVA, 4, NULL);

	

	




//获取入口地址
dwEntryPoint = dwInstance + dwEntryPointRVA;



while (TRUE)
{
	WaitForDebugEvent(&devent, INFINITE);	//等待调试事件
	
	switch (devent.dwDebugEventCode)
	{
	case EXCEPTION_DEBUG_EVENT:				//异常调试事件
		switch (devent.u.Exception.ExceptionRecord.ExceptionCode)
		{

		case EXCEPTION_BREAKPOINT:			//断点异常

			_DealBreakPoint(devent, stContext, dwEntryPoint, pi);							//处理断点异常
			break;
		case EXCEPTION_SINGLE_STEP:			//单步和硬件异常
			GetThreadContext(hIsDebuggedThread, &stContext);
			if (stContext.Dr6 % 0x10 != 0)													//硬件断点
				_DealHardware(devent, stContext);
			else
				_DealSingle(devent, pi);													//处理单步
			break;
		case EXCEPTION_ACCESS_VIOLATION:	//访问异常
			_DealAccess(devent);														//处理访问异常
			break;
		default:
			break;
		}
		break;
	case EXIT_PROCESS_DEBUG_EVENT:
		_DealClean();
	}

	_MyContinueDebugEvent(devent, pi);

}
return 0;
}






//枚举子窗口句柄回调函数
BOOL CALLBACK _EnumChildProc(HWND hWnd, LPARAM lParam)
{
	char szBuffer[256] = { 0 };
	GetWindowText(hWnd, szBuffer, sizeof(szBuffer));
	hChildWindow = NULL;

	if (0 == strcmp(szBuffer, "CPU"))
	{
		hChildWindow = hWnd;
		return FALSE;
	}

	return TRUE;

}



//显示寄存器的值
void _stdcall _ShowContext(CONTEXT* lpstContext)
{
	HWND	hWnd;
	LVITEM  lvItem;												//ListView列表项属性
	char	szBuffer1[256] = { 0 };
	char	szBuffer2[256] = { 0 };
	EnumChildWindows(hWinMain, _EnumChildProc, 0);				//获得CPU的窗口句柄		
	hWnd = hChildWindow;

	ListView_DeleteAllItems(GetDlgItem(hWnd, IDC_LIST5));

	int i= 0;
	while (i < 25)
	{
		
		memset(&lvItem, 0, sizeof(lvItem));
		lvItem.mask = LVIF_TEXT ;
		lvItem.iItem = i;
		SendDlgItemMessage(hWnd, IDC_LIST5, LVM_INSERTITEM, 0, (LPARAM)&lvItem);
		
		
		
		switch (i)
		{
		case 0:
			lstrcpy(szBuffer1, "EAX");
			wsprintf(szBuffer2, TEXT("%08lX"), lpstContext->Eax);
			lvItem.iSubItem = 0;
			lvItem.pszText = szBuffer1;
			SendDlgItemMessage(hWnd, IDC_LIST5, LVM_SETITEM, 0, (LPARAM)&lvItem);

			lvItem.iSubItem = 1;
			lvItem.pszText = szBuffer2;
			SendDlgItemMessage(hWnd, IDC_LIST5, LVM_SETITEM, 0, (LPARAM)&lvItem);
			break;
		case 1:
			lstrcpy(szBuffer1, "EBX");
			wsprintf(szBuffer2, TEXT("%08lX"), lpstContext->Ebx);
			lvItem.iSubItem = 0;
			lvItem.pszText = szBuffer1;
			SendDlgItemMessage(hWnd, IDC_LIST5, LVM_SETITEM, 0, (LPARAM)&lvItem);

			lvItem.iSubItem = 1;
			lvItem.pszText = szBuffer2;
			SendDlgItemMessage(hWnd, IDC_LIST5, LVM_SETITEM, 0, (LPARAM)&lvItem);
			break;
		case 2:
			lstrcpy(szBuffer1, "ECX");
			wsprintf(szBuffer2, TEXT("%08lX"), lpstContext->Ecx);
			lvItem.iSubItem = 0;
			lvItem.pszText = szBuffer1;
			SendDlgItemMessage(hWnd, IDC_LIST5, LVM_SETITEM, 0, (LPARAM)&lvItem);

			lvItem.iSubItem = 1;
			lvItem.pszText = szBuffer2;
			SendDlgItemMessage(hWnd, IDC_LIST5, LVM_SETITEM, 0, (LPARAM)&lvItem);
			break;
		case 3:
			lstrcpy(szBuffer1, "EDX");
			wsprintf(szBuffer2, TEXT("%08lX"), lpstContext->Edx);
			lvItem.iSubItem = 0;
			lvItem.pszText = szBuffer1;
			SendDlgItemMessage(hWnd, IDC_LIST5, LVM_SETITEM, 0, (LPARAM)&lvItem);

			lvItem.iSubItem = 1;
			lvItem.pszText = szBuffer2;
			SendDlgItemMessage(hWnd, IDC_LIST5, LVM_SETITEM, 0, (LPARAM)&lvItem);
			break;
		case 4:
			lstrcpy(szBuffer1, "EIP");
			wsprintf(szBuffer2, TEXT("%08lX"), lpstContext->Eip);
			lvItem.iSubItem = 0;
			lvItem.pszText = szBuffer1;
			SendDlgItemMessage(hWnd, IDC_LIST5, LVM_SETITEM, 0, (LPARAM)&lvItem);

			lvItem.iSubItem = 1;
			lvItem.pszText = szBuffer2;
			SendDlgItemMessage(hWnd, IDC_LIST5, LVM_SETITEM, 0, (LPARAM)&lvItem);
			break;
		case 5:
			lstrcpy(szBuffer1, "ESP");
			wsprintf(szBuffer2, TEXT("%08lX"), lpstContext->Esp);
			lvItem.iSubItem = 0;
			lvItem.pszText = szBuffer1;
			SendDlgItemMessage(hWnd, IDC_LIST5, LVM_SETITEM, 0, (LPARAM)&lvItem);

			lvItem.iSubItem = 1;
			lvItem.pszText = szBuffer2;
			SendDlgItemMessage(hWnd, IDC_LIST5, LVM_SETITEM, 0, (LPARAM)&lvItem);
			break;
		case 6:
			lstrcpy(szBuffer1, "EBP");
			wsprintf(szBuffer2, TEXT("%08lX"), lpstContext->Ebp);
			lvItem.iSubItem = 0;
			lvItem.pszText = szBuffer1;
			SendDlgItemMessage(hWnd, IDC_LIST5, LVM_SETITEM, 0, (LPARAM)&lvItem);

			lvItem.iSubItem = 1;
			lvItem.pszText = szBuffer2;
			SendDlgItemMessage(hWnd, IDC_LIST5, LVM_SETITEM, 0, (LPARAM)&lvItem);
			break;
		case 7:
			lstrcpy(szBuffer1, "ESI");
			wsprintf(szBuffer2, TEXT("%08lX"), lpstContext->Esi);
			lvItem.iSubItem = 0;
			lvItem.pszText = szBuffer1;
			SendDlgItemMessage(hWnd, IDC_LIST5, LVM_SETITEM, 0, (LPARAM)&lvItem);

			lvItem.iSubItem = 1;
			lvItem.pszText = szBuffer2;
			SendDlgItemMessage(hWnd, IDC_LIST5, LVM_SETITEM, 0, (LPARAM)&lvItem);
			break;
		case 8:
			lstrcpy(szBuffer1, "EDI");
			wsprintf(szBuffer2, TEXT("%08lX"), lpstContext->Edi);
			lvItem.iSubItem = 0;
			lvItem.pszText = szBuffer1;
			SendDlgItemMessage(hWnd, IDC_LIST5, LVM_SETITEM, 0, (LPARAM)&lvItem);

			lvItem.iSubItem = 1;
			lvItem.pszText = szBuffer2;
			SendDlgItemMessage(hWnd, IDC_LIST5, LVM_SETITEM, 0, (LPARAM)&lvItem);
			break;

		case 10:
			lstrcpy(szBuffer1, "CS");
			wsprintf(szBuffer2, TEXT("%04lX"), lpstContext->SegCs);
			lvItem.iSubItem = 0;
			lvItem.pszText = szBuffer1;
			SendDlgItemMessage(hWnd, IDC_LIST5, LVM_SETITEM, 0, (LPARAM)&lvItem);

			lvItem.iSubItem = 1;
			lvItem.pszText = szBuffer2;
			SendDlgItemMessage(hWnd, IDC_LIST5, LVM_SETITEM, 0, (LPARAM)&lvItem);
			break;
		case 11:
			lstrcpy(szBuffer1, "ES");
			wsprintf(szBuffer2, TEXT("%04lX"), lpstContext->SegEs);
			lvItem.iSubItem = 0;
			lvItem.pszText = szBuffer1;
			SendDlgItemMessage(hWnd, IDC_LIST5, LVM_SETITEM, 0, (LPARAM)&lvItem);

			lvItem.iSubItem = 1;
			lvItem.pszText = szBuffer2;
			SendDlgItemMessage(hWnd, IDC_LIST5, LVM_SETITEM, 0, (LPARAM)&lvItem);
			break;
		case 12:
			lstrcpy(szBuffer1, "FS");
			wsprintf(szBuffer2, TEXT("%04lX"), lpstContext->SegFs);
			lvItem.iSubItem = 0;
			lvItem.pszText = szBuffer1;
			SendDlgItemMessage(hWnd, IDC_LIST5, LVM_SETITEM, 0, (LPARAM)&lvItem);

			lvItem.iSubItem = 1;
			lvItem.pszText = szBuffer2;
			SendDlgItemMessage(hWnd, IDC_LIST5, LVM_SETITEM, 0, (LPARAM)&lvItem);
			break;
		case 13:
			lstrcpy(szBuffer1, "GS");
			wsprintf(szBuffer2, TEXT("%04lX"), lpstContext->SegGs);
			lvItem.iSubItem = 0;
			lvItem.pszText = szBuffer1;
			SendDlgItemMessage(hWnd, IDC_LIST5, LVM_SETITEM, 0, (LPARAM)&lvItem);

			lvItem.iSubItem = 1;
			lvItem.pszText = szBuffer2;
			SendDlgItemMessage(hWnd, IDC_LIST5, LVM_SETITEM, 0, (LPARAM)&lvItem);
			break;
		case 14:
			lstrcpy(szBuffer1, "SS");
			wsprintf(szBuffer2, TEXT("%04lX"), lpstContext->SegSs);
			lvItem.iSubItem = 0;
			lvItem.pszText = szBuffer1;
			SendDlgItemMessage(hWnd, IDC_LIST5, LVM_SETITEM, 0, (LPARAM)&lvItem);

			lvItem.iSubItem = 1;
			lvItem.pszText = szBuffer2;
			SendDlgItemMessage(hWnd, IDC_LIST5, LVM_SETITEM, 0, (LPARAM)&lvItem);
			break;

		case 16:
			lstrcpy(szBuffer1, "EFlags");
			wsprintf(szBuffer2, TEXT("%04lX"), lpstContext->EFlags);
			lvItem.iSubItem = 0;
			lvItem.pszText = szBuffer1;
			SendDlgItemMessage(hWnd, IDC_LIST5, LVM_SETITEM, 0, (LPARAM)&lvItem);

			lvItem.iSubItem = 1;
			lvItem.pszText = szBuffer2;
			SendDlgItemMessage(hWnd, IDC_LIST5, LVM_SETITEM, 0, (LPARAM)&lvItem);
			break;
		case 18:
			lstrcpy(szBuffer1, "调试寄存器");
			lvItem.iSubItem = 0;
			lvItem.pszText = szBuffer1;
			SendDlgItemMessage(hWnd, IDC_LIST5, LVM_SETITEM, 0, (LPARAM)&lvItem);
			break;
		case 19:
			lstrcpy(szBuffer1, "Dr0");
			wsprintf(szBuffer2, TEXT("%08lX"), lpstContext->Dr0);
			lvItem.iSubItem = 0;
			lvItem.pszText = szBuffer1;
			SendDlgItemMessage(hWnd, IDC_LIST5, LVM_SETITEM, 0, (LPARAM)&lvItem);

			lvItem.iSubItem = 1;
			lvItem.pszText = szBuffer2;
			SendDlgItemMessage(hWnd, IDC_LIST5, LVM_SETITEM, 0, (LPARAM)&lvItem);
			break;
		case 20:
			lstrcpy(szBuffer1, "Dr1");
			wsprintf(szBuffer2, TEXT("%08lX"), lpstContext->Dr1);
			lvItem.iSubItem = 0;
			lvItem.pszText = szBuffer1;
			SendDlgItemMessage(hWnd, IDC_LIST5, LVM_SETITEM, 0, (LPARAM)&lvItem);

			lvItem.iSubItem = 1;
			lvItem.pszText = szBuffer2;
			SendDlgItemMessage(hWnd, IDC_LIST5, LVM_SETITEM, 0, (LPARAM)&lvItem);
			break;

		case 21:
			lstrcpy(szBuffer1, "Dr2");
			wsprintf(szBuffer2, TEXT("%08lX"), lpstContext->Dr2);
			lvItem.iSubItem = 0;
			lvItem.pszText = szBuffer1;
			SendDlgItemMessage(hWnd, IDC_LIST5, LVM_SETITEM, 0, (LPARAM)&lvItem);

			lvItem.iSubItem = 1;
			lvItem.pszText = szBuffer2;
			SendDlgItemMessage(hWnd, IDC_LIST5, LVM_SETITEM, 0, (LPARAM)&lvItem);
			break;
		case 22:
			lstrcpy(szBuffer1, "Dr3");
			wsprintf(szBuffer2, TEXT("%08lX"), lpstContext->Dr3);
			lvItem.iSubItem = 0;
			lvItem.pszText = szBuffer1;
			SendDlgItemMessage(hWnd, IDC_LIST5, LVM_SETITEM, 0, (LPARAM)&lvItem);

			lvItem.iSubItem = 1;
			lvItem.pszText = szBuffer2;
			SendDlgItemMessage(hWnd, IDC_LIST5, LVM_SETITEM, 0, (LPARAM)&lvItem);
			break;
		case 23:
			lstrcpy(szBuffer1, "Dr6");
			wsprintf(szBuffer2, TEXT("%08lX"), lpstContext->Dr6);
			lvItem.iSubItem = 0;
			lvItem.pszText = szBuffer1;
			SendDlgItemMessage(hWnd, IDC_LIST5, LVM_SETITEM, 0, (LPARAM)&lvItem);

			lvItem.iSubItem = 1;
			lvItem.pszText = szBuffer2;
			SendDlgItemMessage(hWnd, IDC_LIST5, LVM_SETITEM, 0, (LPARAM)&lvItem);
			break;
		case 24:
			lstrcpy(szBuffer1, "Dr7");
			wsprintf(szBuffer2, TEXT("%08lX"), lpstContext->Dr7);
			lvItem.iSubItem = 0;
			lvItem.pszText = szBuffer1;
			SendDlgItemMessage(hWnd, IDC_LIST5, LVM_SETITEM, 0, (LPARAM)&lvItem);

			lvItem.iSubItem = 1;
			lvItem.pszText = szBuffer2;
			SendDlgItemMessage(hWnd, IDC_LIST5, LVM_SETITEM, 0, (LPARAM)&lvItem);
			break;

		

		}
		i++;
	}
	

}



//反汇编（显示出来）
int _stdcall _Disassembled(BYTE lpBuffer[], DWORD Address)
{	

	csh			csHandle;				//用来调用Caps引擎API得句柄
	cs_insn*	lpInsn;					//用来保存反汇编指令得信息
	size_t		count;					//保存缓冲区中反汇编指令信息得大小
	HWND		hWnd;
	CONTEXT		stContext;				//线程环境
	DWORD		dwItemEip;				//Eip所在的行
	char		szBuffer1[256] = { 0 };
	char		szBuffer2[256] = { 0 };

	if (cs_open(CS_ARCH_X86, CS_MODE_32, &csHandle))
	{
		MessageBox(NULL, TEXT("反汇编失败！"), TEXT("错误"), MB_OK);
		return -1;
	}
	
	count = cs_disasm(csHandle, lpBuffer, CODE_NUM, Address, 0, &lpInsn);		//对缓冲区中的机器码进行反汇编



	stContext.ContextFlags = CONTEXT_FULL;
	GetThreadContext(hIsDebuggedThread, &stContext);
	EnumChildWindows(hWinMain, _EnumChildProc, 0);				//获得CPU的窗口句柄
	
	hWnd = hChildWindow;
	
	ListView_DeleteAllItems(GetDlgItem(hWnd, IDC_LIST1));

	
	LVITEM       lvItem;					//ListView列表项属性
	int			 i = 0;
	while (i < count)
	{
		memset(&lvItem, 0, sizeof(lvItem));
		lvItem.mask = LVIF_TEXT;
		lvItem.iItem = i;
		SendDlgItemMessage(hWnd, IDC_LIST1, LVM_INSERTITEM, 0, (LPARAM)&lvItem);

		
		
		dwAddressToEip[i][0] = lpInsn[i].address;
		wsprintf(szBuffer1, TEXT("%08lX"), lpInsn[i].address);
		lvItem.pszText = szBuffer1;
		lvItem.iSubItem = 0;
		SendDlgItemMessage(hWnd, IDC_LIST1, LVM_SETITEM, 0, (LPARAM)&lvItem);
		
		if (lpInsn[i].address == stContext.Eip)
		{
			ListView_SetItemState(GetDlgItem(hWnd, IDC_LIST1), i, LVIS_SELECTED, LVIS_SELECTED);
			dwItemEip = i;
		}
		memset(szBuffer2, 0, sizeof(szBuffer2));
		for (int m = 0; m < lpInsn[i].size; m++)
		{
			wsprintf(szBuffer1, TEXT("%02X"), lpInsn[i].bytes[m]);
			lstrcat(szBuffer2, szBuffer1);
		}
		lvItem.pszText = szBuffer2;
		lvItem.iSubItem = 1;
		SendDlgItemMessage(hWnd, IDC_LIST1, LVM_SETITEM, 0, (LPARAM)&lvItem);


		wsprintf(szBuffer1, lpInsn[i].mnemonic);
		wsprintf(szBuffer2, lpInsn[i].op_str);
		lstrcat(szBuffer1, "   ");
		lstrcat(szBuffer1, szBuffer2);
		lvItem.pszText = szBuffer1;
		lvItem.iSubItem = 2;
		SendDlgItemMessage(hWnd, IDC_LIST1, LVM_SETITEM, 0, (LPARAM)&lvItem);

		i++;
	}


	//获得行高，从而使焦点滑倒eip处
	RECT	stRect;
	ListView_GetItemRect(GetDlgItem(hWnd, IDC_LIST1), 0, &stRect, LVIR_SELECTBOUNDS);
	ListView_Scroll(GetDlgItem(hWnd, IDC_LIST1), 0, (stRect.bottom - stRect.top) * dwItemEip);
	ListView_SetHotItem(GetDlgItem(hWnd, IDC_LIST1), dwItemEip);

	cs_free(lpInsn, count);			//释放cs_disasm申请的内存
	cs_close(&csHandle);			//关闭句柄
	return 0;
}




//显示数据窗口
BOOL _stdcall _ShowDataWindow(HINSTANCE	hInstance)
{
	BYTE		Buffer[0x4000];
	
	HWND		hWnd;
	LVITEM		lvItem;												//ListView列表项属性
	char		szBuffer1[256] = { 0 };
	char		szBuffer2[256] = { 0 };

	DWORD		dwReadSize;
	
	if (!ReadProcessMemory(hIsDebuggedProcess, hInstance, Buffer, sizeof(Buffer), &dwReadSize))
	{
		MessageBox(NULL, TEXT("无法查看！"), NULL, MB_OK);
		return 0;
	}
	



	EnumChildWindows(hWinMain, _EnumChildProc, 0);				//获得CPU的窗口句柄		
	hWnd = hChildWindow;
	
	ListView_DeleteAllItems(GetDlgItem(hWnd, IDC_LIST2));
	
	int i = 0;
	while (i < 0x400)
	{

		memset(&lvItem, 0, sizeof(lvItem));
		lvItem.mask = LVIF_TEXT;
		lvItem.iItem = i;
		SendDlgItemMessage(hWnd, IDC_LIST2, LVM_INSERTITEM, 0, (LPARAM)&lvItem);


		wsprintf(szBuffer1, TEXT("%08lX"), (DWORD)hInstance + i * 0x10);
		lvItem.pszText = szBuffer1;
		lvItem.iSubItem = 0;
		SendDlgItemMessage(hWnd, IDC_LIST2, LVM_SETITEM, 0, (LPARAM)&lvItem);

		memset(szBuffer2, 0, sizeof(szBuffer2));
		for (int m = 0; m < 16; m++)
		{
			wsprintf(szBuffer1, TEXT("%02lX"), Buffer[i * 0x10 + m]);

			lstrcat(szBuffer2, szBuffer1);
			lstrcat(szBuffer2, " ");
		}
		lvItem.pszText = szBuffer2;
		lvItem.iSubItem = 1;
		SendDlgItemMessage(hWnd, IDC_LIST2, LVM_SETITEM, 0, (LPARAM)&lvItem);

		memset(szBuffer2, 0, sizeof(szBuffer2));
		for (int m = 0; m < 16; m++)
		{
			if(Buffer[i * 0x10 + m] == 0)
				wsprintf(szBuffer1, TEXT("%c"), '.');
			else
				wsprintf(szBuffer1, TEXT("%c"), char(Buffer[i * 0x10 + m]));
			lstrcat(szBuffer2, szBuffer1);
		}
		lvItem.pszText = szBuffer2;
		lvItem.iSubItem = 2;
		SendDlgItemMessage(hWnd, IDC_LIST2, LVM_SETITEM, 0, (LPARAM)&lvItem);

		i++;

	}

	return 0;

}








//显示堆栈窗口

void _stdcall _ShowPointer()
{
	BYTE		Buffer[0x4000];
	CONTEXT		stContext;
	stContext.ContextFlags = CONTEXT_ALL;
	GetThreadContext(hIsDebuggedThread, &stContext);


	ReadProcessMemory(hIsDebuggedProcess, (LPVOID)(stContext.Esp - (stContext.Esp % 0x4000)), Buffer, sizeof(Buffer), NULL);
	
	HWND		hWnd;
	LVITEM		lvItem;												//ListView列表项属性
	char		szBuffer1[256] = { 0 };
	char		szBuffer2[256] = { 0 };

	EnumChildWindows(hWinMain, _EnumChildProc, 0);				//获得CPU的窗口句柄		
	hWnd = hChildWindow;
	int i = 0;


	ListView_DeleteAllItems(GetDlgItem(hWnd, IDC_LIST3));

	while (i < 0x1000)
	{
		memset(&lvItem, 0, sizeof(lvItem));
		lvItem.mask = LVIF_TEXT;
		lvItem.iItem = i;
		SendDlgItemMessage(hWnd, IDC_LIST3, LVM_INSERTITEM, 0, (LPARAM)&lvItem);


		wsprintf(szBuffer1, TEXT("%08lX"), (stContext.Esp - (stContext.Esp % 0x4000)) + i * 4);
		lvItem.pszText = szBuffer1;
		lvItem.iSubItem = 0;
		SendDlgItemMessage(hWnd, IDC_LIST3, LVM_SETITEM, 0, (LPARAM)&lvItem);

		
		memset(szBuffer2, 0, sizeof(szBuffer2));
		for (int m = 0; m < 4; m++)
		{
			wsprintf(szBuffer1, TEXT("%02lX"), Buffer[i * 4 + m]);
			lstrcat(szBuffer1, szBuffer2);
			lstrcpy(szBuffer2, szBuffer1);
		}
		lvItem.pszText = szBuffer2;
		lvItem.iSubItem = 1;
		SendDlgItemMessage(hWnd, IDC_LIST3, LVM_SETITEM, 0, (LPARAM)&lvItem);

		i++;

	}
	//滑倒esp处
	RECT	stRect;
	ListView_GetItemRect(GetDlgItem(hWnd, IDC_LIST3), 0, &stRect, LVIR_SELECTBOUNDS);
	ListView_Scroll(GetDlgItem(hWnd, IDC_LIST3), 0, (stRect.bottom - stRect.top)* ((stContext.Esp % 0x4000) / 4));
	ListView_SetHotItem(GetDlgItem(hWnd, IDC_LIST3), (stContext.Esp % 0x4000) / 4);
	
	dwItemEsp = stContext.Esp ;

}











//int3断点处理
int _stdcall _DealBreakPoint(DEBUG_EVENT devent, CONTEXT& stContext, DWORD dwEntryPoint, PROCESS_INFORMATION pi)
{
	INT3* lpInt3 = lpHeadInt3;					//辅助头指针进行查询
	BYTE	    lpBuffer[0x1000] = { 0 };			//存放待反汇编的机器码

	while (lpInt3 != NULL)		//判断是否是我们设置的断点
	{
		if (lpInt3->dwAddress == (DWORD)devent.u.Exception.ExceptionRecord.ExceptionAddress)
		{
			flag = 1;			//表示是我们设置的int3断点
			flag1 = 1;			//表示发生了int3断点(让他恢复断点)
			break;
		}
		else if(stStep.dwStepAddress == (DWORD)devent.u.Exception.ExceptionRecord.ExceptionAddress)
		{
			flag = 3;			//表示是我们是单步执行
			flag1 = 0;			//表示发生了int3断点（不让他恢复断点，相当于一次性断点，用来完成单步步过）
			stStep.dwStepAddress = 0;
			break;
		}
		else
			lpInt3 = lpInt3->lpNextINT3;
	}
	if (flag == 0)				//如果不是我们设置的断点，则交给程序处理
		return -1;
	else						//如果是我们设置的断点则进行处理。
	{
		if (devent.u.Exception.dwFirstChance == 1)       //在第一次异常的时候处理
		{


			stContext.ContextFlags = CONTEXT_ALL;
			GetThreadContext(pi.hThread, &stContext);


			
			stContext.Eip--;																	//令eip-1
			if(flag1 == 0)
				WriteProcessMemory(pi.hProcess, (LPVOID)stContext.Eip, &stStep.oldByte, 1, NULL);
			else
			{
				WriteProcessMemory(pi.hProcess, (LPVOID)stContext.Eip, &lpInt3->oldByte, 1, NULL);	//还原原来的字节
				stContext.EFlags = stContext.EFlags | b00000000000000000000000100000000;			//设置TF单步标志位，以便还原断点
			}
			SetThreadContext(pi.hThread, &stContext);
			

			if ((DWORD)devent.u.Exception.ExceptionRecord.ExceptionAddress == dwEntryPoint)		//如果是在入口点
			{
				ReadProcessMemory(pi.hProcess, (LPCVOID)(dwEntryPoint - (dwEntryPoint % 0x1000)), lpBuffer, CODE_NUM, NULL);
				_Disassembled(lpBuffer, dwEntryPoint - (dwEntryPoint % 0x1000));										//进行机器码反汇编，并显示
				_ShowContext(&stContext);															//显示所有寄存器的值
				_ShowDataWindow((HINSTANCE)(dwEntryPoint - dwEntryPointRVA));						//显示数据窗口
				_ShowPointer();																		//显示堆栈窗口
			}
		}

	}

	return 0;

}




//单步异常处理
int _stdcall _DealSingle(DEBUG_EVENT devent, PROCESS_INFORMATION pi)
{
	INT3* lpInt3 = lpHeadInt3;					
	CONTEXT		stContext;
	BYTE		bBuffer;
	DWORD		dwModel;

	if (flag1 == 3)							//刚刚发生了内存断点（为了恢复他）
	{
		flag1 = 0;
		VirtualProtectEx(hIsDebuggedProcess, (LPVOID)stMemory.dwAddress, stMemory.dwSize, dModel, &dwModel);
		if (Button == '5')
			flag = 2;			//直接运行操作
		else if (Button == '7')
			flag = 3;			//单步执行操作（步入）
		else if (Button == '8')
			flag = 3;			//单步(步过)
			
		
	}
	else if (flag1 == 2)					//刚刚发生了硬件断点（说明此单步是为了恢复刚刚触发的硬件断点）
	{
		flag1 = 0;
		stContext.ContextFlags = CONTEXT_ALL;
		GetThreadContext(pi.hThread, &stContext);
		if (stContext.Dr0 != 0)
			stContext.Dr7 = stContext.Dr7 + 1;
		if (stContext.Dr1 != 0)
			stContext.Dr7 = stContext.Dr7 + 4;
		if (stContext.Dr2 != 0)
			stContext.Dr7 = stContext.Dr7 + 16;
		if (stContext.Dr3 != 0)
			stContext.Dr7 = stContext.Dr7 + 64;
		SetThreadContext(pi.hThread, &stContext);


		if (Button == '5')
			flag = 2;			//直接运行操作
		else if (Button == '7')
			flag = 3;			//单步执行操作（步入）
		else if (Button == '8')
		{
			if (stStep.dwStepAddress == 0)
				flag = 3;			//单步
			else
				flag = 2;			//直接
		}
	}
	else if (flag1 == 1)			//刚刚发生了int3断点（说明此单步是为了恢复刚刚触发的int3断点）
	{
		flag1 = 0;
		while (lpInt3 != NULL)
		{

			if (lpInt3->lpNextINT3 == NULL || lpInt3->lpNextINT3->dwAddress >= (DWORD)devent.u.Exception.ExceptionRecord.ExceptionAddress)
			{
				ReadProcessMemory(pi.hProcess, (LPVOID)lpInt3->dwAddress, &bBuffer, 1, NULL);
				if (bBuffer != 0xCC)
					WriteProcessMemory(pi.hProcess, (LPVOID)lpInt3->dwAddress, &NewByte, 1, NULL);
				break;
			}
			lpInt3 = lpInt3->lpNextINT3;
		}
		if (Button == '5')
			flag = 2;			//直接运行操作
		else if (Button == '7')
			flag = 3;			//单步执行操作（步入）
		else if (Button == '8')
			flag = 3;			//单步执行操作（步过）
	}
	else if (flag1 == 0)
		flag = 3;			//单步执行操作

	return 0;

}










//硬件断点处理
void _stdcall _DealHardware(DEBUG_EVENT devent, CONTEXT stContext)
{
	flag = 4;				//硬件执行断点
	flag1 = 2;				//刚刚执行了硬件断点(为了恢复硬件断点)
	if (stContext.Dr6 & 1)
	{
		if((stContext.Dr7 & 0x10000) && (stContext.Dr7 & 0x30000))		//硬件写入断点
		{
			flag1 = 0;
			flag = 6;
		}
		
	}
	else if (stContext.Dr6 & 2)
	{
		if ((stContext.Dr7 & 0x100000) && (stContext.Dr7 & 0x300000))
		{	
			flag1 = 0;
			flag = 6;
		}
	}
	else if (stContext.Dr6 & 4)
	{
		if ((stContext.Dr7 & 0x1000000) && (stContext.Dr7 & 0x3000000))
		{
			flag1 = 0;
			flag = 6;
		}
	}
	else if (stContext.Dr6 & 8)
	{
		if ((stContext.Dr7 & 0x10000000) && (stContext.Dr7 & 0x30000000))
		{
			flag1 = 0;
			flag = 6;
		}
	}

}





//内存断点处理
void _stdcall _DealAccess(DEBUG_EVENT devent)
{
	if (devent.u.Exception.ExceptionRecord.ExceptionAddress == (LPVOID)stMemory.dwAddress)
	{

		flag = 5;		//发生内存断点
		flag1 = 3;		//刚刚发生了内存断点（为了单步恢复内存断点）
	}

	else
	{
		flag = 0;		//程序自己的异常
	}
}





//处理退出正在调试的程序
void _stdcall _DealClean()
{
	lpHeadInt3 = NULL;			//断点列表清空
	
	flag = 0;			//不同标志意义不同
	flag1 = 0;			//标志是刚刚是否发生了int3断点。
	
	memset(dwAddressToEip, 0, sizeof(dwAddressToEip));
	dwEntryPoint = 0;		//入口地址RVA
	stMemory.dwAddress = 0;	//内存断点地址
	EnumChildWindows(hWinMain, _EnumChildProc, 0);				//获得CPU的窗口句柄		
	ListView_DeleteAllItems(GetDlgItem(hChildWindow, IDC_LIST1));
	ListView_DeleteAllItems(GetDlgItem(hChildWindow, IDC_LIST2));
	ListView_DeleteAllItems(GetDlgItem(hChildWindow, IDC_LIST3));
	ListView_DeleteAllItems(GetDlgItem(hChildWindow, IDC_LIST5));
}








//我自己的ContinueDebugEvent，------回复调试子系统
int _stdcall _MyContinueDebugEvent(DEBUG_EVENT devent, PROCESS_INFORMATION pi)
{
	DWORD				dwModel;
	CONTEXT				stContext;			//线程信息块
	stContext.ContextFlags = CONTEXT_ALL;


	
	if (flag == 6)													//硬件写入断点
	{
		GetThreadContext(pi.hThread, &stContext);

		_UpdateCpu(stContext);							//更新CPU窗口显示
		SuspendThread(hDebugThread);					//等待UI线程操作返回
		GetThreadContext(pi.hThread, &stContext);

		
		if (Button == '5')			//运行
		{
			//SetThreadContext(pi.hThread, &stContext);
			ContinueDebugEvent(devent.dwProcessId, devent.dwThreadId, DBG_CONTINUE);
		}
		else if (Button == '7')		//单步执行(单步步入)
		{
			stContext.EFlags = stContext.EFlags | b00000000000000000000000100000000;	//令TF位为1
			SetThreadContext(pi.hThread, &stContext);
			ContinueDebugEvent(devent.dwProcessId, devent.dwThreadId, DBG_CONTINUE);
		}

		else if (Button == '8')		//单步执行（单步步过）
		{


			_ToNextAddress(stContext);
			if (stStep.dwStepAddress == 0)
			{
				stContext.EFlags = stContext.EFlags | b00000000000000000000000100000000;	//令TF位为1
				SetThreadContext(pi.hThread, &stContext);
			}
			else
				WriteProcessMemory(pi.hProcess, (LPVOID)stStep.dwStepAddress, &NewByte, 1, NULL);
			int n = GetLastError();
			ContinueDebugEvent(devent.dwProcessId, devent.dwThreadId, DBG_CONTINUE);

		}




	}

	else if (flag == 5)												//内存断点
	{
		GetThreadContext(pi.hThread, &stContext);

		_UpdateCpu(stContext);							//更新CPU窗口显示
		SuspendThread(hDebugThread);					//等待UI线程操作返回

		GetThreadContext(pi.hThread, &stContext);
		stContext.EFlags = stContext.EFlags | b00000000000000000000000100000000;	//令TF位为1
		SetThreadContext(hIsDebuggedThread, &stContext);							//设置单步为了恢复内存访问断点

		int n = VirtualProtectEx(hIsDebuggedProcess, (LPVOID)stMemory.dwAddress, stMemory.dwSize, stMemory.oldModel, &dwModel);				//取消内存断点
		
		int n1 = GetLastError();
		ContinueDebugEvent(devent.dwProcessId, devent.dwThreadId, DBG_CONTINUE);	//运行
		



	}
	 else if (flag == 4)										//硬件断点
	{
		

		GetThreadContext(pi.hThread, &stContext);
		
		_UpdateCpu(stContext);							//更新CPU窗口显示
		SuspendThread(hDebugThread);					//等待UI线程操作返回


		GetThreadContext(pi.hThread, &stContext);

		stContext.EFlags = stContext.EFlags | b00000000000000000000000100000000;	//令TF位为1

		stContext.Dr7 = stContext.Dr7 - stContext.Dr7 % 86;			//先取消调试断点
		SetThreadContext(hIsDebuggedThread, &stContext);


		if (Button == '5')			//运行
			ContinueDebugEvent(devent.dwProcessId, devent.dwThreadId, DBG_CONTINUE);
		else if (Button == '7')		//单步执行(单步步入)
			ContinueDebugEvent(devent.dwProcessId, devent.dwThreadId, DBG_CONTINUE);


		else if (Button == '8')		//单步执行（单步步过）
		{


			_ToNextAddress(stContext);
			if (stStep.dwStepAddress != 0)
				WriteProcessMemory(pi.hProcess, (LPVOID)stStep.dwStepAddress, &NewByte, 1, NULL);
			ContinueDebugEvent(devent.dwProcessId, devent.dwThreadId, DBG_CONTINUE);

		}




	
	}

	else if (flag == 3)							//单步执行
	{
		GetThreadContext(pi.hThread, &stContext);
		_UpdateCpu(stContext);							//更新CPU窗口显示
		SuspendThread(hDebugThread);					//等待UI线程操作返回

		GetThreadContext(pi.hThread, &stContext);
		if (Button == '5')			//运行
			ContinueDebugEvent(devent.dwProcessId, devent.dwThreadId, DBG_CONTINUE);
		else if (Button == '7')		//单步执行(单步步入)
		{
			stContext.EFlags = stContext.EFlags | b00000000000000000000000100000000;	//令TF位为1
			SetThreadContext(pi.hThread, &stContext);
			ContinueDebugEvent(devent.dwProcessId, devent.dwThreadId, DBG_CONTINUE);
		}

		else if (Button == '8')		//单步执行（单步步过）
		{


			_ToNextAddress(stContext);
			if (stStep.dwStepAddress == 0)
			{
				stContext.EFlags = stContext.EFlags | b00000000000000000000000100000000;	//令TF位为1
				SetThreadContext(pi.hThread, &stContext);
			}
			else
				WriteProcessMemory(pi.hProcess, (LPVOID)stStep.dwStepAddress, &NewByte, 1, NULL);
			int n = GetLastError();
			ContinueDebugEvent(devent.dwProcessId, devent.dwThreadId, DBG_CONTINUE);

		}



	}
	else if (flag == 2)					//刚执行完int3断点处指令，利用单步执行恢复int3断点并直接运行
	{
		ContinueDebugEvent(devent.dwProcessId, devent.dwThreadId, DBG_CONTINUE);
	}
	else if (flag == 1)					//程序执行到了断点处
	{
		GetThreadContext(pi.hThread, &stContext);
		_UpdateCpu(stContext);							//更新CPU窗口显示
		SuspendThread(hDebugThread);				//等待UI线程操作返回

		GetThreadContext(pi.hThread, &stContext);
		if (Button == '5')			//运行
			ContinueDebugEvent(devent.dwProcessId, devent.dwThreadId, DBG_CONTINUE);
		else if (Button == '7')		//单步执行（单步步入）
		{
			stContext.EFlags = stContext.EFlags | b00000000000000000000000100000000;	//令TF位为1
			SetThreadContext(pi.hThread, &stContext);
			ContinueDebugEvent(devent.dwProcessId, devent.dwThreadId, DBG_CONTINUE);
		}

		else if (Button == '8')		//单步执行（单步步过）
		{


			_ToNextAddress(stContext);
			if (stStep.dwStepAddress == 0)
			{
				stContext.EFlags = stContext.EFlags | b00000000000000000000000100000000;	//令TF位为1
				SetThreadContext(pi.hThread, &stContext);
			}
			else
				WriteProcessMemory(pi.hProcess, (LPVOID)stStep.dwStepAddress, &NewByte, 1, NULL);


			ContinueDebugEvent(devent.dwProcessId, devent.dwThreadId, DBG_CONTINUE);

		}
	


	}
	else if (flag == 0)					//非异常调试事件以及程序真正的"异常"
		ContinueDebugEvent(devent.dwProcessId, devent.dwThreadId, DBG_EXCEPTION_NOT_HANDLED);
	

	flag = 0;
		

	return 0;
}



















//更新CPU窗口
void _stdcall _UpdateCpu(CONTEXT stContext)
{
	HWND	hWnd;
	DWORD	dwData;
	RECT	stRect;
	char	szBuffer1[256];
	BYTE	lpBuffer[0x1000];


	//获得CPU的窗口句柄	
	EnumChildWindows(hWinMain, _EnumChildProc, 0);					
	hWnd = hChildWindow;
	
	
	ListView_GetItemText(GetDlgItem(hWnd, IDC_LIST1), 0, 0, szBuffer1,sizeof(szBuffer1));
	ListView_GetItemRect(GetDlgItem(hWnd, IDC_LIST1), 0, &stRect, LVIR_SELECTBOUNDS);

	dwData = strtol(szBuffer1, NULL, 16);

	if ((stContext.Eip - dwData) > 0x1000 || dwData > stContext.Eip)
	{
		ReadProcessMemory(hIsDebuggedProcess, (LPCVOID)(stContext.Eip - (stContext.Eip % 0x1000)), lpBuffer, CODE_NUM, NULL);
		_Disassembled(lpBuffer, stContext.Eip - (stContext.Eip % 0x1000));					//反汇编并显示反汇编窗口
	}
	else
	{
		ListView_GetItemRect(GetDlgItem(hWnd, IDC_LIST1), 0, &stRect, LVIR_SELECTBOUNDS);
		ListView_Scroll(GetDlgItem(hWnd, IDC_LIST1), 0, -((stRect.bottom - stRect.top) * 0x1000));
		int i = 0;
		while (1)
		{


			if (dwAddressToEip[i][0] == stContext.Eip)
			{
				//ListView_SetItemState(GetDlgItem(hWnd, IDC_LIST1), i, LVIS_SELECTED, LVIS_SELECTED);
				ListView_Scroll(GetDlgItem(hWnd, IDC_LIST1), 0, (stRect.bottom - stRect.top) * i);
				ListView_SetHotItem(GetDlgItem(hWnd, IDC_LIST1), i);
				break;
			}
			i++;
		}
		
	}
	//更新数据窗口
	ListView_GetItemText(GetDlgItem(hWnd, IDC_LIST2), 0, 0, szBuffer1, sizeof(szBuffer1))
	dwData = strtol(szBuffer1, NULL, 16);
	if(dwData != 0)
		_ShowDataWindow((HINSTANCE)dwData);



	//更新寄存器窗口
	_ShowContext(&stContext);
	

	//更新堆栈窗口
	ListView_GetItemText(GetDlgItem(hWnd, IDC_LIST3), 0, 0, szBuffer1, sizeof(szBuffer1));
	dwData = strtol(szBuffer1, NULL, 16);
	ListView_SetHotItem(GetDlgItem(hWnd, IDC_LIST3), (stContext.Esp - dwData) / 4);							//设置热点项


	
	ListView_Scroll(GetDlgItem(hWnd, IDC_LIST3), 0,(0 - (stRect.bottom - stRect.top) * 0x1000));					//将esp移动到视野中
	ListView_Scroll(GetDlgItem(hWnd, IDC_LIST3), 0, (stRect.bottom - stRect.top) * ListView_GetHotItem(GetDlgItem(hWnd, IDC_LIST3)));
	
	
	
	dwItemEsp = stContext.Esp;
}



//反汇编下一条指令，辅助完成单步步过
DWORD _stdcall _ToNextAddress(CONTEXT stContext)
{
	
	csh			csHandle;				//用来调用Caps引擎API得句柄
	cs_insn*	lpInsn;					//用来保存反汇编指令得信息
	size_t		count;					//保存缓冲区中反汇编指令信息得大小
	
	BYTE		lpBuffer[0x20];
	ReadProcessMemory(hIsDebuggedProcess, (LPCVOID)stContext.Eip, lpBuffer, sizeof(lpBuffer), NULL);
	if (cs_open(CS_ARCH_X86, CS_MODE_32, &csHandle))
	{
		MessageBox(NULL, TEXT("反汇编失败！"), TEXT("错误"), MB_OK);
		return -1;
	}

	count = cs_disasm(csHandle, lpBuffer, sizeof(lpBuffer), stContext.Eip, 2, &lpInsn);		//对缓冲区中的机器码进行反汇编
	if (strcmp(lpInsn[0].mnemonic, "call") != 0)
	{
		cs_free(lpInsn, count);			
		cs_close(&csHandle);			
		stStep.dwStepAddress = 0;
		return 0;
	}
	stStep.dwStepAddress = lpInsn[1].address;
	stStep.oldByte = lpInsn[1].bytes[0];

	cs_free(lpInsn, count);
	cs_close(&csHandle);


	return 0;
}






//设置Int3断点
int _stdcall _SetInt3(HWND hWnd,DWORD dwItem)
{
	DWORD	dwAddress;
	CONTEXT	stContext;
	char	szBuffer[256];
	char	szBuffer1[256] = "************";
	INT3* lpInt3 = lpHeadInt3;
	INT3* lpInt3Front = NULL;
	ListView_GetItemText(GetDlgItem(hWnd, IDC_LIST1), dwItem, 0, szBuffer, sizeof(szBuffer));
	dwAddress = strtol(szBuffer, NULL, 16);
	
	stContext.ContextFlags = CONTEXT_ALL;
	GetThreadContext(hIsDebuggedThread, &stContext);
	if ((stContext.Dr0 == dwAddress) || (stContext.Dr1 == dwAddress) || (stContext.Dr2 == dwAddress) || (stContext.Dr3 == dwAddress))
	{
		MessageBox(NULL, TEXT("已经设置过硬件执行断点！"), NULL, MB_OK);
		return 0;
	}




	while (lpInt3 != NULL)					//判断是否已经设置断点
	{
		if (lpInt3->dwAddress == dwAddress)				//如果以设置则取消断点
		{
			WriteProcessMemory(hIsDebuggedProcess, (LPVOID)dwAddress, &(lpInt3->oldByte), 1, NULL);

			if (lpInt3Front == NULL)
				lpHeadInt3 = NULL;
			else
				lpInt3Front->lpNextINT3 = lpInt3->lpNextINT3;		
			memset(szBuffer1, 0, sizeof(szBuffer1));
			ListView_SetItemText(GetDlgItem(hWnd, IDC_LIST1), dwItem, 3, szBuffer1);
			return 0;
		}
		lpInt3Front = lpInt3;
		lpInt3 = lpInt3->lpNextINT3;
	}


	//如果没有设置断点则新增加一个断点
	lpInt3Front->lpNextINT3 = new INT3;
	lpInt3Front->lpNextINT3->dwAddress = dwAddress;
	lpInt3Front->lpNextINT3->lpNextINT3 = NULL;
	
	ReadProcessMemory(hIsDebuggedProcess, (LPVOID)dwAddress, &(lpInt3Front->lpNextINT3->oldByte), 1, NULL);
	WriteProcessMemory(hIsDebuggedProcess, (LPVOID)dwAddress, &NewByte, 1, NULL);

	ListView_SetItemText(GetDlgItem(hWnd, IDC_LIST1), dwItem, 3, szBuffer1);

	return 0;
}







//设置硬件执行断点
BOOL _stdcall _SetHardware(HWND hWnd, LVHITTESTINFO lo)
{
	CONTEXT stContext;
	char	szBuffer[256];
	INT3* lpInt3 = lpHeadInt3;
	DWORD	dwData;
	ListView_GetItemText(GetDlgItem(hWnd, IDC_LIST1), lo.iItem, 0, szBuffer, sizeof(szBuffer));
	dwData = strtol(szBuffer, NULL, 16);





	while (lpInt3 != NULL)
	{
		if (lpInt3->dwAddress == dwData)
		{
			MessageBox(NULL, TEXT("已经设置过INT3断点！"), NULL, MB_OK);
			return 0;
		}
		lpInt3 = lpInt3->lpNextINT3;
	}
	stContext.ContextFlags = CONTEXT_ALL;
	GetThreadContext(hIsDebuggedThread, &stContext);


	if (stContext.Dr0 == 0)
	{
		stContext.Dr7 = stContext.Dr7 + 1;
		stContext.Dr0 = dwData;
	}
	else if (stContext.Dr1 == 0)
	{
		stContext.Dr7 = stContext.Dr7 + 4;
		stContext.Dr1 = dwData;
	}
	else if (stContext.Dr2 == 0)
	{
		stContext.Dr7 = stContext.Dr7 + 16;
		stContext.Dr2 = dwData;
	}
	else if (stContext.Dr3 == 0)
	{
		stContext.Dr7 = stContext.Dr7 + 64;
		stContext.Dr3 = dwData;
	}
	else
	{
		MessageBox(NULL, TEXT("硬件断点达到上限！"), NULL, MB_OK);
		return 0;
	}

	SetThreadContext(hIsDebuggedThread, &stContext);
	GetThreadContext(hIsDebuggedThread, &stContext);
	return 1;
}








//设置硬件写断点
BOOL _stdcall _SetWrite(HWND hWnd, LVHITTESTINFO lo, DWORD dwSize)
{
	CONTEXT stContext;
	char	szBuffer[256];
	DWORD	dwData;
	ListView_GetItemText(GetDlgItem(hWnd, IDC_LIST2), lo.iItem, 0, szBuffer, sizeof(szBuffer));
	dwData = strtol(szBuffer, NULL, 16);

	stContext.ContextFlags = CONTEXT_ALL;
	GetThreadContext(hIsDebuggedThread, &stContext);
	if (stContext.Dr0 == 0)
	{
		stContext.Dr7 = stContext.Dr7 + 1;			//启用断点	
		stContext.Dr0 = dwData;						//设置地址
		stContext.Dr7 = stContext.Dr7 + 0x10000;	//设置写断点

		
		if (dwSize == 1)							//设置长度
			stContext.Dr7 = stContext.Dr7 + 0x80000;
		else if (dwSize == 2)
			stContext.Dr7 = stContext.Dr7 + 0x40000;
		else if (dwSize == 3)
			stContext.Dr7 = stContext.Dr7 + 0xC0000;

	}
	else if (stContext.Dr1 == 0)
	{
		stContext.Dr7 = stContext.Dr7 + 4;			//启用断点	
		stContext.Dr1 = dwData;						//设置地址
		stContext.Dr7 = stContext.Dr7 + 0x100000;	//设置写断点
		
		if (dwSize == 1)							//设置长度
			stContext.Dr7 = stContext.Dr7 + 0x800000;
		else if (dwSize == 2)
			stContext.Dr7 = stContext.Dr7 + 0x400000;
		else if (dwSize == 3)
			stContext.Dr7 = stContext.Dr7 + 0xC00000;

	}
	else if (stContext.Dr2 == 0)
	{
		stContext.Dr7 = stContext.Dr7 + 16;			//启用断点	
		stContext.Dr2 = dwData;						//设置地址
		stContext.Dr7 = stContext.Dr7 + 0x1000000;	//设置写断点

		if (dwSize == 1)							//设置长度
			stContext.Dr7 = stContext.Dr7 + 0x8000000;
		else if (dwSize == 2)
			stContext.Dr7 = stContext.Dr7 + 0x4000000;
		else if (dwSize == 3)
			stContext.Dr7 = stContext.Dr7 + 0xC000000;
	}
	else if (stContext.Dr3 == 0)
	{
		stContext.Dr7 = stContext.Dr7 + 64;			//启用断点	
		stContext.Dr3 = dwData;						//设置地址
		stContext.Dr7 = stContext.Dr7 + 0x10000000;	//设置写断点

		if (dwSize == 1)							//设置长度
			stContext.Dr7 = stContext.Dr7 + 0x80000000;
		else if (dwSize == 2)
			stContext.Dr7 = stContext.Dr7 + 0x40000000;
		else if (dwSize == 3)
			stContext.Dr7 = stContext.Dr7 + 0xC0000000;
	}
	else
	{
		MessageBox(NULL, TEXT("硬件断点达到上限！"), NULL, MB_OK);
		return 0;
	}

	
	SetThreadContext(hIsDebuggedThread, &stContext);
	GetThreadContext(hIsDebuggedThread, &stContext);
	return 1;
}






//设置硬件读断点
BOOL _stdcall _SetRead(HWND hWnd, LVHITTESTINFO lo, DWORD dwSize)
{
	CONTEXT stContext;
	char	szBuffer[256];
	DWORD	dwData;
	ListView_GetItemText(GetDlgItem(hWnd, IDC_LIST2), lo.iItem, 0, szBuffer, sizeof(szBuffer));
	dwData = strtol(szBuffer, NULL, 16);

	stContext.ContextFlags = CONTEXT_ALL;
	GetThreadContext(hIsDebuggedThread, &stContext);
	if (stContext.Dr0 == 0)
	{
		stContext.Dr7 = stContext.Dr7 + 1;			//启用断点	
		stContext.Dr0 = dwData;						//设置地址
		stContext.Dr7 = stContext.Dr7 + 0x30000;	//设置读断点

		if (dwSize == 1)							//设置长度
			stContext.Dr7 = stContext.Dr7 + 0x80000;
		else if (dwSize == 2)
			stContext.Dr7 = stContext.Dr7 + 0x40000;
		else if (dwSize == 3)
			stContext.Dr7 = stContext.Dr7 + 0xC0000;

	}
	else if (stContext.Dr1 == 0)
	{
		stContext.Dr7 = stContext.Dr7 + 4;			//启用断点	
		stContext.Dr1 = dwData;						//设置地址
		stContext.Dr7 = stContext.Dr7 + 0x300000;	//设置读断点

		if (dwSize == 1)							//设置长度
			stContext.Dr7 = stContext.Dr7 + 0x800000;
		else if (dwSize == 2)
			stContext.Dr7 = stContext.Dr7 + 0x400000;
		else if (dwSize == 3)
			stContext.Dr7 = stContext.Dr7 + 0xC00000;

	}
	else if (stContext.Dr2 == 0)
	{
		stContext.Dr7 = stContext.Dr7 + 16;			//启用断点	
		stContext.Dr2 = dwData;						//设置地址
		stContext.Dr7 = stContext.Dr7 + 0x3000000;	//设置读断点

		if (dwSize == 1)							//设置长度
			stContext.Dr7 = stContext.Dr7 + 0x8000000;
		else if (dwSize == 2)
			stContext.Dr7 = stContext.Dr7 + 0x4000000;
		else if (dwSize == 3)
			stContext.Dr7 = stContext.Dr7 + 0xC000000;
	}
	else if (stContext.Dr3 == 0)
	{
		stContext.Dr7 = stContext.Dr7 + 64;			//启用断点	
		stContext.Dr3 = dwData;						//设置地址
		stContext.Dr7 = stContext.Dr7 + 0x30000000;	//设置读断点

		if (dwSize == 1)							//设置长度
			stContext.Dr7 = stContext.Dr7 + 0x80000000;
		else if (dwSize == 2)
			stContext.Dr7 = stContext.Dr7 + 0x40000000;
		else if (dwSize == 3)
			stContext.Dr7 = stContext.Dr7 + 0xC0000000;
	}
	else
	{
		MessageBox(NULL, TEXT("硬件断点达到上限！"), NULL, MB_OK);
		return 0;
	}


	SetThreadContext(hIsDebuggedThread, &stContext);
	GetThreadContext(hIsDebuggedThread, &stContext);
	return 1;
}





//设置内存访问断点
BOOL _stdcall _SetMemoryAccess(DWORD dwData, DWORD dwSize)
{
	DWORD	Model;
	if (stMemory.dwAddress != 0)
	{
		VirtualProtectEx(hIsDebuggedProcess, (LPVOID)stMemory.dwAddress, stMemory.dwSize, stMemory.oldModel, &Model);
		Model = stMemory.oldModel;
	}

	if (!VirtualProtectEx(hIsDebuggedProcess, (LPVOID)dwData, dwSize, PAGE_NOACCESS, &stMemory.oldModel))
	{
		stMemory.oldModel = Model;
		MessageBox(NULL, TEXT("内存访问断点设置失败！"), NULL, MB_OK);
		return 0;
	}
	dModel = PAGE_NOACCESS;
	stMemory.dwAddress = dwData;
	stMemory.dwSize = dwSize;

	MessageBox(NULL, TEXT("设置成功"), NULL, MB_OK);
	return 0;
}



//设置内存写入断点
BOOL _stdcall _SetMemoryWrite(DWORD dwData, DWORD dwSize)
{
	DWORD	Model;
	if (stMemory.dwAddress != 0)
	{
		VirtualProtectEx(hIsDebuggedProcess, (LPVOID)stMemory.dwAddress, stMemory.dwSize, stMemory.oldModel, &Model);
		Model = stMemory.oldModel;
	}

	if (!VirtualProtectEx(hIsDebuggedProcess, (LPVOID)dwData, dwSize, PAGE_EXECUTE_READ, &stMemory.oldModel))
	{
		stMemory.oldModel = Model;
		MessageBox(NULL, TEXT("内存写入断点设置失败！"), NULL, MB_OK);
		return 0;
	}
	dModel = PAGE_EXECUTE_READ;
	stMemory.dwAddress = dwData;
	stMemory.dwSize = dwSize;

	MessageBox(NULL, TEXT("设置成功"), NULL, MB_OK);
	return 0;
}



//删除内存断点
BOOL _stdcall _DeleteAccess()
{
	DWORD	model;
	if (stMemory.dwAddress == 0)
		return 0;
	else
	{
		VirtualProtectEx(hIsDebuggedProcess, (LPVOID)stMemory.dwAddress, stMemory.dwSize, stMemory.oldModel, &model);
		stMemory.dwAddress = 0;
	}
	return 0;
}
