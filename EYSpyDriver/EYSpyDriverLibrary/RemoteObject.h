#pragma once

using namespace System;
using namespace System::Reflection;

namespace EY
{
    namespace SpyDriver
    {
        [Serializable]
        public ref class RemoteObject
        {
        public:
            property String^ typeName;
            property array<Object^>^ params;
            property String^ name;
            property String^ assembly;
            property String^ getType;
            virtual Object^ getObject();

        private:
            static Object^ convertRemoteObject(RemoteObject^ remoteObj);

        };
    }
}

