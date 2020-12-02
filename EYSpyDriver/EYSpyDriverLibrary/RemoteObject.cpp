#include "stdafx.h"
#include "RemoteObject.h"
using namespace System::Collections::Generic;
using namespace EY::SpyDriver;


Object^ RemoteObject::getObject()
{
    return RemoteObject::convertRemoteObject(this);
}

Object^ RemoteObject::convertRemoteObject(RemoteObject^ remoteObj)
{
    System::Windows::Forms::MessageBox::Show("Inside of convert Object");
    //  System::Reflection::Assembly^ assembly = System::Reflection::Assembly::LoadFile(remoteObj->assembly);
    System::Reflection::Assembly^ assembly;
    try {
        assembly = System::Reflection::Assembly::LoadFile(remoteObj->assembly);
    }
    catch (System::Exception^ e) {
        System::Windows::Forms::MessageBox::Show(e->Message + " and " + e->InnerException + " and " + e->StackTrace);
    }
    System::Windows::Forms::MessageBox::Show("Got " + assembly->FullName);
    Type^ type;
    try {
        type = assembly->GetType(remoteObj->typeName, true);
    }
    catch (System::Exception^ e) {
        System::Windows::Forms::MessageBox::Show(e->Message + " and " + e->InnerException + " and " + e->StackTrace);
    }
    System::Windows::Forms::MessageBox::Show("Got " + type->FullName);
    if (remoteObj->getType != nullptr) {
        System::Windows::Forms::MessageBox::Show("Get field is not null!");
        if (remoteObj->getType == "property") {
            System::Windows::Forms::MessageBox::Show("Getting field!");
            return type->GetField(remoteObj->name)->GetValue(nullptr);
        }
    }
    List<Type^>^ paramTypeArray = gcnew List<Type^>();
    System::Windows::Forms::MessageBox::Show(remoteObj->params->Length.ToString());
    for (int i = 0; i < remoteObj->params->Length; i++) {
        Type^ paramType;
        try {
            paramType = remoteObj->params[i]->GetType();
        }
        catch (System::Exception^ e) {
            System::Windows::Forms::MessageBox::Show(e->Message + " and " + e->InnerException + " and " + e->StackTrace);
        }
        System::Windows::Forms::MessageBox::Show(paramType->Name);

        if (paramType->Equals(remoteObj->GetType())) {
            remoteObj->params[i] = RemoteObject::convertRemoteObject((RemoteObject^)remoteObj->params[i]);
        }
        paramTypeArray->Add(remoteObj->params[i]->GetType());
    }




    if (remoteObj->name == nullptr) {
        if (remoteObj->params->Length == 0) {
            ConstructorInfo^ constructor = type->GetConstructor(nullptr);
            return constructor->Invoke(nullptr);
        }
        ConstructorInfo^ constructor = type->GetConstructor(paramTypeArray->ToArray());
        return constructor->Invoke(remoteObj->params);
    }
    if (remoteObj->params->Length == 0) {
        MethodInfo^ method = type->GetMethod(nullptr);
        return method->Invoke(nullptr, nullptr);
    }
    MethodInfo^ method = type->GetMethod(remoteObj->name, paramTypeArray->ToArray());
    return method->Invoke(nullptr, remoteObj->params);
}
