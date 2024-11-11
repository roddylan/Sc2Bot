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

    // **NOTE** order matters as the amount of minerals we have gets consumed, seige tanks are improtant to have at each expansion 
    TryBuildSeigeTank();
    TryBuildMissileTurret();
    HandleBuild();
    
    BuildWorkers();
    


    if (TryBuildSupplyDepot()) {
        return;
    }
    if (TryBuildRefinery()) {
        return;
    }
    // TryBuildBarracks();
    // TryBuildBunker();
    // TryBuildFactory();

    
    AttackIntruders();
    return;
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
