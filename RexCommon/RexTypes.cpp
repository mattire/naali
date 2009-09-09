#include "RexTypes.h"
#include "RexUUID.h"

namespace RexTypes
{

asset_type_t GetAssetTypeFromTypeName(const std::string& asset_type)
{
    if (asset_type == ASSETTYPENAME_TEXTURE)
        return RexAT_Texture;
    if (asset_type == ASSETTYPENAME_MESH)
        return RexAT_Mesh;
    if (asset_type == ASSETTYPENAME_SKELETON)
        return RexAT_Skeleton;
    if (asset_type == ASSETTYPENAME_PARTICLE_SCRIPT)
        return RexAT_ParticleScript;
    if (asset_type == ASSETTYPENAME_MATERIAL_SCRIPT)
        return RexAT_MaterialScript;
    if (asset_type == ASSETTYPENAME_FLASH_ANIMATION)
        return RexAT_FlashAnimation;

    return -1;
}

const std::string& GetTypeNameFromAssetType(asset_type_t asset_type)
{
    if (asset_type == RexAT_Texture)
        return ASSETTYPENAME_TEXTURE;
    if (asset_type == RexAT_Mesh)
        return ASSETTYPENAME_MESH;
    if (asset_type == RexAT_Skeleton)
        return ASSETTYPENAME_SKELETON;
    if (asset_type == RexAT_ParticleScript)
        return ASSETTYPENAME_PARTICLE_SCRIPT;
    if (asset_type == RexAT_MaterialScript)
        return ASSETTYPENAME_MATERIAL_SCRIPT;
    
    return ASSETTYPENAME_UNKNOWN;
}

const std::string& GetInventoryTypeString(asset_type_t asset_type)
{
    if (asset_type == RexTypes::RexAT_Texture)
        return IT_TEXTURE;
    if (asset_type == RexTypes::RexAT_Mesh)
        return IT_MESH;
    if (asset_type == RexTypes::RexAT_Skeleton)
        return IT_SKELETON;
    if (asset_type == RexTypes::RexAT_MaterialScript)
        return IT_MATERIAL_SCRIPT;
    if (asset_type == RexTypes::RexAT_ParticleScript)
        return IT_PARTICLE_SCRIPT;
    if (asset_type == RexTypes::RexAT_FlashAnimation)
        return IT_FLASH_ANIMATION;

    return IT_UNKNOWN;
}

const std::string& GetAssetTypeString(asset_type_t asset_type)
{
    if (asset_type == RexTypes::RexAT_Texture)
        return AT_TEXTURE;
    if (asset_type == RexTypes::RexAT_Mesh)
        return AT_MESH;
    if (asset_type == RexTypes::RexAT_Skeleton)
        return AT_SKELETON;
    if (asset_type == RexTypes::RexAT_MaterialScript)
        return AT_MATERIAL_SCRIPT;
    if (asset_type == RexTypes::RexAT_ParticleScript)
        return AT_PARTICLE_SCRIPT;
    if (asset_type == RexTypes::RexAT_FlashAnimation)
        return AT_FLASH_ANIMATION;

    return AT_UNKNOWN;
}

const std::string& GetCategoryNameForAssetType(asset_type_t asset_type)
{
    if (asset_type == RexTypes::RexAT_Texture)
        return CATEGORY_TEXTURE;
    if (asset_type == RexTypes::RexAT_Mesh)
        return CATEGORY_MESH;
    if (asset_type == RexTypes::RexAT_Skeleton)
        return CATEGORY_SKELETON;
    if (asset_type == RexTypes::RexAT_MaterialScript)
        return CATEGORY_MATERIAL_SCRIPT;
    if (asset_type == RexTypes::RexAT_ParticleScript)
        return CATEGORY_PARTICLE_SCRIPT;
    if (asset_type == RexTypes::RexAT_FlashAnimation)
        return CATEGORY_FLASH_ANIMATION;

    return CATEGORY_UNKNOWN;
}

const std::string& GetOpenFileNameFilter(asset_type_t asset_type)
{
    if (asset_type == RexTypes::RexAT_Texture)
        return IMAGE_FILTER;
    if (asset_type == RexTypes::RexAT_Mesh)
        return MESH_FILTER;
    if (asset_type == RexTypes::RexAT_Skeleton)
        return MESHANIMATION_FILTER;
    if (asset_type == RexTypes::RexAT_MaterialScript)
        return MATERIALSCRIPT_FILTER;
    if (asset_type == RexTypes::RexAT_ParticleScript)
        return PARTICLE_FILTER;
    if (asset_type == RexTypes::RexAT_FlashAnimation)
        return FLASHANIMATION_FILTER;
        
    return ALLFILES_FILTER;
}

asset_type_t GetAssetTypeFromFilename(const std::string &filename)
{
    size_t pos = filename.find_last_of('.');
    std::string file_ext = filename.substr(pos + 1);

    if (file_ext == "tga" ||
        file_ext == "bmp" ||
        file_ext == "jpg" ||
        file_ext == "jpeg" ||
        file_ext == "png")
        return RexAT_Texture;
    if (file_ext == "mesh")
        return RexAT_Mesh;
    if (file_ext == "skeleton")
        return RexAT_Skeleton;
    if (file_ext == "particle")
        return RexAT_ParticleScript;
    if (file_ext == "material")
        return RexAT_MaterialScript;
    if (file_ext == "swf")
        return RexAT_FlashAnimation;

    return -1;
}

bool IsNull(RexAssetID id)
{
    if (id.size() == 0)
        return true;

    if (RexUUID::IsValid(id) && RexUUID(id).IsNull())
        return true;

    return false;
}

}
