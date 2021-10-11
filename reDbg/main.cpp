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
#define	b00000000000000000000000100000000	0x100				//��������tf��־
#define	CODE_NUM							0x1000				//ÿ�δ�Ŀ����̶�ȡ���������Ŀ

//int3�ϵ�ṹ
typedef struct Int3
{
	BYTE	oldByte;		//ԭʼ�ֽ�
	DWORD	dwAddress;		//�ϵ��ַ
	Int3* lpNextINT3;
}INT3;

INT3* lpHeadInt3 = NULL;			//�ϵ�����
//����������ַ
typedef struct Step
{
	DWORD	dwStepAddress = 0;
	BYTE	oldByte;
};


//�ڴ�ϵ�ṹ
typedef struct Memory
{
	DWORD	dwAddress = 0;				//�ڴ�ϵ��ַ
	DWORD	dwSize;					//�ϵ��С
	DWORD	oldModel;				//�ɵı���ģʽ
};

Memory	stMemory;			//�ڴ�ϵ�ṹ����һ����
Step	stStep;				//���������ṹ����һ����




int		flag = 0;			//��ͬ��־���岻ͬ
int		flag1 = 0;			//��־�Ǹո��Ƿ�����int3�ϵ㡣
int		flag2 = 0;			//��Ƿ���ര���Ƿ񱻴�
char	Button;				//�洢�ոհ��µİ���
DWORD	dwpid;				//���������̵�PID
DWORD	dModel;				//��Ҫ�ָ����ڴ�ϵ������
DWORD   dwAddressToEip[0x1000][1];	//eip��Ӧ�ڿؼ��е�����
HWND	hWinMain;			//�����ھ��
HWND	hChildWindow;		//�Ӵ��ھ��
DWORD	dwItemEsp;			//espָ���ڶ�ջ���ڵ�����
BYTE	NewByte = 0xCC;		//�ϵ㴦����д����ֽ�
DWORD	dwEntryPoint = 0;	//�����Գ������ڵ�ַ	
DWORD	dwMappingFile;		//��ִ���ļ���ӳ���С
DWORD	dwEntryPointRVA;	//��ڵ�ַRVA

DWORD	dwInstance;			//������ػ���ַ
char	ExeName[MAX_PATH] = { 0 };			//��ִ���ļ���·��
HANDLE  hDebugThread;		//�����̵߳ľ��
HANDLE	hIsDebuggedThread;	//�������߳̾��
HANDLE  hIsDebuggedProcess	= NULL; //�����Խ��̾��
char	szAddress[]			= "��ַ";
char	szCode[]			= "������";
char	szDisassembled[]	= "��������";
char	szEsegesis[]		= "ע��";
char	szRegister[]		= "�Ĵ���";
char	szHex[]				= "HEX����";
char	szAscii[]			= "ASCII";
char	szData[]			= "����";
char	szRegisterData[]	= "ֵ";
char	szPID[]				= "����";
char	szPIDpath[]			= "·��";


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



//UI������
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	MSG		stMsg;
	HACCEL	hAccelerators;
	hWinMain = CreateDialogParam(hInstance, MAKEINTRESOURCE(IDD_DIALOG1), NULL, _MainDialog, NULL);

	//���ؼ��ټ�
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








//�����ڻص�����
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
		
		case ID_40012:		//��������
			Button = '8';
			ResumeThread(hDebugThread);
			break;
		case ID_40011:		//��������
			Button = '7';
			ResumeThread(hDebugThread);
			break;
		case ID_Menu:		//����
			Button = '5';
			ResumeThread(hDebugThread);
			break;

		//�����ڴ�
		case ID_40033:
			DialogBoxParam(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_DIALOG4), NULL, _DialogFind, 0);
			break;

		//�鿴Ӳ���ϵ�
		case ID_40050:
			DialogBoxParam(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_DIALOG6), NULL, _DialogLook, 0);
			break;

		//ɾ���ڴ�ϵ�
		case ID_40052:
			_DeleteAccess();
			break;
		//�鿴�ڴ�ϵ�
		case ID_40053:
			DialogBoxParam(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_DIALOG7), NULL, _DialogSee, 0);

			break;
		//�򿪣����������߳�
		case ID_40001:
			if (dwEntryPoint != 0)
			{
				ResumeThread(hDebugThread);
				Button = '5';	//�ó�������Ȼ�������
				TerminateProcess(hIsDebuggedProcess, 0);
			}
			hDebugThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)_OpenFile, NULL, NULL, NULL);
			break;
		//�򿪸��ӳ��򴰿�
		case ID_40002:
			DialogBoxParam(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_DIALOG9), NULL, _DialogAdd, 0);
			break;
		//��������
		case ID_40017:
			ResumeThread(hDebugThread);
			Button = '5';	//�ó�������Ȼ�������
			TerminateProcess(hIsDebuggedProcess, 0);
			break;
		//�򿪷���ര��
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
					_Disassembled(lpBuffer, stContext.Eip - (stContext.Eip % 0x1000));					//����ಢ��ʾ����ര��
					_ShowContext(&stContext);															//��ʾ�Ĵ�������
					_ShowDataWindow((HINSTANCE)(dwEntryPoint - dwEntryPointRVA));						//��ʾ���ݴ���
					_ShowPointer();																		//��ʾ��ջ����

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
		//���ﲻ����hWinMain����ΪCreateDialogParam( )������û���أ��������ڽ��д��ڵ�ע��ʹ������Ӷ�����WM_INITDIALOG��Ϣ��
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




//���ӽ��̴��ڻص�����
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
	LVCOLUMN    lvColumn;						//�б�ͷ����
	memset(&lvColumn, 0, sizeof(lvColumn));
	LVITEM       lvItem;					//ListView�б�������
	switch (message)
	{


	case WM_NOTIFY:							//��Ӧ�����ؼ���Ϣ��listcontrol��	
		switch (((LPNMHDR)lParam)->code)
		{
		case NM_DBLCLK:			//����Ŀ��˫��
			switch (((LPNMITEMACTIVATE)lParam)->hdr.idFrom)
			{
			case IDC_LIST1:

				ListView_GetItemText(GetDlgItem(hWnd, IDC_LIST1), ((LPNMITEMACTIVATE)lParam)->iItem, 0, szBuffer, sizeof(szBuffer));
				dwData = strtol(szBuffer, NULL, 16);


				//����Ѵ򿪳������Ƚ��������Գ���
				if (dwEntryPoint != 0)
				{
					ResumeThread(hDebugThread);
					Button = '5';	//�ó�������Ȼ�������
					TerminateProcess(hIsDebuggedProcess, 0);
				}
				
				hIsDebuggedProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, dwData);
				
				//��ø��ӽ��̵�·��
				GetProcessImageFileName(hIsDebuggedProcess, ExeName, sizeof(ExeName));				//��ý���Dos·��
				DosPathToNtPath(ExeName, ExeName);													//Dos·��תNT·��


				//�߳̿��գ�Ϊ�˻�ý������̵߳ľ����
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




				//����ģ����գ�����������н��̻���ַ��
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

		//���Ӵ���
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




//Dos·��ת��ΪNT·��
BOOL DosPathToNtPath(LPTSTR pszDosPath, LPTSTR pszNtPath)
{
	TCHAR            szDriveStr[500];
	TCHAR            szDrive[3];
	TCHAR            szDevName[100];
	INT                iDevName;
	INT                i;

	//������
	if (!pszDosPath || !pszNtPath)
		return FALSE;

	//��ȡ���ش��������̷�,��'\0'�ָ�,��������+4
	if (GetLogicalDriveStrings(sizeof(szDriveStr), szDriveStr))
	{
		for (i = 0; szDriveStr[i]; i += 4)
		{
			if (!lstrcmpi(&(szDriveStr[i]), _T("A:\\")) || !lstrcmpi(&(szDriveStr[i]), _T("B:\\")))
				continue;    //��C�̿�ʼ

			//�̷�
			szDrive[0] = szDriveStr[i];
			szDrive[1] = szDriveStr[i + 1];
			szDrive[2] = '\0';
			if (!QueryDosDevice(szDrive, szDevName, 100))//��ѯ Dos �豸��(�̷���NT��ѯDOS)
				return FALSE;

			iDevName = lstrlen(szDevName);
			if (_tcsnicmp(pszDosPath, szDevName, iDevName) == 0)//�Ƿ�Ϊ����
			{
				lstrcpy(pszNtPath, szDrive);//����������
				lstrcat(pszNtPath, pszDosPath + iDevName);//����·��

				return TRUE;
			}
		}
	}

	lstrcpy(pszNtPath, pszDosPath);

	return FALSE;
}






//�ڴ�ϵ�鿴���ڻص�����

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





//Ӳ���ϵ�鿴���ڻص�����
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






//���Ҵ��ڻص�����
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





//���ڴ��ڻص�����
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


//����ര�ڹ���
BOOL CALLBACK _DisassembleDialog(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	RECT			stWindowRect;
	POINT			stPos;
	static LVHITTESTINFO	lo;
	static HMENU	hMenu1;
	

	int n;
	switch (message)
	{

	
	case WM_NOTIFY:							//��Ӧ�����ؼ���Ϣ��listcontrol��	
		switch (((LPNMHDR)lParam)->code)
		{
		
		/*
		case NM_CUSTOMDRAW:		//����Ŀ���ڱ�����
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



		case NM_DBLCLK:			//����Ŀ��˫��
			switch (((LPNMITEMACTIVATE)lParam)->hdr.idFrom)
			{
			case IDC_LIST1:
				if (((LPNMITEMACTIVATE)lParam)->iSubItem == 1)
					_SetInt3(hWnd, ((LPNMITEMACTIVATE)lParam)->iItem);
				break;
			}
			break;
		



		case NM_RCLICK:			//����Ŀ���һ�
			switch (((LPNMITEMACTIVATE)lParam)->hdr.idFrom)
			{
			case IDC_LIST1:
				
				GetCursorPos(&stPos);					//��������
				TrackPopupMenu(GetSubMenu(hMenu1, 0), TPM_LEFTALIGN, stPos.x, stPos.y, NULL, hWnd, NULL);
				lo.pt.x = ((LPNMITEMACTIVATE)lParam)->ptAction.x;
				lo.pt.y = ((LPNMITEMACTIVATE)lParam)->ptAction.y;
				ListView_SubItemHitTest(GetDlgItem(hWnd, IDC_LIST1), &lo);
				break;
			case IDC_LIST2:
				GetCursorPos(&stPos);					//��������
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
		case ID_40018:			//����Ӳ��ִ�жϵ�
			_SetHardware(hWnd, lo);
			break;
		case ID_40024:			//Ӳ��д��BYTE��
		case ID_40046:
			_SetWrite(hWnd, lo, 0);
			break;

		case ID_40025:			//Ӳ��д��WORD��
		case ID_40047:
			_SetWrite(hWnd, lo, 1);
			break;
		case ID_40026:			//Ӳ��д��DWORD��
		case ID_40048:
			_SetWrite(hWnd, lo, 2);
			break;

		case ID_40028:			//Ӳ��д��DWORD-2��
		case ID_40049:
			_SetWrite(hWnd, lo, 3);
			break;

		case ID_40021:			//Ӳ������BYTE��
		case ID_40042:
			_SetRead(hWnd, lo, 0);
			break;
		
		case ID_40022:			//Ӳ������WORD��
		case ID_40043:
			_SetRead(hWnd, lo, 1);
			break;

		case ID_40023:			//Ӳ������DWORD��
		case ID_40044:
			_SetRead(hWnd, lo, 2);
			break;

		case ID_40027:			//Ӳ������DWORD-2��
		case ID_40045:
			_SetRead(hWnd, lo, 3);
			break;



		case ID_40039:			//�ڴ����
			DialogBoxParam(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_DIALOG5), NULL, _DialogMemory, 0);
			break;
		case ID_40040:			//�ڴ�д��
			DialogBoxParam(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_DIALOG5), NULL, _DialogMemory, 1);
			break;
		//case ID_40041:			//�����ϵ�
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
		_ChangeList(hWnd, stWindowRect);				//�ı��б�ʹ������Ӧ
		break;
	case WM_INITDIALOG:
		GetClientRect(hWnd, &stWindowRect);
		_InitList(hWnd, stWindowRect);					//��ʼ���б�	

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




//�ڴ�ϵ����ô��ڻص�����
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
				_SetMemoryAccess(dwData, dwSize);			//�����ڴ���ʶϵ�
			else if (flag == 1)
				_SetMemoryWrite(dwData, dwSize);			//�����ڴ�д��ϵ�
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







//��ʼ���б�ͷ
int _stdcall _InitList(HWND hWnd, RECT	stWindowRect)
{
	LVCOLUMN    lvColumn;						//�б�ͷ����
	memset(&lvColumn, 0, sizeof(lvColumn));
	
	
	//����ര��
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

	//�Ĵ�������
	SendMessage(GetDlgItem(hWnd, IDC_LIST5), LVM_SETEXTENDEDLISTVIEWSTYLE, 0, (LPARAM)(LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER));
	lvColumn.cx = (stWindowRect.right / 8);
	lvColumn.pszText = szRegister;
	SendDlgItemMessage(hWnd, IDC_LIST5, LVM_INSERTCOLUMN, 0, (LPARAM)&lvColumn);

	SendMessage(GetDlgItem(hWnd, IDC_LIST5), LVM_SETEXTENDEDLISTVIEWSTYLE, 0, (LPARAM)(LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER));
	lvColumn.cx = (stWindowRect.right / 8);
	lvColumn.pszText = szRegisterData;
	SendDlgItemMessage(hWnd, IDC_LIST5, LVM_INSERTCOLUMN, 1, (LPARAM)&lvColumn);
	
	//���ݴ���
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


	//��ջ����
	SendMessage(GetDlgItem(hWnd, IDC_LIST3), LVM_SETEXTENDEDLISTVIEWSTYLE, 0, (LPARAM)LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER);
	lvColumn.cx = (stWindowRect.right / 12) * 1;
	lvColumn.pszText = szAddress;
	SendDlgItemMessage(hWnd, IDC_LIST3, LVM_INSERTCOLUMN, 0, (LPARAM)&lvColumn);

	lvColumn.cx = (stWindowRect.right / 12) * 1;
	lvColumn.pszText = szData;
	SendDlgItemMessage(hWnd, IDC_LIST3, LVM_INSERTCOLUMN, 1, (LPARAM)&lvColumn);


	return 0;
}



//�ı��б�ͷ
int _stdcall _ChangeList(HWND hWnd, RECT stWindowRect)
{
	
	
	LVCOLUMN    lvColumn;						//ListView�б�ͷ����
	memset(&lvColumn, 0, sizeof(lvColumn));

	
	//����ര��
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

	//�Ĵ�������
	lvColumn.cx = (stWindowRect.right / 8);
	lvColumn.pszText = szRegister;
	SendDlgItemMessage(hWnd, IDC_LIST5, LVM_SETCOLUMN, 0, (LPARAM)&lvColumn);

	lvColumn.cx = (stWindowRect.right / 8);
	lvColumn.pszText = szRegisterData;
	SendDlgItemMessage(hWnd, IDC_LIST5, LVM_SETCOLUMN, 1, (LPARAM)&lvColumn);

	//���ݴ���
	lvColumn.cx = (stWindowRect.right / 36) * 3;
	lvColumn.pszText = szAddress;
	SendDlgItemMessage(hWnd, IDC_LIST2, LVM_SETCOLUMN, 0, (LPARAM)&lvColumn);

	lvColumn.cx = (stWindowRect.right / 36) * 18;
	lvColumn.pszText = szHex;
	SendDlgItemMessage(hWnd, IDC_LIST2, LVM_SETCOLUMN, 1, (LPARAM)&lvColumn);

	lvColumn.cx = (stWindowRect.right / 36) * 6;
	lvColumn.pszText = szAscii;
	SendDlgItemMessage(hWnd, IDC_LIST2, LVM_SETCOLUMN, 2, (LPARAM)&lvColumn);


	//��ջ����
	lvColumn.cx = (stWindowRect.right / 12) * 1;
	lvColumn.pszText = szAddress;
	SendDlgItemMessage(hWnd, IDC_LIST3, LVM_SETCOLUMN, 0, (LPARAM)&lvColumn);

	lvColumn.cx = (stWindowRect.right / 12) * 1;
	lvColumn.pszText = szData;
	SendDlgItemMessage(hWnd, IDC_LIST3, LVM_SETCOLUMN, 1, (LPARAM)&lvColumn);

	return 0;
}




//�����߳�
int _stdcall _OpenFile()
{
	char ExeName[MAX_PATH] = {0};			//��ִ���ļ���·��

	OPENFILENAME		stOF;
	PROCESS_INFORMATION pi;					//�����½��̵�һЩ�й���Ϣ
	STARTUPINFO			si;					//ָ���½��̵������������ʾ
	DEBUG_EVENT			devent;				//��Ϣ�¼�
	CONTEXT				stContext;			//�߳���Ϣ��
	memset(&stOF, 0, sizeof(stOF));
	memset(&si, 0, sizeof(si));
	si.cb = sizeof(STARTUPINFO);

			
	


	//���ļ�
	stOF.lStructSize = sizeof(stOF);
	stOF.hwndOwner = hWinMain;
	stOF.lpstrFilter = TEXT("��ִ���ļ�(*.exe,*.dll)\0*.exe;*.dll\0�����ļ�(*.*)\0*.*\0\0");
	stOF.lpstrFile = ExeName;
	stOF.nMaxFile = MAX_PATH;
	stOF.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;

	if (!GetOpenFileName(&stOF))
	{
		MessageBox(NULL, TEXT("���ļ�ʧ�ܣ�"), NULL, MB_OK);
		return -1;
	}






	HANDLE	hFile;				//�ļ����
	HANDLE	hMapFile;			//�ڴ�ӳ���ļ�������
	LPVOID	lpMapFile;			//�ڴ�ӳ���ļ�ָ��


	hFile = CreateFile(ExeName, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);

	hMapFile = CreateFileMapping(hFile, NULL, PAGE_READWRITE, 0, 0, NULL);
	lpMapFile = MapViewOfFile(hMapFile, FILE_MAP_READ, 0, 0, 0);
	IMAGE_DOS_HEADER* lpDosMZ = (IMAGE_DOS_HEADER*)lpMapFile;
	IMAGE_NT_HEADERS* lpNT = (IMAGE_NT_HEADERS*)lpMapFile;
	lpNT = (IMAGE_NT_HEADERS*)((BYTE*)lpNT + lpDosMZ->e_lfanew);

	dwMappingFile = lpNT->OptionalHeader.SizeOfImage;						//��ó���ӳ���С
	dwEntryPointRVA = lpNT->OptionalHeader.AddressOfEntryPoint;								//�����ڵ�ַ��RVA


	UnmapViewOfFile(lpMapFile);		//����ӳ��
	CloseHandle(hMapFile);			//�ر��ڴ�ӳ�������
	CloseHandle(hFile);				//�ر��ļ�

	int n;






	

	//���������Գ������
	CreateProcess(
		ExeName,
		NULL,
		NULL,
		NULL,
		FALSE, // ���ɼ̳�
		DEBUG_ONLY_THIS_PROCESS | DEBUG_PROCESS, // ����ģʽ����			(DEBUG_ONLY_THIS_PROCESS��־��ʾ�䲻�ܵ��Խ�����������ԵĻ������½��̲����Ϊ����Խ��̵ĵ��Զ���)
		NULL,
		NULL,
		&si,
		&pi);

	hIsDebuggedThread	= pi.hThread;
	hIsDebuggedProcess	= pi.hProcess;
		
	//��ȡ��ڵ�ַ
	stContext.ContextFlags = CONTEXT_ALL;
	GetThreadContext(pi.hThread, &stContext);
	dwEntryPoint = stContext.Eax;



	
	//�ڳ�����ڴ����¶ϵ㣬�Ӷ�ʹ�ڳ�����ڴ�ʱ�жϵ���������
	lpHeadInt3 = new INT3;
	lpHeadInt3->lpNextINT3 = NULL;
	lpHeadInt3->dwAddress = dwEntryPoint;
	ReadProcessMemory(pi.hProcess, (LPCVOID)dwEntryPoint, &lpHeadInt3->oldByte, 1, NULL);
	WriteProcessMemory(pi.hProcess, (LPVOID)dwEntryPoint, &NewByte, 1, NULL);		//����ڵ�ĵ�һ���ֽڸ�Ϊ0xCC

	while (TRUE)
	{
		WaitForDebugEvent(&devent, INFINITE);	//�ȴ������¼�
		switch (devent.dwDebugEventCode)
		{
		case EXCEPTION_DEBUG_EVENT:				//�쳣�����¼�
			switch (devent.u.Exception.ExceptionRecord.ExceptionCode)
			{
			
			case EXCEPTION_BREAKPOINT:			//�ϵ��쳣
			
				_DealBreakPoint(devent, stContext, dwEntryPoint, pi);							//����ϵ��쳣
				break;
			case EXCEPTION_SINGLE_STEP:			//������Ӳ���쳣
				GetThreadContext(hIsDebuggedThread, &stContext);
				if (stContext.Dr6 % 0x10 != 0)													//Ӳ���ϵ�
					_DealHardware(devent, stContext);
				else
					_DealSingle(devent, pi);													//������
				break;
			case EXCEPTION_ACCESS_VIOLATION:	//�����쳣
				_DealAccess(devent);														//��������쳣
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


//���ӽ����߳�

BOOL _stdcall _AddProcess()
{





	CONTEXT	stContext;
	DEBUG_EVENT			devent = {0};				//��Ϣ�¼�
	PROCESS_INFORMATION pi;					//�����½��̵�һЩ�й���Ϣ
	memset(&pi, 0, sizeof(pi));

	pi.hProcess = hIsDebuggedProcess;
	pi.hThread = hIsDebuggedThread;


	
	DebugActiveProcess(dwpid);

	

	//��ȡ��ڵ�ַRVA;,��ӳ���С

	DWORD lpNT;
	ReadProcessMemory(hIsDebuggedProcess, (LPVOID)(dwInstance + 0x3c), &lpNT, 4, NULL);
	lpNT = lpNT + dwInstance;

	ReadProcessMemory(hIsDebuggedProcess, (LPVOID)(lpNT + 0x50), &dwMappingFile, 4, NULL);
	ReadProcessMemory(hIsDebuggedProcess, (LPVOID)(lpNT + 0x28), &dwEntryPointRVA, 4, NULL);

	

	




//��ȡ��ڵ�ַ
dwEntryPoint = dwInstance + dwEntryPointRVA;



while (TRUE)
{
	WaitForDebugEvent(&devent, INFINITE);	//�ȴ������¼�
	
	switch (devent.dwDebugEventCode)
	{
	case EXCEPTION_DEBUG_EVENT:				//�쳣�����¼�
		switch (devent.u.Exception.ExceptionRecord.ExceptionCode)
		{

		case EXCEPTION_BREAKPOINT:			//�ϵ��쳣

			_DealBreakPoint(devent, stContext, dwEntryPoint, pi);							//����ϵ��쳣
			break;
		case EXCEPTION_SINGLE_STEP:			//������Ӳ���쳣
			GetThreadContext(hIsDebuggedThread, &stContext);
			if (stContext.Dr6 % 0x10 != 0)													//Ӳ���ϵ�
				_DealHardware(devent, stContext);
			else
				_DealSingle(devent, pi);													//������
			break;
		case EXCEPTION_ACCESS_VIOLATION:	//�����쳣
			_DealAccess(devent);														//��������쳣
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






//ö���Ӵ��ھ���ص�����
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



//��ʾ�Ĵ�����ֵ
void _stdcall _ShowContext(CONTEXT* lpstContext)
{
	HWND	hWnd;
	LVITEM  lvItem;												//ListView�б�������
	char	szBuffer1[256] = { 0 };
	char	szBuffer2[256] = { 0 };
	EnumChildWindows(hWinMain, _EnumChildProc, 0);				//���CPU�Ĵ��ھ��		
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
			lstrcpy(szBuffer1, "���ԼĴ���");
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



//����ࣨ��ʾ������
int _stdcall _Disassembled(BYTE lpBuffer[], DWORD Address)
{	

	csh			csHandle;				//��������Caps����API�þ��
	cs_insn*	lpInsn;					//�������淴���ָ�����Ϣ
	size_t		count;					//���滺�����з����ָ����Ϣ�ô�С
	HWND		hWnd;
	CONTEXT		stContext;				//�̻߳���
	DWORD		dwItemEip;				//Eip���ڵ���
	char		szBuffer1[256] = { 0 };
	char		szBuffer2[256] = { 0 };

	if (cs_open(CS_ARCH_X86, CS_MODE_32, &csHandle))
	{
		MessageBox(NULL, TEXT("�����ʧ�ܣ�"), TEXT("����"), MB_OK);
		return -1;
	}
	
	count = cs_disasm(csHandle, lpBuffer, CODE_NUM, Address, 0, &lpInsn);		//�Ի������еĻ�������з����



	stContext.ContextFlags = CONTEXT_FULL;
	GetThreadContext(hIsDebuggedThread, &stContext);
	EnumChildWindows(hWinMain, _EnumChildProc, 0);				//���CPU�Ĵ��ھ��
	
	hWnd = hChildWindow;
	
	ListView_DeleteAllItems(GetDlgItem(hWnd, IDC_LIST1));

	
	LVITEM       lvItem;					//ListView�б�������
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


	//����иߣ��Ӷ�ʹ���㻬��eip��
	RECT	stRect;
	ListView_GetItemRect(GetDlgItem(hWnd, IDC_LIST1), 0, &stRect, LVIR_SELECTBOUNDS);
	ListView_Scroll(GetDlgItem(hWnd, IDC_LIST1), 0, (stRect.bottom - stRect.top) * dwItemEip);
	ListView_SetHotItem(GetDlgItem(hWnd, IDC_LIST1), dwItemEip);

	cs_free(lpInsn, count);			//�ͷ�cs_disasm������ڴ�
	cs_close(&csHandle);			//�رվ��
	return 0;
}




//��ʾ���ݴ���
BOOL _stdcall _ShowDataWindow(HINSTANCE	hInstance)
{
	BYTE		Buffer[0x4000];
	
	HWND		hWnd;
	LVITEM		lvItem;												//ListView�б�������
	char		szBuffer1[256] = { 0 };
	char		szBuffer2[256] = { 0 };

	DWORD		dwReadSize;
	
	if (!ReadProcessMemory(hIsDebuggedProcess, hInstance, Buffer, sizeof(Buffer), &dwReadSize))
	{
		MessageBox(NULL, TEXT("�޷��鿴��"), NULL, MB_OK);
		return 0;
	}
	



	EnumChildWindows(hWinMain, _EnumChildProc, 0);				//���CPU�Ĵ��ھ��		
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








//��ʾ��ջ����

void _stdcall _ShowPointer()
{
	BYTE		Buffer[0x4000];
	CONTEXT		stContext;
	stContext.ContextFlags = CONTEXT_ALL;
	GetThreadContext(hIsDebuggedThread, &stContext);


	ReadProcessMemory(hIsDebuggedProcess, (LPVOID)(stContext.Esp - (stContext.Esp % 0x4000)), Buffer, sizeof(Buffer), NULL);
	
	HWND		hWnd;
	LVITEM		lvItem;												//ListView�б�������
	char		szBuffer1[256] = { 0 };
	char		szBuffer2[256] = { 0 };

	EnumChildWindows(hWinMain, _EnumChildProc, 0);				//���CPU�Ĵ��ھ��		
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
	//����esp��
	RECT	stRect;
	ListView_GetItemRect(GetDlgItem(hWnd, IDC_LIST3), 0, &stRect, LVIR_SELECTBOUNDS);
	ListView_Scroll(GetDlgItem(hWnd, IDC_LIST3), 0, (stRect.bottom - stRect.top)* ((stContext.Esp % 0x4000) / 4));
	ListView_SetHotItem(GetDlgItem(hWnd, IDC_LIST3), (stContext.Esp % 0x4000) / 4);
	
	dwItemEsp = stContext.Esp ;

}











//int3�ϵ㴦��
int _stdcall _DealBreakPoint(DEBUG_EVENT devent, CONTEXT& stContext, DWORD dwEntryPoint, PROCESS_INFORMATION pi)
{
	INT3* lpInt3 = lpHeadInt3;					//����ͷָ����в�ѯ
	BYTE	    lpBuffer[0x1000] = { 0 };			//��Ŵ������Ļ�����

	while (lpInt3 != NULL)		//�ж��Ƿ����������õĶϵ�
	{
		if (lpInt3->dwAddress == (DWORD)devent.u.Exception.ExceptionRecord.ExceptionAddress)
		{
			flag = 1;			//��ʾ���������õ�int3�ϵ�
			flag1 = 1;			//��ʾ������int3�ϵ�(�����ָ��ϵ�)
			break;
		}
		else if(stStep.dwStepAddress == (DWORD)devent.u.Exception.ExceptionRecord.ExceptionAddress)
		{
			flag = 3;			//��ʾ�������ǵ���ִ��
			flag1 = 0;			//��ʾ������int3�ϵ㣨�������ָ��ϵ㣬�൱��һ���Զϵ㣬������ɵ���������
			stStep.dwStepAddress = 0;
			break;
		}
		else
			lpInt3 = lpInt3->lpNextINT3;
	}
	if (flag == 0)				//��������������õĶϵ㣬�򽻸�������
		return -1;
	else						//������������õĶϵ�����д���
	{
		if (devent.u.Exception.dwFirstChance == 1)       //�ڵ�һ���쳣��ʱ����
		{


			stContext.ContextFlags = CONTEXT_ALL;
			GetThreadContext(pi.hThread, &stContext);


			
			stContext.Eip--;																	//��eip-1
			if(flag1 == 0)
				WriteProcessMemory(pi.hProcess, (LPVOID)stContext.Eip, &stStep.oldByte, 1, NULL);
			else
			{
				WriteProcessMemory(pi.hProcess, (LPVOID)stContext.Eip, &lpInt3->oldByte, 1, NULL);	//��ԭԭ�����ֽ�
				stContext.EFlags = stContext.EFlags | b00000000000000000000000100000000;			//����TF������־λ���Ա㻹ԭ�ϵ�
			}
			SetThreadContext(pi.hThread, &stContext);
			

			if ((DWORD)devent.u.Exception.ExceptionRecord.ExceptionAddress == dwEntryPoint)		//���������ڵ�
			{
				ReadProcessMemory(pi.hProcess, (LPCVOID)(dwEntryPoint - (dwEntryPoint % 0x1000)), lpBuffer, CODE_NUM, NULL);
				_Disassembled(lpBuffer, dwEntryPoint - (dwEntryPoint % 0x1000));										//���л����뷴��࣬����ʾ
				_ShowContext(&stContext);															//��ʾ���мĴ�����ֵ
				_ShowDataWindow((HINSTANCE)(dwEntryPoint - dwEntryPointRVA));						//��ʾ���ݴ���
				_ShowPointer();																		//��ʾ��ջ����
			}
		}

	}

	return 0;

}




//�����쳣����
int _stdcall _DealSingle(DEBUG_EVENT devent, PROCESS_INFORMATION pi)
{
	INT3* lpInt3 = lpHeadInt3;					
	CONTEXT		stContext;
	BYTE		bBuffer;
	DWORD		dwModel;

	if (flag1 == 3)							//�ոշ������ڴ�ϵ㣨Ϊ�˻ָ�����
	{
		flag1 = 0;
		VirtualProtectEx(hIsDebuggedProcess, (LPVOID)stMemory.dwAddress, stMemory.dwSize, dModel, &dwModel);
		if (Button == '5')
			flag = 2;			//ֱ�����в���
		else if (Button == '7')
			flag = 3;			//����ִ�в��������룩
		else if (Button == '8')
			flag = 3;			//����(����)
			
		
	}
	else if (flag1 == 2)					//�ոշ�����Ӳ���ϵ㣨˵���˵�����Ϊ�˻ָ��ոմ�����Ӳ���ϵ㣩
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
			flag = 2;			//ֱ�����в���
		else if (Button == '7')
			flag = 3;			//����ִ�в��������룩
		else if (Button == '8')
		{
			if (stStep.dwStepAddress == 0)
				flag = 3;			//����
			else
				flag = 2;			//ֱ��
		}
	}
	else if (flag1 == 1)			//�ոշ�����int3�ϵ㣨˵���˵�����Ϊ�˻ָ��ոմ�����int3�ϵ㣩
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
			flag = 2;			//ֱ�����в���
		else if (Button == '7')
			flag = 3;			//����ִ�в��������룩
		else if (Button == '8')
			flag = 3;			//����ִ�в�����������
	}
	else if (flag1 == 0)
		flag = 3;			//����ִ�в���

	return 0;

}










//Ӳ���ϵ㴦��
void _stdcall _DealHardware(DEBUG_EVENT devent, CONTEXT stContext)
{
	flag = 4;				//Ӳ��ִ�жϵ�
	flag1 = 2;				//�ո�ִ����Ӳ���ϵ�(Ϊ�˻ָ�Ӳ���ϵ�)
	if (stContext.Dr6 & 1)
	{
		if((stContext.Dr7 & 0x10000) && (stContext.Dr7 & 0x30000))		//Ӳ��д��ϵ�
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





//�ڴ�ϵ㴦��
void _stdcall _DealAccess(DEBUG_EVENT devent)
{
	if (devent.u.Exception.ExceptionRecord.ExceptionAddress == (LPVOID)stMemory.dwAddress)
	{

		flag = 5;		//�����ڴ�ϵ�
		flag1 = 3;		//�ոշ������ڴ�ϵ㣨Ϊ�˵����ָ��ڴ�ϵ㣩
	}

	else
	{
		flag = 0;		//�����Լ����쳣
	}
}





//�����˳����ڵ��Եĳ���
void _stdcall _DealClean()
{
	lpHeadInt3 = NULL;			//�ϵ��б����
	
	flag = 0;			//��ͬ��־���岻ͬ
	flag1 = 0;			//��־�Ǹո��Ƿ�����int3�ϵ㡣
	
	memset(dwAddressToEip, 0, sizeof(dwAddressToEip));
	dwEntryPoint = 0;		//��ڵ�ַRVA
	stMemory.dwAddress = 0;	//�ڴ�ϵ��ַ
	EnumChildWindows(hWinMain, _EnumChildProc, 0);				//���CPU�Ĵ��ھ��		
	ListView_DeleteAllItems(GetDlgItem(hChildWindow, IDC_LIST1));
	ListView_DeleteAllItems(GetDlgItem(hChildWindow, IDC_LIST2));
	ListView_DeleteAllItems(GetDlgItem(hChildWindow, IDC_LIST3));
	ListView_DeleteAllItems(GetDlgItem(hChildWindow, IDC_LIST5));
}








//���Լ���ContinueDebugEvent��------�ظ�������ϵͳ
int _stdcall _MyContinueDebugEvent(DEBUG_EVENT devent, PROCESS_INFORMATION pi)
{
	DWORD				dwModel;
	CONTEXT				stContext;			//�߳���Ϣ��
	stContext.ContextFlags = CONTEXT_ALL;


	
	if (flag == 6)													//Ӳ��д��ϵ�
	{
		GetThreadContext(pi.hThread, &stContext);

		_UpdateCpu(stContext);							//����CPU������ʾ
		SuspendThread(hDebugThread);					//�ȴ�UI�̲߳�������
		GetThreadContext(pi.hThread, &stContext);

		
		if (Button == '5')			//����
		{
			//SetThreadContext(pi.hThread, &stContext);
			ContinueDebugEvent(devent.dwProcessId, devent.dwThreadId, DBG_CONTINUE);
		}
		else if (Button == '7')		//����ִ��(��������)
		{
			stContext.EFlags = stContext.EFlags | b00000000000000000000000100000000;	//��TFλΪ1
			SetThreadContext(pi.hThread, &stContext);
			ContinueDebugEvent(devent.dwProcessId, devent.dwThreadId, DBG_CONTINUE);
		}

		else if (Button == '8')		//����ִ�У�����������
		{


			_ToNextAddress(stContext);
			if (stStep.dwStepAddress == 0)
			{
				stContext.EFlags = stContext.EFlags | b00000000000000000000000100000000;	//��TFλΪ1
				SetThreadContext(pi.hThread, &stContext);
			}
			else
				WriteProcessMemory(pi.hProcess, (LPVOID)stStep.dwStepAddress, &NewByte, 1, NULL);
			int n = GetLastError();
			ContinueDebugEvent(devent.dwProcessId, devent.dwThreadId, DBG_CONTINUE);

		}




	}

	else if (flag == 5)												//�ڴ�ϵ�
	{
		GetThreadContext(pi.hThread, &stContext);

		_UpdateCpu(stContext);							//����CPU������ʾ
		SuspendThread(hDebugThread);					//�ȴ�UI�̲߳�������

		GetThreadContext(pi.hThread, &stContext);
		stContext.EFlags = stContext.EFlags | b00000000000000000000000100000000;	//��TFλΪ1
		SetThreadContext(hIsDebuggedThread, &stContext);							//���õ���Ϊ�˻ָ��ڴ���ʶϵ�

		int n = VirtualProtectEx(hIsDebuggedProcess, (LPVOID)stMemory.dwAddress, stMemory.dwSize, stMemory.oldModel, &dwModel);				//ȡ���ڴ�ϵ�
		
		int n1 = GetLastError();
		ContinueDebugEvent(devent.dwProcessId, devent.dwThreadId, DBG_CONTINUE);	//����
		



	}
	 else if (flag == 4)										//Ӳ���ϵ�
	{
		

		GetThreadContext(pi.hThread, &stContext);
		
		_UpdateCpu(stContext);							//����CPU������ʾ
		SuspendThread(hDebugThread);					//�ȴ�UI�̲߳�������


		GetThreadContext(pi.hThread, &stContext);

		stContext.EFlags = stContext.EFlags | b00000000000000000000000100000000;	//��TFλΪ1

		stContext.Dr7 = stContext.Dr7 - stContext.Dr7 % 86;			//��ȡ�����Զϵ�
		SetThreadContext(hIsDebuggedThread, &stContext);


		if (Button == '5')			//����
			ContinueDebugEvent(devent.dwProcessId, devent.dwThreadId, DBG_CONTINUE);
		else if (Button == '7')		//����ִ��(��������)
			ContinueDebugEvent(devent.dwProcessId, devent.dwThreadId, DBG_CONTINUE);


		else if (Button == '8')		//����ִ�У�����������
		{


			_ToNextAddress(stContext);
			if (stStep.dwStepAddress != 0)
				WriteProcessMemory(pi.hProcess, (LPVOID)stStep.dwStepAddress, &NewByte, 1, NULL);
			ContinueDebugEvent(devent.dwProcessId, devent.dwThreadId, DBG_CONTINUE);

		}




	
	}

	else if (flag == 3)							//����ִ��
	{
		GetThreadContext(pi.hThread, &stContext);
		_UpdateCpu(stContext);							//����CPU������ʾ
		SuspendThread(hDebugThread);					//�ȴ�UI�̲߳�������

		GetThreadContext(pi.hThread, &stContext);
		if (Button == '5')			//����
			ContinueDebugEvent(devent.dwProcessId, devent.dwThreadId, DBG_CONTINUE);
		else if (Button == '7')		//����ִ��(��������)
		{
			stContext.EFlags = stContext.EFlags | b00000000000000000000000100000000;	//��TFλΪ1
			SetThreadContext(pi.hThread, &stContext);
			ContinueDebugEvent(devent.dwProcessId, devent.dwThreadId, DBG_CONTINUE);
		}

		else if (Button == '8')		//����ִ�У�����������
		{


			_ToNextAddress(stContext);
			if (stStep.dwStepAddress == 0)
			{
				stContext.EFlags = stContext.EFlags | b00000000000000000000000100000000;	//��TFλΪ1
				SetThreadContext(pi.hThread, &stContext);
			}
			else
				WriteProcessMemory(pi.hProcess, (LPVOID)stStep.dwStepAddress, &NewByte, 1, NULL);
			int n = GetLastError();
			ContinueDebugEvent(devent.dwProcessId, devent.dwThreadId, DBG_CONTINUE);

		}



	}
	else if (flag == 2)					//��ִ����int3�ϵ㴦ָ����õ���ִ�лָ�int3�ϵ㲢ֱ������
	{
		ContinueDebugEvent(devent.dwProcessId, devent.dwThreadId, DBG_CONTINUE);
	}
	else if (flag == 1)					//����ִ�е��˶ϵ㴦
	{
		GetThreadContext(pi.hThread, &stContext);
		_UpdateCpu(stContext);							//����CPU������ʾ
		SuspendThread(hDebugThread);				//�ȴ�UI�̲߳�������

		GetThreadContext(pi.hThread, &stContext);
		if (Button == '5')			//����
			ContinueDebugEvent(devent.dwProcessId, devent.dwThreadId, DBG_CONTINUE);
		else if (Button == '7')		//����ִ�У��������룩
		{
			stContext.EFlags = stContext.EFlags | b00000000000000000000000100000000;	//��TFλΪ1
			SetThreadContext(pi.hThread, &stContext);
			ContinueDebugEvent(devent.dwProcessId, devent.dwThreadId, DBG_CONTINUE);
		}

		else if (Button == '8')		//����ִ�У�����������
		{


			_ToNextAddress(stContext);
			if (stStep.dwStepAddress == 0)
			{
				stContext.EFlags = stContext.EFlags | b00000000000000000000000100000000;	//��TFλΪ1
				SetThreadContext(pi.hThread, &stContext);
			}
			else
				WriteProcessMemory(pi.hProcess, (LPVOID)stStep.dwStepAddress, &NewByte, 1, NULL);


			ContinueDebugEvent(devent.dwProcessId, devent.dwThreadId, DBG_CONTINUE);

		}
	


	}
	else if (flag == 0)					//���쳣�����¼��Լ�����������"�쳣"
		ContinueDebugEvent(devent.dwProcessId, devent.dwThreadId, DBG_EXCEPTION_NOT_HANDLED);
	

	flag = 0;
		

	return 0;
}



















//����CPU����
void _stdcall _UpdateCpu(CONTEXT stContext)
{
	HWND	hWnd;
	DWORD	dwData;
	RECT	stRect;
	char	szBuffer1[256];
	BYTE	lpBuffer[0x1000];


	//���CPU�Ĵ��ھ��	
	EnumChildWindows(hWinMain, _EnumChildProc, 0);					
	hWnd = hChildWindow;
	
	
	ListView_GetItemText(GetDlgItem(hWnd, IDC_LIST1), 0, 0, szBuffer1,sizeof(szBuffer1));
	ListView_GetItemRect(GetDlgItem(hWnd, IDC_LIST1), 0, &stRect, LVIR_SELECTBOUNDS);

	dwData = strtol(szBuffer1, NULL, 16);

	if ((stContext.Eip - dwData) > 0x1000 || dwData > stContext.Eip)
	{
		ReadProcessMemory(hIsDebuggedProcess, (LPCVOID)(stContext.Eip - (stContext.Eip % 0x1000)), lpBuffer, CODE_NUM, NULL);
		_Disassembled(lpBuffer, stContext.Eip - (stContext.Eip % 0x1000));					//����ಢ��ʾ����ര��
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
	//�������ݴ���
	ListView_GetItemText(GetDlgItem(hWnd, IDC_LIST2), 0, 0, szBuffer1, sizeof(szBuffer1))
	dwData = strtol(szBuffer1, NULL, 16);
	if(dwData != 0)
		_ShowDataWindow((HINSTANCE)dwData);



	//���¼Ĵ�������
	_ShowContext(&stContext);
	

	//���¶�ջ����
	ListView_GetItemText(GetDlgItem(hWnd, IDC_LIST3), 0, 0, szBuffer1, sizeof(szBuffer1));
	dwData = strtol(szBuffer1, NULL, 16);
	ListView_SetHotItem(GetDlgItem(hWnd, IDC_LIST3), (stContext.Esp - dwData) / 4);							//�����ȵ���


	
	ListView_Scroll(GetDlgItem(hWnd, IDC_LIST3), 0,(0 - (stRect.bottom - stRect.top) * 0x1000));					//��esp�ƶ�����Ұ��
	ListView_Scroll(GetDlgItem(hWnd, IDC_LIST3), 0, (stRect.bottom - stRect.top) * ListView_GetHotItem(GetDlgItem(hWnd, IDC_LIST3)));
	
	
	
	dwItemEsp = stContext.Esp;
}



//�������һ��ָ�������ɵ�������
DWORD _stdcall _ToNextAddress(CONTEXT stContext)
{
	
	csh			csHandle;				//��������Caps����API�þ��
	cs_insn*	lpInsn;					//�������淴���ָ�����Ϣ
	size_t		count;					//���滺�����з����ָ����Ϣ�ô�С
	
	BYTE		lpBuffer[0x20];
	ReadProcessMemory(hIsDebuggedProcess, (LPCVOID)stContext.Eip, lpBuffer, sizeof(lpBuffer), NULL);
	if (cs_open(CS_ARCH_X86, CS_MODE_32, &csHandle))
	{
		MessageBox(NULL, TEXT("�����ʧ�ܣ�"), TEXT("����"), MB_OK);
		return -1;
	}

	count = cs_disasm(csHandle, lpBuffer, sizeof(lpBuffer), stContext.Eip, 2, &lpInsn);		//�Ի������еĻ�������з����
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






//����Int3�ϵ�
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
		MessageBox(NULL, TEXT("�Ѿ����ù�Ӳ��ִ�жϵ㣡"), NULL, MB_OK);
		return 0;
	}




	while (lpInt3 != NULL)					//�ж��Ƿ��Ѿ����öϵ�
	{
		if (lpInt3->dwAddress == dwAddress)				//�����������ȡ���ϵ�
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


	//���û�����öϵ���������һ���ϵ�
	lpInt3Front->lpNextINT3 = new INT3;
	lpInt3Front->lpNextINT3->dwAddress = dwAddress;
	lpInt3Front->lpNextINT3->lpNextINT3 = NULL;
	
	ReadProcessMemory(hIsDebuggedProcess, (LPVOID)dwAddress, &(lpInt3Front->lpNextINT3->oldByte), 1, NULL);
	WriteProcessMemory(hIsDebuggedProcess, (LPVOID)dwAddress, &NewByte, 1, NULL);

	ListView_SetItemText(GetDlgItem(hWnd, IDC_LIST1), dwItem, 3, szBuffer1);

	return 0;
}







//����Ӳ��ִ�жϵ�
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
			MessageBox(NULL, TEXT("�Ѿ����ù�INT3�ϵ㣡"), NULL, MB_OK);
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
		MessageBox(NULL, TEXT("Ӳ���ϵ�ﵽ���ޣ�"), NULL, MB_OK);
		return 0;
	}

	SetThreadContext(hIsDebuggedThread, &stContext);
	GetThreadContext(hIsDebuggedThread, &stContext);
	return 1;
}








//����Ӳ��д�ϵ�
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
		stContext.Dr7 = stContext.Dr7 + 1;			//���öϵ�	
		stContext.Dr0 = dwData;						//���õ�ַ
		stContext.Dr7 = stContext.Dr7 + 0x10000;	//����д�ϵ�

		
		if (dwSize == 1)							//���ó���
			stContext.Dr7 = stContext.Dr7 + 0x80000;
		else if (dwSize == 2)
			stContext.Dr7 = stContext.Dr7 + 0x40000;
		else if (dwSize == 3)
			stContext.Dr7 = stContext.Dr7 + 0xC0000;

	}
	else if (stContext.Dr1 == 0)
	{
		stContext.Dr7 = stContext.Dr7 + 4;			//���öϵ�	
		stContext.Dr1 = dwData;						//���õ�ַ
		stContext.Dr7 = stContext.Dr7 + 0x100000;	//����д�ϵ�
		
		if (dwSize == 1)							//���ó���
			stContext.Dr7 = stContext.Dr7 + 0x800000;
		else if (dwSize == 2)
			stContext.Dr7 = stContext.Dr7 + 0x400000;
		else if (dwSize == 3)
			stContext.Dr7 = stContext.Dr7 + 0xC00000;

	}
	else if (stContext.Dr2 == 0)
	{
		stContext.Dr7 = stContext.Dr7 + 16;			//���öϵ�	
		stContext.Dr2 = dwData;						//���õ�ַ
		stContext.Dr7 = stContext.Dr7 + 0x1000000;	//����д�ϵ�

		if (dwSize == 1)							//���ó���
			stContext.Dr7 = stContext.Dr7 + 0x8000000;
		else if (dwSize == 2)
			stContext.Dr7 = stContext.Dr7 + 0x4000000;
		else if (dwSize == 3)
			stContext.Dr7 = stContext.Dr7 + 0xC000000;
	}
	else if (stContext.Dr3 == 0)
	{
		stContext.Dr7 = stContext.Dr7 + 64;			//���öϵ�	
		stContext.Dr3 = dwData;						//���õ�ַ
		stContext.Dr7 = stContext.Dr7 + 0x10000000;	//����д�ϵ�

		if (dwSize == 1)							//���ó���
			stContext.Dr7 = stContext.Dr7 + 0x80000000;
		else if (dwSize == 2)
			stContext.Dr7 = stContext.Dr7 + 0x40000000;
		else if (dwSize == 3)
			stContext.Dr7 = stContext.Dr7 + 0xC0000000;
	}
	else
	{
		MessageBox(NULL, TEXT("Ӳ���ϵ�ﵽ���ޣ�"), NULL, MB_OK);
		return 0;
	}

	
	SetThreadContext(hIsDebuggedThread, &stContext);
	GetThreadContext(hIsDebuggedThread, &stContext);
	return 1;
}






//����Ӳ�����ϵ�
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
		stContext.Dr7 = stContext.Dr7 + 1;			//���öϵ�	
		stContext.Dr0 = dwData;						//���õ�ַ
		stContext.Dr7 = stContext.Dr7 + 0x30000;	//���ö��ϵ�

		if (dwSize == 1)							//���ó���
			stContext.Dr7 = stContext.Dr7 + 0x80000;
		else if (dwSize == 2)
			stContext.Dr7 = stContext.Dr7 + 0x40000;
		else if (dwSize == 3)
			stContext.Dr7 = stContext.Dr7 + 0xC0000;

	}
	else if (stContext.Dr1 == 0)
	{
		stContext.Dr7 = stContext.Dr7 + 4;			//���öϵ�	
		stContext.Dr1 = dwData;						//���õ�ַ
		stContext.Dr7 = stContext.Dr7 + 0x300000;	//���ö��ϵ�

		if (dwSize == 1)							//���ó���
			stContext.Dr7 = stContext.Dr7 + 0x800000;
		else if (dwSize == 2)
			stContext.Dr7 = stContext.Dr7 + 0x400000;
		else if (dwSize == 3)
			stContext.Dr7 = stContext.Dr7 + 0xC00000;

	}
	else if (stContext.Dr2 == 0)
	{
		stContext.Dr7 = stContext.Dr7 + 16;			//���öϵ�	
		stContext.Dr2 = dwData;						//���õ�ַ
		stContext.Dr7 = stContext.Dr7 + 0x3000000;	//���ö��ϵ�

		if (dwSize == 1)							//���ó���
			stContext.Dr7 = stContext.Dr7 + 0x8000000;
		else if (dwSize == 2)
			stContext.Dr7 = stContext.Dr7 + 0x4000000;
		else if (dwSize == 3)
			stContext.Dr7 = stContext.Dr7 + 0xC000000;
	}
	else if (stContext.Dr3 == 0)
	{
		stContext.Dr7 = stContext.Dr7 + 64;			//���öϵ�	
		stContext.Dr3 = dwData;						//���õ�ַ
		stContext.Dr7 = stContext.Dr7 + 0x30000000;	//���ö��ϵ�

		if (dwSize == 1)							//���ó���
			stContext.Dr7 = stContext.Dr7 + 0x80000000;
		else if (dwSize == 2)
			stContext.Dr7 = stContext.Dr7 + 0x40000000;
		else if (dwSize == 3)
			stContext.Dr7 = stContext.Dr7 + 0xC0000000;
	}
	else
	{
		MessageBox(NULL, TEXT("Ӳ���ϵ�ﵽ���ޣ�"), NULL, MB_OK);
		return 0;
	}


	SetThreadContext(hIsDebuggedThread, &stContext);
	GetThreadContext(hIsDebuggedThread, &stContext);
	return 1;
}





//�����ڴ���ʶϵ�
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
		MessageBox(NULL, TEXT("�ڴ���ʶϵ�����ʧ�ܣ�"), NULL, MB_OK);
		return 0;
	}
	dModel = PAGE_NOACCESS;
	stMemory.dwAddress = dwData;
	stMemory.dwSize = dwSize;

	MessageBox(NULL, TEXT("���óɹ�"), NULL, MB_OK);
	return 0;
}



//�����ڴ�д��ϵ�
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
		MessageBox(NULL, TEXT("�ڴ�д��ϵ�����ʧ�ܣ�"), NULL, MB_OK);
		return 0;
	}
	dModel = PAGE_EXECUTE_READ;
	stMemory.dwAddress = dwData;
	stMemory.dwSize = dwSize;

	MessageBox(NULL, TEXT("���óɹ�"), NULL, MB_OK);
	return 0;
}



//ɾ���ڴ�ϵ�
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
