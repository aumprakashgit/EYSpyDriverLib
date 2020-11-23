#pragma once

#include "Mem.h"
#include "Commands.h"
#include "ControlProxy.h"

public ref class CustomizedBinder sealed :
    public SerializationBinder
{

private:
    static bool initDone = Init();

    static bool Init();
public:
    Type^ BindToType(String^ assemblyName, String^ typeName) override;





};

