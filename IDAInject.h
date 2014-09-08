#pragma once

#define USE_STANDARD_FILE_FUNCTIONS

#include <Windows.h>
#include <ida.hpp>
#include <idp.hpp>
#include <loader.hpp>

#include <iostream>
#include <vector>
#include <map>

// boost note: if compiling with __stdcall as default, linking fails!
#include <boost/filesystem/operations.hpp>

// state for dlls
enum InjectionState {
	InjectionError,
	InjectionSuccess,
	InjectionNone
};

enum InjectionType {
	OnProcessStart = 1,
	OnDbgAttach = 2,
	OnProcAndAttach = 3
};

struct InjectionDLL 
{
	std::string fileName;
	InjectionState state;
	InjectionType injectionType;
	// more info? image base, ...
};

typedef std::map<std::string, InjectionDLL>::const_iterator IDMCI;
typedef std::map<std::string, InjectionDLL>::iterator IDMI;
typedef std::vector<InjectionDLL>::iterator IDVI;
typedef std::vector<InjectionDLL>::const_iterator IDVCI;

LRESULT CALLBACK dlgProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);
int idaapi callback(void* user_data, int notification_code, va_list va);
void idaapi btnCallback(TView* fields[], int code);
std::string getConfigFile();
const std::vector<std::string>& getSections();
std::vector<InjectionDLL>& getProcStartDlls();
std::vector<InjectionDLL>& getDbgAttachDlls();
void loadConfig();