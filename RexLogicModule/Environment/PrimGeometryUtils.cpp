// For conditions of distribution and use, see copyright notice in license.txt
// Based on Mesmerizer.cs in libopenmetaverse

/*
 * Copyright (c) 2008, openmetaverse.org
 * All rights reserved.
 *
 * - Redistribution and use in source and binary forms, with or without 
 *   modification, are permitted provided that the following conditions are met:
 *
 * - Redistributions of source code must retain the above copyright notice, this
 *   list of conditions and the following disclaimer.
 * - Neither the name of the openmetaverse.org nor the names 
 *   of its contributors may be used to endorse or promote products derived from
 *   this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" 
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE 
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE 
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE 
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR 
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF 
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS 
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN 
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) 
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE 
 * POSSIBILITY OF SUCH DAMAGE.
 * 
 * 
 * This code comes from the OpenSim project. Meshmerizer is written by dahlia
 * <dahliatrimble@gmail.com>
 */

#include "StableHeaders.h"
#include "Environment/PrimGeometryUtils.h"
#include "Environment/PrimMesher.h"
#include "RexTypes.h"
#include "RexLogicModule.h"
#include "OgreMaterialUtils.h"
#include "OgreMaterialResource.h"
#include "Renderer.h"
#include "ServiceManager.h"
#include "CoreException.h"
#include "EC_OpenSimPrim.h"

#ifndef unix
#include <float.h>
#else
#include "CoreTypes.h"
#endif

#include <Ogre.h>

namespace RexLogic
{
    static Ogre::ManualObject* prim_manual_object = 0;
    
    void TransformUV(Ogre::Vector2& uv, float repeat_u, float repeat_v, float offset_u, float offset_v, float rot_sin, float rot_cos)
    {
        const static Ogre::Vector2 half(0.5f, 0.5f);
        
        Ogre::Vector2 centered = uv - half;

        uv.x = centered.y * rot_sin + centered.x * rot_cos;
        uv.y = -centered.x * rot_sin + centered.y * rot_cos; 
        
        uv.x *= repeat_u;
        uv.y *= repeat_v;
        
        uv.x += offset_u;
        uv.y -= offset_v;
        
        uv += half;
    }

    bool CheckCoord(const PrimMesher::Coord& pos)
    {
        if (_isnan(pos.X) || _isnan(pos.Y) || _isnan(pos.Z))
            return false;
        if (!_finite(pos.X) || !_finite(pos.Y) || !_finite(pos.Z))
            return false;
        
        return true;
    }

    Ogre::ManualObject* CreatePrimGeometry(Foundation::Framework* framework, EC_OpenSimPrim& primitive, bool optimisations_enabled)
    {
        PROFILE(Primitive_CreateGeometry)
        
        if (!primitive.HasPrimShapeData)
            return 0;
        
        // Create only a single manual object for prim geometry and reuse it over and over, to avoid Ogre generating
        // a huge load of unnecessary D3D resources, that are never used for anything visible (the manual object will
        // be converted to a mesh anyway)
        if (!prim_manual_object)
        {
            OgreRenderer::RendererPtr renderer = framework->GetServiceManager()->GetService<OgreRenderer::Renderer>(Foundation::Service::ST_Renderer).lock();
            if (!renderer)
                return 0;
            Ogre::SceneManager *sceneMgr = renderer->GetSceneManager();
            prim_manual_object = sceneMgr->createManualObject(renderer->GetUniqueObjectName());
            if (!prim_manual_object)
                return 0;
        }
        
        std::string mat_override;
        if ((primitive.Materials[0].Type == RexTypes::RexAT_MaterialScript) && (!RexTypes::IsNull(primitive.Materials[0].asset_id)))
        {
            mat_override = primitive.Materials[0].asset_id;

            // If cannot find the override material, use default
            // We will probably get resource ready event later for the material & redo this prim
            boost::shared_ptr<OgreRenderer::Renderer> renderer = framework->GetServiceManager()->
                GetService<OgreRenderer::Renderer>(Foundation::Service::ST_Renderer).lock();
            if (!renderer->GetResource(mat_override, OgreRenderer::OgreMaterialResource::GetTypeStatic()))
            {
                mat_override = "LitTextured";
            }
        }
            
        try
        {
            float profileBegin = primitive.ProfileBegin.Get();
            float profileEnd = 1.0f - primitive.ProfileEnd.Get();
            float profileHollow = primitive.ProfileHollow.Get();

            int sides = 4;
            if ((primitive.ProfileCurve.Get() & 0x07) == RexTypes::SHAPE_EQUILATERAL_TRIANGLE)
                sides = 3;
            else if ((primitive.ProfileCurve.Get() & 0x07) == RexTypes::SHAPE_CIRCLE)
                // Reduced prim lod!!!
                sides = 12;
                //sides = 24;
            else if ((primitive.ProfileCurve.Get() & 0x07) == RexTypes::SHAPE_HALF_CIRCLE)
            {
                // half circle, prim is a sphere
                // Reduced prim lod!!!
                sides = 12;
                //sides = 24;

                profileBegin = 0.5f * profileBegin + 0.5f;
                profileEnd = 0.5f * profileEnd + 0.5f;
            }

            int hollowSides = sides;
            if ((primitive.ProfileCurve.Get() & 0xf0) == RexTypes::HOLLOW_CIRCLE)
                // Reduced prim lod!!!
                hollowSides = 12;
                //hollowSides = 24;
            else if ((primitive.ProfileCurve.Get() & 0xf0) == RexTypes::HOLLOW_SQUARE)
                hollowSides = 4;
            else if ((primitive.ProfileCurve.Get() & 0xf0) == RexTypes::HOLLOW_TRIANGLE)
                hollowSides = 3;
            
            PrimMesher::PrimMesh primMesh(sides, profileBegin, profileEnd, profileHollow, hollowSides);
            primMesh.topShearX = primitive.PathShearX.Get();
            primMesh.topShearY = primitive.PathShearY.Get();
            primMesh.pathCutBegin = primitive.PathBegin.Get();
            primMesh.pathCutEnd = 1.0f - primitive.PathEnd.Get();

            if (primitive.PathCurve.Get() == RexTypes::EXTRUSION_STRAIGHT)
            {
                primMesh.twistBegin = primitive.PathTwistBegin.Get() * 180;
                primMesh.twistEnd = primitive.PathTwist.Get() * 180;
                primMesh.taperX = primitive.PathScaleX.Get() - 1.0f;
                primMesh.taperY = primitive.PathScaleY.Get() - 1.0f;
                primMesh.ExtrudeLinear();
            }
            else
            {
                primMesh.holeSizeX = (2.0f - primitive.PathScaleX.Get());
                primMesh.holeSizeY = (2.0f - primitive.PathScaleY.Get());
                primMesh.radius = primitive.PathRadiusOffset.Get();
                primMesh.revolutions = primitive.PathRevolutions.Get();
                primMesh.skew = primitive.PathSkew.Get();
                primMesh.twistBegin = primitive.PathTwistBegin.Get() * 360;
                primMesh.twistEnd = primitive.PathTwist.Get() * 360;
                primMesh.taperX = primitive.PathTaperX.Get();
                primMesh.taperY = primitive.PathTaperY.Get();
                primMesh.ExtrudeCircular();
            }
            
            PROFILE(Primitive_CreateManualObject)
            prim_manual_object->clear();
            prim_manual_object->setBoundingBox(Ogre::AxisAlignedBox());
            
            // Check for highly illegal coordinates in any of the faces
            for (int i = 0; i < primMesh.viewerFaces.size(); ++i)
            {
                if (!(CheckCoord(primMesh.viewerFaces[i].v1) && CheckCoord(primMesh.viewerFaces[i].v2) && CheckCoord(primMesh.viewerFaces[i].v3)))
                {
                    RexLogicModule::LogError("NaN or infinite number encountered in prim face coordinates. Skipping geometry creation.");
                    return 0;
                }
            }
            
            std::string mat_name;
            std::string prev_mat_name;
            
            uint indices = 0;
            bool first_face = true;
            
            for (int i = 0; i < primMesh.viewerFaces.size(); ++i)
            {
                int facenum = primMesh.viewerFaces[i].primFaceNumber;
                
                Color color = primitive.PrimDefaultColor;
                ColorMap::const_iterator c = primitive.PrimColors.find(facenum);
                if (c != primitive.PrimColors.end())
                    color = c->second;
                
                // Skip face if very transparent
                if (color.a <= 0.11f)
                    continue;
                
                if (!mat_override.empty())
                    mat_name = mat_override;
                else
                {
                    unsigned variation = OgreRenderer::LEGACYMAT_VERTEXCOL;
                    
                    // Check for transparency
                    if (color.a < 1.0f)
                        variation = OgreRenderer::LEGACYMAT_VERTEXCOLALPHA;
                    
                    // Check for fullbright
                    bool fullbright = (primitive.PrimDefaultMaterialType & RexTypes::MATERIALTYPE_FULLBRIGHT) != 0;
                    MaterialTypeMap::const_iterator mt = primitive.PrimMaterialTypes.find(facenum);
                    if (mt != primitive.PrimMaterialTypes.end())
                        fullbright = (mt->second & RexTypes::MATERIALTYPE_FULLBRIGHT) != 0;
                    if (fullbright)
                        variation |= OgreRenderer::LEGACYMAT_FULLBRIGHT;
                    
                    std::string suffix = OgreRenderer::GetMaterialSuffix(variation);
                    
                    // Try to find face's texture in texturemap, use default if not found
                    std::string texture_name = primitive.PrimDefaultTextureID;
                    TextureMap::const_iterator t = primitive.PrimTextures.find(facenum);
                    if (t != primitive.PrimTextures.end())
                        texture_name = t->second;
                    
                    mat_name = texture_name + suffix;
                    
                    // Create the material here if texture yet missing, the material will be updated later
                    OgreRenderer::GetOrCreateLegacyMaterial(texture_name, variation);
                }
                
                // Get texture mapping parameters
                float repeat_u = primitive.PrimDefaultRepeatU;
                float repeat_v = primitive.PrimDefaultRepeatV;
                float offset_u = primitive.PrimDefaultOffsetU;
                float offset_v = primitive.PrimDefaultOffsetV;
                float rot = primitive.PrimDefaultUVRotation;
                if (primitive.PrimRepeatU.find(facenum) != primitive.PrimRepeatU.end())
                    repeat_u = primitive.PrimRepeatU[facenum];
                if (primitive.PrimRepeatV.find(facenum) != primitive.PrimRepeatV.end())
                    repeat_v = primitive.PrimRepeatV[facenum];
                if (primitive.PrimOffsetU.find(facenum) != primitive.PrimOffsetU.end())
                    offset_u = primitive.PrimOffsetU[facenum];
                if (primitive.PrimOffsetV.find(facenum) != primitive.PrimOffsetV.end())
                    offset_v = primitive.PrimOffsetV[facenum];
                if (primitive.PrimUVRotation.find(facenum) != primitive.PrimUVRotation.end())
                    rot = primitive.PrimUVRotation[facenum];
                float rot_sin = sin(-rot);
                float rot_cos = cos(-rot);

                if (optimisations_enabled || primitive.DrawType == RexTypes::DRAWTYPE_MESH)
                {
                    if ((first_face) || (mat_name != prev_mat_name))
                    {
                        if (indices)
                            prim_manual_object->end();
                        indices = 0;
                        prim_manual_object->begin(mat_name, Ogre::RenderOperation::OT_TRIANGLE_LIST);
                        prev_mat_name = mat_name;
                        first_face = false;
                    }
                }
                else
                {
                    if (i % 2 == 0)
                    {
                        if (indices)
                            prim_manual_object->end();
                        indices = 0;
                        prim_manual_object->begin(mat_name, Ogre::RenderOperation::OT_TRIANGLE_LIST);
                    }
                }
                
                Ogre::Vector3 pos1(primMesh.viewerFaces[i].v1.X, primMesh.viewerFaces[i].v1.Y, primMesh.viewerFaces[i].v1.Z);
                Ogre::Vector3 pos2(primMesh.viewerFaces[i].v2.X, primMesh.viewerFaces[i].v2.Y, primMesh.viewerFaces[i].v2.Z);
                Ogre::Vector3 pos3(primMesh.viewerFaces[i].v3.X, primMesh.viewerFaces[i].v3.Y, primMesh.viewerFaces[i].v3.Z);

                Ogre::Vector3 n1(primMesh.viewerFaces[i].n1.X, primMesh.viewerFaces[i].n1.Y, primMesh.viewerFaces[i].n1.Z);
                Ogre::Vector3 n2(primMesh.viewerFaces[i].n2.X, primMesh.viewerFaces[i].n2.Y, primMesh.viewerFaces[i].n2.Z);
                Ogre::Vector3 n3(primMesh.viewerFaces[i].n3.X, primMesh.viewerFaces[i].n3.Y, primMesh.viewerFaces[i].n3.Z);
                
                Ogre::Vector2 uv1(primMesh.viewerFaces[i].uv1.U, primMesh.viewerFaces[i].uv1.V);
                Ogre::Vector2 uv2(primMesh.viewerFaces[i].uv2.U, primMesh.viewerFaces[i].uv2.V);
                Ogre::Vector2 uv3(primMesh.viewerFaces[i].uv3.U, primMesh.viewerFaces[i].uv3.V);

                TransformUV(uv1, repeat_u, repeat_v, offset_u, offset_v, rot_sin, rot_cos);
                TransformUV(uv2, repeat_u, repeat_v, offset_u, offset_v, rot_sin, rot_cos);
                TransformUV(uv3, repeat_u, repeat_v, offset_u, offset_v, rot_sin, rot_cos);

                prim_manual_object->position(pos1);
                prim_manual_object->normal(n1);
                prim_manual_object->textureCoord(uv1);
                prim_manual_object->colour(color.r, color.g, color.b, color.a);
                
                prim_manual_object->position(pos2);
                prim_manual_object->normal(n2);
                prim_manual_object->textureCoord(uv2);
                prim_manual_object->colour(color.r, color.g, color.b, color.a);
                
                prim_manual_object->position(pos3);
                prim_manual_object->normal(n3);
                prim_manual_object->textureCoord(uv3);
                prim_manual_object->colour(color.r, color.g, color.b, color.a);
                
                prim_manual_object->index(indices++);
                prim_manual_object->index(indices++);
                prim_manual_object->index(indices++);
            }
            
            // End last subsection
            if (indices)
                prim_manual_object->end();
        }
        catch (Exception& e)
        {
            RexLogicModule::LogError(std::string("Exception while creating primitive geometry: ") + e.what());
            return 0;
        }
        
        return prim_manual_object;
    }
}
