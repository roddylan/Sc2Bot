// BasicSc2BotAttack.cpp
// contains implementation for all offensive (attacking, harrassment, expansion) and attack-like defensive functions

#include "BasicSc2Bot.h"
#include "Utilities.h"
#include "Betweenness.h"
#include "sc2api/sc2_api.h"
#include "sc2api/sc2_unit.h"
#include "sc2api/sc2_interfaces.h"
#include <sc2api/sc2_common.h>
#include <sc2api/sc2_typeenums.h>
#include <sc2api/sc2_unit_filters.h>
#include <sc2lib/sc2_search.h>
#include <iostream>
#include <cmath>

bool BasicSc2Bot::AttackIntruders() {
    const sc2::ObservationInterface* observation = Observation();
    sc2::Units units = observation->GetUnits();
    
    for (auto target : units) {
        if (target->alliance != sc2::Unit::Alliance::Self) {
            sc2::Units myUnits = Observation()->GetUnits(sc2::Unit::Alliance::Self, IsUnit(sc2::UNIT_TYPEID::TERRAN_BUNKER));
            for (auto myUnit : myUnits) {
                Actions()->UnitCommand(myUnit, sc2::ABILITY_ID::BUNKERATTACK, target);
            }
        }
    }
    return true;
}

bool BasicSc2Bot::HandleExpansion() {
    const sc2::ObservationInterface* obs = Observation();
    sc2::Units bases = obs->GetUnits(sc2::Unit::Alliance::Self, sc2::IsTownHall());

    /*
    if (!(obs->GetFoodWorkers() >= n_workers * bases.size() &&
        CountUnitType(sc2::UNIT_TYPEID::TERRAN_MARINE) >= n_marines * bases.size() &&
        CountUnitType(sc2::UNIT_TYPEID::TERRAN_SIEGETANK) >= bases.size())) {
        return false;
    }
    */

    if (bases.size() > 4) {
        return false;
    }

    float min_dist = std::numeric_limits<float>::max();
    sc2::Point3D closest_expansion(0, 0, 0);

    for (const auto& exp : expansion_locations) {
        float cur_dist = sc2::Distance2D(sc2::Point2D(start_location), sc2::Point2D(exp.x, exp.y));
        if (cur_dist < min_dist) {
            sc2::Point2D nearest_command_center = FindNearestCommandCenter(sc2::Point2D(exp.x, exp.y));
            if (nearest_command_center == sc2::Point2D(0, 0)) {
                continue;
            }

            float dist_to_base = sc2::Distance2D(nearest_command_center, sc2::Point2D(exp.x, exp.y));
            std::cout << "distance to base: " << dist_to_base << std::endl;

            if (Query()->Placement(sc2::ABILITY_ID::BUILD_COMMANDCENTER, exp) && dist_to_base > 1.0f) {
                min_dist = cur_dist;
                closest_expansion = exp;
            }
        }
    }

    if (closest_expansion != sc2::Point3D(0, 0, 0)) {
        sc2::Point2D expansion_location(closest_expansion.x, closest_expansion.y);
        sc2::Point2D p(0, 0);

        if (TryBuildStructure(sc2::ABILITY_ID::BUILD_COMMANDCENTER, p, expansion_location)) {
            std::cout << "EXPANSION TIME BABY\n\n";
        }
    }

    return true;
}