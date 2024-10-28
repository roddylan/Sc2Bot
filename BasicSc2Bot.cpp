#include "BasicSc2Bot.h"
#include "sc2api/sc2_api.h"
#include "sc2api/sc2_unit.h"
#include "sc2api/sc2_interfaces.h"
#include <_types/_uint32_t.h>
#include <sc2api/sc2_typeenums.h>
#include <sc2api/sc2_unit_filters.h>

void BasicSc2Bot::OnGameStart() {
    return;
#include <iostream>

void BasicSc2Bot::OnGameStart() { 
    std::cout << "Hello, World!" << std::endl;
    scout = nullptr; // no scout initially
    unexplored_enemy_starting_locations = Observation()->GetGameInfo().enemy_start_locations;
    enemy_starting_location = nullptr;  // we use a scout to find this
    return; 
}

void BasicSc2Bot::OnStep() {
    // HandleBuild(); // TODO: move rest of build inside
    TryBuildSupplyDepot();
    TryBuildBarracks();
    TryBuildRefinery();
    TryBuildBunker();
    TryBuildFactory();
    TryBuildSeigeTank();
    return;
}

struct IsUnit {
    IsUnit(sc2::UNIT_TYPEID type) : type_(type) {}
    sc2::UNIT_TYPEID type_;
    bool operator()(const sc2::Unit& unit) { return unit.unit_type == type_; }
};

size_t BasicSc2Bot::CountUnitType(sc2::UNIT_TYPEID unit_type) {
    return Observation()->GetUnits(sc2::Unit::Alliance::Self, IsUnit(unit_type)).size();
}
    CheckScoutStatus();
    //Actions()->SendChat("Hello, World!", sc2::ChatChannel::All);

    return; 
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
bool BasicSc2Bot::TryBuildSeigeTank() {
    const sc2::ObservationInterface* observation = Observation();

    if (CountUnitType(sc2::UNIT_TYPEID::TERRAN_FACTORY) < 1) {
        return false;
    }
    if (observation->GetVespene() < 125) {
        return false;
    }
    return TryBuildStructure(sc2::ABILITY_ID::TRAIN_SIEGETANK);
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

    if (CountUnitType(sc2::UNIT_TYPEID::TERRAN_SUPPLYDEPOT) > 0) {
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
}

bool BasicSc2Bot::TryBuildStructure(sc2::ABILITY_ID ability_type_for_structure, sc2::UNIT_TYPEID unit_type)
{
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

    Actions()->UnitCommand(unit_to_build, ability_type_for_structure,
        sc2::Point2D(unit_to_build->pos.x + rx * sc2::GetRandomScalar(), unit_to_build->pos.y + ry * sc2::GetRandomScalar()));

    return true;
}

void BasicSc2Bot::OnUnitIdle(const sc2::Unit* unit) {
    // TODO: refactor
    sc2::Units barracks = Observation()->GetUnits(sc2::Unit::Self, sc2::IsUnits({sc2::UNIT_TYPEID::TERRAN_BARRACKS, sc2::UNIT_TYPEID::TERRAN_BARRACKSFLYING}));
    sc2::Units bases = Observation()->GetUnits(sc2::Unit::Self, sc2::IsTownHall());

    switch (unit->unit_type.ToType()) {
    case sc2::UNIT_TYPEID::TERRAN_COMMANDCENTER: {
        if (Observation()->GetFoodWorkers() > (n_workers * bases.size()) && !barracks.empty()){
            HandleBuild(); // TODO: refactor and move
        } else {
            sc2::Agent::Actions()->UnitCommand(unit, sc2::ABILITY_ID::TRAIN_SCV);
        }
        break;
    }
    case sc2::UNIT_TYPEID::TERRAN_ORBITALCOMMAND: {
        // std::cout << "ORBITAL COMMAND\n";
        sc2::Agent::Actions()->UnitCommand(unit, sc2::ABILITY_ID::TRAIN_SCV);
    }
    case sc2::UNIT_TYPEID::TERRAN_SCV: {
        const sc2::ObservationInterface* observation = Observation();
        const sc2::Unit* mineral_target; 
        const sc2::Units refineries = observation->GetUnits(sc2::Unit::Alliance::Self, sc2::IsUnit(sc2::UNIT_TYPEID::TERRAN_REFINERY));
        
        for (const auto &refinery : refineries) {
            if (refinery->assigned_harvesters < refinery->ideal_harvesters) {
                Actions()->UnitCommand(unit, sc2::ABILITY_ID::HARVEST_GATHER, refinery);
                return;
            }
        }
        // TODO: handle diff. bases
        mineral_target = FindNearestMineralPatch(unit->pos);
        Actions()->UnitCommand(unit, sc2::ABILITY_ID::SMART, mineral_target);
        break;
    }
    case sc2::UNIT_TYPEID::TERRAN_BARRACKS: {
        Actions()->UnitCommand(unit, sc2::ABILITY_ID::TRAIN_MARINE);
        break;
    }
    case sc2::UNIT_TYPEID::TERRAN_MARINE: {
        const sc2::GameInfo& game_info = Observation()->GetGameInfo();
        Actions()->UnitCommand(unit, sc2::ABILITY_ID::ATTACK_ATTACK, game_info.enemy_start_locations.front());
        break;
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
    const uint32_t ORBITAL_COMMAND_COST = 150;
    const sc2::ObservationInterface *obs = Observation();

    // track each type of unit
    sc2::Units bases = obs->GetUnits(sc2::Unit::Self, sc2::IsTownHall());
    // TODO: separate barracks, factory, starports from 
    sc2::Units barracks = obs->GetUnits(sc2::Unit::Self, sc2::IsUnits({sc2::UNIT_TYPEID::TERRAN_BARRACKS, sc2::UNIT_TYPEID::TERRAN_BARRACKSFLYING}));
    sc2::Units factory = obs->GetUnits(sc2::Unit::Self, sc2::IsUnits({sc2::UNIT_TYPEID::TERRAN_FACTORY, sc2::UNIT_TYPEID::TERRAN_FACTORYFLYING, sc2::UNIT_TYPEID::TERRAN_FACTORYREACTOR, sc2::UNIT_TYPEID::TERRAN_FACTORYTECHLAB}));
    sc2::Units starports = obs->GetUnits(sc2::Unit::Self, sc2::IsUnits({sc2::UNIT_TYPEID::TERRAN_STARPORT, sc2::UNIT_TYPEID::TERRAN_STARPORTFLYING, sc2::UNIT_TYPEID::TERRAN_STARPORTREACTOR, sc2::UNIT_TYPEID::TERRAN_STARPORTTECHLAB}));

    // TODO: handle other building


    // Handle Orbital Command

    if (!barracks.empty()) {
        for (const auto &base : bases) {
            if (obs->GetMinerals() > 150) {
                Actions()->UnitCommand(base, sc2::ABILITY_ID::MORPH_ORBITALCOMMAND);
            }
        }
    }


}