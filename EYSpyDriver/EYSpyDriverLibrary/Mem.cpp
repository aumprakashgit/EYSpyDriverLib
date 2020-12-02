#include "stdafx.h"

#include "Mem.h"
#include "Messages.h"
#include "Commands.h"
#include "CustomizedBinder.h"

using namespace EY::SpyDriver;

CAtlMap<int, CAtlMap<int, MemoryStore*>> MemoryStore::s_globalStore;
HANDLE MemoryStore::s_hMutex = CreateMutex(NULL, FALSE, NULL);

Object^ MemoryStore::SendDataMessage(UINT Msg, Object^ parameter) {
	Object^ retvalObj = nullptr;
	if (parameter != nullptr) {
		MessageBox::Show("parameter does not equal nullptr!", "Java Debugger");
		StoreParameters(parameter);
	}
	if (Msg == NULL) {
		MessageBox::Show("Msg == NULL", "Java Debugger");
	}
	if (m_notificationWindow.ToPointer() == nullptr) {
		MessageBox::Show("notification.ToPointer is nullptr", "Java Debugger");
	}
	if (m_notificationWindow.ToPointer() == NULL) {
		MessageBox::Show("notification.ToPointer is nullptr", "Java Debugger");
	}
	MessageBox::Show("m_processId is " + m_processId + "\nm_transactionId is " + m_transactionId + "\nMsg is " + Msg + "\nm_notificationWindow is " + m_notificationWindow, "Java Debugger");
	LRESULT res = ::SendMessage((HWND)m_notificationWindow.ToPointer(), Msg, m_processId, m_transactionId);
	MessageBox::Show("LRESULT of SendMessage is " + res, "Java Debugger");
	return GetReturnValue();
}

MemoryStore* MemoryStore::OpenStore(int processId, int transactionId, bool fAddRef) {
	MemoryStore* store = NULL;
	if (WaitForSingleObject(s_hMutex, INFINITE) == WAIT_OBJECT_0) {
		try {
			if (s_globalStore[processId].Lookup(transactionId, store)) {
				_ASSERTE(store != NULL);
				if (fAddRef) {
					store->AddRef();
				}
			}
			else if (fAddRef) {
				store = new MemoryStore(processId, transactionId, IntPtr::Zero);
				s_globalStore[processId][transactionId] = store;
			}
		}
		finally {
			ReleaseMutex(s_hMutex);
		}
	}
	return store;
}

void MemoryStore::OnDestroy() {
	if (WaitForSingleObject(s_hMutex, INFINITE) == WAIT_OBJECT_0) {
		try {
			s_globalStore[m_processId].RemoveKey(m_transactionId);
		}
		finally {
			ReleaseMutex(s_hMutex);
		}
	}
	if (m_notificationWindow != IntPtr::Zero) {
		::SendMessage((HWND)m_notificationWindow.ToPointer(), WM_RELEASEMEM, m_processId, m_transactionId);
	}
}

MemoryStore* MemoryStore::CreateStore(IntPtr notificationWindow) {
	MemoryStore* newStore = NULL;
	if (WaitForSingleObject(s_hMutex, INFINITE) == WAIT_OBJECT_0) {
		try {
			CAtlMap<int, MemoryStore*>& processMap = s_globalStore[GetCurrentProcessId()];

			int nextTid = 0;
			MemoryStore* store;
			//simple free id lookup.  A better performing lookup/store could be done here.
			while (processMap.Lookup(nextTid, store) && nextTid < MAXTID) {
				nextTid++;
			}
			if (nextTid < MAXTID) {
				newStore = new MemoryStore((int)GetCurrentProcessId(), nextTid, notificationWindow);
				processMap[nextTid] = newStore;
			}
			else {
				_ASSERTE(false); //out of transaction space.  A leak has occurred?
			}
		}
		finally {
			ReleaseMutex(s_hMutex);
		}
	}
	return newStore;
}

MemoryStore::MemoryStore(int processId, int transactionId, IntPtr notificationWindow) {
	m_paramKey.Format(_T("MSFT_ManagedSpy_PARAMS.%d.%d"), processId, transactionId);
	m_retvalKey.Format(_T("MSFT_ManagedSpy_RETVAL.%d.%d"), processId, transactionId);
	m_transactionId = transactionId;
	m_processId = processId;
	m_nRefCount = 1;
	m_notificationWindow = notificationWindow;
}


bool MemoryStore::StoreData(CAtlFileMapping<SharedData>& memory, Object^ data, LPCTSTR szMappingName) {
	MessageBox::Show("In StoreData Method", "Java Debugger");
	if (IsHandleValid(memory.GetHandle())) {
		memory.Unmap();
		MessageBox::Show("Memory unmapped", "Java Debugger");
	}
	MemoryStream^ stream = gcnew MemoryStream();
	BinaryFormatter^ formatter = gcnew BinaryFormatter();
	formatter->Binder = gcnew CustomizedBinder();
	//try {
	formatter->Serialize(stream, data);
	//}
	//catch(SerializationException^ ) {
	//	return false;
	//}
	memory.MapSharedMem((unsigned long)stream->Length + sizeof(SIZE_T), szMappingName);
	if (((SharedData*)memory) == NULL) {
		MessageBox::Show("((SharedData*)data) == NULL", "Java Debugger");
		return false;
	}
	((SharedData*)memory)->Size = static_cast<std::uint32_t>(stream->Length);
	BYTE* pdata = (BYTE*)((SharedData*)memory)->Data;

	array<unsigned char>^ streamdata = stream->GetBuffer();
	for (unsigned int i = 0; i < ((SharedData*)memory)->Size; i++) {
		pdata[i] = streamdata[i];
	}
	MessageBox::Show("Store data is returning true", "Java Debugger");
	return true;
}

Object^ MemoryStore::GetData(CAtlFileMapping<SharedData>& data, LPCTSTR szMappingName) {
	//we do this to allow these apis to work on the current process.
	if (data == NULL) {
		MessageBox::Show("data is NULL!", "Java Debugger");
	}
	if (!IsHandleValid(data.GetHandle())) {
		HRESULT s = data.OpenMapping(szMappingName, 0);
		MessageBox::Show("data.OpenMapping(szMappingName, 0) was run and the result is " + s, "Java Debugger");
	}
	if (((SharedData*)data) == NULL) {
		MessageBox::Show("((SharedData*)data) == NULL", "Java Debugger");
		return nullptr; // Aum commented for checking
	}
	array<unsigned char>^ sdata = gcnew array<unsigned char>(((SharedData*)data)->Size);
	BYTE* pdata = (BYTE*)((SharedData*)data)->Data;

	for (unsigned int i = 0; i < ((SharedData*)data)->Size; i++) {
		sdata[i] = pdata[i];
	}

	MemoryStream^ stream = gcnew MemoryStream(sdata);
	BinaryFormatter^ formatter = gcnew BinaryFormatter();
	formatter->Binder = gcnew CustomizedBinder();
	Object^ retvalue = nullptr;

	try {

		retvalue = formatter->Deserialize(stream);
	}
	//	catch(SerializationException^ d) {
	 //       MessageBox::Show("SerializationException^ took place and Stream length is "+stream->Length + "\nThe message is " + d->Message + "\nThe stack trace is " + d->StackTrace + "\nThe inner exception is " + d->InnerException, "Java Debugger");
	//		retvalue = nullptr;
	//	}
	catch (ArgumentNullException^) {
		MessageBox::Show("ArgumentNullException^ took place", "Java Debugger");

	}
	catch (System::Security::SecurityException^) {
		MessageBox::Show("SecurityException^ took place", "Java Debugger");

	}
	MessageBox::Show("SerializationException^ did NOT take place and Stream length is " + stream->Length, "Java Debugger");
	return retvalue;
}

