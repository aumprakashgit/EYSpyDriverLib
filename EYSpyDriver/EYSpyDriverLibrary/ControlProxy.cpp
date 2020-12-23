#include "StdAfx.h"
#include "Commands.h"
#include "ControlProxy.h"
#include "WPFWIndowFinder.h"
//#include "EYWPFElements.h"

using namespace EY::SpyDriver;
//using namespace System::Reflection;

//The constructor sets up
// 1. Class Name (ie Type name)
// 2. Component Name
// 3. list of needed assemblies (these are reloaded in the spying process)
ControlProxy::ControlProxy(Control^ instance) {
	MessageBox::Show("ABOUT TO SET INSTANCE OF CONTROL", "Java Debugger");
	//  this->nativeControl = instance;
	EnsureAssemblyResolve();
	eventWindowHandle = IntPtr::Zero;
	oldHandle = IntPtr::Zero;
	if (instance != nullptr) {
		className = TypeDescriptor::GetClassName(instance);
		componentName = TypeDescriptor::GetComponentName(instance);
		if (String::IsNullOrEmpty(componentName)) { //try the Name property
			PropertyDescriptor^ pd = TypeDescriptor::GetProperties(instance)["Name"];
			if (pd != nullptr) {
				componentName = (String^)pd->GetValue(instance);
			}
			if (String::IsNullOrEmpty(componentName)) {
				Control^ parent = instance->Parent;
				if (parent != nullptr) {
					//our last ditch effort will be to retrieve the field name from reflection from the parent.
					//this may also not work if the parent dynamically created the control or did something similar
					array<System::Reflection::FieldInfo^>^ fields = parent->GetType()->GetFields(System::Reflection::BindingFlags::NonPublic |
						System::Reflection::BindingFlags::Instance);
					if (fields != nullptr && fields->Length > 0) {
						for (int i = 0; i < fields->Length; i++) {
							if (fields[i] != nullptr) {
								Object^ fieldValue = fields[i]->GetValue(parent);
								if (Object::ReferenceEquals(fieldValue, instance)) {
									componentName = fields[i]->Name;
								}
							}
						}
					}
				}
			}
		}

		// this->methodName = instance->GetType()->GetMethod("CreateControl")->Name;
		 //MessageBox::Show("INSTANCE SET, instance = " + instance->Text + " NativeControl = " + nativeControl->Text + " MethodName = "+methodName, "Java Debugger");
		this->Handle = instance->Handle;
		//store assemblies and type.
		this->typeName = instance->GetType()->AssemblyQualifiedName;
		array<Assembly^>^ domainasms = System::AppDomain::CurrentDomain->GetAssemblies();
		assemblyPaths = gcnew List<String^>(domainasms->Length);
		array<AssemblyName^>^ referencedAssemblyNames = instance->GetType()->Assembly->GetReferencedAssemblies();

		for (int j = 0; j < referencedAssemblyNames->Length; j++) {
			MessageBox::Show("Adding dll " + referencedAssemblyNames[j], "Java Debugger");
			assemblyPaths->Add(Assembly::Load(referencedAssemblyNames[j])->Location);
		}
		assemblyPaths->Add(instance->GetType()->Assembly->Location);
		assemblyPaths->Add(Assembly::GetCallingAssembly()->Location);
		Desktop::proxyCache->Add(Handle, this);
		MessageBox::Show("Proxy added to Cache", "Java Debugger");
		//subscribe on handlecreates and destroys so we can properly manage ourselves
		instance->HandleCreated += gcnew EventHandler(this, &ControlProxy::OnHandleCreated);
		instance->HandleDestroyed += gcnew EventHandler(this, &ControlProxy::OnHandleDestroyed);
	}
}

Control^ EY::SpyDriver::ControlProxy::GetNativeControl()
{
	Desktop::EnableHook(Handle);
	Control^ c = System::Windows::Forms::Control::FromHandle(Handle);
	return c;
}

//This instance represents a native window when called through this ctor.
//we initialize the component name to the handle and the class name to the window class.
ControlProxy::ControlProxy(IntPtr windowHandle) {
	Handle = windowHandle;
	componentName = Handle.ToString();
	TCHAR szName[255];

	if (::GetClassName((HWND)windowHandle.ToPointer(), szName, 255)) {
		className = gcnew System::String((LPTSTR)szName);
	}
}

//When a control changes its handle, we inform the ControlProxy
//We need to do this because the ControlProxy does all of its mojo using window
//handles.  So, if we didn't tell the Spying process about this handle change,
//all future commands (set/get values, etc) would go to a destroyed window.
void ControlProxy::OnHandleCreated(Object^ sender, EventArgs^ args) {
	Control^ w = (Control^)sender;
	if (w != nullptr) {
		if (Desktop::proxyCache->ContainsKey(w->Handle)) {
			Desktop::proxyCache->Remove(w->Handle);
		}
		if (oldHandle != IntPtr::Zero && Desktop::proxyCache->ContainsKey(w->Handle)) {
			Desktop::proxyCache->Remove(oldHandle);
		}
		Desktop::proxyCache->Add(w->Handle, this);
		if (eventWindowHandle != IntPtr::Zero) {
			::SendMessage((HWND)eventWindowHandle.ToPointer(), WM_HANDLECHANGED,
				(WPARAM)oldHandle.ToPointer(), (LPARAM)w->Handle.ToPointer());
		}
		oldHandle = IntPtr::Zero;
	}
}

//If we are not creating the handle again, this will inform the spying process
//that the control is gone forever.
void ControlProxy::OnHandleDestroyed(Object^ sender, EventArgs^ args) {
	Control^ w = (Control^)sender;
	if (w != nullptr) {
		if (w->RecreatingHandle) {
			oldHandle = w->Handle;
		}
		else {
			if (Desktop::proxyCache->ContainsKey(w->Handle)) {
				Desktop::proxyCache->Remove(w->Handle);
			}
			w->HandleCreated -= gcnew EventHandler(this, &ControlProxy::OnHandleCreated);
			w->HandleDestroyed -= gcnew EventHandler(this, &ControlProxy::OnHandleDestroyed);
			if (eventWindowHandle != IntPtr::Zero) {
				::SendMessage((HWND)eventWindowHandle.ToPointer(), WM_WINDOWDESTROYED,
					(WPARAM)w->Handle.ToPointer(), 0);
			}
		}
	}
}

bool ControlProxy::IsManaged::get()
{
	if (Handle == IntPtr::Zero)
	{
		return false;
	}

	auto proc = this->OwningProcess;
	if (proc == nullptr)
	{
		return false;
	}

	if (!Desktop::IsProcessAccessible(proc->Id) || !Desktop::IsManagedProcess(proc->Id))
	{
		return false;
	}

	if (ComponentType != nullptr || !String::IsNullOrEmpty(typeName))
	{
		return true;
	}

	auto retvalObj = Desktop::SendMarshaledMessage(Handle, WM_ISMANAGED, nullptr);
	if (retvalObj != nullptr)
	{
		return (bool)(System::Boolean^)retvalObj;
	}

	return false;
}

ControlProxy^ ControlProxy::FromHandle(System::IntPtr windowHandle) {
	if (windowHandle == IntPtr::Zero) {
		return nullptr;
	}

	return Desktop::GetProxy(windowHandle);
}

array<ControlProxy^>^ ControlProxy::TopLevelWindows::get() {
	return Desktop::GetTopLevelWindows();
}

array<ControlProxy^>^ ControlProxy::Children::get() {
	if (Handle == IntPtr::Zero) {
		return gcnew array<ControlProxy^>(0);
	}

	Desktop::childWindows->Clear();
	EnumChildWindows((HWND)this->Handle.ToPointer(), (WNDENUMPROC)EnumChildrenCallback,
		(LPARAM)this->Handle.ToPointer());

	array<ControlProxy^>^ winarr = gcnew array<ControlProxy^>(Desktop::childWindows->Count);
	Desktop::childWindows->CopyTo(winarr);
	Desktop::childWindows->Clear();
	return winarr;
}

BOOL CALLBACK EnumChildrenCallback(HWND handle, LPARAM arg) {
	HWND targetParent = (HWND)arg;
	if (GetParent(handle) == targetParent) {
		Desktop::childWindows->Add(ControlProxy::FromHandle((IntPtr)handle));
	}
	return TRUE;
}

Type^ ControlProxy::ComponentType::get() {
	if (componentType == nullptr && !String::IsNullOrEmpty(typeName)) { //we have to build the initial property proxy list
		EnsureAssemblyResolve();
		if (assemblyPaths == nullptr) {
			MessageBox::Show("assemblyPaths == nullptr", "Java Debugger");
		}
		if (assemblyPaths->Count == 0) {
			MessageBox::Show("assemblyPaths->Count == 0", "Java Debugger");
		}
		if (assemblyPaths != nullptr && assemblyPaths->Count > 0) {
			for (int i = 0; i < assemblyPaths->Count; i++) {
				if (!loadedAssemblies->Contains(assemblyPaths[i])) {
					loadedAssemblies->Add(assemblyPaths[i]);
					//try {
					MessageBox::Show("Loading assembly: " + assemblyPaths[i], "Java Debugger");
					assemblies->Add(Assembly::LoadFile(assemblyPaths[i]));
					// }
					// catch (...) {
					// }
				}
			}
		}
		MessageBox::Show("typeName is " + typeName, "Java Debugger");


		for (int i = 0; i < assemblies->Count; i++)
		{

			componentType = assemblies[i]->GetType(typeName);
			if (componentType != nullptr) {
				MessageBox::Show(assemblies[i]->GetName() + " " + assemblies[i]->Location + " does have " + typeName, "Java Debugger");
				return componentType;
			}
			else {
				MessageBox::Show(assemblies[i]->GetName() + " " + assemblies[i]->Location + " does not have " + typeName, "Java Debugger");
			}
		}
		componentType = Type::GetType(typeName, true); //try to load the type of the control over here.
		if (componentType != nullptr) return componentType;
		typeName = nullptr;
	}
	return componentType;
}

Control^ ControlProxy::NativeControl::get() {
	if (componentType == nullptr && !String::IsNullOrEmpty(typeName)) { //we have to build the initial property proxy list
		EnsureAssemblyResolve();
		if (assemblyPaths != nullptr && assemblyPaths->Count > 0) {
			for (int i = 0; i < assemblyPaths->Count; i++) {
				if (!loadedAssemblies->Contains(assemblyPaths[i])) {
					loadedAssemblies->Add(assemblyPaths[i]);
					try {
						assemblies->Add(Assembly::LoadFile(assemblyPaths[i]));
					}
					catch (...) {
					}
				}
			}
		}
	}
	return nativeControl;
}

//just a nice to have function so you don't have to declare the p/invoke
void ControlProxy::SendMessage(int message, IntPtr wParam, IntPtr lParam) {
	::SendMessage((HWND)this->Handle.ToPointer(), (UINT)message, (WPARAM)wParam.ToPointer(), (LPARAM)lParam.ToPointer());
}

void ControlProxy::SubscribeEvent(EventDescriptor^ ed) {
	EventDescriptorCollection^ edColl = GetEvents();
	if (edColl != nullptr && edColl->Count > 0) {
		int ind = edColl->IndexOf(ed);
		if (ind >= 0) {
			List<Object^>^ params = gcnew List<Object^>();
			params->Add(Desktop::eventWindow->Handle);
			params->Add(ed->Name);
			params->Add(ind);
			Desktop::SendMarshaledMessage(Handle, WM_SUBSCRIBEEVENT, params);
		}
	}
}

void ControlProxy::UnsubscribeEvent(EventDescriptor^ ed) {
	EventDescriptorCollection^ edColl = GetEvents();
	if (edColl != nullptr && edColl->Count > 0) {
		int ind = edColl->IndexOf(ed);
		if (ind >= 0) {
			List<Object^>^ params = gcnew List<Object^>();
			params->Add(ind);

			Desktop::SendMarshaledMessage(Handle, WM_UNSUBSCRIBEEVENT, params);
		}
	}
}

//Object^ ControlProxy::Invoke(String^ methodName, array<Object^>^ parameters) {
//    List<Object^>^ messageParams = gcnew List<Object^>();
//    messageParams->Add(methodName);
//    if (parameters != nullptr) {
//        messageParams->AddRange(parameters);
//    }
//    return Desktop::SendMarshaledMessage(Handle, WM_INVOKE, messageParams);
//}

Object^ ControlProxy::Invoke(String^ methodName, array<Object^>^ parameters) {
	List<Object^>^ messageParams = gcnew List<Object^>();
	messageParams->Add(methodName);
	if (parameters != nullptr) {
		messageParams->AddRange(parameters);
	}
	return Desktop::SendMarshaledMessage(Handle, WM_INVOKE, messageParams);
}

ControlProxy^ ControlProxy::WPFFromHandle(System::IntPtr mainWindowHandle, DWORD processId) {
	if (mainWindowHandle == IntPtr::Zero) {
		return nullptr;
	}

	return Desktop::WPFGetProxy(mainWindowHandle, processId);
}

Object^ ControlProxy::WPFInvoke(IntPtr mainWindowHandle, String^ methodName, array<Object^>^ parameters) {

	Handle = mainWindowHandle;
	List<Object^>^ messageParams = gcnew List<Object^>();
	messageParams->Add(methodName);
	if (parameters != nullptr) {
		messageParams->AddRange(parameters);
	}
	return Desktop::SendMarshaledMessage(Handle, WM_WPF_INVOKE, messageParams);
}


//std::vector<Handles::child_data> ControlProxy::GetWPFChildElements(IntPtr mainWindowHandle) {
//
//	//Handle = mainWindowHandle;
//	HWND handle = (HWND)mainWindowHandle.ToPointer();
//
//	//Enumerate all visible windows and store handle and caption in "windows"
//	std::vector<Handles::child_data> windows = Handles().enum_windows(handle).get_results();
//
//	return windows;
//}

int ControlProxy::FindCountofWindow(LPCWSTR childElementName) {
	return EY::SpyDriver::FindCountofWindow(childElementName);
}


