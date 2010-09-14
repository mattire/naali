/**
 *  For conditions of distribution and use, see copyright notice in license.txt
 *
 *  @file   EC_Movable.h
 *  @brief  Contains Entity Actions for moving entity with this component in scene.
 */

#ifndef incl_EC_Movable_EC_Movable_h
#define incl_EC_Movable_EC_Movable_h

#include "ComponentInterface.h"
#include "Declare_EC.h"
#include "Vector3D.h"

class Quaternion;

namespace ProtocolUtilities
{
    class WorldStream;
    typedef boost::shared_ptr<WorldStream> WorldStreamPtr;
}

/// Contains Entity Actions for moving entity with this component in scene.
/**
        Currently this EC is pretty much for testing and debugging purposes only.
        Supports the following actions:
        -Move(Forward)
        -Move(Backward)
        -Move(Left)
        -Move(Right)
        -Rotate(Left)
        -Rotate(Right)
*/
class EC_Movable : public Foundation::ComponentInterface
{
    DECLARE_EC(EC_Movable);
    Q_OBJECT

public:
    /// Destructor.
    ~EC_Movable();

    void SetWorldStreamPtr(ProtocolUtilities::WorldStreamPtr worldStream);

public slots:
    /** Moves the entity this component is attached to.
        @param direction Forward, Backward, Left or Right
    */
    void Move(const QString &direction);

    /** Rotates the entity this component is attached to.
        @param direction Left or Right
    */
    void Rotate(const QString &direction);

private:
    /** Constructor.
        @param module Declaring module.
     */
    explicit EC_Movable(Foundation::ModuleInterface *module);

    void SendMultipleObjectUpdatePacket(const Vector3df &deltaPos, const Quaternion &deltaOri);

    ProtocolUtilities::WorldStreamPtr worldStream_;

private slots:
    /// Registers the action this EC provides to the parent entity, when it's set.
    void RegisterActions();
};

#endif
