// This is the main DLL file.

#include "stdafx.h"
#include "Commands.h"
#include "RemoteObject.h"
//#include "EYWPFElements.h"

//using namespace System::Diagnostics;

static HHOOK _messageHookHandle = NULL;

//-----------------------------------------------------------------------------
//Spying Process functions follow
//-----------------------------------------------------------------------------
void Desktop::EnableHook(IntPtr windowHandle) {

	HINSTANCE hinstDLL;
	hinstDLL = LoadLibrary((LPCTSTR)_T("EYSpyDriverLibrary.dll")); // ManagedSpyLib.dll todo: Need to change to EYSpyDriverLibrary dll

	DisableHook();
	DWORD tid = GetWindowThreadProcessId((HWND)windowHandle.ToPointer(), NULL);
	_messageHookHandle = SetWindowsHookEx(WH_CALLWNDPROC,
		(HOOKPROC)GetProcAddress(hinstDLL, "MessageHookProc"),
		hinstDLL,
		tid);
}

void Desktop::DisableHook() {
	if (_messageHookHandle != NULL) {
		UnhookWindowsHookEx(_messageHookHandle);
		_messageHookHandle = NULL;
	}
}

Object^ Desktop::SendMarshaledMessage(IntPtr hWnd, UINT Msg, Object^ parameter, bool hookRequired) {
	if (hWnd == IntPtr::Zero) {
		MessageBox::Show("Marshal Message is Returning null due to hWnd == IntPtr::Zero", "Java Debugger");
		return nullptr;
	}
	MemoryStore* store = MemoryStore::CreateStore(hWnd);
	if (store == NULL) {
		MessageBox::Show("Marshal Message is Returning null due to store == NULL", "Java Debugger");
		return nullptr;
	}
	Object^ retval = nullptr;
	if (hookRequired) {
		Desktop::EnableHook(hWnd);
	}

	try {
		retval = store->SendDataMessage(Msg, parameter);
	}
	finally {
		if (hookRequired) {
			Desktop::EnableHook(hWnd);	//enable it during deletion for memory release messages.
		}
		delete store;
		if (hookRequired) {
			Desktop::DisableHook();
		}
	}
	if (retval == nullptr) {
		MessageBox::Show("Marshal Message is Returning null due to retval == nullptr", "Java Debugger");
	}
	return retval;
}

BOOL CALLBACK EnumCallback(HWND handle, LPARAM arg) {
	Desktop::topLevelWindows->Add(Desktop::GetProxy((System::IntPtr)handle));
	return TRUE;
}

array<ControlProxy^>^ Desktop::GetTopLevelWindows() {
	_ASSERTE(topLevelWindows->Count == 0);

	EnumWindows((WNDENUMPROC)EnumCallback, (LPARAM)0);

	array<ControlProxy^>^ winarr = gcnew array<ControlProxy^>(topLevelWindows->Count);
	topLevelWindows->CopyTo(winarr);
	topLevelWindows->Clear();
	return winarr;
}

bool Desktop::IsProcessAccessible(DWORD processID)
{
	// Check for 64-bit processes:
	if (Environment::Is64BitOperatingSystem)
	{
		// We're running on 64-bit OS. So we should check whether other processes have the same bitness as ours.
		auto isWow64 = FALSE;
		auto handle = OpenProcess(PROCESS_ALL_ACCESS, FALSE, processID);
		try
		{
			auto result = IsWow64Process(handle, &isWow64);
			if (result == 0)
			{
				// Open process error, we can't handle this process.
				return false;
			}

			if (!isWow64 && !Environment::Is64BitProcess)
			{
				// wrong arch, we can't handle this process.
				return false;
			}
		}
		finally
		{
			CloseHandle(handle);
		}
	}

	Process^ process = Process::GetProcessById(processID);
	return process != nullptr;
}

bool Desktop::IsManagedProcess(DWORD processID) {
	if (managedProcesses->Contains(processID))
	{
		return true;
	}

	if (processID == 0 || unmanagedProcesses->Contains(processID))
	{
		return false;
	}

	Process^ process = Process::GetProcessById(processID);
	auto isManaged = false;
	auto modules = process->Modules;
	for (auto i = 0; i < modules->Count; i++) {
		auto module = modules[i];
		auto moduleName = module->ModuleName;
		if (moduleName == _T("mscorlib.dll") || moduleName == _T("mscorlib.ni.dll")) {
			isManaged = true;
			break;
			//// Try to load assembly.
			//try
			//{
			//	AssemblyName::GetAssemblyName(module->FileName);
			//	isManaged = true;
			//	break;
			//}
			//catch (BadImageFormatException ^)
			//{
			//	// Oh, not managed.
			//}
		}
	}

	if (isManaged)
	{
		managedProcesses->Add(processID);
	}
	else
	{
		unmanagedProcesses->Add(processID);
	}

	return isManaged;
}

ControlProxy^ Desktop::GetProxy(IntPtr windowHandle)
{
	ControlProxy^ proxy = nullptr;
	if (proxyCache->ContainsKey(windowHandle))
	{
		MessageBox::Show("ProxyCache contains proxy", "Java Debugger");
		proxy = proxyCache[windowHandle];
	}
	else {
		MessageBox::Show("ProxyCache does NOT contain proxy", "Java Debugger");
	}

	if (proxy == nullptr)
	{
		DWORD procid = 0;
		GetWindowThreadProcessId((HWND)windowHandle.ToPointer(), &procid);
		MessageBox::Show("Is Process Accessible: " + IsProcessAccessible(procid) + "\nIs Managed Process: " + IsManagedProcess(procid), "Java Debugger");
		if (IsProcessAccessible(procid) && IsManagedProcess(procid))
		{
			MessageBox::Show("Process is accessible and is managed, inside the If statement", "Java Debugger");
			List<Object^>^ params = gcnew List<Object^>();
			params->Add(Desktop::eventWindow->Handle);
			int c = params->Count;
			if (c == 0) {
				MessageBox::Show("Params has size 0", "Java Debugger");
			}
			proxy = (ControlProxy^)SendMarshaledMessage(windowHandle, WM_GETPROXY, params);
		}

		// Only cache managed proxies (CONSIDER: hook wndproc of native windows to track lifetime).
		if (proxy != nullptr && !proxyCache->ContainsKey(windowHandle))
		{
			proxyCache->Add(windowHandle, proxy);
		}
	}

	if (proxy == nullptr)
	{
		MessageBox::Show("Creating window the wrong way :(", "Java Debugger");
		proxy = gcnew ControlProxy(windowHandle); // native window or error condition
	}

	return proxy;
}

void EventRegister::OnEventFired(Object^ sender, EventArgs^ args) {
	List<Object^>^ params = gcnew List<Object^>();
	params->Add((Object^)sourceWindow->Handle);
	params->Add((Object^)eventCode);
	if (args == EventArgs::Empty || args->GetType()->IsSerializable) {
		params->Add(args);
	}
	else {
		if (MouseEventArgs::typeid->IsAssignableFrom(args->GetType())) {
			params->Add(gcnew SerializableMouseEventArgs((MouseEventArgs^)args));
		}
		else {
			params->Add(gcnew NonSerializableEventArgs(args));
		}
	}
	Desktop::SendMarshaledMessage(targetEventReceiver, WM_EVENTFIRED, params, false);
}

//-----------------------------------------------------------------------------
//Spied Process functions follow
//-----------------------------------------------------------------------------
__declspec(dllexport)
LRESULT CALLBACK MessageHookProc(int nCode, WPARAM wparam, LPARAM lparam) {
	try {
		if (nCode == HC_ACTION) {
			EY::SpyDriver::Desktop::OnMessage(nCode, wparam, lparam);
		}
	}
	catch (...) {}

	return CallNextHookEx(_messageHookHandle, nCode, wparam, lparam);
}

Object^ Desktop::GetEventHandler(Type^ eventHandlerType, Object^ instance) {

	Object^ o = nullptr;
	if (instance == nullptr || eventHandlerType == nullptr) {
		return nullptr;
	}
	array<Object^>^ dmparams = gcnew array<Object^>(1);
	dmparams[0] = instance;

	if (eventMethodCache->ContainsKey(eventHandlerType)) {
		return eventMethodCache[eventHandlerType]->Invoke(nullptr, dmparams);
	}
	try {
		String^ fnName = gcnew String(L"GenerateEH");
		fnName = fnName + eventTypeCount.ToString();

		//get the event callback methodinfo.
		if (eventCallback == nullptr) {
			eventCallback = EventRegister::typeid->GetMethod(L"OnEventFired", BindingFlags::Instance | BindingFlags::Public);
		}

		array<ConstructorInfo^>^ ctors = eventHandlerType->GetConstructors();
		if (ctors->Length < 1) {
			return nullptr;
		}
		ConstructorInfo^ ehCtor = ctors[0];

		array<Type^>^ params = gcnew array<Type^>(1);
		params[0] = Object::typeid;

		DynamicMethod^ dm = gcnew DynamicMethod(fnName, EventHandler::typeid,
			params, Desktop::typeid);

		ILGenerator^ methodIL = dm->GetILGenerator();
		methodIL->Emit(OpCodes::Ldarg_0);
		methodIL->Emit(OpCodes::Ldftn, eventCallback);
		methodIL->Emit(OpCodes::Newobj, ehCtor);
		methodIL->Emit(OpCodes::Ret);

		o = dm->Invoke(nullptr, dmparams);
		eventTypeCount++;
		eventMethodCache->Add(eventHandlerType, dm);
	}
	catch (...) {
		o = nullptr;
	}
	return o;
}

void Desktop::SubscribeEvent(Control^ target, IntPtr eventWindow, String^ eventName, int eventCode) {
	EventRegister^ eventreg = gcnew EventRegister();
	eventreg->eventCode = eventCode;
	eventreg->sourceWindow = target;
	eventreg->targetEventReceiver = eventWindow;
	eventreg->eventInfo = target->GetType()->GetEvent(eventName);

	//if everything is valid, add it to our list and subscribe
	if (eventreg->sourceWindow != nullptr &&
		eventreg->targetEventReceiver != IntPtr::Zero &&
		eventreg->eventInfo != nullptr) {
		Dictionary<int, EventRegister^>^ windowEventList;

		if (!eventCallbacks->ContainsKey(target->Handle)) {
			windowEventList = gcnew Dictionary<int, EventRegister^>();
			eventCallbacks->Add(target->Handle, windowEventList);
		}
		else {
			windowEventList = eventCallbacks[target->Handle];
		}
		if (windowEventList->ContainsKey(eventCode)) {
			UnsubscribeEvent(target, eventCode);
		}
		MethodInfo^ addmethod = eventreg->eventInfo->GetType()->GetMethod(L"AddEventHandler");
		_ASSERTE(addmethod != nullptr);
		array<Object^>^ addparams = gcnew array<Object^>(2);
		addparams[0] = eventreg->sourceWindow;
		addparams[1] = GetEventHandler(eventreg->eventInfo->EventHandlerType, eventreg);
		addmethod->Invoke(eventreg->eventInfo, addparams);
		windowEventList->Add(eventCode, eventreg);
	}
}

void Desktop::UnsubscribeEvent(Control^ target, int eventCode) {
	if (target != nullptr && target->Handle != IntPtr::Zero) {
		if (eventCallbacks->ContainsKey(target->Handle)) {
			Dictionary<int, EventRegister^>^ windowEventList = eventCallbacks[target->Handle];
			_ASSERTE(windowEventList != nullptr);
			if (windowEventList != nullptr && windowEventList->ContainsKey(eventCode)) {
				EventRegister^ eventreg = windowEventList[eventCode];
				if (eventreg != nullptr) {
					windowEventList->Remove(eventCode);
					if (eventreg->eventInfo != nullptr) {
						MethodInfo^ removemethod = eventreg->eventInfo->GetType()->GetMethod(L"RemoveEventHandler");
						_ASSERTE(removemethod != nullptr);
						array<Object^>^ removeparams = gcnew array<Object^>(2);
						removeparams[0] = eventreg->sourceWindow;
						removeparams[1] = GetEventHandler(eventreg->eventInfo->EventHandlerType, eventreg);
						removemethod->Invoke(eventreg->eventInfo, removeparams);
					}
				}
			}
		}
	}
}

void Desktop::OnMessage(int nCode, WPARAM wparam, LPARAM lparam)
{
	auto msg = (CWPSTRUCT*)lparam;

	if (msg != NULL) {
		if (msg->message == WM_ISMANAGED) {
			//query whether this window is managed.
			Control^ w = System::Windows::Forms::Control::FromHandle((System::IntPtr)msg->hwnd);
			MemoryStore* store = MemoryStore::OpenStore(msg);
			if (store != NULL) {
				if (w != nullptr) {
					store->StoreReturnValue((Object^)true);
				}
				else {
					store->StoreReturnValue((Object^)false);
				}
			}
		}
		else if (msg->message == WM_GETPROXY) {
			Control^ wd = System::Windows::Forms::Control::FromHandle((System::IntPtr)msg->hwnd);
			MessageBox::Show("Control availability check right after entering WM_GETPROXY" + wd->Text, "Java Debugger");
			MemoryStore* store = MemoryStore::OpenStore(msg);
			if (store != NULL) {
				List<Object^>^ params = (List<Object^>^)store->GetParameters();
				if (params->Count == 1) {
					ControlProxy^ proxy = nullptr;
					if (proxyCache->ContainsKey((System::IntPtr)msg->hwnd)) {
						proxy = proxyCache[(System::IntPtr)msg->hwnd];
					}
					if (proxy == nullptr) {
						System::IntPtr inthwnd = (System::IntPtr) msg->hwnd;
						MessageBox::Show("hwnd for FromHandle is " + inthwnd, "Java Debugger");
						Control^ w = System::Windows::Forms::Control::FromHandle((System::IntPtr)msg->hwnd);
						Type^ type = w->GetType();
						MessageBox::Show("Type of Obj is " + type->FullName, "Java Debugger");
						// MethodInfo^ method = type->GetMethod("Hide");
						// method->Invoke(w, nullptr);
						if (w != nullptr) {
							proxy = gcnew ControlProxy(w);
							MessageBox::Show("Proxy Created", "Java Debugger");
						}
					}

					if (proxy != nullptr) {
						MessageBox::Show("Proxy not null", "Java Debugger");
						//do this even if an existing proxy in case it is a leftover.
						//note that ManagedSpy only supports one client.
						proxy->SetEventWindow((IntPtr)params[0]);
						store->StoreReturnValue(proxy);
						Control^ wd2 = System::Windows::Forms::Control::FromHandle((System::IntPtr)msg->hwnd);
						MessageBox::Show("Control availability check right after store value" + wd2->Text, "Java Debugger");
					}
					else {
						MessageBox::Show("Proxy is NULL", "Java Debugger");
					}
				}
			}
		}
		else if (msg->message == WM_INVOKE) {
			Control^ w = System::Windows::Forms::Control::FromHandle((System::IntPtr)msg->hwnd);
			MessageBox::Show("Inside of WM_INVOKE!!!!!", "Java Debugger");
			MemoryStore* store = MemoryStore::OpenStore(msg);
			List<Object^>^ params = (List<Object^>^) store->GetParameters();
			MessageBox::Show(params[0]->GetType()->Name, "Java Debugger");
			try {
				for (int i = 0; i < params->Count; i++) {
					if (params[i]->GetType()->Equals(Assembly::GetExecutingAssembly()->GetType("EY.SpyDriver.RemoteObject"))) {
						params[i] = ((EY::SpyDriver::RemoteObject^)params[i])->getObject();
					};
				}
			}
			catch (System::Exception^ e) {
				System::Windows::Forms::MessageBox::Show(e->Message + " and " + e->InnerException + " and " + e->StackTrace);
			}
			Type^ type = w->GetType();
			MethodInfo^ method = type->GetMethod(params[0]->ToString(), BindingFlags::NonPublic | BindingFlags::Public | BindingFlags::Instance);

			Object^ retVal = nullptr;
			if (params->Count == 1) {
				retVal = method->Invoke(w, nullptr);
			}
			else {
				params->RemoveAt(0);
				retVal = method->Invoke(w, params->ToArray());
			}
			store->StoreReturnValue(retVal);
		}
		else if (msg->message == WM_RELEASEMEM) {
			MemoryStore* store = MemoryStore::OpenStore((int)msg->wParam, (int)msg->lParam, false);
			if (store) {
				store->Release();
			}
		}
		else if (msg->message == WM_GETMGDPROPERTY) {
			MessageBox::Show("Inside of WM_GETPROP", "Java Debugger");
			Control^ w = System::Windows::Forms::Control::FromHandle((System::IntPtr)msg->hwnd);
			MessageBox::Show("Control type and assembly is " + w->GetType()->FullName + " and " + w->GetType()->Assembly->Location, "Java Debugger");
			MemoryStore* store = MemoryStore::OpenStore(msg);
			if (store == NULL) {
				MessageBox::Show("store == NULL", "Java Debugger");
			}
			if (w != nullptr && store != NULL) {
				String^ propname = (String^)store->GetParameters();
				MessageBox::Show("propname is " + propname, "Java Debugger");
				PropertyDescriptor^ pd = TypeDescriptor::GetProperties(w)[propname];
				if (pd != nullptr) {
					Object^ val = pd->GetValue(w);
					MessageBox::Show("val is " + val, "Java Debugger");
					store->StoreReturnValue(val);
				}
			}
		}
		else if (msg->message == WM_RESETMGDPROPERTY) {
			Control^ w = System::Windows::Forms::Control::FromHandle((System::IntPtr)msg->hwnd);
			MemoryStore* store = MemoryStore::OpenStore(msg);
			if (w != nullptr && store != NULL) {
				String^ propname = (String^)store->GetParameters();
				PropertyDescriptor^ pd = TypeDescriptor::GetProperties(w)[propname];
				if (pd != nullptr) {
					pd->ResetValue(w);
				}
			}
		}
		else if (msg->message == WM_SETMGDPROPERTY) {
			Control^ w = System::Windows::Forms::Control::FromHandle((System::IntPtr)msg->hwnd);
			MemoryStore* store = MemoryStore::OpenStore(msg);
			if (w != nullptr && store != NULL) {
				List<Object^>^ params = (List<Object^>^)store->GetParameters();
				if (params != nullptr && params->Count == 2) {
					PropertyDescriptor^ pd = TypeDescriptor::GetProperties(w)[(String^)params[0]];
					if (pd != nullptr) {
						pd->SetValue(w, params[1]);
					}
				}
			}
		}
		else if (msg->message == WM_SUBSCRIBEEVENT) {
			Control^ w = System::Windows::Forms::Control::FromHandle((System::IntPtr)msg->hwnd);
			MemoryStore* store = MemoryStore::OpenStore(msg);
			if (w != nullptr && store != NULL) {
				List<Object^>^ params = (List<Object^>^)store->GetParameters();
				if (params != nullptr && params->Count == 3) {
					SubscribeEvent(w, (IntPtr)params[0], (String^)params[1], (int)params[2]);
				}
			}
		}
		else if (msg->message == WM_UNSUBSCRIBEEVENT) {
			Control^ w = System::Windows::Forms::Control::FromHandle((System::IntPtr)msg->hwnd);
			MemoryStore* store = MemoryStore::OpenStore(msg);
			if (w != nullptr && store != NULL) {
				List<Object^>^ params = (List<Object^>^)store->GetParameters();
				if (params != nullptr && params->Count == 1) {
					UnsubscribeEvent(w, (int)params[0]);
				}
			}
		}
		else if (msg->message == WM_WPF_INVOKE) {

			MessageBox::Show("Message handle inside WM_WPF_INVOKE " + (System::IntPtr)msg->hwnd, "Java Debugger");
			Control^ w = System::Windows::Forms::Control::FromHandle((System::IntPtr)msg->hwnd);


				//vector<Handles::child_data> windows = Handles().enum_windows(msg->hwnd).get_results();

			MessageBox::Show("Inside of WM_WPF_INVOKE!!!!!", "Java Debugger");
			MemoryStore* store = MemoryStore::OpenStore(msg);
			List<Object^>^ params = (List<Object^>^) store->GetParameters();
			MessageBox::Show(params[0]->GetType()->Name, "Java Debugger");
			try {
				for (int i = 0; i < params->Count; i++) {
					if (params[i]->GetType()->Equals(Assembly::GetExecutingAssembly()->GetType("EY.SpyDriver.RemoteObject"))) {
						params[i] = ((EY::SpyDriver::RemoteObject^)params[i])->getObject();
					};
				}
			}
			catch (System::Exception^ e) {
				System::Windows::Forms::MessageBox::Show(e->Message + " and " + e->InnerException + " and " + e->StackTrace);
			}
			Type^ type = w->GetType();
			MethodInfo^ method = type->GetMethod(params[0]->ToString(), BindingFlags::NonPublic | BindingFlags::Public | BindingFlags::Instance);

			Object^ retVal = nullptr;
			if (params->Count == 1) {
				retVal = method->Invoke(w, nullptr);
			}
			else {
				params->RemoveAt(0);
				retVal = method->Invoke(w, params->ToArray());
			}
			store->StoreReturnValue(retVal);
		}
	}
}


ControlProxy^ Desktop::WPFGetProxy(IntPtr mainWindowHandle, DWORD processId)
{
	ControlProxy^ proxy = nullptr;
	if (proxyCache->ContainsKey(mainWindowHandle))
	{
		MessageBox::Show("ProxyCache contains proxy", "Java Debugger");
		proxy = proxyCache[mainWindowHandle];
	}
	else {
		MessageBox::Show("ProxyCache does NOT contain proxy", "Java Debugger");
	}

	if (proxy == nullptr)
	{
		DWORD procid = 0;
		GetWindowThreadProcessId((HWND)mainWindowHandle.ToPointer(), &procid);
		MessageBox::Show("Is Process Accessible: " + IsProcessAccessible(procid) + "\nIs Managed Process: " + IsManagedProcess(procid), "Java Debugger");
		if (IsProcessAccessible(procid) && IsManagedProcess(procid))
		{
			MessageBox::Show("Process is accessible and is managed, inside the If statement", "Java Debugger");
			List<Object^>^ params = gcnew List<Object^>();
			params->Add(Desktop::eventWindow->Handle);
			int c = params->Count;
			if (c == 0) {
				MessageBox::Show("Params has size 0", "Java Debugger");
			}
			proxy = (ControlProxy^)SendMarshaledMessage(mainWindowHandle, WM_GETPROXY, params);
		}

		// Only cache managed proxies (CONSIDER: hook wndproc of native windows to track lifetime).
		if (proxy != nullptr && !proxyCache->ContainsKey(mainWindowHandle))
		{
			proxyCache->Add(mainWindowHandle, proxy);
		}
	}

	if (proxy == nullptr)
	{
		MessageBox::Show("Creating window the wrong way :(", "Java Debugger");
		proxy = gcnew ControlProxy(mainWindowHandle); // native window or error condition
	}

	return proxy;
}
