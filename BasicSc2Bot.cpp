#include "BasicSc2Bot.h"
#include "sc2api/sc2_api.h"
#include "sc2api/sc2_unit.h"
#include "sc2api/sc2_interfaces.h"
#include <sc2api/sc2_typeenums.h>
#include <sc2api/sc2_unit_filters.h>
#include <iostream>
struct IsUnit {
    IsUnit(sc2::UNIT_TYPEID type) : type_(type) {}
    sc2::UNIT_TYPEID type_;
    bool operator()(const sc2::Unit& unit) { return unit.unit_type == type_; }
};
static std::string GetStringFromRace(const sc2::Race RaceIn)
{
    if (RaceIn == sc2::Race::Terran)
    {
        return "terran";
    }
    else if (RaceIn == sc2::Race::Protoss)
    {
        return "protoss";
    }
    else if (RaceIn == sc2::Race::Zerg)
    {
        return "zerg";
    }
    else if (RaceIn == sc2::Race::Random)
    {
        return "random";
    }

    return "random";
}
void BasicSc2Bot::OnGameFullStart() {
    std::cout << "Player ID: " << Observation()->GetPlayerID() << std::endl;
    sc2::PlayerInfo player_info_1 = Observation()->GetGameInfo().player_info[0];
    sc2::PlayerInfo player_info_2 = Observation()->GetGameInfo().player_info[1];
    bool curr_player_is_1 = player_info_1.player_id == Observation()->GetPlayerID();
    std::cout << "Player Name: " << (curr_player_is_1 ? player_info_1.player_name : player_info_2.player_name) << std::endl;
    std::cout << "Player Race: " << (curr_player_is_1 ? GetStringFromRace(player_info_1.race_actual) : GetStringFromRace(player_info_2.race_actual)) << std::endl;
    std::cout << "Player Race Requested: " << (curr_player_is_1 ? GetStringFromRace(player_info_1.race_requested) : GetStringFromRace(player_info_2.race_requested)) << std::endl;
    std::cout << "Player Type: " << (curr_player_is_1 ? player_info_1.player_type : player_info_2.player_type) << std::endl;
    std::cout << "Opponent ID: " << (curr_player_is_1 ? player_info_2.player_id : player_info_1.player_id) << std::endl;
    std::cout << "Opponent Name: " << (curr_player_is_1 ? player_info_2.player_name : player_info_1.player_name) << std::endl;
    std::cout << "Opponent Race: " << (curr_player_is_1 ? GetStringFromRace(player_info_2.race_actual) : GetStringFromRace(player_info_1.race_actual)) << std::endl;
    std::cout << "Opponent Race Requested: " << (curr_player_is_1 ? GetStringFromRace(player_info_2.race_requested) : GetStringFromRace(player_info_1.race_requested)) << std::endl;
    std::cout << "Let the Games Begin!" << std::endl;
}
void BasicSc2Bot::OnGameStart() { 
    
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
    return; }

// 14 supply; 16 barracks; 16 gas;
// 3 racks timing
// Zerg/Terran - Marines only
// Protos - Marines and Morauters
// Reactor; Command Center; Orbital;
// Bunker; Maines; 2 Barracks

// Alliance
//! Belongs to the player. Self = 1,
//! Ally of the player. Ally = 2,
//! A neutral unit, usually a non-player unit like a mineral field. Neutral = 3,
//! Enemy of the player. Enemy = 4

void BasicSc2Bot::OnUnitIdle(const sc2::Unit* unit){
    switch (unit->unit_type.ToType()){
    case sc2::UNIT_TYPEID::TERRAN_COMMANDCENTER: {
            Actions()->UnitCommand(unit, sc2::ABILITY_ID::TRAIN_SCV);
            break;
        }
        default: {
            break;
        }
    }
}

struct IsUnit {
    IsUnit(sc2::UNIT_TYPEID type) : type_(type) {}
    sc2::UNIT_TYPEID type_;
    bool operator()(const sc2::Unit& unit) { return unit.unit_type == type_; }
};

size_t BasicSc2Bot::CountUnitType(sc2::UNIT_TYPEID unit_type) {
    return Observation()->GetUnits(sc2::Unit::Alliance::Self, IsUnit(unit_type)).size();
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