/**
 *  For conditions of distribution and use, see copyright notice in license.txt
 *
 *  @file   ComponentInterface.cpp
 *  @brief  Base class for all components. Inherit from this class when creating new components.
 */

#include "StableHeaders.h"

#include "ComponentInterface.h"
#include "AttributeInterface.h"

#include "Framework.h"
#include "Entity.h"
#include "SceneManager.h"
#include "EventManager.h"
#include <QDomDocument>

namespace Foundation
{

ComponentInterface::ComponentInterface(Framework* framework) :
    parent_entity_(0),
    framework_(framework),
    change_(AttributeChange::None)
{
}

ComponentInterface::ComponentInterface(const ComponentInterface &rhs) :
    framework_(rhs.framework_),
    parent_entity_(rhs.parent_entity_),
    change_(AttributeChange::None)
{
}

ComponentInterface::~ComponentInterface()
{
    // Removes itself from EventManager 
    if (framework_ != 0)
    {
        boost::shared_ptr<EventManager> event_manager_ = framework_->GetEventManager();
        if (event_manager_ != 0)
            event_manager_->UnregisterEventSubscriber(this);
    }
}

void ComponentInterface::SetName(const QString& name)
{
    // no point to send a signal if name have stayed same as before.
    if(name_ == name)
        return;

    name_ = name;
    emit OnComponentNameChanged(name.toStdString());
}

void ComponentInterface::SetParentEntity(Scene::Entity* entity)
{
    parent_entity_ = entity;
    if (parent_entity_)
        emit ParentEntitySet();
    else
        emit ParentEntityDetached();
}

Scene::Entity* ComponentInterface::GetParentEntity() const
{
    return parent_entity_;
}

AttributeInterface* ComponentInterface::GetAttribute(const std::string &name) const
{
    for(unsigned int i = 0; i < attributes_.size(); ++i)
        if(attributes_[i]->GetNameString() == name)
            return attributes_[i];
    return 0;
}

QDomElement ComponentInterface::BeginSerialization(QDomDocument& doc, QDomElement& base_element) const
{
    QDomElement comp_element = doc.createElement("component");
    comp_element.setAttribute("type", TypeName());
    if (!name_.isEmpty())
        comp_element.setAttribute("name", name_);
    
    if (!base_element.isNull())
        base_element.appendChild(comp_element);
    else
        doc.appendChild(comp_element);
    
    return comp_element;
}

void ComponentInterface::WriteAttribute(QDomDocument& doc, QDomElement& comp_element, const QString& name, const QString& value) const
{
    QDomElement attribute_element = doc.createElement("attribute");
    attribute_element.setAttribute("name", name);
    attribute_element.setAttribute("value", value);
    comp_element.appendChild(attribute_element);
}

void ComponentInterface::WriteAttribute(QDomDocument& doc, QDomElement& comp_element, const QString& name, const QString& value, const QString &type) const
{
    QDomElement attribute_element = doc.createElement("attribute");
    attribute_element.setAttribute("name", name);
    attribute_element.setAttribute("value", value);
    attribute_element.setAttribute("type", type);
    comp_element.appendChild(attribute_element);
}

bool ComponentInterface::BeginDeserialization(QDomElement& comp_element)
{
    QString type = comp_element.attribute("type");
    if (type == TypeName())
    {
        SetName(comp_element.attribute("name"));
        return true;
    }
    return false;
}

QString ComponentInterface::ReadAttribute(QDomElement& comp_element, const QString &name) const
{
    QDomElement attribute_element = comp_element.firstChildElement("attribute");
    while (!attribute_element.isNull())
    {
        if (attribute_element.attribute("name") == name)
            return attribute_element.attribute("value");
        
        attribute_element = attribute_element.nextSiblingElement("attribute");
    }
    
    return QString();
}

QString ComponentInterface::ReadAttributeType(QDomElement& comp_element, const QString &name) const
{
    QDomElement attribute_element = comp_element.firstChildElement("attribute");
    while (!attribute_element.isNull())
    {
        if (attribute_element.attribute("name") == name)
            return attribute_element.attribute("type");
        
        attribute_element = attribute_element.nextSiblingElement("attribute");
    }
    
    return QString();
}

void ComponentInterface::ComponentChanged(AttributeChange::Type change)
{
    change_ = change;
    
    if (parent_entity_)
    {
        Scene::SceneManager* scene = parent_entity_->GetScene();
        if (scene)
            scene->EmitComponentChanged(this, change);
    }
    
    // Trigger also internal change
    emit OnChanged();
}

void ComponentInterface::AttributeChanged(Foundation::AttributeInterface* attribute, AttributeChange::Type change)
{
    // Trigger scenemanager signal
    if (parent_entity_)
    {
        Scene::SceneManager* scene = parent_entity_->GetScene();
        if (scene)
            scene->EmitAttributeChanged(this, attribute, change);
    }
    
    // Trigger internal signal
    emit OnAttributeChanged(attribute, change);
}

void ComponentInterface::ResetChange()
{
    for (uint i = 0; i < attributes_.size(); ++i)
        attributes_[i]->ResetChange();
    
    change_ = AttributeChange::None;
}

void ComponentInterface::SerializeTo(QDomDocument& doc, QDomElement& base_element) const
{
    if (!IsSerializable())
        return;

    QDomElement comp_element = BeginSerialization(doc, base_element);

    for (uint i = 0; i < attributes_.size(); ++i)
        WriteAttribute(doc, comp_element, attributes_[i]->GetNameString().c_str(), attributes_[i]->ToString().c_str());
}

void ComponentInterface::DeserializeFrom(QDomElement& element, AttributeChange::Type change)
{
    if (!IsSerializable())
        return;

    if (!BeginDeserialization(element))
        return;

    for (uint i = 0; i < attributes_.size(); ++i)
    {
        std::string attr_str = ReadAttribute(element, attributes_[i]->GetNameString().c_str()).toStdString();
        attributes_[i]->FromString(attr_str, change);
    }
}

}
