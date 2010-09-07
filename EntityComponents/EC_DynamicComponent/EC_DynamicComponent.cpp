// For conditions of distribution and use, see copyright notice in license.txt

#include "StableHeaders.h"
#include "EC_DynamicComponent.h"
#include "ModuleInterface.h"
#include "ModuleManager.h"
#include "Entity.h"
#include "LoggingFunctions.h"

DEFINE_POCO_LOGGING_FUNCTIONS("EC_DynamicComponent")

#include <QDomDocument>

namespace
{
    //! Function that is used by std::sort algorithm to sort attributes by their name.
    bool CmpAttributeByName(const Foundation::AttributeInterface *a, const Foundation::AttributeInterface *b)
    {
        return a->GetNameString() < b->GetNameString();
    }

    //! Function that is used by std::sort algorithm to sort DeserializeData by their name.
    bool CmpAttributeDataByName(const EC_DynamicComponent::DeserializeData &a, const EC_DynamicComponent::DeserializeData &b)
    {
        return a.name_ < b.name_;
    }
}

EC_DynamicComponent::EC_DynamicComponent(Foundation::ModuleInterface *module):
    Foundation::ComponentInterface(module->GetFramework())
{
}

EC_DynamicComponent::~EC_DynamicComponent()
{
}

void EC_DynamicComponent::SerializeTo(QDomDocument& doc, QDomElement& base_element) const
{
    QDomElement comp_element = BeginSerialization(doc, base_element);

    Foundation::AttributeVector::const_iterator iter = attributes_.begin();
    while(iter != attributes_.end())
    {
        WriteAttribute(doc, comp_element, (*iter)->GetNameString().c_str(), (*iter)->ToString().c_str(), (*iter)->TypenameToString().c_str());
        iter++;
    }
}

void EC_DynamicComponent::DeserializeFrom(QDomElement& element, AttributeChange::Type change)
{
    if (!BeginDeserialization(element))
        return;

    std::vector<DeserializeData> deserializedAttributes;
    QDomElement child = element.firstChildElement("attribute");
    while(!child.isNull())
    {
        QString name = child.attribute("name");
        QString type = child.attribute("type");
        QString value = child.attribute("value");
        DeserializeData attributeData(name.toStdString(), type.toStdString(), value.toStdString());
        deserializedAttributes.push_back(attributeData);

        child = child.nextSiblingElement("attribute");
    }

    // Sort both lists in alphabetical order.
    Foundation::AttributeVector oldAttributes = attributes_;
    std::stable_sort(oldAttributes.begin(), oldAttributes.end(), &CmpAttributeByName);
    std::stable_sort(deserializedAttributes.begin(), deserializedAttributes.end(), &CmpAttributeDataByName);

    std::vector<DeserializeData> addAttributes;
    std::vector<DeserializeData> remAttributes;
    Foundation::AttributeVector::iterator iter1 = oldAttributes.begin();
    std::vector<DeserializeData>::iterator iter2 = deserializedAttributes.begin();
    // Check what attributes we need to add or remove from the dynamic component (done by comparing two list differences).
    while(iter1 != oldAttributes.end() || iter2 != deserializedAttributes.end())
    {
        // No point to continue the iteration if other list is empty. We can just push all new attributes into the dynamic component.
        if(iter1 == oldAttributes.end())
        {
            for(;iter2 != deserializedAttributes.end(); iter2++)
            {
                addAttributes.push_back(*iter2);
            }
            break;
        }
        // Only old attributes are left and they can be removed from the dynamic component.
        else if(iter2 == deserializedAttributes.end())
        {
            for(;iter1 != oldAttributes.end(); iter1++)
                remAttributes.push_back(DeserializeData((*iter1)->GetNameString().c_str()));
            break;
        }

        // Attribute has already created and we only need to update it's value.
        if((*iter1)->GetNameString() == (*iter2).name_)
        {
            SetAttribute(QString::fromStdString(iter2->name_), QString::fromStdString(iter2->value_), AttributeChange::Local);
            iter2++;
            iter1++;
        }
        // Found a new attribute that need to be created and added to the component.
        else if((*iter1)->GetNameString() > (*iter2).name_)
        {
            addAttributes.push_back(*iter2);
            iter2++;
        }
        // Couldn't find the attribute in a new list so it need to be removed from the component.
        else
        {
            remAttributes.push_back(DeserializeData((*iter1)->GetNameString().c_str()));
            iter1++;
        }
    }

    while(!addAttributes.empty())
    {
        DeserializeData attributeData = addAttributes.back();
        Foundation::AttributeInterface *attribute = CreateAttribute(attributeData.type_.c_str(), attributeData.name_.c_str());
        if (attribute)
            attribute->FromString(attributeData.value_, AttributeChange::Local);
        addAttributes.pop_back();
    }
    while(!remAttributes.empty())
    {
        DeserializeData attributeData = remAttributes.back();
        RemoveAttribute(QString::fromStdString(attributeData.name_));
        remAttributes.pop_back();
    }
}

Foundation::AttributeInterface *EC_DynamicComponent::CreateAttribute(const QString &typeName, const QString &name)
{
    Foundation::AttributeInterface *attribute = 0;
    if(ContainAttribute(name))
        return attribute;
    attribute = framework_->GetComponentManager()->CreateAttribute(this, typeName.toStdString(), name.toStdString());
    if(attribute)
        emit AttributeAdded(name);
    return attribute;
}

void EC_DynamicComponent::RemoveAttribute(const QString &name)
{
    Foundation::AttributeVector::iterator iter = attributes_.begin();
    while(iter != attributes_.end())
    {
        if((*iter)->GetName() == name)
        {
            SAFE_DELETE(*iter);
            attributes_.erase(iter);
            emit AttributeRemoved(name);
            break;
        }
        iter++;
    }
}

void EC_DynamicComponent::ComponentChanged(const QString &changeType)
{
    if(changeType == "Local")
        Foundation::ComponentInterface::ComponentChanged(AttributeChange::Local);
    else if(changeType == "LocalOnly")
        Foundation::ComponentInterface::ComponentChanged(AttributeChange::LocalOnly);
    else if(changeType == "Network")
        Foundation::ComponentInterface::ComponentChanged(AttributeChange::Network);
    else
        LogWarning("Cannot emit ComponentChanged event cause \"" + changeType.toStdString() + "\" changeType is not supported.");
}

void EC_DynamicComponent::AddQVariantAttribute(const QString &name)
{
    //Check if the attribute has already been created.
    if(!ContainAttribute(name))
    {
        Foundation::Attribute<QVariant> *attribute = new Foundation::Attribute<QVariant>(this, name.toStdString().c_str());
        emit AttributeAdded(name);
    }
}

QVariant EC_DynamicComponent::GetAttribute(int index) const
{
    QVariant value;
    if(index < attributes_.size())
    {
        /*Foundation::AttributeVector::const_iterator iter = attributes_.begin();
        for(uint i = 0; i < index; i++)
            iter++;*/

        Foundation::AttributeInterface *attribute = attributes_[index];//(*iter);
        if(!attribute)
            return value;
        Foundation::Attribute<QVariant> *variantAttribute = dynamic_cast<Foundation::Attribute<QVariant>*>(attribute);
        //In case the attribute is not QVariant type then return QVariant value as string.
        if(!variantAttribute)
        {
            value = QVariant(attribute->ToString().c_str());
            return value;
        }
        value = variantAttribute->Get();
    }
    return value;
}

QVariant EC_DynamicComponent::GetAttribute(const QString &name) const
{
    QVariant value;
    Foundation::AttributeInterface *attribute = 0;
    Foundation::AttributeVector::const_iterator iter = attributes_.begin();
    while(iter != attributes_.end())
    {
        Foundation::AttributeInterface *attributeInterface = *iter;
        std::string attrName = attributeInterface->GetNameString();
        if(attrName == name.toStdString())
        {
            attribute = attributeInterface;
            break;
        }
        iter++;
    }

    if(!attribute)
        return value;
    Foundation::Attribute<QVariant> *variantAttribute = dynamic_cast<Foundation::Attribute<QVariant>*>(attribute);
    //In case the attribute is not QVariant type then return QVariant value as string.
    if(!variantAttribute)
    {
        //except if it's a bool, make a qvariant of that .. as a test
        if(attribute->TypenameToString() == "bool")
        {
            Foundation::Attribute<bool> *boolAttribute = dynamic_cast<Foundation::Attribute<bool>*>(attribute);
            value = QVariant(boolAttribute->Get());
            return value;
        }
        //do this trick for real/floats too
        else if(attribute->TypenameToString() == "real")
        {
            Foundation::Attribute<Real> *realAttribute = dynamic_cast<Foundation::Attribute<Real>*>(attribute);
            value = QVariant(realAttribute->Get());
            return value;
        }
        else
        {
            value = QVariant(attribute->ToString().c_str());
            return value;
        }
    }

    return variantAttribute->Get();
}

void EC_DynamicComponent::SetAttribute(int index, const QVariant &value, AttributeChange::Type change)
{
    if(index < attributes_.size())
    {
        Foundation::AttributeVector::const_iterator iter = attributes_.begin();
        for(uint i = 0; i < index; i++)
            iter++;

        Foundation::AttributeInterface *attribute = (*iter);
        if(!attribute)
            return;
        //Check if attribute is Attribute<QVariant> type and if not use attribute interface's FromString method to convert it to right format.
        Foundation::Attribute<QVariant> *variantAttribute = dynamic_cast<Foundation::Attribute<QVariant>*>(attribute);
        if(!variantAttribute)
        {
            //except if it's a bool, get the value from the qvariant .. as a test
            //! @todo commeted out cause lines below causes null pointer crash
            /*Foundation::Attribute<bool> *boolAttribute = dynamic_cast<Foundation::Attribute<bool>*>(attribute);
            if(boolAttribute)
            {
                if (value.type() == QVariant::Bool)
                {
                    variantAttribute->Set(value, change);
                }
            }
            //and for real/floats..
            Foundation::Attribute<Real> *realAttribute = dynamic_cast<Foundation::Attribute<Real>*>(attribute);
            if(realAttribute)
            {
                if (value.type() == QVariant::Double)
                {
                    variantAttribute->Set(value, change);
                }
            }

            else
            {
                attribute->FromString(value.toString().toStdString(), change);
            }*/
            attribute->FromString(value.toString().toStdString(), change);
            return;
        }

        variantAttribute->Set(value, change);
        //Foundation::ComponentInterface::ComponentChanged(change);
    }
}

void EC_DynamicComponent::SetAttribute(const QString &name, const QVariant &value, AttributeChange::Type change)
{
    Foundation::AttributeInterface *attribute = 0;
    Foundation::AttributeVector::const_iterator iter = attributes_.begin();
    while(iter != attributes_.end())
    {
        Foundation::AttributeInterface *attributeInterface = *iter;
        std::string attrName = attributeInterface->GetNameString();
        if(attrName == name.toStdString())
        {
            attribute = attributeInterface;
            break;
        }
        iter++;
    }

    if(!attribute)
        return;
    //Check if attribute is Attribute<QVariant> type and if not use attribute interface's FromString method to convert it to right format.
    Foundation::Attribute<QVariant> *variantAttribute = dynamic_cast<Foundation::Attribute<QVariant>*>(attribute);
    if(!variantAttribute)
    {
        attribute->FromString(value.toString().toStdString(), change);
        return;
    }
    variantAttribute->Set(value, change);
    //Foundation::ComponentInterface::ComponentChanged(change);
}

QString EC_DynamicComponent::GetAttributeName(int index) const
{
    if(index < attributes_.size())
    {
        /*Foundation::AttributeVector::const_iterator iter = attributes_.begin();
        for(uint i = 0; i < index; i++)
            iter++;
        return (*iter)->GetName();*/
        return attributes_[index]->GetName();
    }
    return QString();
}

uint EC_DynamicComponent::GetParentEntityId() const
{
    return GetParentEntity()->GetId();
}

bool EC_DynamicComponent::ContainSameAttributes(const EC_DynamicComponent &comp) const
{
    Foundation::AttributeVector myAttributeVector = GetAttributes();
    Foundation::AttributeVector attributeVector = comp.GetAttributes();
    if(attributeVector.size() != myAttributeVector.size())
        return false;
    if(attributeVector.empty() && myAttributeVector.empty())
        return true;

    std::sort(myAttributeVector.begin(), myAttributeVector.end(), &CmpAttributeByName);
    std::sort(attributeVector.begin(), attributeVector.end(), &CmpAttributeByName);

    Foundation::AttributeVector::const_iterator iter1 = myAttributeVector.begin();
    Foundation::AttributeVector::const_iterator iter2 = attributeVector.begin();
    while(iter1 != myAttributeVector.end() && iter2 != attributeVector.end())
    {
        // Compare attribute names and type and if they mach continue iteration if not components aren't exatly the same.
        if((*iter1)->GetNameString() == (*iter2)->GetNameString() &&
           (*iter1)->TypenameToString() == (*iter2)->TypenameToString())
        {
            if(iter1 != myAttributeVector.end())
                iter1++;
            if(iter2 != attributeVector.end())
                iter2++;
        }
        else
        {
            return false;
        }
    }
    return true;

    /*// Get both attributes and check if they are holding exact number of attributes.
    Foundation::AttributeVector myAttributeVector = GetAttributes();
    Foundation::AttributeVector attributeVector = comp.GetAttributes();
    if(attributeVector.size() != myAttributeVector.size())
        return false;
    
    // Compare that every attribute is same in both components.
    QSet<Foundation::AttributeInterface*> myAttributeSet;
    QSet<Foundation::AttributeInterface*> attributeSet;
    for(uint i = 0; i < myAttributeSet.size(); i++)
    {
        attributeSet.insert(myAttributeVector[i]);
        myAttributeSet.insert(attributeVector[i]);
    }
    if(attributeSet != myAttributeSet)
        return false;
    return true;*/
}

bool EC_DynamicComponent::ContainAttribute(const QString &name) const
{
    Foundation::AttributeVector::const_iterator iter = attributes_.begin();
    while(iter != attributes_.end())
    {
        if((*iter)->GetName() == name.toStdString())
        {
            return true;
        }
        iter++;
    }
    return false;
}
