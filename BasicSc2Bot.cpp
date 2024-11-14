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
    scout = nullptr; // no scout initially
    unexplored_enemy_starting_locations = Observation()->GetGameInfo().enemy_start_locations;
    enemy_starting_location = nullptr;  // we use a scout to find this
}

void BasicSc2Bot::OnGameFullStart() {
    const sc2::ObservationInterface *obs = Observation();
    sc2::GameInfo gin = obs->GetGameInfo();
    sc2::Point3D start_3d = obs->GetUnits(sc2::Unit::Alliance::Self, IsUnit(sc2::UNIT_TYPEID::TERRAN_COMMANDCENTER))[0]->pos;
    sc2::Point2DI start = sc2::Point2DI(round(start_3d.x), round(start_3d.y));

	//sc2::Point2DI pinchpoint = FindPinchPointAroundPoint(obs.pathing_grid, start);
	//PrintMap(obs.pathing_grid, pinchpoint);
	return;
}

void BasicSc2Bot::OnStep() {
    // HandleBuild(); // TODO: move rest of build inside

    // **NOTE** order matters as the amount of minerals we have gets consumed, seige tanks are improtant to have at each expansion 
    HandleBuild();
    TryBuildSeigeTank();
    TryBuildMissileTurret();
    
    BuildWorkers();
    
    TryBuildBarracks();
    TryBuildRefinery();
    TryBuildBunker();
    TryBuildFactory();
    TryBuildSeigeTank();
    CheckScoutStatus();
    TryBuildSupplyDepot();

    /*
    if (TryBuildSupplyDepot()) {
        return;
    }
    if (TryBuildRefinery()) {
        return;
    }
    // TryBuildBarracks();
    // TryBuildBunker();
    // TryBuildFactory();
    */
    
    AttackIntruders();
    return;
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
        std::cout << "supply depot created!" << std::endl;
        Actions()->UnitCommand(unit, sc2::ABILITY_ID::MORPH_SUPPLYDEPOT_LOWER);
        break;
    }
    }
}

void BasicSc2Bot::OnUnitDestroyed(const sc2::Unit* unit) {
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
        if (TryScouting(*unit)) {
            break;
        }
        AssignWorkers(unit);
        break;
    }
    // case sc2::UNIT_TYPEID::TERRAN_ORBITALCOMMAND: {
    //     // std::cout << "ORBITAL COMMAND\n";
    //     sc2::Agent::Actions()->UnitCommand(unit, sc2::ABILITY_ID::TRAIN_SCV);
    // }
    case sc2::UNIT_TYPEID::TERRAN_MULE: {
        AssignWorkers(unit);
        break;
    }
    case sc2::UNIT_TYPEID::TERRAN_SUPPLYDEPOT: {
       // std::cout << "SUPPLY DEPOT IDLE" << std::endl;
        Actions()->UnitCommand(unit, sc2::ABILITY_ID::MORPH_SUPPLYDEPOT_LOWER);
        break;
    }
    case sc2::UNIT_TYPEID::TERRAN_BARRACKS: {
        StartTrainingUnit(*unit);
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
    case sc2::UNIT_TYPEID::TERRAN_FACTORY: {
        UpgradeFactoryTechLab(unit);
    }
    default: {
        break;
    }
    }
}

/*
 * Picks unit for the barrack to train and instructs it to train it
 */
void BasicSc2Bot::StartTrainingUnit(const sc2::Unit& barrack_to_train) {
    const sc2::ObservationInterface* observation = Observation();
    const sc2::Units marines = observation->GetUnits(sc2::Unit::Alliance::Self, sc2::IsUnit(sc2::UNIT_TYPEID::TERRAN_MARINE));
    size_t marine_count = marines.size();
    if (marine_count < 8) {
        Actions()->UnitCommand(&barrack_to_train, sc2::ABILITY_ID::TRAIN_MARINE);
        return;
    }
    const sc2::Units marauders = observation->GetUnits(sc2::Unit::Alliance::Self, sc2::IsUnit(sc2::UNIT_TYPEID::TERRAN_MARAUDER));
    size_t marauder_count = marauders.size();

    
}
