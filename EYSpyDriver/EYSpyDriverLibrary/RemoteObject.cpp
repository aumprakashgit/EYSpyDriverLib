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
    System::Reflection::Assembly^ assembly = System::Reflection::Assembly::LoadFile(remoteObj->assembly);
    Type^ type = assembly->GetType(remoteObj->typeName, true);
    if (remoteObj->getType != nullptr) {
        if (remoteObj->getType == "property") {
            return type->GetField(remoteObj->name)->GetValue(nullptr);
        }
    }
    List<Type^>^ paramTypeArray = gcnew List<Type^>();
    for (int i = 0; i < remoteObj->params->Length; i++) {
        Type^ paramType = remoteObj->params[i]->GetType();
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
