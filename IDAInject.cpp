/************************************************
*
* Author: Jan Newger
* Date: 4.16.2008
*
************************************************/

#include <process.h>
#include "IDAInject.h"
#include <dbg.hpp>
#include "IDAInjectGUI.h"

#include <NInjectLib/IATModifier.h>
#include <NInjectLib/InjectLib.h>
#include <SimpleConfig/SimpleConfig.h>

using namespace boost::filesystem;
using namespace std;

// constant strings used for config file
static const string ConfigFile = "IDAInject.cfg";
static const string SectionStart = "[ProcessStart]";
static const string SectionAttach = "[DbgAttach]";
static const char* SecStrings[2] = {"[ProcessStart]", "[DbgAttach]"};
static const vector<string> Sections = vector<string>(SecStrings, &SecStrings[2]);

// injection completed flag
bool gInjectionDone;

// global list of all DLLs
vector<InjectionDLL> procStartDlls;
vector<InjectionDLL> dbgAttachDlls;

/*********************************************************************
* Function: init
*
* init is a plugin_t function. It is executed when the plugin is
* initially loaded by IDA.
* Three return codes are possible:
*    PLUGIN_SKIP - Plugin is unloaded and not made available
*    PLUGIN_KEEP - Plugin is kept in memory
*    PLUGIN_OK   - Plugin will be loaded upon 1st use
*
* Check are added here to ensure the plug-in is compatible with
* the current disassembly.
*********************************************************************/
int __stdcall init(void)
{
	if (!hook_to_notification_point(HT_DBG, callback, NULL))
	{
		msg("Could not hook to notification point\n");
		return PLUGIN_SKIP;
	}
	loadConfig();
	return PLUGIN_KEEP;
}

/*********************************************************************
* Function: term
*
* term is a plugin_t function. It is executed when the plugin is
* unloading. Typically cleanup code is executed here.
*********************************************************************/
void __stdcall term(void)
{
	unhook_from_notification_point(HT_DBG, callback, NULL);
}

/*********************************************************************
* Function: run
*
* run is a plugin_t function. It is executed when the plugin is run.
*
* The argument 'arg' can be passed by adding an entry in
* plugins.cfg or passed manually via IDC:
*
*   success RunPlugin(string name, long arg);
*********************************************************************/
void __stdcall run(int arg)
{

	//  Uncomment the following code to allow plugin unloading.
	//  This allows the editing/building of the plugin without
	//  restarting IDA.
	//
	//  1. to unload the plugin execute the following IDC statement:
	//        RunPlugin("IDAInject", 666);
	//  2. Make changes to source code and rebuild within Visual Studio
	//  3. Copy plugin to IDA plugin dir
	//     (may be automatic if option was selected within wizard)
	//  4. Run plugin via the menu, hotkey, or IDC statement
	//
	if (arg == 666)
	{
		PLUGIN.flags |= PLUGIN_UNL;
		msg("Unloading IDAStealth plugin...\n");
		//unhook_from_notification_point(HT_DBG, callback, NULL);
	}
	else
	{
		loadConfig();
		showGUI();
	}
}

string getConfigFile()
{
	path retVal;
	char idaExe[MAX_PATH];
	if (GetModuleFileName(NULL, idaExe, MAX_PATH))
	{
		path p(idaExe);
		p.remove_leaf();
		p /= "cfg";
		p /= ConfigFile;
		retVal = p;
	}
	return retVal.native_file_string();
}

// perform actual injection for attach event in background thread,
// so we can block to read back an error code
unsigned int __stdcall backgroundThread(void* pArguments)
{
	DWORD pID = (DWORD)pArguments;

	// read dlls to be injected
	Process process(pID);
	for (IDVI it=dbgAttachDlls.begin(); it!=dbgAttachDlls.end(); ++it)
	{
		if (it->injectionType == OnDbgAttach)
		{
			InjectLibrary injector(it->fileName, process);
			it->state = (injector.injectLib() ? InjectionSuccess : InjectionError);
		}
	}
	
	// inform timer callback about return code
	gInjectionDone = true;
	_endthreadex(0);
	return 0;
}

// timer callback; runs in the context of the main thread, so wen ca safely use any GUI functions
void CALLBACK timerCallbackProc(HWND hwnd, UINT message, UINT idTimer, DWORD dwTime)
{
	if (gInjectionDone)
	{
		KillTimer(hwnd, idTimer);
		// list status for all dlls
		for (IDVCI cit=dbgAttachDlls.begin(); cit!=dbgAttachDlls.end(); ++cit)
		{
			if (cit->injectionType == OnProcessStart)
			{
				if (cit->state == InjectionSuccess)
					msg("IDAInject: Successfully injected %s into process\n", cit->fileName.c_str());
				else msg("IDAInject: FAILED to inject %s into process\n", cit->fileName.c_str());
			}
		}
		gInjectionDone = false;
	}
}

// translate section string to enum type
InjectionType getInjectionType(const string& sectionName)
{
	if (sectionName == SectionAttach) return OnDbgAttach;
	else return OnProcessStart;
}

vector<InjectionDLL>& getProcStartDlls()
{
	return procStartDlls;
}

vector<InjectionDLL>& getDbgAttachDlls()
{
	return dbgAttachDlls;
}

// parse config file and populate respective list
void loadConfig()
{	
	procStartDlls.clear();
	dbgAttachDlls.clear();

	SimpleConfig config(getConfigFile(), Sections);	
	const vector<string>& dbgSection = config.getSection(SectionAttach);
	for (SCVCI cit=dbgSection.begin(); cit!=dbgSection.end(); ++cit)
	{
		InjectionDLL dll;
		dll.fileName = *cit;
		dll.state = InjectionNone;
		dll.injectionType = OnDbgAttach;
		dbgAttachDlls.push_back(dll);
	}

	const vector<string>& startSection = config.getSection(SectionStart);
	for (SCVCI cit=startSection.begin(); cit!=startSection.end(); ++cit)
	{
		InjectionDLL dll;
		dll.fileName = *cit;
		dll.state = InjectionNone;
		dll.injectionType = OnProcessStart;
		procStartDlls.push_back(dll);
	}
}

const std::vector<string>& getSections()
{
	return Sections;
}

void setDllsState(InjectionState newState, vector<InjectionDLL>& dlls)
{
	for (IDVI it=dlls.begin(); it!=dlls.end(); ++it)
		it->state = newState;
}

int idaapi callback(void* user_data, int notification_code, va_list va)
{
	switch (notification_code)
	{
	case dbg_process_attach:
		{
			// create a new thread to handle library injection, because the code will block 
			// when it reads back the error code
			const debug_event_t* dbgEvent = va_arg(va, const debug_event_t*);
			HANDLE hThread = (HANDLE)_beginthreadex(NULL, 0, &backgroundThread, (void*)dbgEvent->pid, 0, NULL);
			CloseHandle(hThread);
			// retrieve free timer ID and start timer to carry out GUI functions asynchronously
			UINT_PTR id = SetTimer(0, 0, USER_TIMER_MINIMUM, NULL);
			SetTimer((HWND)callui(ui_get_hwnd).vptr, id, USER_TIMER_MINIMUM, (TIMERPROC)timerCallbackProc);
		}
		break;

	case dbg_process_start:
		{
			if (procStartDlls.empty()) break;
			const debug_event_t* evt = va_arg(va, const debug_event_t*);
			IDVI it = procStartDlls.begin();
			try
			{	
				Process process(evt->pid);
				IATModifier iatMod(process);
				module_info_t mInfo;
				bool found = false;
				// search all modules to find .exe header
				for (bool ok=get_first_module(&mInfo); ok && !found; ok=get_next_module(&mInfo))
					if (iatMod.setIBA((char*)mInfo.base)) found = true;
				// then add import descriptors for all the dlls to be injected
				if (found)
				{
					vector<string> dlls;
					// generate list of dlls
					for (; it!=procStartDlls.end(); ++it) dlls.push_back(it->fileName);
					iatMod.writeIAT(dlls);

					// print success messages and set all dll states
					for (vector<string>::const_iterator cit=dlls.begin(); cit!=dlls.end(); ++cit)
						msg("IDAInject: Successfully injected %s into process\n", cit->c_str());
					setDllsState(InjectionSuccess, procStartDlls);
				}
				else
				{
					warning("IDAInject: Failed to find applications import descriptor table!");
					setDllsState(InjectionError, procStartDlls);
				}
			}
			catch (const WriteIIDException& e)
			{
				msg("IDAInject: %s\n", e.what());
				setDllsState(InjectionError, procStartDlls);
			}
			catch (const IATModifierException& e)
			{
				msg("IDAInject: %s\n", e.what());
				setDllsState(InjectionError, procStartDlls);
			}
			catch (const MemoryAccessException& e)
			{
				msg("IDAInject: %s", e.what());
				setDllsState(InjectionError, procStartDlls);
			}
		}
		break;
		
	case dbg_process_exit:
		// reset error codes of currently injected dlls
		setDllsState(InjectionNone, procStartDlls);
		setDllsState(InjectionNone, dbgAttachDlls);
		break;
	}

	return 0;
}

//////////////////////////////////////////////////////////////////////////

char comment[] 	= "Short one line description about the plugin";
char help[] 	= "My plugin:\n"
"\n"
"Multi-line\n"
"description\n";

/* Plugin name listed in (Edit | Plugins) */
char wanted_name[] 	= "IDAInject";

/* plug-in hotkey */
char wanted_hotkey[] 	= "";

/* defines the plugins interface to IDA */
plugin_t PLUGIN =
{
	IDP_INTERFACE_VERSION,
	0,              // plugin flags
	init,           // initialize
	term,           // terminate. this pointer may be NULL.
	run,            // invoke plugin
	comment,        // comment about the plugin
	help,           // multiline help about the plugin
	wanted_name,    // the preferred short name of the plugin
	wanted_hotkey   // the preferred hotkey to run the plugin
};
