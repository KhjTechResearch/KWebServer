#include <windows.h>
#include "tp_stub.h"
#include "ncbind.hpp"


#define EXPORT(hr) extern "C" __declspec(dllexport) hr __stdcall

#ifdef _MSC_VER
#pragma comment(linker, "/EXPORT:V2Link=_V2Link@4")
#pragma comment(linker, "/EXPORT:V2Unlink=_V2Unlink@0")
#endif

//--------------------------------------
int WINAPI
DllEntryPoint(HINSTANCE /*hinst*/, unsigned long /*reason*/, void* /*lpReserved*/)
{
	return 1;
}

//---------------------------------------------------------------------------
static tjs_int GlobalRefCountAtInit = 0;

EXPORT(HRESULT) V2Link(iTVPFunctionExporter *exporter)
{
	// �X�^�u�̏�����(�K���L�q����)
	TVPInitImportStub(exporter);

	NCB_LOG_W("V2Link");

	// AutoRegister�œo�^���ꂽ�N���X����o�^����
	ncbAutoRegister::AllRegist();

	// ���̎��_�ł� TVPPluginGlobalRefCount �̒l��
	GlobalRefCountAtInit = TVPPluginGlobalRefCount;
	// �Ƃ��čT���Ă����BTVPPluginGlobalRefCount �͂��̃v���O�C������
	// �Ǘ�����Ă��� tTJSDispatch �h���I�u�W�F�N�g�̎Q�ƃJ�E���^�̑��v�ŁA
	// ������ɂ͂���Ɠ������A����������Ȃ��Ȃ��ĂȂ��ƂȂ�Ȃ��B
	// �����Ȃ��ĂȂ���΁A�ǂ����ʂ̂Ƃ���Ŋ֐��Ȃǂ��Q�Ƃ���Ă��āA
	// �v���O�C���͉���ł��Ȃ��ƌ������ƂɂȂ�B

	return S_OK;
}
//---------------------------------------------------------------------------
EXPORT(HRESULT) V2Unlink()
{
	// �g���g��������A�v���O�C����������悤�Ƃ���Ƃ��ɌĂ΂��֐�

	// �������炩�̏����Ńv���O�C��������ł��Ȃ��ꍇ��
	// ���̎��_�� E_FAIL ��Ԃ��悤�ɂ���B
	// �����ł́ATVPPluginGlobalRefCount �� GlobalRefCountAtInit ����
	// �傫���Ȃ��Ă���Ύ��s�Ƃ������Ƃɂ���B
	if (TVPPluginGlobalRefCount > GlobalRefCountAtInit) {
		NCB_LOG_W("V2Unlink ...failed");
		return E_FAIL;
		// E_FAIL ���A��ƁAPlugins.unlink ���\�b�h�͋U��Ԃ�
	} else {
		NCB_LOG_W("V2Unlink");
	}
	/*
		�������A�N���X�̏ꍇ�A�����Ɂu�I�u�W�F�N�g���g�p���ł���v�Ƃ������Ƃ�
		�m�邷�ׂ�����܂���B��{�I�ɂ́APlugins.unlink �ɂ��v���O�C���̉����
		�댯�ł���ƍl���Ă������� (�������� Plugins.link �Ń����N������A�Ō��
		�Ńv���O�C������������A�v���O�����I���Ɠ����Ɏ����I�ɉ��������̂��g)�B
	*/

	// AutoRegister�œo�^���ꂽ�N���X�����폜����
	ncbAutoRegister::AllUnregist();

	// �X�^�u�̎g�p�I��(�K���L�q����)
	TVPUninitImportStub();

	return S_OK;
}/*
void TVPExtractArchive(const ttstr & name, const ttstr & _destdir, bool allowextractprotected)
{
	// extract file to
	bool TVPAllowExtractProtectedStorage_save = TVPAllowExtractProtectedStorage;
	TVPAllowExtractProtectedStorage = allowextractprotected;

	try
	{

		ttstr destdir(_destdir);
		tjs_char last = _destdir.GetLastChar();
		if (_destdir.GetLen() >= 1 && (last != TJS_W('/') && last != TJS_W('\\')))
			destdir += TJS_W('/');
		tTVPArchive *arc = TVPOpenArchive(name);
		try
		{
			tjs_int count = arc->GetCount();
			for (tjs_int i = 0; i < count; i++)
			{
				ttstr name = arc->GetName(i);

				tTJSBinaryStream *src = arc->CreateStreamByIndex(i);
				try
				{
					tTVPStreamHolder dest(destdir + name, TJS_BS_WRITE);
					tjs_uint8 * buffer = new tjs_uint8[TVP_LOCAL_TEMP_COPY_BLOCK_SIZE];
					try
					{
						tjs_uint read;
						while (true)
						{
							read = src->Read(buffer, TVP_LOCAL_TEMP_COPY_BLOCK_SIZE);
							if (read == 0) break;
							dest->WriteBuffer(buffer, read);
						}
					}
					catch (...)
					{
						delete[] buffer;
						throw;
					}
					delete[] buffer;
				}
				catch (...)
				{
					//					delete src;
					//					throw;
				}
				delete src;
			}

		}
		catch (...)
		{
			arc->Release();
			throw;
		}

		arc->Release();
	}
	catch (...)
	{
		TVPAllowExtractProtectedStorage =
			TVPAllowExtractProtectedStorage_save;
		throw;
	}
	TVPAllowExtractProtectedStorage =
		TVPAllowExtractProtectedStorage_save;
}
*/
//---------------------------------------------------------------------------
// static�ϐ��̎���

// auto register �擪�|�C���^
ncbAutoRegister::ThisClassT const*
ncbAutoRegister::_top[ncbAutoRegister::LINE_COUNT] = NCB_INNER_AUTOREGISTER_LINES_INSTANCE;

