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

	//sc2::Point2DI pinchpoint = FindPinchPointAroundPoint(obs.pathing_grid, start);
	//PrintMap(obs.pathing_grid, pinchpoint);
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
    int n_battle_cruisers = obs->GetUnits(sc2::Unit::Self, sc2::IsUnit(sc2::UNIT_TYPEID::TERRAN_BATTLECRUISER)).size();
    if (n_battle_cruisers > 0 || bases.size() == 0) {
        HandleExpansion();  
    }
    
    HandleBuild();
    BuildWorkers();

    if (TryBuildStarPort()) {
        return;
    }
    if (TryBuildFusionCore()) {
        return;
    }
    if (TryBuildSupplyDepot()) {
        return;
    }
    if (TryBuildRefinery()) {
        return;
    }
    if (TryBuildSeigeTank()) {
        return;
    }
    if (TryBuildMissileTurret()) {
        return;
    }
    
    // TryBuildBarracks();
    // TryBuildRefinery();
    // TryBuildBunker();
    // TryBuildFactory();
    // TryBuildSeigeTank();
    CheckScoutStatus();
    // TryBuildSupplyDepot();
    /*
    
    
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
    case sc2::UNIT_TYPEID::TERRAN_STARPORT: {
        /*
         * For now, lower all supply depots by default
         * In the future, maybe we can take advantage of raising/lowering them to control movement
         */
        std::cout << "starport created!" << std::endl;
        UpgradeStarportTechLab(unit);
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

/*
 * Tries to send out the unit provided to scout out the enemy's base
 * Returns true if the unit was assigned the task, false otherwise
 */
bool BasicSc2Bot::TryScouting(const sc2::Unit &unit_to_scout) {
    if (this->scout != nullptr) {
        // we already have a scout, don't need another one
        return false;
    }

    // make unit_to_scout the current scout
    this->scout = &unit_to_scout;

    // if we haven't discovered the enemy's base location, try and find it
    if (!this->unexplored_enemy_starting_locations.empty()) {

        
        const sc2::GameInfo& info = Observation()->GetGameInfo();
        // start from the back so we can .pop_back() (no pop_front equivalent)
        Actions()->UnitCommand(&unit_to_scout, sc2::ABILITY_ID::ATTACK_ATTACK, this->unexplored_enemy_starting_locations.back());
        return true;
    }
    return false;
}

void BasicSc2Bot::CheckScoutStatus() {
    const sc2::ObservationInterface *observation = Observation();
    if (this->scout == nullptr) {
        return;
    }

    if (!this->unexplored_enemy_starting_locations.empty()) {
        // get all known enemy bases
        sc2::Units enemy_bases = Observation()->GetUnits(sc2::Unit::Enemy, [](const sc2::Unit& unit) {
            return unit.unit_type == sc2::UNIT_TYPEID::TERRAN_COMMANDCENTER
                || unit.unit_type == sc2::UNIT_TYPEID::ZERG_HATCHERY
                || unit.unit_type == sc2::UNIT_TYPEID::PROTOSS_NEXUS;
            });
        for (const sc2::Point2D starting_position : unexplored_enemy_starting_locations) {
            if (sc2::DistanceSquared2D(starting_position, this->scout->pos) < 25) {
                unexplored_enemy_starting_locations.pop_back();
                // move to the next base position
                if (!this->unexplored_enemy_starting_locations.empty()) {
                    Actions()->UnitCommand(this->scout, sc2::ABILITY_ID::ATTACK_ATTACK, unexplored_enemy_starting_locations.back());
                }
            }
        }

    }
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
    case sc2::UNIT_TYPEID::TERRAN_STARPORT: {
        UpgradeStarportTechLab(unit);
    }
    default: {
        break;
    }
    }
}

