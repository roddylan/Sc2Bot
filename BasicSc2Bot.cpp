#include "BasicSc2Bot.h"
#include "Utilities.h"
#include "Betweenness.h"
#include "sc2api/sc2_api.h"
#include "sc2api/sc2_unit.h"
#include "sc2api/sc2_interfaces.h"
#include <_types/_uint32_t.h>
#include <sc2api/sc2_common.h>
#include <sc2api/sc2_typeenums.h>
#include <sc2api/sc2_unit_filters.h>
#include <sc2lib/sc2_search.h>
#include <iostream>
#include <cmath>

void BasicSc2Bot::OnGameStart() {
    const sc2::ObservationInterface *obs = Observation();
    sc2::QueryInterface *query = Query();
    expansion_locations = sc2::search::CalculateExpansionLocations(obs, query);
    start_location = obs->GetStartLocation();
}

void BasicSc2Bot::OnGameFullStart() {
    const sc2::ObservationInterface *obs = Observation();
    sc2::GameInfo gin = obs->GetGameInfo();
    sc2::Point3D start_3d = obs->GetUnits(sc2::Unit::Alliance::Self, IsUnit(sc2::UNIT_TYPEID::TERRAN_COMMANDCENTER))[0]->pos;
    sc2::Point2DI start = sc2::Point2DI(round(start_3d.x), round(start_3d.y));

    // TODO: uncomment
	// sc2::Point2DI pinchpoint = FindPinchPointAroundPoint(obs.pathing_grid, start);
	// sc2::Point2DI pinchpoint = FindPinchPointAroundPoint(gin.pathing_grid, start);
	// PrintMap(gin.pathing_grid, pinchpoint);

    std::cout << "width=" << gin.width << "\nheight=" << gin.height << std::endl;

	return;
}

void BasicSc2Bot::OnStep() {
    // HandleBuild(); // TODO: move rest of build inside
    HandleBuild();
    
    BuildWorkers();
    
    // TryBuildSupplyDepot();
    // TryBuildBarracks();
    // TryBuildRefinery();
    // TryBuildBunker();
    // TryBuildFactory();
    TryBuildSeigeTank();
    AttackIntruders();
    return;
}
sc2::Filter isEnemy = [](const sc2::Unit& unit) {
    return unit.alliance != sc2::Unit::Alliance::Self; 
    };


bool BasicSc2Bot::LoadBunker(const sc2::Unit* marine) {
    sc2::Units myUnits = Observation()->GetUnits(sc2::Unit::Alliance::Self, IsUnit(sc2::UNIT_TYPEID::TERRAN_BUNKER));
    bool bunkerLoaded = false;
    for (auto target : myUnits) {
        if (target->passengers.size() == 4) {
            continue;
        }
        Actions()->UnitCommand(marine, sc2::ABILITY_ID::SMART, target);
        bunkerLoaded = true;
    }
    return bunkerLoaded;
}
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
size_t BasicSc2Bot::CountUnitType(sc2::UNIT_TYPEID unit_type) {
    return Observation()->GetUnits(sc2::Unit::Alliance::Self, IsUnit(unit_type)).size();
}
bool BasicSc2Bot::UpgradeFactoryTechLab(const sc2::Unit* factory) {
        
    Actions()->UnitCommand(factory, sc2::ABILITY_ID::BUILD_TECHLAB_FACTORY);
    
    return true;

}
bool BasicSc2Bot::TryBuildFactory() {
    const sc2::ObservationInterface* observation = Observation();
    
    if (CountUnitType(sc2::UNIT_TYPEID::TERRAN_BARRACKS) < 1) {
        return false;
    }
    if (CountUnitType(sc2::UNIT_TYPEID::TERRAN_FACTORY) > 0) {
        return false;
    }
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

        Actions()->UnitCommand(unit, sc2::ABILITY_ID::TRAIN_SIEGETANK);
    }
    return true;
}

bool BasicSc2Bot::TryBuildBunker() {

    const sc2::ObservationInterface* observation = Observation();

    if (CountUnitType(sc2::UNIT_TYPEID::TERRAN_BARRACKS) < 1) {
        return false;
    }
    if (CountUnitType(sc2::UNIT_TYPEID::TERRAN_BUNKER) > 2) {
        
        return false;
    }

    return TryBuildStructure(sc2::ABILITY_ID::BUILD_BUNKER);
}

bool BasicSc2Bot::TryBuildBarracks() {
    const sc2::ObservationInterface* observation = Observation();

    if (CountUnitType(sc2::UNIT_TYPEID::TERRAN_SUPPLYDEPOT) < 1) {

        return false;
    }

    if (CountUnitType(sc2::UNIT_TYPEID::TERRAN_BARRACKS) > 0) {

        return false;
    }

    return TryBuildStructure(sc2::ABILITY_ID::BUILD_BARRACKS);
}

bool BasicSc2Bot::TryBuildSupplyDepot() {
    const sc2::ObservationInterface* observation = Observation();

    if (CountUnitType(sc2::UNIT_TYPEID::TERRAN_SUPPLYDEPOT) > 2) {
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
    const sc2::Unit* unit_to_build = nullptr;
    sc2::Units units = observation->GetUnits(sc2::Unit::Alliance::Self);
    for (const auto& unit : units) {
        for (const auto& order : unit->orders) {
            if (order.ability_id == sc2::ABILITY_ID::BUILD_REFINERY) {
                return false;
            }
        }

        if (unit->unit_type == sc2::UNIT_TYPEID::TERRAN_SCV) {
            unit_to_build = unit;
        }
    }
    
    const sc2::Unit* target;
    if(unit_to_build != nullptr) {
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

void BasicSc2Bot::OnUnitIdle(const sc2::Unit* unit) {
    // TODO: refactor
    sc2::Units barracks = Observation()->GetUnits(sc2::Unit::Self, sc2::IsUnits({sc2::UNIT_TYPEID::TERRAN_BARRACKS, sc2::UNIT_TYPEID::TERRAN_BARRACKSFLYING}));
    sc2::Units bases = Observation()->GetUnits(sc2::Unit::Self, sc2::IsTownHall());

    switch (unit->unit_type.ToType()) {
    // case sc2::UNIT_TYPEID::TERRAN_COMMANDCENTER: {
    //     if (Observation()->GetFoodWorkers() > (n_workers * bases.size()) && !barracks.empty()){
    //         // std::cout << n_workers * bases.size() << std::endl;
    //         // TODO: refactor and move
    //     } else {
    //         sc2::Agent::Actions()->UnitCommand(unit, sc2::ABILITY_ID::TRAIN_SCV);
    //     }
    //     break;
    // }
    // case sc2::UNIT_TYPEID::TERRAN_ORBITALCOMMAND: {
    //     // std::cout << "ORBITAL COMMAND\n";
    //     sc2::Agent::Actions()->UnitCommand(unit, sc2::ABILITY_ID::TRAIN_SCV);
    // }
    case sc2::UNIT_TYPEID::TERRAN_SCV: {
        AssignWorkers(unit);
        break;
    }
    case sc2::UNIT_TYPEID::TERRAN_MULE: {
        AssignWorkers(unit);
        break;
    }
    case sc2::UNIT_TYPEID::TERRAN_BARRACKS: {
        Actions()->UnitCommand(unit, sc2::ABILITY_ID::TRAIN_MARINE);
        break;
    }
    case sc2::UNIT_TYPEID::TERRAN_MARINE: {
        // if the bunkers are full
        
        if (!LoadBunker(unit)) {
            const sc2::GameInfo& game_info = Observation()->GetGameInfo();
            Actions()->UnitCommand(unit, sc2::ABILITY_ID::ATTACK_ATTACK
            , game_info.enemy_start_locations.front(), true);
            // std::cout << "sent";
        }
        
        break;
    }
    case sc2::UNIT_TYPEID::TERRAN_FACTORY:{
        UpgradeFactoryTechLab(unit);
    }
    default: {
        break;
    }
    }
}

sc2::Filter scvFilter = [](const sc2::Unit& unit) {
    return unit.unit_type == sc2::UNIT_TYPEID::TERRAN_SCV;
    };

const sc2::Unit* BasicSc2Bot::FindNearestMineralPatch(const sc2::Point2D& start) {
    sc2::Units units = Observation()->GetUnits(sc2::Unit::Alliance::Neutral);
    float distance = std::numeric_limits<float>::max();
    const sc2::Unit* target = nullptr;
    for (const auto& u : units) {
        if (u->unit_type == sc2::UNIT_TYPEID::NEUTRAL_MINERALFIELD) {
            float d = sc2::DistanceSquared2D(u->pos, start);
            if (d < distance) {
                distance = d;
                target = u;
            }
        }
    }
    return target;
}
const sc2::Unit* BasicSc2Bot::FindNearestVespeneGeyser(const sc2::Point2D& start) {
    sc2::Units units = Observation()->GetUnits(sc2::Unit::Alliance::Neutral);
    float distance = std::numeric_limits<float>::max();
    const sc2::Unit* target = nullptr;
    for (const auto& u : units) {
        if (u->unit_type == sc2::UNIT_TYPEID::NEUTRAL_VESPENEGEYSER) {
            float d = sc2::DistanceSquared2D(u->pos, start);
            if (d < distance) {
                distance = d;
                target = u;
            }
        }
    }
    return target;
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

    sc2::Units techlab_factory = obs->GetUnits(sc2::Unit::Self, sc2::IsUnit(sc2::UNIT_TYPEID::TERRAN_FACTORYTECHLAB));
    sc2::Units techlab_starports = obs->GetUnits(sc2::Unit::Self, sc2::IsUnit(sc2::UNIT_TYPEID::TERRAN_STARPORTTECHLAB));
    // TODO: handle other building


    // current counts
    uint32_t n_minerals = obs->GetMinerals();
    uint32_t n_gas = obs->GetMinerals();

    // goal amnts
    const size_t n_barracks_target = 1;
    const size_t n_factory_target = 1;
    const size_t n_armory_target = 1;
    const size_t n_engg_target = 1;

    // Handle Orbital Command

    if (!barracks.empty()) {
        for (const auto &base : bases) {
            if (n_minerals > 150) {
                Actions()->UnitCommand(base, sc2::ABILITY_ID::MORPH_ORBITALCOMMAND);
            }
        }
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

    /*
    TryBuildSupplyDepot();
    TryBuildBarracks();
    TryBuildRefinery();
    TryBuildBunker();
    TryBuildFactory();
    */

    // if (!barracks.empty() && factory.size() < n_factory_target)

    // build engg bay for missile turret
    // TODO: improve count
    if (CountUnitType(sc2::UNIT_TYPEID::TERRAN_ENGINEERINGBAY) < bases.size() * 2) {
        if (n_minerals > 150 && n_gas > 100) {
            std::cout << "building engg bay\n\n";
            TryBuildStructure(sc2::ABILITY_ID::BUILD_ENGINEERINGBAY);
        }
    }


}

void BasicSc2Bot::BuildWorkers() {
    const sc2::ObservationInterface *obs = Observation();
    sc2::Units bases = obs->GetUnits(sc2::Unit::Alliance::Self, sc2::IsTownHall());
    
    // build MULEs
    for (const auto &base : bases) {
        if (base->unit_type == sc2::UNIT_TYPEID::TERRAN_ORBITALCOMMAND || base->unit_type == sc2::UNIT_TYPEID::TERRAN_ORBITALCOMMANDFLYING) {
            // if we havent reached goal amnt
            if (CountUnitType(sc2::UNIT_TYPEID::TERRAN_MULE) < bases.size() * n_mules && base->energy > 50) {
                // if we find a nearby mineral patch
                if (FindNearestMineralPatch(base->pos)) {
                    Actions()->UnitCommand(base, sc2::ABILITY_ID::EFFECT_CALLDOWNMULE);
                }
            } else if (base->energy > 75) {
                // still create mules, but larger energy req
                if (FindNearestMineralPatch(base->pos)) {
                    Actions()->UnitCommand(base, sc2::ABILITY_ID::EFFECT_CALLDOWNMULE);
                }
            }
        }
    }

    for (const auto &base : bases) {
        // build SCV
        // TODO: maybe just use ideal_harvesters (max)
        if (base->assigned_harvesters < base->ideal_harvesters && base->build_progress == 1 && base->orders.empty()) {
            if (obs->GetMinerals() >= 50) {
                sc2::Agent::Actions()->UnitCommand(base, sc2::ABILITY_ID::TRAIN_SCV);
            }
        }
    }
}

void BasicSc2Bot::AssignWorkers(const sc2::Unit *unit) {
    const sc2::ObservationInterface *obs = Observation();
    const sc2::Units refineries = obs->GetUnits(sc2::Unit::Alliance::Self, sc2::IsUnit(sc2::UNIT_TYPEID::TERRAN_REFINERY));
    const sc2::Units bases = obs->GetUnits(sc2::Unit::Alliance::Self, sc2::IsTownHall());
    const sc2::Unit* mineral_target;


    if (unit->unit_type == sc2::UNIT_TYPEID::TERRAN_SCV) {
        for (const auto &refinery : refineries) {
            if (refinery->assigned_harvesters < refinery->ideal_harvesters) {
                // std::cout << "refinery\n";
                Actions()->UnitCommand(unit, sc2::ABILITY_ID::HARVEST_GATHER, refinery);
            }
            std::cout << refinery->assigned_harvesters << " : " << refinery->ideal_harvesters << std::endl;
        }

        for (const auto &base : bases) {
            // if building
            if (base->build_progress != 1) {
                continue;
            }
            // TODO: maybe just use ideal_harvesters (max)
            if (base->assigned_harvesters < base->ideal_harvesters) {
                mineral_target = FindNearestMineralPatch(unit->pos);
                Actions()->UnitCommand(unit, sc2::ABILITY_ID::SMART, mineral_target);
                return;
            }
            
        }
    }

    if (unit->unit_type == sc2::UNIT_TYPEID::TERRAN_MULE) {
        mineral_target = FindNearestMineralPatch(unit->pos);
        Actions()->UnitCommand(unit, sc2::ABILITY_ID::SMART, mineral_target);
    }
}


bool BasicSc2Bot::HandleExpansion() {
    const sc2::ObservationInterface *obs = Observation();
    sc2::Units bases = obs->GetUnits(sc2::Unit::Alliance::Self, sc2::IsTownHall());
    

    // dont need to expand past 4
    // TODO: add more timing logic + expand based on scout
    if (bases.size() > 4) {
        return false;
    }

    // dont expand if not enough workers, marines, siege tank
    // TODO: change limits
    if (!(obs->GetFoodWorkers() >= n_workers * bases.size() && CountUnitType(sc2::UNIT_TYPEID::TERRAN_MARINE) >= n_marines * bases.size() && CountUnitType(sc2::UNIT_TYPEID::TERRAN_SIEGETANK) >= bases.size() )) {
        return false;
    }

    // find nearest expansion location
    float min_dist = std::numeric_limits<float>::max();
    sc2::Point3D closest_expansion;

    for (const auto &exp : expansion_locations) {
        float cur_dist = sc2::Distance2D(start_location, exp);
        
        if (cur_dist < min_dist) {
            // check if actually able to expand here
            if (!(Query()->Placement(sc2::ABILITY_ID::BUILD_COMMANDCENTER, exp))) {
                min_dist = cur_dist;
                closest_expansion = exp;
            }
        }
    }

    if (TryBuildStructure(sc2::ABILITY_ID::BUILD_COMMANDCENTER, closest_expansion)) {
        std::cout << "EXPANSION TIME BABY\n\n";
    }



    return true;

}

bool BasicSc2Bot::TryBuildStructure(sc2::ABILITY_ID ability_type_for_structure, sc2::Point2D location, bool isExpansion) {
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

    // check if scv can get there
    // todo: fix
    if (Query()->PathingDistance(unit_to_build, location) < 0.1F) {
        return false;
    }

    if (Query()->Placement(ability_type_for_structure, location)) {
        Actions()->UnitCommand(unit_to_build, ability_type_for_structure, location);
        return true;
    }
    return false;
}