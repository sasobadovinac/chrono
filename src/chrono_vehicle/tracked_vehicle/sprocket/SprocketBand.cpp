// =============================================================================
// PROJECT CHRONO - http://projectchrono.org
//
// Copyright (c) 2014 projectchrono.org
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
// Tracked vehicle continuous-band sprocket model constructed with data from file
// (JSON format).
//
// =============================================================================

#include "chrono/assets/ChTriangleMeshShape.h"

#include "chrono_vehicle/ChVehicleModelData.h"
#include "chrono_vehicle/tracked_vehicle/sprocket/SprocketBand.h"
#include "chrono_vehicle/utils/ChUtilsJSON.h"

using namespace rapidjson;

namespace chrono {
namespace vehicle {

// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
SprocketBand::SprocketBand(const std::string& filename) : ChSprocketBand(""), m_has_mesh(false) {
    Document d = ReadFileJSON(filename);
    if (d.IsNull())
        return;

    Create(d);

    GetLog() << "Loaded JSON: " << filename.c_str() << "\n";
}

SprocketBand::SprocketBand(const rapidjson::Document& d) : ChSprocketBand(""), m_has_mesh(false) {
    Create(d);
}

void SprocketBand::Create(const rapidjson::Document& d) {
    // Invoke base class method.
    ChPart::Create(d);

    // Read inertia properties
    m_num_teeth = d["Number Teeth"].GetInt();
    m_gear_mass = d["Gear Mass"].GetDouble();
    m_gear_inertia = ReadVectorJSON(d["Gear Inertia"]);
    m_axle_inertia = d["Axle Inertia"].GetDouble();
    m_separation = d["Gear Separation"].GetDouble();

    // Read profile information
    assert(d.HasMember("Profile"));
    m_gear_outer_radius = d["Profile"]["Outer Radius"].GetDouble();
    m_gear_base_width = d["Profile"]["Base Width"].GetDouble();
    m_gear_tip_width = d["Profile"]["Tip Width"].GetDouble();
    m_gear_tooth_depth = d["Profile"]["Tooth Depth"].GetDouble();
    m_gear_arc_radius = d["Profile"]["Arc Radius"].GetDouble();
    m_gear_guide_wheel_width = d["Profile"]["Guide Wheel Width"].GetDouble();
    m_gear_guide_wheel_gap = d["Profile"]["Guide Wheel Gap"].GetDouble();
    m_gear_RA = d["Profile"]["Assembly Radius"].GetDouble();

    // Read contact material data
    assert(d.HasMember("Contact Material"));

    // Load default values (in case not all are provided in the JSON file)
    m_mat_info.mu = 0.4f;
    m_mat_info.cr = 0.1f;
    m_mat_info.Y = 1e7f;
    m_mat_info.nu = 0.3f;
    m_mat_info.kn = 2e5f;
    m_mat_info.gn = 40.0f;
    m_mat_info.kt = 2e5f;
    m_mat_info.gt = 20.0f;

    const Value& mat = d["Contact Material"];

    m_mat_info.mu = mat["Coefficient of Friction"].GetFloat();
    m_mat_info.cr = mat["Coefficient of Restitution"].GetFloat();
    if (mat.HasMember("Properties")) {
        m_mat_info.Y = mat["Properties"]["Young Modulus"].GetFloat();
        m_mat_info.nu = mat["Properties"]["Poisson Ratio"].GetFloat();
    }
    if (mat.HasMember("Coefficients")) {
        m_mat_info.kn = mat["Coefficients"]["Normal Stiffness"].GetFloat();
        m_mat_info.gn = mat["Coefficients"]["Normal Damping"].GetFloat();
        m_mat_info.kt = mat["Coefficients"]["Tangential Stiffness"].GetFloat();
        m_mat_info.gt = mat["Coefficients"]["Tangential Damping"].GetFloat();
    }

    // Read sprocket visualization
    if (d.HasMember("Visualization")) {
        assert(d["Visualization"].HasMember("Mesh Filename"));
        assert(d["Visualization"].HasMember("Mesh Name"));
        m_meshFile = d["Visualization"]["Mesh Filename"].GetString();
        m_meshName = d["Visualization"]["Mesh Name"].GetString();
        m_has_mesh = true;
    }
}

void SprocketBand::CreateContactMaterial(ChContactMethod contact_method) {
    switch (contact_method) {
        case ChContactMethod::NSC: {
            auto matNSC = chrono_types::make_shared<ChMaterialSurfaceNSC>();
            matNSC->SetFriction(m_mat_info.mu);
            matNSC->SetRestitution(m_mat_info.cr);
            m_material = matNSC;
            break;
        }
        case ChContactMethod::SMC: {
            auto matSMC = chrono_types::make_shared<ChMaterialSurfaceSMC>();
            matSMC->SetFriction(m_mat_info.mu);
            matSMC->SetRestitution(m_mat_info.cr);
            matSMC->SetYoungModulus(m_mat_info.Y);
            matSMC->SetPoissonRatio(m_mat_info.nu);
            matSMC->SetKn(m_mat_info.kn);
            matSMC->SetGn(m_mat_info.gn);
            matSMC->SetKt(m_mat_info.kt);
            matSMC->SetGt(m_mat_info.gt);
            m_material = matSMC;
            break;
        }
    }
}

// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
void SprocketBand::AddVisualizationAssets(VisualizationType vis) {
    if (vis == VisualizationType::MESH && m_has_mesh) {
        auto trimesh = chrono_types::make_shared<geometry::ChTriangleMeshConnected>();
        trimesh->LoadWavefrontMesh(vehicle::GetDataFile(m_meshFile), false, false);
        auto trimesh_shape = chrono_types::make_shared<ChTriangleMeshShape>();
        trimesh_shape->SetMesh(trimesh);
        trimesh_shape->SetName(m_meshName);
        trimesh_shape->SetStatic(true);
        m_gear->AddAsset(trimesh_shape);
    } else {
        ChSprocketBand::AddVisualizationAssets(vis);
    }
}

}  // end namespace vehicle
}  // end namespace chrono
