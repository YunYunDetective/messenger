#include <windows.h>
#include <tchar.h>
#include <process.h>
#include "tp_stub.h"
#include "ncbind/ncbind.hpp"
#include <map>
using namespace std;

// �g���g���̃E�C���h�E�N���X
#define KRWINDOWCLASS _T("TTVPWindowForm")
#define WM_SHELLEXECUTED (WM_APP + 0x01)
#define KEYSIZE 256

// �m�ۂ����A�g�����
static map<ttstr,ATOM> *atoms = NULL;

// ���O����V�X�e���O���[�o���A�g���擾
static ATOM getAtom(const TCHAR *str)
{
	ttstr name(str);
	map<ttstr,ATOM>::const_iterator n = atoms->find(name);
	if (n != atoms->end()) {
		return n->second;
	}
	ATOM atom = GlobalAddAtom(str);
	(*atoms)[name] = atom;
	return atom;
}

static void getKey(tTJSVariant &key, ATOM atom)
{
	TCHAR buf[KEYSIZE+1];
	UINT len = GlobalGetAtomName(atom, buf, KEYSIZE);
	if (len > 0) {
		buf[len] = '\0';
		key = buf;
	}
}


//---------------------------------------------------------------------------
// ���b�Z�[�W��M�֐�
//---------------------------------------------------------------------------
static bool __stdcall MyReceiver(void *userdata, tTVPWindowMessage *Message)
{
	iTJSDispatch2 *obj = (iTJSDispatch2 *)userdata;
	switch (Message->Msg) {
	case WM_COPYDATA: // �O������̒ʐM
		{
			COPYDATASTRUCT *copyData = (COPYDATASTRUCT*)Message->LParam;
			tTJSVariant key;
			getKey(key, (ATOM)copyData->dwData);
			tTJSVariant msg((const tjs_char *)copyData->lpData);
			tTJSVariant *p[] = {&key, &msg};
			obj->FuncCall(0, L"onMessageReceived", NULL, NULL, 2, p, obj);
		}
		return true;
	case WM_SHELLEXECUTED: // �V�F�����s���I������
		{
			tTJSVariant process = (tjs_int)(HANDLE)Message->WParam;
			tTJSVariant endCode = (tjs_int)(DWORD)Message->LParam;
			tTJSVariant *p[] = {&process, &endCode};
			obj->FuncCall(0, L"onShellExecuted", NULL, NULL, 2, p, obj);
		}
		return true;
	}
	return false;
}

/**
 * ���\�b�h�ǉ��p�N���X
 */
class WindowAdd {

protected:
	iTJSDispatch2 *objthis; //< �I�u�W�F�N�g���̎Q��
	bool messageEnable;     //< ���b�Z�[�W�������L����

public:
	// �R���X�g���N�^
	WindowAdd(iTJSDispatch2 *objthis) : objthis(objthis), messageEnable(false) {}

	// �f�X�g���N�^
	~WindowAdd() {
		setMessageEnable(false);
	}

	/**
	 * ���b�Z�[�W��M���L�����ǂ�����ݒ�
	 * @param enable true �Ȃ�L��
	 */
	void setMessageEnable(bool enable) {
		if (messageEnable != enable) {
			messageEnable = enable;
			tTJSVariant mode     = enable ? (tTVInteger)(tjs_int)wrmRegister : (tTVInteger)(tjs_int)wrmUnregister;
			tTJSVariant proc     = (tTVInteger)reinterpret_cast<tjs_int>(MyReceiver);
			tTJSVariant userdata = (tTVInteger)(tjs_int)objthis;
			tTJSVariant *p[3] = {&mode, &proc, &userdata};
			objthis->FuncCall(0, L"registerMessageReceiver", NULL, NULL, 3, p, objthis);
		}
	}
	
	/**
	 * @return ���b�Z�[�W��M���L�����ǂ���
	 */
	bool getMessageEnable() {
		return messageEnable;
	}

	// ���M���b�Z�[�W���
	struct MsgInfo {
		HWND hWnd;
		COPYDATASTRUCT copyData;
		MsgInfo(HWND hWnd, const TCHAR *key, const tjs_char *msg) : hWnd(hWnd) {
			copyData.dwData = getAtom(key);
			copyData.cbData = (TJS_strlen(msg) + 1) * sizeof(tjs_char);
			copyData.lpData = (PVOID)msg;
		}
	};

	// ���b�Z�[�W���M����
	static BOOL CALLBACK enumWindowsProc(HWND hWnd, LPARAM parent) {
		MsgInfo *info = (MsgInfo*)parent;
		TCHAR buf[100];
		GetClassName(hWnd, buf, sizeof buf);
		if (info->hWnd != hWnd && _tcscmp(buf, KRWINDOWCLASS) == 0) {
			SendMessage(hWnd, WM_COPYDATA, (WPARAM)info->hWnd, (LPARAM)&info->copyData);
		}
		return TRUE;
	}

	/**
	 * ���b�Z�[�W���M
	 * @param key ���ʃL�[
	 * @param msg ���b�Z�[�W
	 */
	void sendMessage(const TCHAR *key, const tjs_char *msg) {
		tTJSVariant val;
		objthis->PropGet(0, TJS_W("HWND"), NULL, &val, objthis);
		MsgInfo info(reinterpret_cast<HWND>((tjs_int)(val)), key, msg);
		EnumWindows(enumWindowsProc, (LPARAM)&info);
	}

	// -------------------------------------------------------

	/**
	 * ���s���
	 */
	struct ExecuteInfo {
		HANDLE process;         // �҂��Ώۃv���Z�X
		iTJSDispatch2 *objthis; // this �ێ��p
		ExecuteInfo(HANDLE process, iTJSDispatch2 *objthis) : process(process), objthis(objthis) {};
	};
	
	/**
	 * �I���҂��X���b�h
	 */
	static void waitProcess(void *data) {
		// �p�����[�^�����p��
		HANDLE process         = ((ExecuteInfo*)data)->process;
		iTJSDispatch2 *objthis = ((ExecuteInfo*)data)->objthis;
		delete data;

		// �v���Z�X�҂�
		WaitForSingleObject(process, INFINITE);
		DWORD dt;
		GetExitCodeProcess(process, &dt); // ���ʎ擾
		CloseHandle(process);

		// ���M
		tTJSVariant val;
		objthis->PropGet(0, TJS_W("HWND"), NULL, &val, objthis);
		PostMessage(reinterpret_cast<HWND>((tjs_int)(val)), WM_SHELLEXECUTED, (WPARAM)process, (LPARAM)dt);
	}

	/**
	 * �v���Z�X�̒�~
	 * @param keyName ���ʃL�[
	 * @param endCode �I���R�[�h
	 */
	void terminateProcess(int process, int endCode) {
		TerminateProcess((HANDLE)process, endCode);
	}

	/**
	 * �v���Z�X�̎��s
	 * @param keyName ���ʃL�[
	 * @param target �^�[�Q�b�g
	 * @praam param �p�����[�^
	 */
	int shellExecute(LPCTSTR target, LPCTSTR param) {
		SHELLEXECUTEINFO si;
		ZeroMemory(&si, sizeof(si));
		si.cbSize = sizeof(si);
		si.lpVerb = _T("open");
		si.lpFile = target;
		si.lpParameters = param;
		si.nShow = SW_SHOWNORMAL;
		si.fMask = SEE_MASK_FLAG_NO_UI | SEE_MASK_NOCLOSEPROCESS;
		if (ShellExecuteEx(&si)) {
			_beginthread(waitProcess, 0, new ExecuteInfo(si.hProcess, objthis));
			return (int)si.hProcess;
		}
		return (int)INVALID_HANDLE_VALUE;
	}
};

// �C���X�^���X�Q�b�^
NCB_GET_INSTANCE_HOOK(WindowAdd)
{
	NCB_INSTANCE_GETTER(objthis) { // objthis �� iTJSDispatch2* �^�̈����Ƃ���
		ClassT* obj = GetNativeInstance(objthis);	// �l�C�e�B�u�C���X�^���X�|�C���^�擾
		if (!obj) {
			obj = new ClassT(objthis);				// �Ȃ��ꍇ�͐�������
			SetNativeInstance(objthis, obj);		// objthis �� obj ���l�C�e�B�u�C���X�^���X�Ƃ��ēo�^����
		}
		return obj;
	}
};

// �t�b�N���A�^�b�`
NCB_ATTACH_CLASS_WITH_HOOK(WindowAdd, Window) {
	Property(L"messageEnable", &WindowAdd::getMessageEnable, &WindowAdd::setMessageEnable);
	Method(L"sendMessage", &WindowAdd::sendMessage);
	Method(L"shellExecute", &WindowAdd::shellExecute);
	Method(L"terminateProcess", &WindowAdd::terminateProcess);
}


/**
 * �o�^�����O
 */
void PreRegistCallback()
{
	atoms = new map<ttstr,ATOM>;
}

/**
 * �J��������
 */
void PostUnregistCallback()
{
	map<ttstr,ATOM>::const_iterator i = atoms->begin();
	while (i != atoms->end()) {
		GlobalDeleteAtom(i->second);
	}
	delete atoms;
	atoms = NULL;
}

NCB_PRE_REGIST_CALLBACK(PreRegistCallback);
NCB_POST_UNREGIST_CALLBACK(PostUnregistCallback);
