#include "BasicSc2Bot.h"
#include <iostream>

void BasicSc2Bot::OnGameStart() { 
    std::cout << "Hello, World!" << std::endl;
    scout = nullptr; // no scout initially
    unexplored_enemy_starting_locations = Observation()->GetGameInfo().enemy_start_locations;
    enemy_starting_location = nullptr;  // we use a scout to find this
    return; 
}

void BasicSc2Bot::OnStep() { 
    //std::cout << Observation()->GetGameLoop() << std::endl;
    TryBuildSupplyDepot();
    CheckScoutStatus();
    //Actions()->SendChat("Hello, World!", sc2::ChatChannel::All);

    return; 
}


void BasicSc2Bot::OnUnitIdle(const sc2::Unit* unit){
    const sc2::ObservationInterface* observation = Observation();
    switch (unit->unit_type.ToType()){
    case sc2::UNIT_TYPEID::TERRAN_COMMANDCENTER: {
        Actions()->UnitCommand(unit, sc2::ABILITY_ID::TRAIN_SCV);
        break;
    }
    case sc2::UNIT_TYPEID::TERRAN_SCV: {
        if (TryScouting(*unit)) {
            return;
        }
        break;
    }
        default: {
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
        std::cout << "FOUND ENEMY BASE" << std::endl;
    }
}

/*
 * Tries to send out the unit provided to scout out the enemy's base
 */
bool BasicSc2Bot::TryScouting(const sc2::Unit &unit_to_scout) {
    // to start with, we do not know where the enemy base is but we have a list of candidates
    static std::vector<sc2::Point2D> unexplored_base_locations = Observation()->GetGameInfo().enemy_start_locations;
    if (this->scout != nullptr) {
        // we already have a scout, don't need another one
        return false;
    }
    std::cout << "PICKING SCOUT" << std::endl;

    // make unit_to_scout the current scout
    this->scout = &unit_to_scout;

    // if we haven't discovered the enemy's base location, try and find it
    if (!this->unexplored_enemy_starting_locations.empty()) {

        
        const sc2::GameInfo& info = Observation()->GetGameInfo();
        // start from the back so we can .pop_back() (no pop_front equivalent)
        Actions()->UnitCommand(&unit_to_scout, sc2::ABILITY_ID::ATTACK_ATTACK, unexplored_base_locations.back());
        return true;
    }
    return false;
}

void BasicSc2Bot::CheckScoutStatus() {
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
                std::cout << "NEAR POTENTIAL BASE AT" << starting_position.x << "," << starting_position.y << std::endl;
                // move to the next base position
                if (!this->unexplored_enemy_starting_locations.empty()) {
                    Actions()->UnitCommand(this->scout, sc2::ABILITY_ID::ATTACK_ATTACK, unexplored_enemy_starting_locations.back());
                }
            }
        }

    }
}

bool BasicSc2Bot::TryBuildSupplyDepot()
{
    const sc2::ObservationInterface* observation = Observation();

    // If we are not supply capped, don't build a supply depot.
    if (observation->GetFoodUsed() <= observation->GetFoodCap() - 2) {
        return false;
    }

    // Try and build a depot. Find a random SCV and give it the order.
    return TryBuildStructure(sc2::ABILITY_ID::BUILD_SUPPLYDEPOT);

}

bool BasicSc2Bot::TryBuildStructure(sc2::ABILITY_ID ability_type_for_structure, sc2::UNIT_TYPEID unit_type)
{
    const sc2::ObservationInterface* observation = Observation();

    // If a unit already is building a structure of this type, do nothing.
    // Also get an SCV to build the structure.
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
    
    Actions()->UnitCommand(unit_to_build,
        ability_type_for_structure,
        sc2::Point2D(unit_to_build->pos.x + rx * 15.0f, unit_to_build->pos.y + ry * 15.0f));

    return true;
}