#include "stdafx.h"
#include "CustomizedBinder.h"
#include "ControlProxy.h"

bool CustomizedBinder::Init()
{
    if (!initDone) {
        initDone = true;
        ControlProxy::assemblies;
        if (ControlProxy::assemblies != nullptr) {
            for (int i = 0; i < ControlProxy::assemblies->Count; i++) {
                if (ControlProxy::assemblies[i] != nullptr) {
                    Assembly^ a = ControlProxy::assemblies[i];
                    Assembly::Load(a->FullName);
                }
            }
        }
        if (!ControlProxy::assemblies->Contains(Assembly::GetCallingAssembly())) {
            Assembly::Load(Assembly::GetCallingAssembly()->FullName);
        }
        return true;
    }
}

Type^ CustomizedBinder::BindToType(String^ assemblyName, String^ typeName)
{
    return Type::GetType(String::Format("{0}, {1}", typeName, assemblyName));
}
