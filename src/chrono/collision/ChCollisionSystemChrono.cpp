// =============================================================================
// PROJECT CHRONO - http://projectchrono.org
//
// Copyright (c) 2021 projectchrono.org
// All rights reserved.
//
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file at the top level of the distribution and at
// http://projectchrono.org/license-chrono.txt.
//
// =============================================================================
// Authors: Radu Serban
// =============================================================================
//
// Chrono custom multicore collision system.
// Contains both the broadphase and the narrow phase methods.
//
// =============================================================================

#include "chrono/physics/ChSystem.h"
#include "chrono/collision/ChCollisionSystemChrono.h"
#include "chrono/multicore_math/utility.h"

namespace chrono {
namespace collision {

ChCollisionSystemChrono::ChCollisionSystemChrono() : use_aabb_active(false) {
    data_manager = chrono_types::make_shared<ChCollisionData>();

    broadphase.data_manager = data_manager;
    narrowphase.data_manager = data_manager;
    aabb_generator.data_manager = data_manager;
}

ChCollisionSystemChrono::~ChCollisionSystemChrono() {}

void ChCollisionSystemChrono::SetBroadphaseNumBins(ChVector<int> num_bins, bool fixed) {
    broadphase.bins_per_axis = vec3(num_bins.x(), num_bins.y(), num_bins.z());
    broadphase.fixed_bins = fixed;
}

void ChCollisionSystemChrono::SetBroadphaseGridDensity(double density) {
    broadphase.grid_density = real(density);
}

void ChCollisionSystemChrono::SetNarrowphaseAlgorithm(ChNarrowphase::Algorithm algorithm) {
    narrowphase.algorithm = algorithm;
}

void ChCollisionSystemChrono::SetNarrowphaseEnvelope(real envelope) {
    narrowphase.envelope = envelope;
}

void ChCollisionSystemChrono::EnableActiveBoundingBox(const ChVector<>& aabbmin, const ChVector<>& aabbmax) {
    aabb_min = FromChVector(aabbmin);
    aabb_max = FromChVector(aabbmax);

    use_aabb_active = true;
}

bool ChCollisionSystemChrono::GetAABB(ChVector<>& aabbmin, ChVector<>& aabbmax) const {
    aabbmin = ToChVector(aabb_min);
    aabbmax = ToChVector(aabb_max);

    return use_aabb_active;
}

void ChCollisionSystemChrono::Add(ChCollisionModel* model) {
    if (!model->GetPhysicsItem()->GetCollide())
        return;

    ChCollisionModelChrono* pmodel = static_cast<ChCollisionModelChrono*>(model);

    int body_id = pmodel->GetBody()->GetId();
    short2 fam = S2(pmodel->GetFamilyGroup(), pmodel->GetFamilyMask());
    // The offset for this shape will the current total number of points in the convex data list
    auto shape_data = data_manager->shape_data;
    int convex_data_offset = (int)shape_data.convex_rigid.size();
    // Insert the points into the global convex list
    shape_data.convex_rigid.insert(shape_data.convex_rigid.end(), pmodel->local_convex_data.begin(),
                                   pmodel->local_convex_data.end());

    // Shape index in the collision model
    int local_shape_index = 0;

    for (auto s : pmodel->GetShapes()) {
        auto shape = std::static_pointer_cast<ChCollisionShapeChrono>(s);
        real3 obA = shape->A;
        real3 obB = shape->B;
        real3 obC = shape->C;
        int length = 1;
        int start;
        // Compute the global offset of the convex data structure based on the number of points
        // already present

        switch (shape->GetType()) {
            case ChCollisionShape::Type::SPHERE:
                start = (int)shape_data.sphere_rigid.size();
                shape_data.sphere_rigid.push_back(obB.x);
                break;
            case ChCollisionShape::Type::ELLIPSOID:
                start = (int)shape_data.box_like_rigid.size();
                shape_data.box_like_rigid.push_back(obB);
                break;
            case ChCollisionShape::Type::BOX:
                start = (int)shape_data.box_like_rigid.size();
                shape_data.box_like_rigid.push_back(obB);
                break;
            case ChCollisionShape::Type::CYLINDER:
                start = (int)shape_data.box_like_rigid.size();
                shape_data.box_like_rigid.push_back(obB);
                break;
            case ChCollisionShape::Type::CYLSHELL:
                start = (int)shape_data.box_like_rigid.size();
                shape_data.box_like_rigid.push_back(obB);
                break;
            case ChCollisionShape::Type::CONE:
                start = (int)shape_data.box_like_rigid.size();
                shape_data.box_like_rigid.push_back(obB);
                break;
            case ChCollisionShape::Type::CAPSULE:
                start = (int)shape_data.capsule_rigid.size();
                shape_data.capsule_rigid.push_back(real2(obB.x, obB.y));
                break;
            case ChCollisionShape::Type::ROUNDEDBOX:
                start = (int)shape_data.rbox_like_rigid.size();
                shape_data.rbox_like_rigid.push_back(real4(obB, obC.x));
                break;
            case ChCollisionShape::Type::ROUNDEDCYL:
                start = (int)shape_data.rbox_like_rigid.size();
                shape_data.rbox_like_rigid.push_back(real4(obB, obC.x));
                break;
            case ChCollisionShape::Type::ROUNDEDCONE:
                start = (int)shape_data.rbox_like_rigid.size();
                shape_data.rbox_like_rigid.push_back(real4(obB, obC.x));
                break;
            case ChCollisionShape::Type::CONVEX:
                start = (int)(obB.y + convex_data_offset);
                length = (int)obB.x;
                break;
            case ChCollisionShape::Type::TRIANGLE:
                start = (int)shape_data.triangle_rigid.size();
                shape_data.triangle_rigid.push_back(obA);
                shape_data.triangle_rigid.push_back(obB);
                shape_data.triangle_rigid.push_back(obC);
                break;
            default:
                start = -1;
                break;
        }

        shape_data.ObA_rigid.push_back(obA);
        shape_data.ObR_rigid.push_back(shape->R);
        shape_data.start_rigid.push_back(start);
        shape_data.length_rigid.push_back(length);

        shape_data.fam_rigid.push_back(fam);
        shape_data.typ_rigid.push_back(shape->GetType());
        shape_data.id_rigid.push_back(body_id);
        shape_data.local_rigid.push_back(local_shape_index);
        data_manager->num_rigid_shapes++;
        local_shape_index++;
    }
}

#define ERASE_MACRO(x, y) x.erase(x.begin() + y);
#define ERASE_MACRO_LEN(x, y, z) x.erase(x.begin() + y, x.begin() + y + z);

void ChCollisionSystemChrono::Remove(ChCollisionModel* model) {
    /*
    ChCollisionModelChrono* pmodel = static_cast<ChCollisionModelChrono*>(model);
    int body_id = pmodel->GetBody()->GetId();
    //loop over the models we nned to remove
    //std::cout << "removing: " << pmodel->GetNumShapes() << " objects" << std::endl;
    for (int j = 0; j < pmodel->GetNumShapes(); j++) {
        //find a model to remove
        bool removed = false;
        for (int i = 0; i < shape_data.id_rigid.size(); i++) {
            if (shape_data.id_rigid[i] == body_id) {
                int index = i;
                data_manager->num_rigid_shapes--;

                int start = shape_data.start_rigid[index];
                int length = shape_data.length_rigid[index];
                int type = shape_data.typ_rigid[index];

                //std::cout << "removing: type " << type << " " << start<< " " <<j << std::endl;


                switch (type) {
                case ChCollisionShape::Type::SPHERE:
                    ERASE_MACRO_LEN(shape_data.sphere_rigid, start, length);
                    break;
                case ChCollisionShape::Type::ELLIPSOID:
                    ERASE_MACRO_LEN(shape_data.box_like_rigid, start, length);
                    break;
                case ChCollisionShape::Type::BOX:
                    ERASE_MACRO_LEN(shape_data.box_like_rigid, start, length);
                    break;
                case ChCollisionShape::Type::CYLINDER:
                    ERASE_MACRO_LEN(shape_data.box_like_rigid, start, length);
                    break;
                case ChCollisionShape::Type::CYLSHELL:
                    ERASE_MACRO_LEN(shape_data.box_like_rigid, start, length);
                    break;
                case ChCollisionShape::Type::CONE:
                    ERASE_MACRO_LEN(shape_data.box_like_rigid, start, length);
                    break;
                case ChCollisionShape::Type::CAPSULE:
                    ERASE_MACRO_LEN(shape_data.capsule_rigid, start, length);
                    break;
                case ChCollisionShape::Type::ROUNDEDBOX:
                    ERASE_MACRO_LEN(shape_data.rbox_like_rigid, start, length);
                    break;
                case ChCollisionShape::Type::ROUNDEDCYL:
                    ERASE_MACRO_LEN(shape_data.rbox_like_rigid, start, length);
                    break;
                case ChCollisionShape::Type::ROUNDEDCONE:
                    ERASE_MACRO_LEN(shape_data.rbox_like_rigid, start, length);
                    break;
                case ChCollisionShape::Type::CONVEX:
                    ERASE_MACRO_LEN(shape_data.convex_rigid, start, length);
                    break;
                case ChCollisionShape::Type::TRIANGLE:
                    ERASE_MACRO_LEN(shape_data.convex_rigid, start, 3);
                    break;
                }

                ERASE_MACRO(shape_data.ObA_rigid, index);
                ERASE_MACRO(shape_data.ObR_rigid, index);
                ERASE_MACRO(shape_data.start_rigid, index);
                ERASE_MACRO(shape_data.length_rigid, index);

                ERASE_MACRO(shape_data.fam_rigid, index);
                ERASE_MACRO(shape_data.typ_rigid, index);
                ERASE_MACRO(shape_data.id_rigid, index);
                removed = true;
                break;
            }
        }
        //std::cout << "decrement start "<< std::endl;
        if (removed) {
            //we removed a model, all of the starts are off by one, decrement all starts before removing a second model
            for (int i = 0; i < shape_data.start_rigid.size(); i++) {
                if (shape_data.start_rigid[i] != 0) {
                    shape_data.start_rigid[i] -= 1;
                }
            }
        }

    }
*/
}
#undef ERASE_MACRO
#undef ERASE_MACRO_LEN

void ChCollisionSystemChrono::SetNumThreads(int nthreads) {
#ifdef _OPENMP
    omp_set_num_threads(nthreads);
#endif
}

void ChCollisionSystemChrono::Synchronize() {
    custom_vector<real3>& position = data_manager->state_data.pos_rigid;
    custom_vector<quaternion>& rotation = data_manager->state_data.rot_rigid;
    custom_vector<char>& active = data_manager->state_data.active_rigid;
    custom_vector<char>& collide = data_manager->state_data.collide_rigid;

    auto blist = m_system->Get_bodylist();
    int nbodies = static_cast<int>(blist.size());

    position.resize(3 * nbodies);
    rotation.resize(4 * nbodies);
    active.resize(nbodies);
    collide.resize(nbodies);

    data_manager->state_data.num_rigid_bodies = nbodies;
    data_manager->state_data.num_fluid_bodies = 0;

#pragma omp parallel for
    for (int i = 0; i < nbodies; i++) {
        auto& body = blist[i];

        ChVector<>& body_pos = body->GetPos();
        ChQuaternion<>& body_rot = body->GetRot();

        position[i] = real3(body_pos.x(), body_pos.y(), body_pos.z());
        rotation[i] = quaternion(body_rot.e0(), body_rot.e1(), body_rot.e2(), body_rot.e3());

        active[i] = body->IsActive();
        collide[i] = body->GetCollide();
    }
}

void ChCollisionSystemChrono::Run() {
    if (use_aabb_active) {
        body_active.resize(data_manager->state_data.num_rigid_bodies);
        std::fill(body_active.begin(), body_active.end(), false);

        GetOverlappingAABB(body_active, aabb_min, aabb_max);

#pragma omp parallel for
        for (int i = 0; i < data_manager->state_data.active_rigid.size(); i++) {
            if (data_manager->state_data.active_rigid[i] != 0 && data_manager->state_data.collide_rigid[i] != 0) {
                data_manager->state_data.active_rigid[i] = body_active[i];
            }
        }
    }

    m_timer_broad.start();
    aabb_generator.GenerateAABB(narrowphase.envelope);

    // Compute the bounding box of things
    broadphase.DetermineBoundingBox();
    broadphase.OffsetAABB();
    broadphase.ComputeTopLevelResolution();

    // Everything is offset and ready to go!
    broadphase.DispatchRigid();

    m_timer_broad.stop();

    m_timer_narrow.start();
    if (data_manager->state_data.num_fluid_bodies != 0) {
        narrowphase.DispatchFluid();
    }
    if (data_manager->num_rigid_shapes != 0) {
        narrowphase.ProcessRigids(broadphase.bins_per_axis);

    } else {
        data_manager->host_data.c_counts_rigid_fluid.clear();
        data_manager->num_rigid_fluid_contacts = 0;
    }

    m_timer_narrow.stop();
}

void ChCollisionSystemChrono::GetBoundingBox(ChVector<>& aabb_min, ChVector<>& aabb_max) const {
    aabb_min.x() = data_manager->measures.min_bounding_point.x;
    aabb_min.y() = data_manager->measures.min_bounding_point.y;
    aabb_min.z() = data_manager->measures.min_bounding_point.z;

    aabb_max.x() = data_manager->measures.max_bounding_point.x;
    aabb_max.y() = data_manager->measures.max_bounding_point.y;
    aabb_max.z() = data_manager->measures.max_bounding_point.z;
}

void ChCollisionSystemChrono::ReportContacts(ChContactContainer* mcontactcontainer) {
    //// TODO

    ////assert(dynamic_cast<ChContactContainerMulticore*>(mcontactcontainer));

    // Resize global arrays with composite material properties.
    // NOTE: important to do this here, to set size to zero if no contacts (in case some other added by a custom user
    // callback)
    mcontactcontainer->BeginAddContact();

    uint num_contacts = data_manager->num_rigid_contacts;
    if (num_contacts <= 0) {
        return;
    }

    ////auto container = static_cast<ChContactContainerMulticore*>(mcontactcontainer);

    auto& bids = data_manager->host_data.bids_rigid_rigid;  // global IDs of bodies in contact
    auto& sids = data_manager->host_data.contact_shapeIDs;  // global IDs of shapes in contact
    auto& sindex = data_manager->shape_data.local_rigid;    // collision model indexes of shapes in contact

    // Loop over all current contacts, create the composite material, and load material properties in the data manager
    // (on a per contact basis). Snce this is contact method-specific, we defer to the underlying contact container.
#pragma omp parallel for
    for (int i = 0; i < (signed)num_contacts; i++) {
        auto b1 = bids[i].x;                  // global IDs of bodies in contact
        auto b2 = bids[i].y;                  //
        auto s1 = int(sids[i] >> 32);         // global IDs of shapes in contact
        auto s2 = int(sids[i] & 0xffffffff);  //
        auto s1_index = sindex[s1];           // collision model indexes of shapes in contact
        auto s2_index = sindex[s2];           //

        ////container->AddContact(i, b1, s1, b2, s2);
    }

    mcontactcontainer->EndAddContact();
}

void ChCollisionSystemChrono::ResetTimers() {
    m_timer_broad.reset();
    m_timer_narrow.reset();
}

double ChCollisionSystemChrono::GetTimerCollisionBroad() const {
    return m_timer_broad();
}

double ChCollisionSystemChrono::GetTimerCollisionNarrow() const {
    return m_timer_narrow();
}

void ChCollisionSystemChrono::GetOverlappingAABB(custom_vector<char>& active_id, real3 Amin, real3 Amax) {
    aabb_generator.GenerateAABB(narrowphase.envelope);
#pragma omp parallel for
    for (int i = 0; i < data_manager->shape_data.typ_rigid.size(); i++) {
        real3 Bmin = data_manager->host_data.aabb_min[i];
        real3 Bmax = data_manager->host_data.aabb_max[i];

        bool inContact = (Amin.x <= Bmax.x && Bmin.x <= Amax.x) && (Amin.y <= Bmax.y && Bmin.y <= Amax.y) &&
                         (Amin.z <= Bmax.z && Bmin.z <= Amax.z);
        if (inContact) {
            active_id[data_manager->shape_data.id_rigid[i]] = true;
        }
    }
}

std::vector<vec2> ChCollisionSystemChrono::GetOverlappingPairs() {
    std::vector<vec2> pairs;
    pairs.resize(data_manager->host_data.pair_shapeIDs.size());
    for (int i = 0; i < data_manager->host_data.pair_shapeIDs.size(); i++) {
        vec2 pair = I2(int(data_manager->host_data.pair_shapeIDs[i] >> 32),
                       int(data_manager->host_data.pair_shapeIDs[i] & 0xffffffff));
        pairs[i] = pair;
    }
    return pairs;
}

}  // end namespace collision
}  // end namespace chrono
