#include "IDAInjectGUI.h"
#include "IDAInject.h"
#include "resource.h"
#include <SimpleConfig/SimpleConfig.h>

using namespace std;

// column headers for dll config window
const char* header[] =
{
	"Filename",
	"Injection Type"
};

// selected item in chooser2 window
ulong selectedIndex;

// colum widths
const int widths[] = { 50, 14 };

vector<InjectionDLL>& getRealEntry(ulong index, ulong& realIndex);
vector<InjectionDLL>& getRealEntry(ulong index);

// return number of lines
ulong idaapi sizer(void* obj)
{
	return getProcStartDlls().size() + getDbgAttachDlls().size();
}

// function that generates the list line
void idaapi genEntry(void* obj, ulong n, char* const* arrptr)
{
	// generate column headers
	if (n == 0)
	{
		for (int i=0; i<qnumber(header); ++i) qstrncpy(arrptr[i], header[i], MAXSTR);
		return;
	}

	--n;
	ulong realIndex;
	vector<InjectionDLL>& vec = getRealEntry(n, realIndex);
	qsnprintf(arrptr[0], MAXSTR, "%s", vec[realIndex].fileName.c_str());
	qsnprintf(arrptr[1], MAXSTR, "%s", vec[realIndex].injectionType == OnDbgAttach ? "On debugger attach" : "On process start");
}

// rewrite complete config
void rewriteConfig()
{
	vector<string> procStartSection;
	vector<string> dbgAttachSection;
	const vector<InjectionDLL>& procStartDlls = getProcStartDlls();
	const vector<InjectionDLL>& dbgAttachDlls = getDbgAttachDlls();
	for (IDVCI cit=procStartDlls.begin(); cit!=procStartDlls.end(); ++cit) procStartSection.push_back(cit->fileName);
	for (IDVCI cit=dbgAttachDlls.begin(); cit!=dbgAttachDlls.end(); ++cit) dbgAttachSection.push_back(cit->fileName);
	
	const vector<string>& sections = getSections();
	SimpleConfig config(getConfigFile(), sections);
	config.writeSection(sections[0], procStartSection);
	config.writeSection(sections[1], dbgAttachSection);
}

vector<InjectionDLL>& getRealEntry(ulong index)
{
	vector<InjectionDLL>& procStartDlls = getProcStartDlls();
	if (index < procStartDlls.size()) return procStartDlls;
	else return getDbgAttachDlls();
}

// translate given (zero-based) index reference to actual vector and calculate corresponding index
vector<InjectionDLL>& getRealEntry(ulong index, ulong& realIndex)
{
	vector<InjectionDLL>& procStartDlls = getProcStartDlls();
	if (index < procStartDlls.size())
	{
		realIndex = index;
		return procStartDlls;
	}
	else
	{
		realIndex = index - procStartDlls.size();
		return getDbgAttachDlls();
	}
}

bool isNewOperation;
LRESULT CALLBACK dlgProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch(message)
	{
	case WM_INITDIALOG:
		{
			ulong realIndex;
			vector<InjectionDLL>& vec = getRealEntry(selectedIndex, realIndex);
			SetDlgItemText(hDlg, IDC_FILENAME, vec[realIndex].fileName.c_str());
			InjectionType injType = vec[realIndex].injectionType;
			CheckDlgButton(hDlg, IDC_DBGATTACH, injType & OnDbgAttach ? BST_CHECKED : BST_UNCHECKED);
			CheckDlgButton(hDlg, IDC_PROCSTART, injType & OnProcessStart ? BST_CHECKED : BST_UNCHECKED);
			SetDlgItemInt(hDlg, IDC_LOADORDER, realIndex, FALSE);
			return true;
		}
		break;

	case WM_COMMAND:
		if (LOWORD(wParam) == IDOK)
		{
			char fileName[MAX_PATH];
			DWORD nChars = GetDlgItemText(hDlg, IDC_FILENAME, fileName, MAX_PATH);
			if (nChars == 0)
			{
				MessageBox(hDlg, "Please enter a valid filename!", "Error", MB_ICONERROR);
				break;
			}
			
			// get real index for this entry in its associated dll list
			ulong realIndex;
			vector<InjectionDLL>& vec = getRealEntry(selectedIndex, realIndex);
			
			// retrieve load order index
			BOOL numberOk;
			ulong newLoadOrder = GetDlgItemInt(hDlg, IDC_LOADORDER, &numberOk, FALSE);
			if (numberOk == FALSE || newLoadOrder >= vec.size())
			{
				MessageBox(hDlg, "Please enter a valid index for load order!", "Error", MB_ICONERROR);
				break;
			}
			
			InjectionType injType = (IsDlgButtonChecked(hDlg, IDC_DBGATTACH) == BST_CHECKED ? OnDbgAttach : OnProcessStart);
			InjectionDLL options;
			options.fileName = fileName;
			options.injectionType = injType;

			vector<InjectionDLL>& editVec = (injType == OnDbgAttach ? getDbgAttachDlls() : getProcStartDlls());
			// move the item to the other vector
			if (vec[realIndex].injectionType != injType)
			{
				vector<InjectionDLL>& oldVec = getRealEntry(selectedIndex);
				// check against new list
				if (newLoadOrder > editVec.size())
				{
					MessageBox(hDlg, "Please enter a valid index for load order!", "Error", MB_ICONERROR);
					break;
				}
				// delete old entry and push new dummy entry
				oldVec.erase(oldVec.begin() + realIndex);
				editVec.insert(editVec.begin() + newLoadOrder, options);
			}
			else
			{
				editVec[realIndex] = options;
				if (newLoadOrder != realIndex)
				{
					// need to move entry in list
					InjectionDLL tmp = editVec[newLoadOrder];
					editVec[newLoadOrder] = vec[realIndex];
					editVec[realIndex] = tmp;
				}
			}

			// re-create config file with new values
			rewriteConfig();
			EndDialog(hDlg, LOWORD(wParam));
		}
		else if (LOWORD(wParam) == IDCANCEL) EndDialog(hDlg, LOWORD(wParam));
		else if (LOWORD(wParam) == IDC_SELECT)
		{
			char* file = askfile_c(0, NULL, "Locate injection DLL!");
			if (file) SetDlgItemText(hDlg, IDC_FILENAME, file);
		}
		break;

	case WM_CLOSE:
		EndDialog(hDlg, LOWORD(wParam));
		break;

	case WM_DESTROY:	
		break;
	}

	return false;
}

void editEntry(ulong n)
{
	HWND hIDAWnd = (HWND)callui(ui_get_hwnd).vptr;
	selectedIndex = n;
	DialogBox(GetModuleHandle("IDAInject.plw"), (LPCTSTR)IDD_DIALOG, hIDAWnd, (DLGPROC)dlgProc);
}

// edit callback for dll config window
void idaapi editEntryCB(void* obj, ulong n)
{
	editEntry(--n);
}

 ulong idaapi delEntryCB(void* obj, ulong n)
 {
	 ulong realIndex;
	 vector<InjectionDLL>& vec = getRealEntry(--n, realIndex);
	 vec.erase(vec.begin() + realIndex);
	 rewriteConfig();
	 return 0;
 }

 // create new dll entry with default values on procStart vector and show dialog to edit entry
 void idaapi newEntryCB(void* obj)
 {
	isNewOperation = true;	
	InjectionDLL dll;
	dll.injectionType = OnProcessStart;
	dll.state = InjectionNone;
	vector<InjectionDLL>& dlls = getProcStartDlls();
	dlls.push_back(dll);
	editEntry(dlls.size() - 1);
 }

void showGUI()
{
	choose2(false,								// non-modal window
			-1, -1, -1, -1,						// position is determined by Windows
			NULL,								// pass the created array
			qnumber(header),					// number of columns
			widths,								// widths of columns
			sizer,								// function that returns number of lines
			genEntry,							// function that generates a line
			"IDA Inject",						// window title
			-1,									// use the default icon for the window
			0,									// position the cursor on the first line
			delEntryCB,							// "kill" callback
			newEntryCB,							// "new" callback
			NULL,								// "update" callback
			editEntryCB,							// "edit" callback
			NULL,								// function to call when the user pressed Enter
			NULL,								// function to call when the window is closed
			NULL,								// use default popup menu items
			NULL);								// use the same icon for all lines
}