#include <windows.h>
#include <tchar.h>
#include "tp_stub.h"
#include "ncbind/ncbind.hpp"

#define KRWINDOWCLASS _T("TTVPWindowForm")

//---------------------------------------------------------------------------
// ���b�Z�[�W��M�֐�
//---------------------------------------------------------------------------
static bool __stdcall MyReceiver(void *userdata, tTVPWindowMessage *Message)
{
	iTJSDispatch2 *obj = (iTJSDispatch2 *)userdata;
	if (Message->Msg == WM_COPYDATA) {
		COPYDATASTRUCT *copyData = (COPYDATASTRUCT*)Message->LParam;
		tTJSVariant key((tjs_int32)copyData->dwData);
		tTJSVariant msg((const tjs_char *)copyData->lpData);
		tTJSVariant *p[] = {&key, &msg};
		obj->FuncCall(0, L"onMessageReceived", NULL, NULL, 2, p, obj);
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
		MsgInfo(HWND hWnd, int key, const tjs_char *msg) : hWnd(hWnd) {
			copyData.dwData = key;
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
	void sendMessage(int key, const tjs_char *msg) {
		tTJSVariant val;
		objthis->PropGet(0, TJS_W("HWND"), NULL, &val, objthis);
		MsgInfo info(reinterpret_cast<HWND>((tjs_int)(val)), key, msg);
		EnumWindows(enumWindowsProc, (LPARAM)&info);
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
}
