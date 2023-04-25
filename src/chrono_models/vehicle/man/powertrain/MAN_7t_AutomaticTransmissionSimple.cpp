// =============================================================================
// PROJECT CHRONO - http://projectchrono.org
//
// Copyright (c) 2023 projectchrono.org
// All rights reserved.
//
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file at the top level of the distribution and at
// http://projectchrono.org/license-chrono.txt.
//
// =============================================================================
// Authors: Radu Serban, Rainer Gericke
// =============================================================================
//
// Simple automatic transmission to use EngineSimple.
// - no torque converter
// - identical to AutomaticTransmissionSimpleMap, but only one gear
//
// =============================================================================

#include "chrono_models/vehicle/man/powertrain/MAN_7t_AutomaticTransmissionSimple.h"

namespace chrono {
namespace vehicle {
namespace man {

const double rpm2rads = CH_C_PI / 30;

MAN_7t_AutomaticTransmissionSimple::MAN_7t_AutomaticTransmissionSimple(const std::string& name)
    : ChAutomaticTransmissionSimpleMap(name) {}

void MAN_7t_AutomaticTransmissionSimple::SetGearRatios(std::vector<double>& fwd, double& rev) {
    rev = -0.167;

    fwd.push_back(0.157);
}

void MAN_7t_AutomaticTransmissionSimple::SetShiftPoints(std::vector<std::pair<double, double>>& shift_bands) {
    shift_bands.push_back(std::pair<double, double>(1000 * rpm2rads, 2200 * rpm2rads));
}

}  // namespace man
}  // end namespace vehicle
}  // end namespace chrono
