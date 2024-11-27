// BasicSc2BotScout.cpp
// implementation for scouting functions

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


/*
 * Tries to send out the unit provided to scout out the enemy's base
 * Returns true if the unit was assigned the task, false otherwise
 */
bool BasicSc2Bot::TryScoutingForAttack(const sc2::Unit *unit_to_scout, bool refill_enemy_locations) {
    static std::vector<sc2::Point2D> unexplored_enemy_starting_locations;
    static int times_called;
    if (times_called % 4 == 0) {
        unexplored_enemy_starting_locations = Observation()->GetGameInfo().enemy_start_locations;
    }
    ++times_called;
    
    if (unexplored_enemy_starting_locations.empty()) {
        unexplored_enemy_starting_locations = Observation()->GetGameInfo().enemy_start_locations;
    }

    // refill to send again
    if (refill_enemy_locations) {
        unexplored_enemy_starting_locations = Observation()->GetGameInfo().enemy_start_locations;
    }

    // if we haven't discovered the enemy's base location, try and find it
    if (!unexplored_enemy_starting_locations.empty()) {

        const sc2::GameInfo& info = Observation()->GetGameInfo();
        // start from the back so we can .pop_back() (no pop_front equivalent)
        sc2::Point2D target = unexplored_enemy_starting_locations.back();
        // remove point
        unexplored_enemy_starting_locations.pop_back();
        // send 
        Actions()->UnitCommand(unit_to_scout, sc2::ABILITY_ID::ATTACK_ATTACK, target);
        return true;
    }
    return false;
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

