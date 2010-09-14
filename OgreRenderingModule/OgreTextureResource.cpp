// For conditions of distribution and use, see copyright notice in license.txt

#include "StableHeaders.h"
#include "OgreTextureResource.h"
#include "OgreRenderingModule.h"

#include <Ogre.h>

namespace OgreRenderer
{
    OgreTextureResource::OgreTextureResource(const std::string& id, TextureQuality texturequality) : 
        ResourceInterface(id),
        texturequality_(texturequality),
        level_(-1)
    {
    }

    OgreTextureResource::OgreTextureResource(const std::string& id, TextureQuality texturequality, Foundation::TexturePtr source) : 
        ResourceInterface(id),
        texturequality_(texturequality),
        level_(-1)
    {
        SetData(source);
    }

    OgreTextureResource::~OgreTextureResource()
    {
        if (!ogre_texture_.isNull())
        {
            std::string tex_name = ogre_texture_->getName();
            ogre_texture_.setNull(); 
            
            try
            {
                Ogre::TextureManager::getSingleton().remove(tex_name);
            }
            catch (...) {}
        }
    }
    
    bool OgreTextureResource::SetDataFromImage(Foundation::AssetPtr source)
    {
        if (!source)
        {
            OgreRenderingModule::LogError("Null source image data pointer");
            return false;
        }
        if (!source->GetSize())
        {
            OgreRenderingModule::LogError("Zero sized image asset");
            return false;
        }
        try
        {
            RemoveTexture();

            Ogre::DataStreamPtr stream(new Ogre::MemoryDataStream((void*)source->GetData(), source->GetSize(), false));
            Ogre::Image image;
            image.load(stream);
            if (texturequality_ == Texture_Low)
                image.resize(image.getWidth() / 2, image.getHeight() / 2);
            ogre_texture_ = Ogre::TextureManager::getSingleton().loadImage(id_, Ogre::ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME, image);
            
        }
        catch (Ogre::Exception &e)
        {
            OgreRenderingModule::LogError("Failed to create texture " + id_ + ": " + std::string(e.what()));
            RemoveTexture();
            return false;
        }

        OgreRenderingModule::LogDebug("Ogre texture " + id_ + " created");
        level_ = 0;
        return true;
    }
    
    bool OgreTextureResource::SetData(Foundation::TexturePtr source)
    {
        if (!source)
        {
            OgreRenderingModule::LogError("Null source texture data pointer");
            return false;
        }
        if ((!source->GetWidth()) || (!source->GetHeight()))
        {
            OgreRenderingModule::LogError("Texture with zero dimension(s)");
            return false;
        }
            
        Ogre::PixelFormat pixel_format;
        if (source->GetFormat() != -1)
        {
            pixel_format = (Ogre::PixelFormat)source->GetFormat();
        }
        else
        {
            switch (source->GetComponents())
            {
            case 1:
                pixel_format = Ogre::PF_L8;
                break;

            case 2:
                pixel_format = Ogre::PF_BYTE_LA;
                break;

            case 3:
                pixel_format = Ogre::PF_B8G8R8;
                break;

            case 4:
                pixel_format = Ogre::PF_A8B8G8R8;
                break;

            default:
                OgreRenderingModule::LogError("Illegal number of components in texture: " + ToString<uint>(source->GetComponents()));
                return false; 
            }
        }

        try
        {
            if (ogre_texture_.isNull())
            {   
                ogre_texture_ = Ogre::TextureManager::getSingleton().createManual(
                    id_, Ogre::ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME, Ogre::TEX_TYPE_2D,
                    source->GetWidth(), source->GetHeight(), Ogre::MIP_DEFAULT, pixel_format, Ogre::TU_DEFAULT); 

                if (ogre_texture_.isNull())
                {
                    OgreRenderingModule::LogError("Failed to create texture " + id_);
                    return false; 
                }   
            }
            else
            {
                // See if size/format changed, have to delete/recreate internal resources
                if ((source->GetWidth() != ogre_texture_->getWidth()) ||
                    (source->GetHeight() != ogre_texture_->getHeight()) ||
                    (pixel_format != ogre_texture_->getFormat()))
                {
                    ogre_texture_->freeInternalResources();
                    ogre_texture_->setWidth(source->GetWidth());
                    ogre_texture_->setHeight(source->GetHeight());
                    ogre_texture_->setFormat(pixel_format);
                    ogre_texture_->createInternalResources();
                }
            }

            // For the highest level texture, reduce size in low quality mode
            if ((!source->GetLevel()) && (texturequality_ == Texture_Low))
            {
                Ogre::Image tempImage;
                Ogre::DataStreamPtr stream(new Ogre::MemoryDataStream((void*)source->GetData(), source->GetDataSize(), false));
                tempImage.loadRawData(stream, source->GetWidth(), source->GetHeight(), 1, pixel_format);
                tempImage.resize(source->GetWidth() / 2, source->GetHeight() / 2);
                if (!ogre_texture_->getBuffer().isNull())
                    ogre_texture_->getBuffer()->blitFromMemory(tempImage.getPixelBox());
            }
            else
            {
                Ogre::Box dimensions(0,0, source->GetWidth(), source->GetHeight());
                Ogre::PixelBox pixel_box(dimensions, pixel_format, (void*)source->GetData());
                if (!ogre_texture_->getBuffer().isNull())
                    ogre_texture_->getBuffer()->blitFromMemory(pixel_box);
            }
        }
        catch (Ogre::Exception &e)
        {
            OgreRenderingModule::LogError("Failed to create texture " + id_ + ": " + std::string(e.what()));
            return false;
        }

        OgreRenderingModule::LogDebug("Ogre texture " + id_ + " updated");
        level_ = source->GetLevel();
        return true;
    }

    bool OgreTextureResource::HasAlpha() const
    {
        if (ogre_texture_.get())
        {
            // As far as the legacy materials are concerned, DXT1 is not alpha
            if (ogre_texture_->getFormat() == Ogre::PF_DXT1)
                return false;
            if (Ogre::PixelUtil::hasAlpha(ogre_texture_->getFormat()))
                return true;
        }
        
        return false;
    }

    static const std::string type_name("OgreTexture");
        
    const std::string& OgreTextureResource::GetType() const
    {
        return type_name;
    }
    
    const std::string& OgreTextureResource::GetTypeStatic()
    {
        return type_name;
    }    
    
    bool OgreTextureResource::IsValid() const
    {
        return (!ogre_texture_.isNull());
    }
    
    void OgreTextureResource::RemoveTexture()
    {
        if (!ogre_texture_.isNull())
        {
            std::string tex_name = ogre_texture_->getName();
            ogre_texture_.setNull(); 
            
            try
            {
                Ogre::TextureManager::getSingleton().remove(tex_name);
            }
            catch (...) {}
        }
    }
}