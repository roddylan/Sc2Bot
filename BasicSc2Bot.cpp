// BasicSc2Bot.cpp
// Implementation for main loop stuff (game start, step, idle, etc.)

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


void BasicSc2Bot::OnGameStart() {
    const sc2::ObservationInterface *obs = Observation();
    sc2::QueryInterface *query = Query();
    expansion_locations = sc2::search::CalculateExpansionLocations(obs, query);
    start_location = obs->GetStartLocation();
    base_location = start_location;
    scout = nullptr; // no scout initially
    unexplored_enemy_starting_locations = Observation()->GetGameInfo().enemy_start_locations;
    enemy_starting_location = nullptr;  // we use a scout to find this
}

void BasicSc2Bot::OnGameFullStart() {
    const sc2::ObservationInterface *obs = Observation();
    sc2::GameInfo gin = obs->GetGameInfo();
    sc2::Point3D start_3d = obs->GetUnits(sc2::Unit::Alliance::Self, IsUnit(sc2::UNIT_TYPEID::TERRAN_COMMANDCENTER))[0]->pos;
    sc2::Point2DI start = sc2::Point2DI(round(start_3d.x), round(start_3d.y));
    /*
	sc2::Point2DI pinchpoint = FindPinchPointAroundPoint(obs->GetGameInfo().pathing_grid, start, 3, 16);
	PrintMap(obs->GetGameInfo().pathing_grid ,pinchpoint);
    */
	return;
}

void BasicSc2Bot::OnStep() {
    // HandleBuild(); // TODO: move rest of build inside
    const sc2::ObservationInterface *obs = Observation();
    sc2::Units bases = obs->GetUnits(sc2::Unit::Self, sc2::IsTownHall());
    // skip a few frames for speed; avoid duplicate commands
    int skip_frame = 5;

    if (obs->GetGameLoop() % skip_frame) {
        return;
    }

    // **NOTE** order matters as the amount of minerals we have gets consumed, seige tanks are important to have at each expansion 
    HandleBuild();
    
    BuildWorkers();
    RecheckUnitIdle();

    CheckScoutStatus();
    AttackIntruders();
    
    // if (TryBuildSeigeTank()) {
    //     return;
    // }
    // if (TryBuildMissileTurret()) {
    //     return;
    // }
    if (TryBuildSupplyDepot()) {
        return;
    }
    if (TryBuildRefinery()) {
        return;
    }
    return;
}

/*
* The OnUnitIdle hook that's automatically called by the game is only called ONCE when the unit starts idling.
* This is an issue for barracks because when OnUnitIdle is called for them but they don't have the resources to
* train a unit, they won't take an action and OnUnitIdle is never called on them again so they never get a kick to
* start training when resources are available.
*/
void BasicSc2Bot::RecheckUnitIdle() {
    const sc2::Units& idle_units = Observation()->GetUnits(sc2::Unit::Alliance::Self, [](const sc2::Unit& unit) {
        return unit.orders.empty();
    });
    for (const sc2::Unit* unit : idle_units) {
        OnUnitIdle(unit);
    }
}

void BasicSc2Bot::OnUnitCreated(const sc2::Unit* unit) {
    switch (unit->unit_type.ToType()) {
    case sc2::UNIT_TYPEID::TERRAN_SCV: {
        TryScouting(*unit);
        break;
    }
    case sc2::UNIT_TYPEID::TERRAN_SUPPLYDEPOT: {
        /*
         * For now, lower all supply depots by default
         * In the future, maybe we can take advantage of raising/lowering them to control movement
         */
        //std::cout << "supply depot created!" << std::endl;
        Actions()->UnitCommand(unit, sc2::ABILITY_ID::MORPH_SUPPLYDEPOT_LOWER);
        break;
    }
    case sc2::UNIT_TYPEID::TERRAN_MEDIVAC: {
        const sc2::Unit* injured_marine = FindInjuredMarine();
        if (injured_marine) {
            Actions()->UnitCommand(unit, sc2::ABILITY_ID::SMART, injured_marine);
            return;
        }
        sc2::Point2D largest_marine_cluster = FindLargestMarineCluster(unit->pos, *unit);
        if (largest_marine_cluster == sc2::Point2D(0, 0)) return;
        Actions()->UnitCommand(unit, sc2::ABILITY_ID::SMART, largest_marine_cluster);
        break;

    }
    case sc2::UNIT_TYPEID::TERRAN_THOR: {
        // TODO: thor should go to choke point when created?
        sc2::Point2D largest_marine_cluster = FindLargestMarineCluster(unit->pos, *unit);
        if (largest_marine_cluster == sc2::Point2D(0, 0)) return;
        Actions()->UnitCommand(unit, sc2::ABILITY_ID::SMART, largest_marine_cluster);
        break;
    }
    case sc2::UNIT_TYPEID::TERRAN_MARINE: {
        // TODO: marine should go to choke point when created
        sc2::Point2D largest_marine_cluster = FindLargestMarineCluster(unit->pos, *unit);
        if (largest_marine_cluster == sc2::Point2D(0, 0)) return;
        Actions()->UnitCommand(unit, sc2::ABILITY_ID::SMART, largest_marine_cluster);
        break;
    }
    }
    
}

void BasicSc2Bot::OnUnitDestroyed(const sc2::Unit* unit) {
    static int mineral_fields_destoryed;
    if (unit->mineral_contents == 0) {
        ++mineral_fields_destoryed;
        std::cout << "mineral_destoryed count " << mineral_fields_destoryed << std::endl;
        if (mineral_fields_destoryed % 5) {
            HandleExpansion(true);
        }
        std::cout << "Minerals destroyed" << std::endl;
        

    }
    if (unit == this->scout) {
        // the scout was destroyed, so we found the base!
        const sc2::GameInfo& info = Observation()->GetGameInfo();
        sc2::Point2D closest_base_position = info.enemy_start_locations[0];
        for (const sc2::Point2D& position : info.enemy_start_locations) {
            if (sc2::DistanceSquared2D(unit->pos, position) < sc2::DistanceSquared2D(unit->pos, closest_base_position)) {
                closest_base_position = position;
            }
        }
        this->enemy_starting_location = &closest_base_position;
    }
}

void BasicSc2Bot::OnUnitIdle(const sc2::Unit* unit) {
    // TODO: refactor
    sc2::Units barracks = Observation()->GetUnits(sc2::Unit::Self, sc2::IsUnits({ sc2::UNIT_TYPEID::TERRAN_BARRACKS, sc2::UNIT_TYPEID::TERRAN_BARRACKSFLYING }));
    sc2::Units bases = Observation()->GetUnits(sc2::Unit::Self, sc2::IsTownHall());

    switch (unit->unit_type.ToType()) {
    case sc2::UNIT_TYPEID::TERRAN_SCV: {
        AssignWorkers(unit);
        if (TryScouting(*unit)) {
            break;
        }
        AssignWorkers(unit);
        break;
    }
    case sc2::UNIT_TYPEID::TERRAN_STARPORT: {
        AssignStarportAction(*unit);
        break;
    }
    case sc2::UNIT_TYPEID::TERRAN_MEDIVAC: {
        int skip_frame = 10;

        if (Observation()->GetGameLoop() % skip_frame) {
            return;
        }
        const sc2::Unit* injured_marine = FindInjuredMarine();
        if (injured_marine) {
            Actions()->UnitCommand(unit, sc2::ABILITY_ID::SMART, injured_marine);
            return;
        }
        sc2::Point2D largest_marine_cluster = FindLargestMarineCluster(unit->pos, *unit);
        if (largest_marine_cluster == sc2::Point2D(0, 0)) return;
        Actions()->UnitCommand(unit, sc2::ABILITY_ID::SMART, largest_marine_cluster);
        break;

    }
    case sc2::UNIT_TYPEID::TERRAN_ORBITALCOMMAND: {
        // std::cout << "ORBITAL COMMAND\n";
        //sc2::Agent::Actions()->UnitCommand(unit, sc2::ABILITY_ID::TRAIN_SCV);
        break;
    }
    // case sc2::UNIT_TYPEID::TERRAN_MULE: {
    //     AssignWorkers(unit);
    //     break;
    // }
    case sc2::UNIT_TYPEID::TERRAN_SUPPLYDEPOT: {
       // std::cout << "SUPPLY DEPOT IDLE" << std::endl;
        Actions()->UnitCommand(unit, sc2::ABILITY_ID::MORPH_SUPPLYDEPOT_LOWER);
        break;
    }
    case sc2::UNIT_TYPEID::TERRAN_BARRACKS: {
        AssignBarrackAction(*unit);
        break;
    }
    case sc2::UNIT_TYPEID::TERRAN_BARRACKSTECHLAB: {
        AssignBarrackTechLabAction(*unit);
        break;
    }
    case sc2::UNIT_TYPEID::TERRAN_ENGINEERINGBAY: {
        AssignEngineeringBayAction(*unit);
        break;
    }
    case sc2::UNIT_TYPEID::TERRAN_MARINE: {
        // if the bunkers are full

        if (!LoadBunker(unit)) {
            const sc2::GameInfo& game_info = Observation()->GetGameInfo();
            /*Actions()->UnitCommand(unit, sc2::ABILITY_ID::ATTACK_ATTACK
                , game_info.enemy_start_locations.front(), true);*/
            // std::cout << "sent";
        }

        break;
    }
    case sc2::UNIT_TYPEID::TERRAN_FACTORY: {
        UpgradeFactoryTechLab(unit);
    }
    default: {
        break;
    }
    }
}

/**
 * @brief Called when the unit in the current observation has lower health or shields than in the previous observation.
 * 
 * @param unit The damaged unit.
 * @param health The change in health (damage is positive)
 * @param shields The change in shields (damage is positive)
 */
void BasicSc2Bot::OnUnitDamaged(const sc2::Unit *unit, float health, float shields) {
    const sc2::ObservationInterface *obs = Observation();
    const sc2::Units units = obs->GetUnits(sc2::Unit::Alliance::Self, sc2::IsUnits({
        sc2::UNIT_TYPEID::TERRAN_MARINE,
        sc2::UNIT_TYPEID::TERRAN_MARAUDER,
        sc2::UNIT_TYPEID::TERRAN_SIEGETANK,
        sc2::UNIT_TYPEID::TERRAN_SIEGETANKSIEGED,
        sc2::UNIT_TYPEID::TERRAN_THOR,
        sc2::UNIT_TYPEID::TERRAN_CYCLONE,
        sc2::UNIT_TYPEID::TERRAN_CYCLONE,
    }));
    const sc2::Units scvs = obs->GetUnits(sc2::Unit::Alliance::Self, sc2::IsUnit(sc2::UNIT_TYPEID::TERRAN_SCV));
    const sc2::Point2D base_pos = FindNearestCommandCenter(unit->pos);
    
    // area around base
    const float rad = 20;

}