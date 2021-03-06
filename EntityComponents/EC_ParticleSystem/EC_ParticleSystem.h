// For conditions of distribution and use, see copyright notice in license.txt

#ifndef incl_EC_ParticleSystem_EC_ParticleSystem_h
#define incl_EC_ParticleSystem_EC_ParticleSystem_h

#include "ComponentInterface.h"
#include "AttributeInterface.h"
#include "ResourceInterface.h"
#include "Declare_EC.h"

namespace Ogre
{
    class ParticleSystem;
    class SceneNode;
}

namespace OgreRenderer
{
    class Renderer;
}

class EC_ParticleSystem : public Foundation::ComponentInterface
{
    DECLARE_EC(EC_ParticleSystem);
    Q_OBJECT
public:
    ~EC_ParticleSystem();

    virtual bool IsSerializable() const { return true; }

    bool HandleResourceEvent(event_id_t event_id, Foundation::EventDataInterface* data);

    bool HandleEvent(event_category_id_t category_id, event_id_t event_id, Foundation::EventDataInterface* data);

    Attribute<QString> particleId_;
    Attribute<bool> castShadows_;

    Attribute<float> renderingDistance_;

public slots:
    //! Create a new particle system. System name will be same as component name.
    /*! \return true if successful
    */
    void CreateParticleSystem(const QString &systemName);
    void DeleteParticleSystem();

private slots:
    void UpdateSignals();
    void AttributeUpdated(Foundation::ComponentInterface *component, AttributeInterface *attribute);

private:
    explicit EC_ParticleSystem(Foundation::ModuleInterface *module);
    Foundation::ComponentPtr FindPlaceable() const;
    request_tag_t RequestResource(const std::string& id, const std::string& type);

    boost::weak_ptr<OgreRenderer::Renderer> renderer_;
    Ogre::ParticleSystem* particleSystem_;
    Ogre::SceneNode* node_;
    request_tag_t particle_tag_;
    event_category_id_t resource_event_category_;
};

#endif