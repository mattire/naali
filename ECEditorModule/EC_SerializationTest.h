// For conditions of distribution and use, see copyright notice in license.txt

#ifndef incl_ECEditorModule_EC_SerializationTest_h
#define incl_ECEditorModule_EC_SerializationTest_h

#include "Framework.h"
#include "AttributeInterface.h"
#include "AssetInterface.h"
#include "ComponentInterface.h"
#include "Declare_EC.h"
#include "Vector3D.h"
#include "Quaternion.h"

namespace ECEditor
{
    class EC_SerializationTest : public Foundation::ComponentInterface
    {
        DECLARE_EC(EC_SerializationTest);

        Q_OBJECT

    public:
        virtual ~EC_SerializationTest();

        virtual bool IsSerializable() const { return true; }
        
        Foundation::Attribute<Vector3df> attr1_;
        Foundation::Attribute<Quaternion> attr2_;
        Foundation::Attribute<QString> attr3_;
        Foundation::Attribute<Foundation::AssetReference> attr4_;
        
    private:
        EC_SerializationTest(Foundation::ModuleInterface* module);
    };
}

#endif
