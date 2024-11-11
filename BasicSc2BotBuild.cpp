// BasicSc2BotBuild.cpp
// contains implementation for all build/upgrade related functions

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

bool BasicSc2Bot::UpgradeFactoryTechLab(const sc2::Unit* factory) {
        
    Actions()->UnitCommand(factory, sc2::ABILITY_ID::BUILD_TECHLAB_FACTORY);
    
    return true;

}

bool BasicSc2Bot::TryBuildFactory() {
    const sc2::ObservationInterface* observation = Observation();
    
    if (CountUnitType(sc2::UNIT_TYPEID::TERRAN_BARRACKS) < 1) {
        return false;
    }
    /*
    if (CountUnitType(sc2::UNIT_TYPEID::TERRAN_FACTORY) > 0) {
        return false;
    }
    */
    if (observation->GetVespene() < 100) {
        return false;
    }
    return TryBuildStructure(sc2::ABILITY_ID::BUILD_FACTORY);
}

bool BasicSc2Bot::TryBuildSeigeTank() {
    const sc2::ObservationInterface* observation = Observation();

    if (CountUnitType(sc2::UNIT_TYPEID::TERRAN_FACTORY) < 1) {
        return false;
    }
    if (observation->GetVespene() < 125) {
        return false;
    }
    sc2::Units units = observation->GetUnits(sc2::Unit::Alliance::Self, IsUnit(sc2::UNIT_TYPEID::TERRAN_FACTORY));
    for (auto unit : units) {
        if (CountNearbySeigeTanks(unit) > 0 && units.size() > 1) continue;
        Actions()->UnitCommand(unit, sc2::ABILITY_ID::TRAIN_SIEGETANK);
    }
    return true;
}

bool BasicSc2Bot::TryBuildBunker() {

    const sc2::ObservationInterface* observation = Observation();


    if (CountUnitType(sc2::UNIT_TYPEID::TERRAN_BARRACKS) < 1) {
        return false;
    }
    /*
    if (CountUnitType(sc2::UNIT_TYPEID::TERRAN_BUNKER) > 2) {
        
        return false;
    }
    */
    return TryBuildStructure(sc2::ABILITY_ID::BUILD_BUNKER);
}

bool BasicSc2Bot::TryBuildBarracks() {
    const sc2::ObservationInterface* observation = Observation();

    if (CountUnitType(sc2::UNIT_TYPEID::TERRAN_SUPPLYDEPOT) < 1) {
       // std::cout << "supply dept < 1" << std::endl;

        return false;
    }

    if (CountUnitType(sc2::UNIT_TYPEID::TERRAN_BARRACKS) > 0) {
        return false;
    }
    return TryBuildStructure(sc2::ABILITY_ID::BUILD_BARRACKS);
}

bool BasicSc2Bot::TryBuildSupplyDepot() {
    const sc2::ObservationInterface* observation = Observation();
    // Lower + normal supply depots = total # of depots


    size_t n_supply_depots = observation->GetUnits(sc2::Unit::Alliance::Self, sc2::IsUnit(sc2::UNIT_TYPEID::TERRAN_SUPPLYDEPOT)).size();
    size_t n_lower_supply_depots = observation->GetUnits(sc2::Unit::Alliance::Self, sc2::IsUnit(sc2::UNIT_TYPEID::TERRAN_SUPPLYDEPOTLOWERED)).size();
    std::cout << "n suppply depots " << n_supply_depots << std::endl;
    size_t n_bases = observation->GetUnits(sc2::Unit::Alliance::Self, sc2::IsTownHall()).size();
    std::cout << "n basess " << n_bases << std::endl;
    // make a new supply depot if we are at 2/3 unit capacity
    uint32_t current_supply_use = observation->GetFoodUsed();
    uint32_t max_supply = observation->GetFoodCap();

    if (3 * current_supply_use < 2 * max_supply) {
        // do not build if current_supply_use/max_suply < 2/3
        return false;
    }
    // do not build if theres more than 2 per base
    if ((n_supply_depots + n_lower_supply_depots) >= 2 * n_bases) {
        return false;
    }

    if (observation->GetMinerals() < 100) {
        return false;
    }

    return TryBuildStructure(sc2::ABILITY_ID::BUILD_SUPPLYDEPOT);
}


bool BasicSc2Bot::TryBuildRefinery() {
    const sc2::ObservationInterface* observation = Observation();
    // if we already have 2 refineries -> dont build refinery...this will have to change for later in the game

    if (CountUnitType(sc2::UNIT_TYPEID::TERRAN_REFINERY) > 1) {
        return false;
    }

    return BuildRefinery();
}

bool BasicSc2Bot::BuildRefinery() {
    const sc2::ObservationInterface* observation = Observation();
    const sc2::Unit* unit_to_build = GetGatheringScv();

    const sc2::Unit* target;
    if (unit_to_build != nullptr) {
        const sc2::Unit* target = FindNearestVespeneGeyser(unit_to_build->pos);
        Actions()->UnitCommand(unit_to_build, sc2::ABILITY_ID::BUILD_REFINERY,
            target);
    }

    return true;
}

bool BasicSc2Bot::TryBuildStructure(sc2::ABILITY_ID ability_type_for_structure, sc2::UNIT_TYPEID unit_type) {
    const sc2::ObservationInterface* observation = Observation();

    const sc2::Unit* unit_to_build = nullptr;
    sc2::Units units = observation->GetUnits(sc2::Unit::Alliance::Self);
    sc2::Units bases = Observation()->GetUnits(sc2::Unit::Self, sc2::IsTownHall());
    for (const auto& unit : units) {
        for (const auto& order : unit->orders) {
            if (order.ability_id == ability_type_for_structure) {
                return false;
            }
        }

        if (unit->unit_type == unit_type) {
            unit_to_build = unit;
        }

    }


    float rx = sc2::GetRandomScalar();
    float ry = sc2::GetRandomScalar();
    sc2::Point2D nearest_command_center = FindNearestCommandCenter(unit_to_build->pos, true);
    sc2::Point2D start_location = bases.size() > 1 && nearest_command_center != sc2::Point2D(0, 0) ? nearest_command_center : sc2::Point2D(this->start_location.x, this->start_location.y);
    switch (unit_type) {
    case sc2::UNIT_TYPEID::TERRAN_BUNKER: {
        Actions()->ToggleAutocast(unit_to_build->tag, sc2::ABILITY_ID::BUNKERATTACK);
        break;
    }
    // TODO: fix placement so far enough away enough from obstructions so tech lab can be built on it
    case sc2::UNIT_TYPEID::TERRAN_FACTORY: {

        
        Actions()->UnitCommand(unit_to_build, ability_type_for_structure,
            sc2::Point2D(start_location.x + rx * 15.0F, start_location.y + ry * 15.0F));
            // sc2::Point2D(unit_to_build->pos.x + 70000000, unit_to_build->pos.y + 7000000000));
        return true;
    }
    default: {
        break;
    }
    }

    Actions()->UnitCommand(unit_to_build, ability_type_for_structure,
        sc2::Point2D(start_location.x + rx * 15.0F, start_location.y + ry * 15.0F));
        // sc2::Point2D(unit_to_build->pos.x + rx * sc2::GetRandomScalar(), unit_to_build->pos.y + ry * sc2::GetRandomScalar()));
        // sc2::Point2D(unit_to_build->pos.x + rx * 30.0F, unit_to_build->pos.y + ry * 30.0F));
    

    return true;
}

bool BasicSc2Bot::TryBuildStructure(sc2::ABILITY_ID ability_type_for_structure, sc2::Point2D location, sc2::Point2D expansion_starting_point) {
    const sc2::ObservationInterface *obs = Observation();
    sc2::Units workers = obs->GetUnits(sc2::Unit::Alliance::Self, IsUnit(sc2::UNIT_TYPEID::TERRAN_SCV));

    // no workers
    if (workers.empty()) {
        return false;
    }

    const sc2::Unit* unit_to_build = nullptr;
    for (const auto& worker : workers) {
        for (const auto& order : worker->orders) {
            if (order.ability_id == ability_type_for_structure) {
                return false;
            }
        }
        
        unit_to_build = worker;
    }
    // todo: this as possible fix?
    float rx = sc2::GetRandomScalar();
    float ry = sc2::GetRandomScalar();
  
    sc2::Point2D nearestCommandCenter = unit_to_build != nullptr ? FindNearestCommandCenter(unit_to_build->pos) : start_location;
    sc2::Point2D point(nearestCommandCenter.x + rx, nearestCommandCenter.y + ry);
    if (expansion_starting_point != sc2::Point2D(0, 0)) {
        point = expansion_starting_point;
    }

    if (ability_type_for_structure == sc2::ABILITY_ID::BUILD_BUNKER) {
        int map_height = obs->GetGameInfo().height;
        int map_width = obs->GetGameInfo().width;
        const size_t bunker_cost = 100;
        if (obs->GetMinerals() < 4 * bunker_cost) {
            return false;
        }
        bool all_bunkers_placed = true;

        sc2::Point2D direction = sc2::Point2D(0, 1); 
        float distance_between_bunkers = 10.0f;

        for (int i = 0; i < 4; i++) {
            sc2::Point2D placement_point = nearestCommandCenter + direction * (i * distance_between_bunkers);
            bool found_valid_placement = false;

            // Check if the placement point is valid
            if (Query()->Placement(ability_type_for_structure, placement_point, unit_to_build)) {
                point = placement_point;
                found_valid_placement = true;
            }
            if (found_valid_placement) {
                Actions()->UnitCommand(unit_to_build, ability_type_for_structure, point);
            }
            else {
                all_bunkers_placed = false;
                break;
            }
        }

        return all_bunkers_placed;
    }
   
    
    while (!Query()->Placement(ability_type_for_structure, point, unit_to_build)) {
        point.x += 10;
        point.y += 10;
    }
    if (ability_type_for_structure == sc2::ABILITY_ID::BUILD_COMMANDCENTER) {
        sc2::Point3D expansion_point(point.x, point.y, 0);

    }
    Actions()->UnitCommand(unit_to_build, ability_type_for_structure, point);

    // check if scv can get there
    // todo: fix
    /*
    if (Query()->PathingDistance(unit_to_build, location) < 0.1F) {
        return false;
    }

    if (Query()->Placement(ability_type_for_structure, location)) {
        Actions()->UnitCommand(unit_to_build, ability_type_for_structure, location);
        return true;
    }
    */
    return true;
}

bool BasicSc2Bot::TryBuildMissileTurret() {
    // TODO: make it so it only builds missiles around each expansion and not favour the first one
    const sc2::ObservationInterface* observation = Observation();

    sc2::Units engineering_bays = observation->GetUnits(sc2::Unit::Alliance::Self, sc2::IsUnit(sc2::UNIT_TYPEID::TERRAN_ENGINEERINGBAY));

    if (engineering_bays.empty()) {
        return false;  
    }
    if (observation->GetMinerals() < 75) {
        return false;
    }
    size_t max_turrets_per_base = 1;
    size_t base_count = observation->GetUnits(sc2::Unit::Alliance::Self, sc2::IsTownHall()).size();
    size_t turret_count = observation->GetUnits(sc2::Unit::Alliance::Self, sc2::IsUnit(sc2::UNIT_TYPEID::TERRAN_MISSILETURRET)).size();
    if (max_turrets_per_base * base_count < turret_count) {
        return false;
    }
    return TryBuildStructure(sc2::ABILITY_ID::BUILD_MISSILETURRET);

}


/**
 * @brief Handle unit upgrades
 * 
 */
void BasicSc2Bot::HandleUpgrades() {
    const sc2::ObservationInterface *obs = Observation();

    auto cur_upgrades = obs->GetUpgrades();

    // get no. of bases
    size_t base_count = obs->GetUnits(sc2::Unit::Alliance::Self, sc2::IsTownHall()).size();
}

/**
 * @brief Handle building logic
 * 
 */
void BasicSc2Bot::HandleBuild() {
    // mineral costs
    static const uint32_t SUPPLY_DEPOT_COST = 100;
    static const uint32_t REFINERY_COST = 75;
    static const uint32_t FACTORY_MINERAL_COST = 150;
    static const uint32_t BUNKER_COST = 100;
    static const uint32_t BARRACKS_COST = 150;
    static const uint32_t ORBITAL_COMMAND_COST = 150;

    // vespene gas costs
    static const uint32_t FACTORY_GAS_COST = 100;
    
    const sc2::ObservationInterface *obs = Observation();

    // track each type of unit
    sc2::Units bases = obs->GetUnits(sc2::Unit::Self, sc2::IsTownHall());
    // TODO: separate barracks, factory, starports from techlab
    sc2::Units barracks = obs->GetUnits(sc2::Unit::Self, sc2::IsUnits({sc2::UNIT_TYPEID::TERRAN_BARRACKS, sc2::UNIT_TYPEID::TERRAN_BARRACKSFLYING}));
    sc2::Units factory = obs->GetUnits(sc2::Unit::Self, sc2::IsUnits({sc2::UNIT_TYPEID::TERRAN_FACTORY, sc2::UNIT_TYPEID::TERRAN_FACTORYFLYING, sc2::UNIT_TYPEID::TERRAN_FACTORYREACTOR, sc2::UNIT_TYPEID::TERRAN_FACTORYTECHLAB}));
    sc2::Units starports = obs->GetUnits(sc2::Unit::Self, sc2::IsUnits({sc2::UNIT_TYPEID::TERRAN_STARPORT, sc2::UNIT_TYPEID::TERRAN_STARPORTFLYING, sc2::UNIT_TYPEID::TERRAN_STARPORTREACTOR, sc2::UNIT_TYPEID::TERRAN_STARPORTTECHLAB}));
    sc2::Units engg_bays = obs->GetUnits(sc2::Unit::Self, sc2::IsUnit(sc2::UNIT_TYPEID::TERRAN_ENGINEERINGBAY));
    sc2::Units bunkers = obs->GetUnits(sc2::Unit::Self, sc2::IsUnit(sc2::UNIT_TYPEID::TERRAN_BUNKER));
    sc2::Units techlab_factory = obs->GetUnits(sc2::Unit::Self, sc2::IsUnit(sc2::UNIT_TYPEID::TERRAN_FACTORYTECHLAB));
    sc2::Units techlab_starports = obs->GetUnits(sc2::Unit::Self, sc2::IsUnit(sc2::UNIT_TYPEID::TERRAN_STARPORTTECHLAB));
    // TODO: handle other building


    // current counts
    uint32_t n_minerals = obs->GetMinerals();
    uint32_t n_gas = obs->GetMinerals();

    // goal amnts
    // TODO: change target amounts
    const size_t n_barracks_target = 2;
    const size_t n_factory_target = 2;
    const size_t n_armory_target = 2;
    const size_t n_engg_target = 1;
    const size_t n_bunkers_target = 8;
    
    // Handle Orbital Command

    if (!barracks.empty()) {
        for (const auto &base : bases) {
            //std::cout << "inseting pos: " << base->pos.x << " " << base->pos.y << " " << base->pos.z << std::endl;
            if (n_minerals > 150) {
                Actions()->UnitCommand(base, sc2::ABILITY_ID::MORPH_ORBITALCOMMAND);
            }
        }
    }
    
    if (n_minerals >= 400) {
        HandleExpansion();
    }

    // for (const auto &barrack : barracks) {
    //     // check if busy or building
    //     if (!barrack->orders.empty() || barrack->build_progress != 1) {
    //         return;
    //     }
    //     // techlab
    //     // TODO: reactor
    //     // Actions()->UnitCommand(barrack, sc2::ABILITY_ID::BUILD_TECHLAB_BARRACKS);
    // }
    
    // build supply depot

    // build barracks
    //std::cout << "n_workers=" << obs->GetFoodWorkers() << std::endl;
    if (barracks.size() < n_barracks_target * bases.size()) {
        if (obs->GetFoodWorkers() >= n_workers_init * bases.size()) {
           // std::cout << "building barracks\n\n";
            TryBuildBarracks();
        }
    }
    if (bunkers.size() < n_bunkers_target * bases.size() && n_minerals >= BUNKER_COST) {
       
        sc2::Units scvs = obs->GetUnits(sc2::Unit::Self, sc2::IsUnit(sc2::UNIT_TYPEID::TERRAN_SCV));
        bool bunker_built = false;

        for (auto& scv : scvs) {
            bool is_building_scv = false;
            for (const auto& order : scv->orders) {
                if (order.ability_id == sc2::ABILITY_ID::TRAIN_SCV) {
                    is_building_scv = true;
                    break;
                }
            }

            if (scv->orders.empty() && !is_building_scv) {
                TryBuildBunker();
                bunker_built = true;
                break;
            }
        }

        if (bunker_built) {
            // SCV already assigned to build bunker
        }
       
    }

    // build factory
    if (!barracks.empty() && factory.size() < (n_factory_target * bases.size())) {
        if (n_minerals > FACTORY_MINERAL_COST && n_gas > FACTORY_GAS_COST) {
            //std::cout << "building factory\n\n";
            TryBuildFactory();
        }
    }
    

    // build engg bay for missile turret
    // TODO: improve count
    if (engg_bays.size() < bases.size() * n_engg_target) {
        if (n_minerals > 150 && n_gas > 100) {
            std::cout << "building engg bay\n\n";
            TryBuildStructure(sc2::ABILITY_ID::BUILD_ENGINEERINGBAY);
        }
    }


}