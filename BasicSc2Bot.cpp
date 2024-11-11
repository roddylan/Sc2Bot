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
    TryBuildSeigeTank();
    TryBuildMissileTurret();
    HandleBuild();
    
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

sc2::Filter isEnemy = [](const sc2::Unit& unit) {
    return unit.alliance != sc2::Unit::Alliance::Self; 
    };

bool InBunker (sc2::Units myUnits, const sc2::Unit* marine) {
    for (auto& bunker : myUnits) {
        for (const auto& passenger : bunker->passengers) {
            // check if already in bunker
            if (passenger.tag == marine->tag) {
                return true;
            }
        }
    }
    return false;
}
bool BasicSc2Bot::LoadBunker(const sc2::Unit* marine) {
    sc2::Units myUnits = Observation()->GetUnits(sc2::Unit::Alliance::Self, IsUnit(sc2::UNIT_TYPEID::TERRAN_BUNKER));
    bool bunkerLoaded = false;

    if (InBunker(myUnits, marine)) return true;

    for (auto target : myUnits) {
        if (target->passengers.size() < 4) {
            Actions()->UnitCommand(marine, sc2::ABILITY_ID::SMART, target);
            if (!InBunker(myUnits, marine)) {
                continue;
            }
        }
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

/*
 * Gets an SCV that is currently gathering, or return nullptr if there are none.
 * Useful to call when you need to assign an SCV to do a task but you don't want to
 * interrupt other important tasks.
 */
const sc2::Unit *BasicSc2Bot::GetGatheringScv() {
    const sc2::Units &gathering_scv_units = Observation()->GetUnits(sc2::Unit::Alliance::Self, [](const sc2::Unit& unit) {
        if (unit.unit_type.ToType() != sc2::UNIT_TYPEID::TERRAN_SCV) {
            return false;
        }
        for (const sc2::UnitOrder& order : unit.orders) {
            if (order.ability_id == sc2::ABILITY_ID::HARVEST_GATHER) {
                return true;
            }
        }
        return false;
    });

    if (gathering_scv_units.empty()) {
        return nullptr;
    }
    
    return gathering_scv_units[0];
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
int BasicSc2Bot::CountNearbySeigeTanks(const sc2::Unit* factory) {
    sc2::Units seige_tanks = Observation()->GetUnits(sc2::Unit::Alliance::Self, IsUnit(sc2::UNIT_TYPEID::TERRAN_SIEGETANK));
    int count = 0;
    for (const auto &seige_tank : seige_tanks) {
        float distance = sc2::Distance2D(seige_tank->pos, factory->pos);
        if (distance < 2.0f) {
            ++count;
        }
    }
    return count;
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
        if (TryScouting(*unit)) {
            break;
        }
        AssignWorkers(unit);
        break;
    }
    case sc2::UNIT_TYPEID::TERRAN_ORBITALCOMMAND: {
        // std::cout << "ORBITAL COMMAND\n";
        sc2::Agent::Actions()->UnitCommand(unit, sc2::ABILITY_ID::TRAIN_SCV);
    }
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
    if (n_minerals >= 400) {
        HandleExpansion();
    }
    // Handle Orbital Command

    if (!barracks.empty()) {
        for (const auto &base : bases) {
            //std::cout << "inseting pos: " << base->pos.x << " " << base->pos.y << " " << base->pos.z << std::endl;
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
        // if (base->assigned_harvesters < base->ideal_harvesters && base->build_progress == 1 && base->orders.empty()) {
        if (obs->GetFoodWorkers() < n_workers) {
            if (obs->GetMinerals() >= 50) {
                sc2::Agent::Actions()->UnitCommand(base, sc2::ABILITY_ID::TRAIN_SCV);
            }
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

void BasicSc2Bot::AssignWorkers(const sc2::Unit *unit) {
    const sc2::ObservationInterface *obs = Observation();
    const sc2::Units refineries = obs->GetUnits(sc2::Unit::Alliance::Self, sc2::IsUnit(sc2::UNIT_TYPEID::TERRAN_REFINERY));
    const sc2::Units bases = obs->GetUnits(sc2::Unit::Alliance::Self, sc2::IsTownHall());
    const sc2::Unit* mineral_target;
    std::cout << "bases size: " << bases.size() << std::endl;

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
    const sc2::ObservationInterface* obs = Observation();
    sc2::Units bases = obs->GetUnits(sc2::Unit::Alliance::Self, sc2::IsTownHall());

    /*
    if (!(obs->GetFoodWorkers() >= n_workers * bases.size() &&
        CountUnitType(sc2::UNIT_TYPEID::TERRAN_MARINE) >= n_marines * bases.size() &&
        CountUnitType(sc2::UNIT_TYPEID::TERRAN_SIEGETANK) >= bases.size())) {
        return false;
    }
    */

    if (bases.size() > 4) {
        return false;
    }

    float min_dist = std::numeric_limits<float>::max();
    sc2::Point3D closest_expansion(0, 0, 0);

    for (const auto& exp : expansion_locations) {
        float cur_dist = sc2::Distance2D(sc2::Point2D(start_location), sc2::Point2D(exp.x, exp.y));
        if (cur_dist < min_dist) {
            sc2::Point2D nearest_command_center = FindNearestCommandCenter(sc2::Point2D(exp.x, exp.y));
            if (nearest_command_center == sc2::Point2D(0, 0)) {
                continue;
            }

            float dist_to_base = sc2::Distance2D(nearest_command_center, sc2::Point2D(exp.x, exp.y));
            std::cout << "distance to base: " << dist_to_base << std::endl;

            if (Query()->Placement(sc2::ABILITY_ID::BUILD_COMMANDCENTER, exp) && dist_to_base > 1.0f) {
                min_dist = cur_dist;
                closest_expansion = exp;
            }
        }
    }

    if (closest_expansion != sc2::Point3D(0, 0, 0)) {
        sc2::Point2D expansion_location(closest_expansion.x, closest_expansion.y);
        sc2::Point2D p(0, 0);

        if (TryBuildStructure(sc2::ABILITY_ID::BUILD_COMMANDCENTER, p, expansion_location)) {
            std::cout << "EXPANSION TIME BABY\n\n";
        }
    }

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

const sc2::Point2D BasicSc2Bot::FindNearestCommandCenter(const sc2::Point2D& start, bool not_start_location) {

    sc2::Units bases = Observation()->GetUnits(sc2::Unit::Self, sc2::IsTownHall());
    float distance = std::numeric_limits<float>::max();
    const sc2::Unit* target = nullptr;

    for (const auto& base : bases) {
        if (base->unit_type == sc2::UNIT_TYPEID::TERRAN_ORBITALCOMMAND || base->unit_type == sc2::UNIT_TYPEID::TERRAN_COMMANDCENTER || base->unit_type == sc2::UNIT_TYPEID::TERRAN_ORBITALCOMMANDFLYING) {
            if (not_start_location && base->pos == start_location) continue;
            float d = sc2::DistanceSquared2D(base->pos, start);
            if (d < distance) {
                distance = d;
                target = base;
            }
        }
    }

    if (target != nullptr) {
        return target->pos;  
    }
    else {
        return sc2::Point2D(0, 0);
    }
}
