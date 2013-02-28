#include "PluginDefinition.h"
#include "menuCmdID.h"
#include "trex.h"
#include "Utils.h"

//
// The plugin data that Notepad++ needs
//
FuncItem funcItem[nbFunc];

//
// The data of Notepad++ that you can use in your plugin commands
//
NppData nppData;

bool do_active_commenting;
bool do_active_wrapping;
bool fingertext_found;
bool fingertext_enabled;

std::string doc_start;
std::string doc_line;
std::string doc_end;

HWND curScintilla;
#define SCI_UNUSED 0


LRESULT SendScintilla(UINT Msg, WPARAM wParam, LPARAM lParam)
{
	return ::SendMessage(curScintilla, Msg, wParam, lParam); 
}

bool updateScintilla()
{
	int which = -1;
	::SendMessage(nppData._nppHandle, NPPM_GETCURRENTSCINTILLA, 0, (LPARAM)&which);
	if(which == -1) return false;
	curScintilla = (which == 0) ? nppData._scintillaMainHandle : nppData._scintillaSecondHandle;
	return true;
}

//
// Initialize your plugin data here
// It will be called while plugin loading
void pluginInit(HANDLE hModule)
{
	InitializeParsers();

	do_active_commenting = true;
	do_active_wrapping = true;

	doc_start = "/**";
	//doc_start = "/**************************************************************************************//**";
	doc_line  = " *  ";
	//doc_end   = " ******************************************************************************************/";
	doc_end   = " */";
}

//
// Here you can do the clean up, save the parameters (if any) for the next session
//
void pluginCleanUp()
{
	CleanUpParsers();
}

//
// Initialization of your plugin commands
// You should fill your plugins commands here
void commandMenuInit()
{
	ShortcutKey *sk = new ShortcutKey();
	sk->_isAlt = TRUE;
	sk->_isCtrl = TRUE;
	sk->_isShift = TRUE;
	sk->_key = 'D';

	setCommand(0, TEXT("DoxyIt - Function"), doxyItFunction, sk, false);
	setCommand(1, TEXT("DoxyIt - File"), doxyItFile, NULL, false);
	setCommand(2, TEXT("Active commenting"), activeCommenting, NULL, do_active_commenting);
	//setCommand(3, TEXT("Active word wrapping"), activeWrapping, NULL, do_active_wrapping);
}

//
// Here you can do the clean up (especially for the shortcut)
//
void commandMenuCleanUp()
{
	// Don't forget to deallocate your shortcut here
	delete funcItem[0]._pShKey;
}

//
// This function help you to initialize your plugin commands
//
bool setCommand(size_t index, TCHAR *cmdName, PFUNCPLUGINCMD pFunc, ShortcutKey *sk, bool check0nInit)
{
	if (index >= nbFunc || !pFunc) return false;

	lstrcpy(funcItem[index]._itemName, cmdName);
	funcItem[index]._pFunc = pFunc;
	funcItem[index]._init2Check = check0nInit;
	funcItem[index]._pShKey = sk;

	return true;
}



bool checkFingerText()
{
	if(fingertext_found)
	{
		CommunicationInfo ci;
		ci.internalMsg = FINGERTEXT_ISENABLED;
		ci.srcModuleName = NPP_PLUGIN_NAME;
		ci.info = NULL;
		::SendMessage(nppData._nppHandle, NPPM_MSGTOPLUGIN, (WPARAM) TEXT("FingerText.dll"), (LPARAM) &ci);
		return ci.info;
	}
	else
		return false;
}


// --- Menu call backs ---

void doxyItFunction()
{
	std::string doc_block;
	int lang_type;
	int startPos, endPos;
	int startLine, endLine;
	int indentStart, indentEnd;
	char *indent = NULL;

	if(!updateScintilla()) return;

	// Check if it is enabled
	fingertext_enabled = checkFingerText();

	// Get the current language type and parse it
	::SendMessage(nppData._nppHandle, NPPM_GETCURRENTLANGTYPE, 0, (LPARAM) &lang_type);
	doc_block = Parse(lang_type);
	if(doc_block.length() == 0) return;
	
	// Keep track of where we started
	startPos = SendScintilla(SCI_GETCURRENTPOS, 0, 0);
	startLine = SendScintilla(SCI_LINEFROMPOSITION, startPos, 0);

	// Get the whitespace of the next line so we can insert it infront of 
	// all the lines of the document block that is going to be inserted
	indentStart = SendScintilla(SCI_POSITIONFROMLINE, startLine + 1, 0);
	indentEnd = SendScintilla(SCI_GETLINEINDENTPOSITION, startLine + 1, 0);
	if(indentStart != indentEnd) indent = getRange(indentStart, indentEnd);
	

	SendScintilla(SCI_BEGINUNDOACTION, 0, 0);

	SendScintilla(SCI_REPLACESEL, SCI_UNUSED, (LPARAM) doc_block.c_str());
	
	// get the end of the document block
	endPos = SendScintilla(SCI_GETCURRENTPOS, 0, 0);
	endLine = SendScintilla(SCI_LINEFROMPOSITION, endPos, 0);

	if(indent) insertBeforeLines(indent, startLine, endLine + 1);
	
	SendScintilla(SCI_ENDUNDOACTION, 0, 0);


	// Activate FingerText
	if(fingertext_enabled)
	{
		CommunicationInfo ci;
		ci.internalMsg = FINGERTEXT_ACTIVATE;
		ci.srcModuleName = NPP_PLUGIN_NAME;
		ci.info = NULL;

		// Reset to where we started
		SendScintilla(SCI_SETCURRENTPOS, startPos, 0);

		::SendMessage(nppData._nppHandle, NPPM_MSGTOPLUGIN, (WPARAM) TEXT("FingerText.dll"), (LPARAM) &ci);
	}

	if(indent) delete[] indent;
	// return (return_val, function_name, (parameters))
}

void doxyItFile()
{
	TCHAR fileName[MAX_PATH];
	char fname[MAX_PATH];
	std::ostringstream doc_block;
	
	::SendMessage(nppData._nppHandle, NPPM_GETFILENAME, MAX_PATH, (LPARAM) fileName);
	wcstombs(fname, fileName, sizeof(fname));

	if(!updateScintilla()) return;

	// Check if it is enabled
	fingertext_enabled = checkFingerText();

	doc_block << doc_start << "\r\n";
	doc_block << doc_line << "\\file " << fname << "\r\n";
	doc_block << doc_line << "\\brief \r\n";
	doc_block << doc_line << "\r\n";
	doc_block << doc_line << "\\author \r\n";
	doc_block << doc_line << "\\version 1.0\r\n";
	doc_block << doc_end;

	SendScintilla(SCI_REPLACESEL, SCI_UNUSED, (LPARAM) doc_block.str().c_str());
}

void activeCommenting()
{
	do_active_commenting = !do_active_commenting;
	::SendMessage(nppData._nppHandle, NPPM_SETMENUITEMCHECK, funcItem[2]._cmdID, (LPARAM) do_active_commenting);
}

/*
void activeWrapping()
{
	do_active_wrapping = !do_active_wrapping;
	::SendMessage(nppData._nppHandle, NPPM_SETMENUITEMCHECK, funcItem[3]._cmdID, (LPARAM) do_active_wrapping);
}
*/

// --- Notification callbacks ---

void doxyItNewLine()
{
	char *buffer;
	int curPos, curLine, lineLen;

	if(!updateScintilla()) return;

	curPos = (int) SendScintilla(SCI_GETCURRENTPOS, SCI_UNUSED, SCI_UNUSED);
	curLine = (int) SendScintilla(SCI_LINEFROMPOSITION, curPos, SCI_UNUSED);
	lineLen = (int) SendScintilla(SCI_LINELENGTH, curLine - 1, SCI_UNUSED);
		
	buffer = new char[lineLen + 1];
	SendScintilla(SCI_GETLINE, curLine - 1, (LPARAM) buffer);
	buffer[lineLen] = '\0';
		
	// Creates a new comment block
	if(strncmp(buffer, doc_start.c_str(), doc_start.length()) == 0)
	{
		std::string temp = "\r\n" + doc_end;
		SendScintilla(SCI_REPLACESEL, 0, (LPARAM) doc_line.c_str());
		SendScintilla(SCI_INSERTTEXT, -1, (LPARAM) temp.c_str());
	}

	// Adds a new line of the comment block
	if(strncmp(buffer, doc_line.c_str(), doc_line.length()) == 0)
	{
		SendScintilla(SCI_REPLACESEL, SCI_UNUSED, (LPARAM) doc_line.c_str());
	}

	delete[] buffer;
}
