// For conditions of distribution and use, see copyright notice in license.txt

#include "StableHeaders.h"
#include "CameraControllable.h"
#include "EntityComponent/EC_NetworkPosition.h"
#include "SceneEvents.h"
#include "Entity.h"
#include "SceneManager.h"
#include "EC_OgrePlaceable.h"
#include "Renderer.h"
#include "EC_OgrePlaceable.h"
#include "EC_OgreMesh.h"
#include "EntityComponent/EC_AvatarAppearance.h"
#include "InputEvents.h"
#include "InputServiceInterface.h"
#include "EnvironmentModule.h"
#include "Terrain.h"
#include "EC_Terrain.h"
#include "EventManager.h"
#include "ConfigurationManager.h"
#include "ServiceManager.h"
#include "ModuleManager.h"

#include <Ogre.h>

#include <math.h>

using namespace RexTypes;

namespace RA = RexTypes::Actions;

namespace RexLogic
{
    CameraControllable::CameraControllable(Foundation::Framework *fw) :
        framework_(fw),
        action_event_category_(fw->GetEventManager()->QueryEventCategory("Action")),
        current_state_(ThirdPerson),
        firstperson_pitch_(0),
        firstperson_yaw_(0),
        drag_pitch_(0),
        drag_yaw_(0)
    {
        camera_distance_ = 7.f;
        framework_->GetDefaultConfig().SetSetting("Camera", "default_distance", camera_distance_);
        camera_min_distance_ = framework_->GetDefaultConfig().DeclareSetting("Camera", "min_distance", 1.f);
        camera_max_distance_ = framework_->GetDefaultConfig().DeclareSetting("Camera", "max_distance", 50.f);

        camera_offset_ = ParseString<Vector3df>(
            framework_->GetDefaultConfig().DeclareSetting("Camera", "third_person_offset", ToString(Vector3df(0, 0, 1.8f))));

        camera_offset_firstperson_ = ParseString<Vector3df>(
            framework_->GetDefaultConfig().DeclareSetting("Camera", "first_person_offset", ToString(Vector3df(0.5f, 0, 0.8f))));

        sensitivity_ = framework_->GetDefaultConfig().DeclareSetting("Camera", "translation_sensitivity", 25.f);
        zoom_sensitivity_ = framework_->GetDefaultConfig().DeclareSetting("Camera", "zoom_sensitivity", 0.015f);
        firstperson_sensitivity_ = framework_->GetDefaultConfig().DeclareSetting("Camera", "mouselook_rotation_sensitivity", 1.3f);

        zoom_sensitivity_ = framework_->GetDefaultConfig().DeclareSetting("Camera", "zoom_sensitivity", 0.015f);
        firstperson_sensitivity_ = framework_->GetDefaultConfig().DeclareSetting("Camera", "mouselook_rotation_sensitivity", 1.3f);

        zoom_sensitivity_ = framework_->GetDefaultConfig().DeclareSetting("Camera", "zoom_sensitivity", 0.015f);
        firstperson_sensitivity_ = framework_->GetDefaultConfig().DeclareSetting("Camera", "mouselook_rotation_sensitivity", 1.3f);

        useTerrainConstraint_ = framework_->GetDefaultConfig().DeclareSetting("Camera", "use_terrain_constraint", true);
        terrainConstraintOffset_ = framework_->GetDefaultConfig().DeclareSetting("Camera", "terrain_constraint_offset", 0.75f);

        useBoundaryBoxConstraint_ = framework_->GetDefaultConfig().DeclareSetting("Camera", "use_boundarybox_constraint", true);

        boundaryBoxMin_ = ParseString<Vector3df>(
            framework_->GetDefaultConfig().DeclareSetting("Camera", "boundarybox_min", ToString(Vector3df(0, 0, 0))));

        boundaryBoxMax_ = ParseString<Vector3df>(
            framework_->GetDefaultConfig().DeclareSetting("Camera", "boundarybox_max", ToString(Vector3df(256.f, 256.f, 256.f))));

        action_trans_[RexTypes::Actions::MoveForward] = Vector3df::NEGATIVE_UNIT_Z;
        action_trans_[RexTypes::Actions::MoveBackward] = Vector3df::UNIT_Z;
        action_trans_[RexTypes::Actions::MoveLeft] = Vector3df::NEGATIVE_UNIT_X;
        action_trans_[RexTypes::Actions::MoveRight] = Vector3df::UNIT_X;
        action_trans_[RexTypes::Actions::MoveUp] = Vector3df::UNIT_Y;
        action_trans_[RexTypes::Actions::MoveDown] = Vector3df::NEGATIVE_UNIT_Y;

        movement_.x_.rel_ = 0;
        movement_.y_.rel_ = 0;

        // angle of rotation: 10 degrees or 0.175 radians
        rotation_angle_theta = 0.175f;
        rotation_angle_phi = 0.175f;
        mouse_drag_sensitivity = 2;
        isRotating = false;
        isUpDown= false;
        mouse_position_map.clear();
        isDoubleClickZoom = false;
    }

    void CameraControllable::SetCameraEntity(Scene::EntityPtr camera)
    {
        camera_entity_ = camera;
    }
    
    bool CameraControllable::HandleSceneEvent(event_id_t event_id, Foundation::EventDataInterface* data)
    {
        //! \todo This is where our user agent model design breaks down. We assume only one controllable entity exists and that it is a target for the camera.
        //!       Should be changed so in some way the target can be changed and is not automatically assigned. -cm
        if (event_id == Scene::Events::EVENT_CONTROLLABLE_ENTITY)
            target_entity_ = checked_static_cast<Scene::Events::EntityEventData*>(data)->entity;
        
        return false;
    }

    bool CameraControllable::HandleInputEvent(event_id_t event_id, Foundation::EventDataInterface* data)
    {
        if (event_id == Input::Events::INPUTSTATE_THIRDPERSON && current_state_ != ThirdPerson)
        {
            current_state_ = ThirdPerson;
            firstperson_pitch_ = 0.0f;
        }

        if (event_id == Input::Events::INPUTSTATE_FIRSTPERSON && current_state_ != FirstPerson)
        {
            current_state_ = FirstPerson;
            firstperson_pitch_ = 0.0f;
        }

        if (event_id == Input::Events::INPUTSTATE_FREECAMERA && current_state_ != FreeLook)
        {
            current_state_ = FreeLook;
            firstperson_pitch_ = 0.0f;
        }

        if (event_id == Input::Events::SCROLL || 
            event_id == Input::Events::ZOOM_IN_PRESSED || 
            event_id == Input::Events::ZOOM_OUT_PRESSED)
        {
            CameraZoomEvent event_data;
            event_data.entity = camera_entity_.lock();

            switch (event_id)
            {
                case Input::Events::SCROLL:
                    event_data.amount = checked_static_cast<Input::Events::SingleAxisMovement*>(data)->z_.rel_;
                    break;

                case Input::Events::ZOOM_IN_PRESSED:
                    event_data.amount = 100;
                    break;

                case Input::Events::ZOOM_OUT_PRESSED:
                    event_data.amount = -100;
                    break;
            }

            if (event_data.entity) // only send the event if we have an existing entity, no point otherwise
                framework_->GetEventManager()->SendEvent(action_event_category_, RexTypes::Actions::Zoom, &event_data);
        }

        if (event_id == Input::Events::MOUSELOOK)
        {
            Input::Events::Movement *m = checked_static_cast <Input::Events::Movement *> (data);
            movement_.x_.rel_ += m->x_.rel_;
            movement_.y_.rel_ += m->y_.rel_;
            movement_.x_.abs_ = m->x_.abs_;
            movement_.y_.abs_ = m->y_.abs_;
        }

        if (event_id == Input::Events::INPUTSTATE_CAMERATRIPOD)
        {
            current_state_ = Tripod;
            firstperson_pitch_ = 0.0f;
        }
     
        if (event_id == Input::Events::MOUSEDRAG)
        {
            if (current_state_ == FocusOnObject)
            {
                Input::Events::Movement *mouse_pos = checked_static_cast<Input::Events::Movement*>(data); 
                if (mouse_position_map.size() < 4)
                {
                    if(mouse_position_map.size() < 2)
                    {
                        mouse_position_map["x1"] =  mouse_pos->x_.abs_;
                        mouse_position_map["y1"] =  mouse_pos->y_.abs_;
                    }else
                    {
                        mouse_position_map["x2"] =  mouse_pos->x_.abs_;
                        mouse_position_map["y2"] =  mouse_pos->y_.abs_;
                    }
                }
                if (mouse_position_map.size() == 4)
                {
                    mouse_position_map["x1"] = mouse_position_map["x2"];
                    mouse_position_map["y1"] = mouse_position_map["y2"];
                    mouse_position_map["x2"] = mouse_pos->x_.abs_;
                    mouse_position_map["y2"] = mouse_pos->y_.abs_;
                }
            }
        }
        if (event_id == Input::Events::MOUSE_DOUBLECLICK)
        {
            if (current_state_ == FocusOnObject)
            {
                isDoubleClickZoom = true;
                doubleClickZoomDistance = Radius / 2.0;
            }
        }
        return false;
    }

    bool CameraControllable::HandleActionEvent(event_id_t event_id, Foundation::EventDataInterface* data)
    {
        if (event_id == RexTypes::Actions::Zoom)
        {
            if (current_state_ == FocusOnObject)
            {
                float value = checked_static_cast<CameraZoomEvent*>(data)->amount;
                Radius -= (value * zoom_sensitivity_) / 2.0;
                if ( Radius > 0.5)
                {
                    FocusOnObjectZoom();
                }
            } else             
            {
                float value = checked_static_cast<CameraZoomEvent*>(data)->amount;
                camera_distance_ -= (value * zoom_sensitivity_) / 2.0;
                camera_distance_ = clamp(camera_distance_, camera_min_distance_, camera_max_distance_);
            }
        }

        if (current_state_ == FreeLook)
        {
            ActionTransMap::const_iterator it = action_trans_.find(event_id);
            if (it != action_trans_.end())
            {
                // start movement
                const Vector3df &vec = it->second;
                free_translation_.x = ( vec.x == 0 ) ? free_translation_.x : vec.x;
                free_translation_.y = ( vec.y == 0 ) ? free_translation_.y : vec.y;
                free_translation_.z = ( vec.z == 0 ) ? free_translation_.z : vec.z;
            }
            it = action_trans_.find(event_id - 1);
            if (it != action_trans_.end())
            {
                // stop movement
                const Vector3df &vec = it->second;
                free_translation_.x = ( vec.x == 0 ) ? free_translation_.x : 0;
                free_translation_.y = ( vec.y == 0 ) ? free_translation_.y : 0;
                free_translation_.z = ( vec.z == 0 ) ? free_translation_.z : 0;
            }
            normalized_free_translation_ = free_translation_;
            normalized_free_translation_.normalize();
        }

        if (event_id == RA::MoveBackward || event_id == RA::MoveDown || event_id == RA::MoveForward || event_id == RA::MoveLeft 
            || event_id == RA::MoveRight || event_id == RA::MoveUp || event_id == RA::RotateLeft || event_id == RA::RotateRight)
        {
            if (current_state_ == FocusOnObject)
            {
                event_category_id_t event_category = framework_->GetEventManager()->QueryEventCategory("Input");
                framework_->GetEventManager()->SendEvent(event_category, Input::Events::INPUTSTATE_THIRDPERSON, 0);
            }
        }

        return false;
    }

    void CameraControllable::AddTime(f64 frametime)
    {
        drag_yaw_ = static_cast<float>(movement_.x_.rel_) * -0.005f;
        drag_pitch_ = static_cast<float>(movement_.y_.rel_) * -0.005f;
        movement_.x_.rel_ = 0;
        movement_.y_.rel_ = 0;

        Foundation::RenderServiceInterface *renderer = framework_->GetService<Foundation::RenderServiceInterface>();
        Scene::EntityPtr target = target_entity_.lock();
        Scene::EntityPtr camera = camera_entity_.lock();
        
        if (renderer && target && camera)
        {
            OgreRenderer::EC_OgrePlaceable *camera_placeable = camera->GetComponent<OgreRenderer::EC_OgrePlaceable>().get();

            // for smoothness, we apparently need to get rotation from network position and position from placeable. Go figure. -cm
            EC_NetworkPosition *netpos = target->GetComponent<EC_NetworkPosition>().get();
            OgreRenderer::EC_OgrePlaceable *placeable = target->GetComponent<OgreRenderer::EC_OgrePlaceable>().get();
            if (netpos && placeable)
            {
                Vector3df avatar_pos = placeable->GetPosition();
                Quaternion avatar_orientation = netpos->orientation_; 

                if (current_state_ == FirstPerson || current_state_ == ThirdPerson)
                {
                    // this is mostly for third person camera, but also needed by first person camera since it sets proper initial orientation for the camera
                    Vector3df pos = avatar_pos;
                    pos += (avatar_orientation * Vector3df::NEGATIVE_UNIT_X * camera_distance_);
                    pos += (avatar_orientation * camera_offset_);
                    camera_placeable->SetPosition(pos);
                    Vector3df lookat = avatar_pos + avatar_orientation * camera_offset_;
                    camera_placeable->LookAt(lookat); 
                }
                
                if (current_state_ == FirstPerson)
                {
                    bool fallback = true;

                    // Try to use head bone from target entity to get the first person camera position
                    OgreRenderer::EC_OgreMesh *mesh = target->GetComponent<OgreRenderer::EC_OgreMesh>().get();
                    EC_AvatarAppearance *appearance = target->GetComponent<EC_AvatarAppearance>().get();
                    if (mesh && appearance)
                    {
                        Ogre::Entity* ent = mesh->GetEntity();
                        if (ent)
                        {
                            Ogre::SkeletonInstance* skel = ent->getSkeleton();
                            
                            std::string view_bone_name;
                            float adjustheight = mesh->GetAdjustPosition().z;
                            
                            if (appearance->HasProperty("viewbone"))
                            {
                                // This bone property is exclusively for view tracking & assumed to be correct position, no offset
                                view_bone_name = appearance->GetProperty("viewbone");
                            }
                            else if (appearance->HasProperty("headbone"))
                            {
                                view_bone_name = appearance->GetProperty("headbone");
                                // The biped head bone is anchored at the neck usually. Therefore a guessed fixed offset is needed,
                                // which is not preferable, but necessary
                                adjustheight += 0.15f;
                            }
                            
                            if (!view_bone_name.empty())
                            {
                                if (skel && skel->hasBone(view_bone_name))
                                {
                                    // Hack: force Ogre to update skeleton with current animation state, even if avatar invisible
                                    if (ent->getAllAnimationStates())
                                        skel->setAnimationState(*ent->getAllAnimationStates());
                                    
                                    Ogre::Bone* bone = skel->getBone(view_bone_name);
                                    Ogre::Vector3 headpos = bone->_getDerivedPosition();
                                    Vector3df ourheadpos(-headpos.z + 0.5f, -headpos.x, headpos.y + adjustheight);
                                    RexTypes::Vector3 campos = avatar_pos + (avatar_orientation * ourheadpos);
                                    camera_placeable->SetPosition(campos);
                                    fallback = false;
                                }
                            }
                        }
                    }
                    // Fallback using fixed position
                    if (fallback)
                    {
                        RexTypes::Vector3 campos = avatar_pos + (avatar_orientation * camera_offset_firstperson_);
                        camera_placeable->SetPosition(campos);
                    }

                    // update camera pitch
                    if (drag_pitch_ != 0)
                    {
                        firstperson_pitch_ += drag_pitch_ * firstperson_sensitivity_;
                        firstperson_pitch_ = clamp(firstperson_pitch_, -HALF_PI, HALF_PI);
                    }
                    camera_placeable->SetPitch(firstperson_pitch_);
                }

                if (current_state_ == FreeLook)
                {
                    const float trans_dt = (float)frametime * sensitivity_;

                    RexTypes::Vector3 pos = camera_placeable->GetPosition();
                    pos += camera_placeable->GetOrientation() * normalized_free_translation_ * trans_dt;
                    ClampPosition(pos);
                    camera_placeable->SetPosition(pos);

                    camera_placeable->SetPitch(drag_pitch_ * firstperson_sensitivity_);
                    camera_placeable->SetYaw(drag_yaw_ * firstperson_sensitivity_);
                }

                if (current_state_ == Tripod)
                {
                    const float trans_dt = (float)frametime * sensitivity_;

                    RexTypes::Vector3 pos = camera_placeable->GetPosition();
                    pos += camera_placeable->GetOrientation() * normalized_free_translation_ * trans_dt;
                    ClampPosition(pos);
                    camera_placeable->SetPosition(pos);

                    camera_placeable->SetPitch(drag_pitch_ * firstperson_sensitivity_);
                    camera_placeable->SetYaw(drag_yaw_ * firstperson_sensitivity_);
                }

                if (current_state_ == FocusOnObject)
                {
                    if (mouse_position_map.size() == 4)
                    {
                        keep_mouse_position["x1"] = keep_mouse_position["x2"];
                        keep_mouse_position["y1"] = keep_mouse_position["y2"];
                        keep_mouse_position["x2"] = mouse_position_map["x2"];
                        keep_mouse_position["y2"] = mouse_position_map["y2"];
                        if (keep_mouse_position["x1"] != keep_mouse_position["x2"])
                        {
                            isUpDown = false;
                            if ((mouse_position_map["x2"] - mouse_position_map["x1"]) > mouse_drag_sensitivity)
                            {
                                rotation_direction = 1;
                                RotateCameraAroundObject();
                            }
                            if ((mouse_position_map["x1"] - mouse_position_map["x2"]) > mouse_drag_sensitivity)
                            {
                                rotation_direction = -1;
                                RotateCameraAroundObject();
                            }
                        }

                        if (keep_mouse_position["y1"] != keep_mouse_position["y2"])
                        {
                            isUpDown = true;
                            if ((mouse_position_map["y2"] - mouse_position_map["y1"] ) > mouse_drag_sensitivity)
                            {
                                rotation_direction = 1;
                                RotateCameraAroundObject();
                            }
                            if ((mouse_position_map["y1"] - mouse_position_map["y2"]) > mouse_drag_sensitivity)
                            {
                                rotation_direction = -1;
                                RotateCameraAroundObject();
                            }
                        }
                    }
                    if (isDoubleClickZoom)
                    {
                        Radius -= (100 * zoom_sensitivity_) / 2.0;
                        if ( Radius > 1.0)
                        {
                            FocusOnObjectZoom();
                        }else
                        {
                            isDoubleClickZoom = false;
                        }
                    }
                }
            }
        }
        
        // switch between first and third person modes, depending on how close we are to the avatar
        switch (current_state_)
        {
        case FirstPerson:
            if (camera_distance_ != camera_min_distance_)
            {
                current_state_ = ThirdPerson;
                event_category_id_t event_category = framework_->GetEventManager()->QueryEventCategory("Input");
                framework_->GetEventManager()->SendEvent(event_category, Input::Events::INPUTSTATE_THIRDPERSON, 0);
                
                firstperson_pitch_ = 0.0f;
            }
            break;
        case ThirdPerson:
            if (camera_distance_ == camera_min_distance_)
            {
                event_category_id_t event_category = framework_->GetEventManager()->QueryEventCategory("Input");
                framework_->GetEventManager()->SendEvent(event_category, Input::Events::INPUTSTATE_FIRSTPERSON, 0);
                current_state_ = FirstPerson;
                
                firstperson_pitch_ = 0.0f;
            }
            break;
        }
    }

    //experimental for py api
    void CameraControllable::SetYawPitch(float newyaw, float newpitch)
    {
        firstperson_yaw_ = newyaw;
        firstperson_pitch_ = newpitch;
    }

    void CameraControllable::ClampPosition(Vector3df &position)
    {
        float min_z;

        // The 'terrain constraint' option enforces that the camera position is always kept above the terrain ground plane.
        if (useTerrainConstraint_)
        {
            boost::shared_ptr<Environment::EnvironmentModule> env = framework_->GetModuleManager()->GetModule<Environment::EnvironmentModule>().lock();
            if (env)
            {
                Scene::EntityWeakPtr terrain = env->GetTerrainHandler()->GetTerrainEntity();
                if (!terrain.expired())
                {
                    Environment::EC_Terrain *ec_terrain = terrain.lock()->GetComponent<Environment::EC_Terrain>().get();
                    if (ec_terrain && ec_terrain->AllPatchesLoaded())
                    {
                        float terrain_height = ec_terrain->InterpolateHeightValue(position.x, position.y);
                        min_z = terrain_height + terrainConstraintOffset_;
                        if (!useBoundaryBoxConstraint_ && position.z < min_z)
                            position.z = min_z;
                    }
                }
            }
        }

        // The 'boundary box constraint' option enforces that the camera position is always kept inside the scene bounding box.
        if (useBoundaryBoxConstraint_)
        {
            position.x = clamp(position.x, boundaryBoxMin_.x, boundaryBoxMax_.x);
            position.y = clamp(position.y, boundaryBoxMin_.y, boundaryBoxMax_.y);

            if (useTerrainConstraint_)
                position.z = clamp(position.z, min_z, boundaryBoxMax_.z);
            else
                position.z = clamp(position.z, boundaryBoxMin_.z, boundaryBoxMax_.z);
        }
    }

    void CameraControllable::SetFocusOnObject(float x, float y, float z)
    {
        current_state_ = FocusOnObject;
        mouse_position_map.clear();
        keep_mouse_position["x1"] = 0;
        keep_mouse_position["y1"] = 0;
        keep_mouse_position["x2"] = 0;
        keep_mouse_position["y2"] = 0;

        Foundation::RenderServiceInterface *renderer = framework_->GetService<Foundation::RenderServiceInterface>();
        Scene::EntityPtr target = target_entity_.lock();
        Scene::EntityPtr camera = camera_entity_.lock();

        if (renderer && target && camera)
        {
            OgreRenderer::EC_OgrePlaceable *camera_placeable = camera->GetComponent<OgreRenderer::EC_OgrePlaceable>().get();
            EC_NetworkPosition *netpos = target->GetComponent<EC_NetworkPosition>().get();
            OgreRenderer::EC_OgrePlaceable *placeable = target->GetComponent<OgreRenderer::EC_OgrePlaceable>().get();
            if (camera_placeable && netpos && placeable)
            {
                Vector3df camera_position = camera_placeable->GetPosition();
                center_x = x;
                center_y = y;
                center_z = z;

                Radius = sqrt(
                    (camera_position.x - center_x)*(camera_position.x - center_x) + 
                    (camera_position.y - center_y)*(camera_position.y - center_y) +
                    (camera_position.z - center_z)*(camera_position.z - center_z));
                Theta = acos ((camera_position.z - center_z) / Radius);
                Phi = atan2 ((camera_position.y - center_y), (camera_position.x - center_x));
            }
        }
    }

    void CameraControllable::RotateCameraAroundObject()
    {
        drag_yaw_ = static_cast<float>(movement_.x_.rel_) * -0.005f;
        drag_pitch_ = static_cast<float>(movement_.y_.rel_) * -0.005f;
        movement_.x_.rel_ = 0;
        movement_.y_.rel_ = 0;

        Foundation::RenderServiceInterface *renderer = framework_->GetService<Foundation::RenderServiceInterface>();
        Scene::EntityPtr target = target_entity_.lock();
        Scene::EntityPtr camera = camera_entity_.lock();

        if (renderer && target && camera)
        {
            OgreRenderer::EC_OgrePlaceable *camera_placeable =  camera->GetComponent<OgreRenderer::EC_OgrePlaceable>().get();
            EC_NetworkPosition *netpos = target->GetComponent<EC_NetworkPosition>().get();
            OgreRenderer::EC_OgrePlaceable *placeable =  target->GetComponent<OgreRenderer::EC_OgrePlaceable>().get();
            if (camera_placeable && netpos && placeable)
            {
                if ( !isUpDown )
                {
                    Phi += rotation_direction * rotation_angle_phi;//(rotation_angle_z*PI)/180;

                    new_x = center_x + Radius * sin(Theta) * cos(Phi);
                    new_y = center_y + Radius * sin(Theta) * sin(Phi);
                    new_z = center_z + Radius * cos(Theta);
                    
                    camera_placeable->SetPosition(Vector3df(new_x, new_y, new_z));
                    camera_placeable->LookAt(Vector3df(center_x, center_y, center_z));
                }
                else
                {
                    Theta += rotation_direction * rotation_angle_theta;

                    new_x = center_x + Radius * sin(Theta) * cos(Phi);
                    new_y = center_y + Radius * sin(Theta) * sin(Phi);
                    new_z = center_z + Radius * cos(Theta);
                    
                    camera_placeable->SetPosition(Vector3df(new_x, new_y, new_z));
                    camera_placeable->LookAt(Vector3df(center_x, center_y, center_z));
                }
            }
        }
    }

    void CameraControllable::FocusOnObjectZoom()
    {
        boost::shared_ptr<OgreRenderer::Renderer> renderer = framework_->GetServiceManager()->GetService<OgreRenderer::Renderer>(Foundation::Service::ST_Renderer).lock();
        Scene::EntityPtr target = target_entity_.lock();
        Scene::EntityPtr camera = camera_entity_.lock();

        if (renderer && target && camera)
        {
            OgreRenderer::EC_OgrePlaceable *camera_placeable = 
                checked_static_cast<OgreRenderer::EC_OgrePlaceable*>(camera->GetComponent(OgreRenderer::EC_OgrePlaceable::TypeNameStatic()).get());

            EC_NetworkPosition *netpos = checked_static_cast<EC_NetworkPosition*>(target->GetComponent(EC_NetworkPosition::TypeNameStatic()).get());
            OgreRenderer::EC_OgrePlaceable *placeable = 
                checked_static_cast<OgreRenderer::EC_OgrePlaceable*>(target->GetComponent(OgreRenderer::EC_OgrePlaceable::TypeNameStatic()).get());
            if (netpos && placeable)
            {
                new_x = center_x + Radius * sin(Theta) * cos(Phi);
                new_y = center_y + Radius * sin(Theta) * sin(Phi);
                new_z = center_z + Radius * cos(Theta);

                camera_placeable->SetPosition(Vector3df(new_x, new_y, new_z));
                camera_placeable->LookAt(Vector3df(center_x, center_y, center_z));
            }
        }
    }
    
}

