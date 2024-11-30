// BasicSc2BotAttack.cpp
// contains implementation for all offensive (attacking, harrassment, expansion) and attack-like defensive functions

#include "BasicSc2Bot.h"
#include "Utilities.h"
#include "Betweenness.h"
#include "sc2api/sc2_api.h"
#include "sc2api/sc2_unit.h"
#include "sc2api/sc2_interfaces.h"
#include <cstddef>
#include <limits>
#include <sc2api/sc2_common.h>
#include <sc2api/sc2_data.h>
#include <sc2api/sc2_map_info.h>
#include <sc2api/sc2_typeenums.h>
#include <sc2api/sc2_unit_filters.h>
#include <sc2lib/sc2_search.h>
#include <iostream>
#include <cmath>

/*
    Sends squad of units to protoss start location that is closest to the last death location of a unit
*/
void BasicSc2Bot::SendSquadProtoss() {

    const uint64_t minute = 1344;
    // Dont continue if less than 13 mins has elapsed (build order not ready yet)
    if (Observation()->GetGameLoop() < 5 * minute) {
        return;
    }
    std::vector<sc2::Point2D> enemy_locations = Observation()->GetGameInfo().enemy_start_locations;
    // Get Army units
    sc2::Units marines = Observation()->GetUnits(sc2::Unit::Alliance::Self, sc2::IsUnit(sc2::UNIT_TYPEID::TERRAN_MARINE));
    sc2::Units banshees = Observation()->GetUnits(sc2::Unit::Alliance::Self, sc2::IsUnit(sc2::UNIT_TYPEID::TERRAN_BANSHEE));
    sc2::Units marauders = Observation()->GetUnits(sc2::Unit::Alliance::Self, sc2::IsUnit(sc2::UNIT_TYPEID::TERRAN_MARAUDER));
    sc2::Units liberators = Observation()->GetUnits(sc2::Unit::Alliance::Self, sc2::IsUnit(sc2::UNIT_TYPEID::TERRAN_LIBERATOR));
    sc2::Units tanks = Observation()->GetUnits(sc2::Unit::Alliance::Self, sc2::IsUnit(sc2::UNIT_TYPEID::TERRAN_SIEGETANK));
    sc2::Units vikings = Observation()->GetUnits(sc2::Unit::Alliance::Self, sc2::IsUnit(sc2::UNIT_TYPEID::TERRAN_VIKINGFIGHTER));
    sc2::Units battlecruisers = Observation()->GetUnits(sc2::Unit::Alliance::Self, sc2::IsUnit(sc2::UNIT_TYPEID::TERRAN_BATTLECRUISER));
    sc2::Units thors = Observation()->GetUnits(sc2::Unit::Alliance::Self, sc2::IsUnit(sc2::UNIT_TYPEID::TERRAN_THOR));
    sc2::Units medivacs = Observation()->GetUnits(sc2::Unit::Alliance::Self, sc2::IsUnit(sc2::UNIT_TYPEID::TERRAN_MEDIVAC));

    // Create a squad of units with empty orders
    std::vector<const sc2::Unit*> squad;
    auto filter_units = [](const sc2::Units& units, std::vector<const sc2::Unit*>& squad) {
        for (const auto& unit : units) {
            if (unit->orders.empty()) {
                squad.push_back(unit);
            }
        }

        };
    // Filter out units that have orders already
    filter_units(thors, squad);
    
    filter_units(battlecruisers, squad);
    filter_units(tanks, squad);
    filter_units(banshees, squad);
    filter_units(marauders, squad);
    filter_units(liberators, squad);
    filter_units(vikings, squad);
    sc2::Point2D closest_start_location;
    if (!scout_died) {
        // Get enemy start location closest to last death location to prevent from sending to empty location
        float min_distance = std::numeric_limits<float>::max();
        for (const auto& start_location : enemy_locations) {
            float distance = sc2::Distance2D(start_location, BasicSc2Bot::last_death_location);
            if (distance < min_distance) {
                min_distance = distance;
                closest_start_location = start_location;
            }
        }
    }

    // Tracks the game loop when the last unit was sent
    static uint64_t last_send_time = 0;
    // Tracks the game loop of the last reset
    static uint64_t last_reset_time = 0;
    // FPS
    const int frames_per_second = 22;
    // 3 minutes in game loops (22 FPS � 60 seconds � 3)
    const uint64_t reset_interval = 4032;
    // Keeps track of how many times units have been sent
    static int units_sent = 0;

    // Reset units_sent every 3 minutes
    uint64_t current_time = Observation()->GetGameLoop();
    if (current_time > last_reset_time + reset_interval) {
        units_sent = 0;
        last_reset_time = current_time;
        std::cout << "Resetting units_sent to 0 after 3 minutes" << std::endl;
    }

    // Add delay between sending units
    if (current_time > (last_send_time + 15 * frames_per_second) && squad.size() > 10) {
        if (!scout_died) {
            filter_units(marines, squad);
            Actions()->UnitCommand(squad, sc2::ABILITY_ID::ATTACK_ATTACK, closest_start_location);
            std::cout << "sending them to: " << closest_start_location.x << " " << closest_start_location.y  << std::endl;

        }
        else {
            filter_units(marines, squad);
            Actions()->UnitCommand(squad, sc2::ABILITY_ID::ATTACK_ATTACK, *enemy_starting_location);
          
            
        }

        // Update last send time
        last_send_time = current_time;
        units_sent++;
    }

    // Stop sending more units if all enemy locations are covered
    if (units_sent >= enemy_locations.size()) {
        return;
    }
}
/*
    Sends squad of units to enemy start location that is closest to the last death location of a unit
*/
void BasicSc2Bot::SendSquad() {
    
    const uint64_t minute = 1344;
    // Dont continue if less than 13 mins has elapsed (build order not ready yet)
    if (Observation()->GetGameLoop() < 13 * minute) {
        return;
    }
    std::vector<sc2::Point2D> enemy_locations = Observation()->GetGameInfo().enemy_start_locations;
    // Get Army units
    sc2::Units marines = Observation()->GetUnits(sc2::Unit::Alliance::Self, sc2::IsUnit(sc2::UNIT_TYPEID::TERRAN_MARINE));
    sc2::Units banshees = Observation()->GetUnits(sc2::Unit::Alliance::Self, sc2::IsUnit(sc2::UNIT_TYPEID::TERRAN_BANSHEE));
    sc2::Units marauders = Observation()->GetUnits(sc2::Unit::Alliance::Self, sc2::IsUnit(sc2::UNIT_TYPEID::TERRAN_MARAUDER));
    sc2::Units liberators = Observation()->GetUnits(sc2::Unit::Alliance::Self, sc2::IsUnit(sc2::UNIT_TYPEID::TERRAN_LIBERATOR));
    sc2::Units tanks = Observation()->GetUnits(sc2::Unit::Alliance::Self, sc2::IsUnit(sc2::UNIT_TYPEID::TERRAN_SIEGETANK));
    sc2::Units vikings = Observation()->GetUnits(sc2::Unit::Alliance::Self, sc2::IsUnit(sc2::UNIT_TYPEID::TERRAN_VIKINGFIGHTER));
    sc2::Units battlecruisers = Observation()->GetUnits(sc2::Unit::Alliance::Self, sc2::IsUnit(sc2::UNIT_TYPEID::TERRAN_BATTLECRUISER));
    sc2::Units thors = Observation()->GetUnits(sc2::Unit::Alliance::Self, sc2::IsUnit(sc2::UNIT_TYPEID::TERRAN_THOR));
    sc2::Units medivacs = Observation()->GetUnits(sc2::Unit::Alliance::Self, sc2::IsUnit(sc2::UNIT_TYPEID::TERRAN_MEDIVAC));

    // Create a squad of units with empty orders
    std::vector<const sc2::Unit*> squad;
    auto filter_units = [](const sc2::Units& units, std::vector<const sc2::Unit*>& squad) {
        for (const auto& unit : units) {
            if (unit->orders.empty()) {
                squad.push_back(unit);
            }
        }

        };
    // Filter out units that have orders already
    filter_units(thors, squad);
    filter_units(battlecruisers, squad);
    filter_units(tanks, squad);
    filter_units(banshees, squad);
    filter_units(marauders, squad);
    filter_units(liberators, squad);
    filter_units(vikings, squad);
    sc2::Point2D closest_start_location;
    if (!scout_died) {
        // Get enemy start location closest to last death location to prevent from sending to empty location
        float min_distance = std::numeric_limits<float>::max();
        for (const auto& start_location : enemy_locations) {
            float distance = sc2::Distance2D(start_location, BasicSc2Bot::last_death_location);
            if (distance < min_distance) {
                min_distance = distance;
                closest_start_location = start_location;
            }
        }
    }
    
    // Tracks the game loop when the last unit was sent
    static uint64_t last_send_time = 0;
    // Tracks the game loop of the last reset
    static uint64_t last_reset_time = 0;
    // FPS
    const int frames_per_second = 22;
    // 3 minutes in game loops (22 FPS � 60 seconds � 3)
    const uint64_t reset_interval = 4032;
    // Keeps track of how many times units have been sent
    static int units_sent = 0;

    // Reset units_sent every 3 minutes
    uint64_t current_time = Observation()->GetGameLoop();
    if (current_time > last_reset_time + reset_interval) {
        units_sent = 0;
        last_reset_time = current_time;
        std::cout << "Resetting units_sent to 0 after 3 minutes" << std::endl;
    }

    sc2::Point2D location{};
    // Add delay between sending units
    if (current_time > (last_send_time + 15 * frames_per_second) && squad.size() > 11) {
        filter_units(marines, squad);
        if (!scout_died) {
            location = closest_start_location;
            // Actions()->UnitCommand(squad, sc2::ABILITY_ID::ATTACK_ATTACK, closest_start_location);

        }
        else {
            location = *enemy_starting_location;
            // Actions()->UnitCommand(squad, sc2::ABILITY_ID::ATTACK_ATTACK, *enemy_starting_location);
        }
        
        for (const auto &unit : squad) {
            if (!unit->orders.empty()) {
                continue;
            }
            Actions()->UnitCommand(unit, sc2::ABILITY_ID::ATTACK_ATTACK, location);
        }
        
        // Update last send time
        last_send_time = current_time;
        units_sent++;
    }

    // Stop sending more units if all enemy locations are covered
    if (units_sent >= enemy_locations.size()) {
        return;
    }
}

bool BasicSc2Bot::AttackIntruders() {
    /*
    * This method does too much work to be called every frame. Call it every few hundred frames instead
    */
    static size_t last_frame_checked = 0;
    const sc2::ObservationInterface* observation = Observation();
    const uint32_t& current_frame = observation->GetGameLoop();
    if (current_frame - last_frame_checked < 400) {
        return false;
    }
    last_frame_checked = current_frame;
    const sc2::Units &enemy_units = observation->GetUnits(sc2::Unit::Alliance::Enemy);
    
    for (const sc2::Unit *target : enemy_units) {
        sc2::Units myUnits = Observation()->GetUnits(sc2::Unit::Alliance::Self, IsUnit(sc2::UNIT_TYPEID::TERRAN_BUNKER));
        for (auto myUnit : myUnits) {
            Actions()->UnitCommand(myUnit, sc2::ABILITY_ID::BUNKERATTACK, target);
        }
    }

    /*
    * Attack enemy units that are near the base
    */
    
    const sc2::Units &bases = observation->GetUnits(sc2::Unit::Alliance::Self, sc2::IsUnit(sc2::UNIT_TYPEID::TERRAN_COMMANDCENTER));


    for (const sc2::Unit* base : bases) {
        const sc2::Unit* enemy_near_base = nullptr;
        for (const sc2::Unit* enemy_unit : enemy_units) {
            if (sc2::DistanceSquared2D(base->pos, enemy_unit->pos) < 40 * 40) {
                enemy_near_base = enemy_unit;
                break;
            }
        }
        // we didn't find a nearby enemy to attack
        if (enemy_near_base == nullptr) {
            continue;
        }

        const sc2::Units& defending_units = observation->GetUnits(sc2::Unit::Alliance::Self, sc2::IsUnits({ 
            sc2::UNIT_TYPEID::TERRAN_MARINE, sc2::UNIT_TYPEID::TERRAN_MARAUDER,
            sc2::UNIT_TYPEID::TERRAN_SIEGETANK, sc2::UNIT_TYPEID::TERRAN_SIEGETANKSIEGED,
            sc2::UNIT_TYPEID::TERRAN_THOR, sc2::UNIT_TYPEID::TERRAN_THORAP, 
            sc2::UNIT_TYPEID::TERRAN_VIKINGFIGHTER, sc2::UNIT_TYPEID::TERRAN_VIKINGASSAULT
        }));
        // for (const sc2::Unit* defending_unit : defending_units) {
        //     Actions()->UnitCommand(defending_unit, sc2::ABILITY_ID::ATTACK_ATTACK, enemy_near_base);
        // }
        Actions()->UnitCommand(defending_units, sc2::ABILITY_ID::ATTACK_ATTACK, enemy_near_base);

        // move the medivacs to the battle so that they heal the units
        if (!defending_units.empty()) {
            const sc2::Units& medivacs = observation->GetUnits(sc2::Unit::Alliance::Self, sc2::IsUnit(sc2::UNIT_TYPEID::TERRAN_MEDIVAC));
            for (const sc2::Unit* medivac : medivacs) {
                const sc2::Point2D& pos_to_move_to = defending_units.front()->pos;
                Actions()->UnitCommand(medivac, sc2::ABILITY_ID::MOVE_MOVE, pos_to_move_to);
            }
        }

        break;
    }

    return true;
}


bool BasicSc2Bot::HandleExpansion(bool resources_depleted) {
    const sc2::ObservationInterface* obs = Observation();
    sc2::Units bases = obs->GetUnits(sc2::Unit::Alliance::Self, sc2::IsTownHall());
    sc2::Units siege_tanks = obs->GetUnits(sc2::Unit::Alliance::Self, 
        sc2::IsUnit(sc2::UNIT_TYPEID::TERRAN_SIEGETANK));
    sc2::Units marines = obs->GetUnits(sc2::Unit::Alliance::Self, 
        sc2::IsUnit(sc2::UNIT_TYPEID::TERRAN_MARINE));

    size_t n_bases = bases.size();
    size_t n_siege_tanks = siege_tanks.size();
    size_t n_marines = marines.size();

    /*
    if (!(obs->GetFoodWorkers() >= n_workers * bases.size() &&
        CountUnitType(sc2::UNIT_TYPEID::TERRAN_MARINE) >= n_marines * bases.size() &&
        CountUnitType(sc2::UNIT_TYPEID::TERRAN_SIEGETANK) >= bases.size())) {
        return false;
    }
    */
    // TODO: change limit
    if (resources_depleted) {
        goto expand;
    }
    // TODO: change siege tank req
    // if (n_bases > 0 && n_siege_tanks < (n_bases * 1 + 1) && n_marines >= this->n_marines * n_bases) {
    // if (n_bases > 0 && (n_siege_tanks < (n_bases * this->n_tanks) || n_marines < this->n_marines * n_bases)) {
    // if (n_bases > 0 && (n_siege_tanks < (n_bases * 1 + 1) || n_marines < this->n_marines * n_bases)) {
    if (n_bases > 0 && (n_marines < this->n_marines * n_bases)) {
        // only expand when enough units to defend base + protect expansion
        return false;
    }
    // std::cout << "n_siege_tanks=" << n_siege_tanks;
    // if (n_bases > 1 && n_marines >= this->n_marines * n_bases) {
    //     // only expand when enough units to defend base + protect expansion
    //     return false;
    // }

    if (bases.size() > 4) {
        return false;
    }
    if (obs->GetMinerals() < std::min<size_t>(bases.size() * 600, 1800)) {
        return false;
    }
expand:
    int64_t game_loop = Observation()->GetGameLoop();

    
    const int64_t ten_minutes_in_loops = 13440;
    // if less than 10 min mark dont create more than 3 bases
    if (game_loop < ten_minutes_in_loops && bases.size() > 1) {
        return false;
    }
    const int64_t twenty_minutes_in_loops = 26880;
    if (game_loop < twenty_minutes_in_loops && bases.size() > 3) {
        return false;
    }
    const int64_t twenty_five_minutes_in_loops = 33600;
    if (game_loop < twenty_five_minutes_in_loops && bases.size() > 4) {
        return false;
    }
    
    const int64_t thirty_minutes_in_loops = 40320;
    if (game_loop < thirty_minutes_in_loops && bases.size() > 5) {
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
            base_location = closest_expansion; // set base to closest expansion
           // std::cout << "EXPANSION TIME BABY\n\n";
        }
    }

    return true;
}

/**
 * @brief Handle attack for squad with tanks
 * 
 * @param squad 
 */
void BasicSc2Bot::TankAttack(const sc2::Units &squad) {
    // TODO: maybe just pass in enemies_in_range
    
    // squad of up to 16
    
    // vector of tanks in squad
    const sc2::ObservationInterface *obs = Observation();

    sc2::Units tanks{};
    sc2::Units enemies = obs->GetUnits(sc2::Unit::Alliance::Enemy);
    if (enemies.empty() || squad.empty()) {
        return;
    }
    sc2::Units enemies_in_range{};

    // attack range
    const float TANK_RANGE = 7;        // regular attack range
    const float TANK_SIEGE_RANGE = 13; // siege attack range
    const float THRESHOLD = (TANK_RANGE + TANK_SIEGE_RANGE) / 2; // threshold distance to choose b/w modes

    
    // get siege tanks in squad
    for (const auto &unit : squad) {
        if (unit->unit_type == sc2::UNIT_TYPEID::TERRAN_SIEGETANK ||
            unit->unit_type == sc2::UNIT_TYPEID::TERRAN_SIEGETANKSIEGED) {
            tanks.push_back(unit);
        }
    }

    if (tanks.empty()) {
        return;
    }

    // get enemies in range
    for (const auto &enemy : enemies) {
        for (const auto &unit : squad) {
            if (sc2::Distance2D(unit->pos, enemy->pos) < unit->detect_range) {
                enemies_in_range.push_back(enemy);
                break;
            }
        }
    }
    if (enemies_in_range.empty()) {
        // TODO: add buffer timer (ie. 3s since enemy in view)
        // Actions()->UnitCommand(tanks, sc2::ABILITY_ID::MORPH_UNSIEGE);
        return;
    }

    // if there is enemy in range
    for (const auto &tank : tanks) {
        // find closest
        float min_dist = std::numeric_limits<float>::max();
 
        for (const auto &enemy : enemies_in_range) {
            float dist = sc2::Distance2D(tank->pos, enemy->pos);
            if (dist < min_dist) {
                min_dist = dist;
            }
        }
        
        // closest enemy detected within siege range -> move back and go siege mode
        if (min_dist <= THRESHOLD) {
            Actions()->UnitCommand(tank, sc2::ABILITY_ID::MORPH_SIEGEMODE);
        } 
        
        if (min_dist > TANK_SIEGE_RANGE) {
            Actions()->UnitCommand(tank, sc2::ABILITY_ID::MORPH_UNSIEGE);
        }
        AttackWithUnit(tank, enemies_in_range, false);
    }
}

/**
 * @brief Handle attack for squad with tanks (with enemies)
 * 
 * @param squad 
 * @param enemies 
 */
void BasicSc2Bot::TankAttack(const sc2::Units &squad, const sc2::Units &enemies) {
    // TODO: maybe just pass in enemies_in_range
    
    // squad of up to 16
    
    // vector of tanks in squad
    const sc2::ObservationInterface *obs = Observation();

    sc2::Units tanks{};
    if (enemies.empty() || squad.empty()) {
        return;
    }

    // attack range
    const float TANK_RANGE = 7;        // regular attack range
    const float TANK_SIEGE_RANGE = 13; // siege attack range
    const float THRESHOLD = (TANK_RANGE + TANK_SIEGE_RANGE) / 2; // threshold distance to choose b/w modes

    
    // get siege tanks in squad
    for (const auto &unit : squad) {
        if (unit->unit_type == sc2::UNIT_TYPEID::TERRAN_SIEGETANK ||
            unit->unit_type == sc2::UNIT_TYPEID::TERRAN_SIEGETANKSIEGED) {
            tanks.push_back(unit);
        }
    }

    if (tanks.empty()) {
        return;
    }
    

    // if there is enemy in range
    for (const auto &tank : tanks) {
        // find closest
        float min_dist = std::numeric_limits<float>::max();
 
        for (const auto &enemy : enemies) {
            // cant attack flying
            if (enemy->is_flying) {
                continue;
            }
            float dist = sc2::Distance2D(tank->pos, enemy->pos);
            if (dist < min_dist) {
                min_dist = dist;
            }
        }
        
        // closest enemy detected within siege range -> move back and go siege mode
        if (min_dist <= THRESHOLD) {
            Actions()->UnitCommand(tank, sc2::ABILITY_ID::MORPH_SIEGEMODE);
        } 
        
        if (min_dist > TANK_SIEGE_RANGE) {
            Actions()->UnitCommand(tank, sc2::ABILITY_ID::MORPH_UNSIEGE);
        }
        AttackWithUnit(tank, enemies, false);
    }
}

/**
 * @brief Attacking enemies with unit
 * 
 * @param unit 
 * @param enemies in range
 * @param atk_pos flag for attacking position instead of enemy (default true)
 */
void BasicSc2Bot::AttackWithUnit(const sc2::Unit *unit, const sc2::Units &enemies, const bool &atk_pos) {
    if (enemies.empty()) {
        return;
    }

    // attack enemy
    if (atk_pos) {
        Actions()->UnitCommand(unit, sc2::ABILITY_ID::ATTACK, enemies.front()->pos);
    } else {
        Actions()->UnitCommand(unit, sc2::ABILITY_ID::ATTACK, enemies.front());
    }
}


/**
 * @brief Initial logic for launching attack on enemy base
 * 
 */
void BasicSc2Bot::LaunchAttack() {
    const sc2::ObservationInterface *obs = Observation();
    sc2::ActionInterface *act = Actions();

    // TODO: army composition requirements

    // dont attack if not ready
    // if (obs->GetFoodArmy() < (200 - obs->GetFoodWorkers() - N_ARMY_THRESHOLD)) {
    //     return;
    // }
    // if (obs->GetFoodArmy() < (170 - obs->GetFoodWorkers() - N_ARMY_THRESHOLD)) {
    // if (obs->GetFoodArmy() < (195 - obs->GetFoodWorkers() - N_ARMY_THRESHOLD)) {
    if (obs->GetFoodArmy() < (175 - obs->GetFoodWorkers() - N_ARMY_THRESHOLD)) {
    // if (obs->GetFoodArmy() < (130 - obs->GetFoodWorkers() - N_ARMY_THRESHOLD)) {
    // if (obs->GetFoodArmy() < (120 - obs->GetFoodWorkers() - N_ARMY_THRESHOLD)) {
        return;
    }

    // if (!sent) {
    //     return;
    // }
    
    sc2::Units enemies = obs->GetUnits(sc2::Unit::Alliance::Enemy);
    sc2::Units enemy_bases = obs->GetUnits(sc2::Unit::Alliance::Enemy, sc2::IsTownHall());

    // TODO: decide if keep some at base or send all to attack

    // basic ground troops (marine, marauder)
    sc2::Units marines = obs->GetUnits(sc2::Unit::Alliance::Self, sc2::IsUnit(sc2::UNIT_TYPEID::TERRAN_MARINE));
    sc2::Units marauders = obs->GetUnits(sc2::Unit::Alliance::Self, sc2::IsUnit(sc2::UNIT_TYPEID::TERRAN_MARAUDER));
    
    // basic air troops
    sc2::Units medivacs = obs->GetUnits(sc2::Unit::Alliance::Self, sc2::IsUnit(sc2::UNIT_TYPEID::TERRAN_MEDIVAC));

    // mech ground troops
    sc2::Units siege_tanks = obs->GetUnits(sc2::Unit::Alliance::Self, sc2::IsUnits({
        sc2::UNIT_TYPEID::TERRAN_SIEGETANK, sc2::UNIT_TYPEID::TERRAN_SIEGETANKSIEGED
    }));
    sc2::Units thors = obs->GetUnits(sc2::Unit::Alliance::Self, sc2::IsUnits({
        sc2::UNIT_TYPEID::TERRAN_THOR, sc2::UNIT_TYPEID::TERRAN_THORAP
    }));

    // mech air troops
    sc2::Units vikings = obs->GetUnits(sc2::Unit::Alliance::Self, sc2::IsUnits({
        sc2::UNIT_TYPEID::TERRAN_VIKINGASSAULT, sc2::UNIT_TYPEID::TERRAN_VIKINGFIGHTER
    }));
    sc2::Units liberators = obs->GetUnits(sc2::Unit::Alliance::Self, sc2::IsUnit(sc2::UNIT_TYPEID::TERRAN_LIBERATOR));
    sc2::Units banshees = obs->GetUnits(sc2::Unit::Alliance::Self, sc2::IsUnit(sc2::UNIT_TYPEID::TERRAN_BANSHEE));
    sc2::Units battlecruisers = obs->GetUnits(sc2::Unit::Alliance::Self, sc2::IsUnit(sc2::UNIT_TYPEID::TERRAN_BATTLECRUISER));

    
    sc2::Units raid_squad{};

    // float ratio = static_cast<double>(2) / 3;
    float ratio = 1;
    
    const size_t split_marines = marines.size() * ratio;
    const size_t split_marauders = marauders.size() * ratio;
    const size_t split_siege_tanks = siege_tanks.size() * ratio;
    const size_t split_thors = thors.size() * ratio;
    const size_t split_vikings = vikings.size() * ratio;
    const size_t split_medivacs = medivacs.size() * ratio;
    const size_t split_liberators = liberators.size() * ratio;
    const size_t split_banshees = banshees.size() * ratio;
    const size_t split_battlecruisers = battlecruisers.size() * ratio;

    SquadSplit(split_marines, marines, raid_squad);
    SquadSplit(split_marauders, marauders, raid_squad);
    SquadSplit(split_siege_tanks, siege_tanks, raid_squad);
    SquadSplit(split_thors, thors, raid_squad);
    SquadSplit(split_vikings, vikings, raid_squad);
    SquadSplit(split_medivacs, medivacs, raid_squad);
    SquadSplit(split_liberators, liberators, raid_squad);
    SquadSplit(split_banshees, banshees, raid_squad);
    SquadSplit(split_battlecruisers, battlecruisers, raid_squad);
    
    // do nothing if raid squad dne
    if (raid_squad.empty()) {
        return;
    }

    for (const auto &unit : raid_squad) {
        if (enemies.empty() && unit->unit_type == sc2::UNIT_TYPEID::TERRAN_VIKINGFIGHTER) {
            if (!unit->orders.empty()) {
                continue;
            }
            sc2::Point2D location{};
            // bool check = ScoutRandom(raid_squad.front(), location);
            bool check = ScoutRandom(unit, location);
            
            if (check) {
                std::cout << "Scout Random: " << check << std::endl;
                std::cout << "search random location\n";
                std::cout << "(" << location.x << ", " << location.y << ")\n";
                act->UnitCommand(unit, sc2::ABILITY_ID::SMART, location);
            }
        }
        else {
            act->UnitCommand(unit, sc2::ABILITY_ID::SMART, enemies.front());
        }
    }

    
}



/**
 * @brief Select most dangerous target to attack
 * 
 * @param unit 
 * @param enemies possible enemies to target
 * @return sc2::Unit* 
 */
const sc2::Unit* BasicSc2Bot::ChooseAttackTarget(const sc2::Unit *unit, const sc2::Units &enemies) {
    float max_ratio = 0; // danger ratio/score
    const sc2::Unit *target = nullptr;

    float range = unit->detect_range;

    // TODO: finish differentiate from siege tanks
    if (unit->unit_type == sc2::UNIT_TYPEID::TERRAN_SIEGETANK ||
        unit->unit_type == sc2::UNIT_TYPEID::TERRAN_SIEGETANKSIEGED) {
            range = 13;
        }

    // find most dangerous enemy (maximize health/distance)
    // TODO: filter out buildings
    for (const auto &enemy : enemies) {
        // enemy already dead
        if (!enemy->is_alive) {
            continue;
        }
        float distance = sc2::Distance2D(enemy->pos, unit->pos);
        float health = enemy->health + enemy->shield;

        // out of attack range
        if (distance > range) {
            continue;
        }

        // danger ratio
        float ratio = (health + 1) / (distance + 1); // + 1 to prevent 0 division

        if (ratio > max_ratio) {
            target = enemy;
            max_ratio = ratio;
        }
    }

    return target;
}

/**
 * @brief Handle attacking enemies with viking
 * 
 * @param squad with vikings
 * @param enemies 
 */
void BasicSc2Bot::VikingAttack(const sc2::Units &squad, const sc2::Units &enemies) {
    // all enemies or squad dead
    if (enemies.empty() || squad.empty()) {
        return;
    }

    // const sc2::ObservationInterface *obs = Observation();

    // track viking units
    sc2::Units vikings{};

    for (const auto &unit : squad) {
        if (unit->unit_type == sc2::UNIT_TYPEID::TERRAN_VIKINGASSAULT || 
            unit->unit_type == sc2::UNIT_TYPEID::TERRAN_VIKINGFIGHTER) {
            vikings.push_back(unit);
        }
    }

    // no vikings in squad
    if (vikings.empty()) {
        return;
    }
    
    // range constants
    const float AIR_RANGE = 9;
    const float GROUND_RANGE = 6;

    // prioritize attacking air enemies
    sc2::Units air_enemies{};
    sc2::Units ground_enemies{};

    // action interface
    sc2::ActionInterface *act = Actions();

    
    // float max_danger_ground{};

    for (const auto &enemy : enemies) {
        if (enemy->is_flying) {
            air_enemies.push_back(enemy);
        } else {
            ground_enemies.push_back(enemy);
        }
    }
    for (const auto &viking : vikings) {
        // most dangerous units
        const sc2::Unit *target_air = nullptr;
        const sc2::Unit *target_ground = nullptr;
        
        // track danger ratios
        float max_danger_air{};
        float min_dist = std::numeric_limits<float>::max();

        for (const auto &enemy : air_enemies) {
            // handle flying enemies
            float dist = sc2::Distance2D(viking->pos, enemy->pos);
            
            // not in range
            // if (dist > AIR_RANGE) {
            //     continue;
            // }

            float hp = enemy->health + enemy->shield;
            float cur_ratio = hp / dist;

            if (cur_ratio > max_danger_air) {
                max_danger_air = cur_ratio;
                target_air = enemy;
            }
        }
        // if attackable air unit
        if (target_air != nullptr) {
            act->UnitCommand(viking, sc2::ABILITY_ID::MORPH_VIKINGFIGHTERMODE);
            // act->UnitCommand(viking, sc2::ABILITY_ID::ATTACK, target_air);
            AttackWithUnit(viking, {target_air}, false);
            // dont check ground if already found an air target
            continue;
        }

        // TODO: retreat viking when no targetable (flying) enemies
    }
}

/**
 * @brief Handle attack
 * 
 * @param unit attacking unit
 */
void BasicSc2Bot::AttackWithUnit(const sc2::Unit *unit) {
    const sc2::ObservationInterface *obs = Observation();


    const sc2::Units enemies = obs->GetUnits(sc2::Unit::Alliance::Enemy);

    if (enemies.empty()) {
        return;
    }

    if (unit->orders.empty() || unit->orders.front().ability_id != sc2::ABILITY_ID::ATTACK) {
        if (unit->unit_type == sc2::UNIT_TYPEID::TERRAN_SIEGETANK || 
            unit->unit_type == sc2::UNIT_TYPEID::TERRAN_SIEGETANKSIEGED) {
            TankAttack({unit}, enemies);
        } else if (unit->unit_type == sc2::UNIT_TYPEID::TERRAN_VIKINGASSAULT ||
                   unit->unit_type == sc2::UNIT_TYPEID::TERRAN_VIKINGFIGHTER) {
            VikingAttack({unit}, enemies);
        } else {
            Actions()->UnitCommand(unit, sc2::ABILITY_ID::ATTACK, enemies.front()->pos);
        }
    }
}

/**
 * @brief Handle attacking in general case (attack whenever enemy in range)
 * 
 */
void BasicSc2Bot::HandleAttack() {
    const sc2::ObservationInterface *obs = Observation();
    sc2::ActionInterface *act = Actions();

    sc2::Units units = obs->GetUnits(sc2::Unit::Alliance::Self, NotStructure());
    
    for (const auto &unit : units) {
        HandleAttack(unit, obs);
    }

}

/**
 * @brief Handle attacking in general case for unit (attack whenver enemy in range)
 * 
 * @param unit 
 */
void BasicSc2Bot::HandleAttack(const sc2::Unit *unit, const sc2::ObservationInterface *obs) {
    // prioritize enemy units over structures
    float range{};
    // cant do anything with mule
    if (unit->unit_type == sc2::UNIT_TYPEID::TERRAN_MULE) {
        return;
    }
    
    if (unit->unit_type == sc2::UNIT_TYPEID::TERRAN_SIEGETANK || 
        unit->unit_type == sc2::UNIT_TYPEID::TERRAN_SIEGETANK) {
        range = 13;
    } else {
        range = unit->detect_range;
    }

    sc2::Units enemies = obs->GetUnits(sc2::Unit::Alliance::Enemy);
    
    // rocks
    sc2::Units neutral{};

    // no enemies, find rocks
    if (enemies.empty()) {
        // get destructible rocks
        neutral = obs->GetUnits(sc2::Unit::Alliance::Neutral, [](const sc2::Unit &unit) {
            // check for rocks
            if (
                unit.unit_type == sc2::UNIT_TYPEID::NEUTRAL_DEBRISRAMPLEFT ||
                unit.unit_type == sc2::UNIT_TYPEID::NEUTRAL_DEBRISRAMPRIGHT ||
                unit.unit_type == sc2::UNIT_TYPEID::NEUTRAL_DESTRUCTIBLECITYDEBRIS6X6 ||
                unit.unit_type == sc2::UNIT_TYPEID::NEUTRAL_DESTRUCTIBLEDEBRIS6X6 ||
                unit.unit_type == sc2::UNIT_TYPEID::NEUTRAL_DESTRUCTIBLEDEBRISRAMPDIAGONALHUGEBLUR ||
                unit.unit_type == sc2::UNIT_TYPEID::NEUTRAL_DESTRUCTIBLEDEBRISRAMPDIAGONALHUGEULBR ||
                unit.unit_type == sc2::UNIT_TYPEID::NEUTRAL_DESTRUCTIBLEROCK6X6 ||
                unit.unit_type == sc2::UNIT_TYPEID::NEUTRAL_DESTRUCTIBLEROCKEX1DIAGONALHUGEBLUR
            ) {
                return true;
            }
            return false;
        });

        // no rocks or enemies, do nothing
        if (neutral.empty()) {
            return;
        }
    }

    sc2::Units attacking{};
    sc2::Units enemies_in_range{};
    sc2::Units neutral_in_range{};
    
    // set attacking as enemy if possible
    if (enemies.size() > 0) {
        sc2::Units enemy_units_in_range{};
        sc2::Units enemy_structures_in_range{};

        for (const auto &enemy : enemies) {
            float dist = sc2::Distance2D(enemy->pos, unit->pos);

            // dont include enemy if not in range
            if (dist > range) {
                continue;
            }

            if (IsStructure(enemy->unit_type)) {
                enemy_structures_in_range.push_back(enemy);
            } else {
                enemy_units_in_range.push_back(enemy);
            }
            
            enemies_in_range.push_back((enemy));
        }
        
        // TODO: townhall, turret check
        // set units to attack
        attacking = enemy_units_in_range.empty() ? enemy_structures_in_range : enemy_units_in_range;


    } else if (neutral.size() > 0) {
        // set attacking to rocks in range

        for (const auto &rock : neutral) {
            float dist = sc2::Distance2D(rock->pos, unit->pos);

            // dont include enemy if not in range
            if (dist > range) {
                continue;
            }

            neutral_in_range.push_back(rock);
        }

        attacking = neutral_in_range;
    }

    
    // if (enemy_units_in_range.empty()) {
    //     // attack buildings if no units to attack
    //     attacking = enemy_structures_in_range;
    // }
    
    // default attack
    if (unit->unit_type == sc2::UNIT_TYPEID::TERRAN_SIEGETANK ||
        unit->unit_type == sc2::UNIT_TYPEID::TERRAN_SIEGETANKSIEGED) {
        if (enemies_in_range.empty()) {
            TankAttack({unit}, neutral_in_range);
        } else {
            TankAttack({unit}, enemies_in_range);
        }
        // TankAttack({unit}, attacking);
        return;
    }
    // attack with viking
    if (unit->unit_type == sc2::UNIT_TYPEID::TERRAN_VIKINGASSAULT ||
        unit->unit_type == sc2::UNIT_TYPEID::TERRAN_VIKINGFIGHTER) {
        VikingAttack({unit}, attacking);
        return;
    }
    // attack with battlecruiser
    if (unit->unit_type == sc2::UNIT_TYPEID::TERRAN_BATTLECRUISER) {
        BattlecruiserAttack({unit}, enemies_in_range);
    }

    // // scv attack
    // // only attack with scvs holding mineral and isnt repairing
    // if (unit->unit_type == sc2::UNIT_TYPEID::TERRAN_SCV) {
    //     return;
    //     // TODO: fix
    //     if (unit->buffs.size() > 0) {
    //         sc2::BUFF_ID buff = unit->buffs.front();
    //         if (buff != sc2::BUFF_ID::CARRYHIGHYIELDMINERALFIELDMINERALS &&
    //             buff != sc2::BUFF_ID::CARRYMINERALFIELDMINERALS) {
    //             // scv cant attack
    //             return;
    //         }
    //     }
    //     if (unit->orders.size() > 0) {
    //         sc2::UnitOrder order = unit->orders.front();
    //         if (order.ability_id != sc2::ABILITY_ID::HARVEST_GATHER &&
    //             order.ability_id != sc2::ABILITY_ID::HARVEST_RETURN) {
    //             // scv cant attack
    //             return;
    //         }
    //     }
    //     // TODO: being overwritten in assign worker, fix
    //     // std::cout << "attacking with scv \n";
    // }

    // default attack
    AttackWithUnit(unit, attacking);
    return;
    

}

/**
 * @brief Handle attack for squad with battlecruisers
 * 
 * @param squad with battlecruisers
 * @param enemies attackable enemies
 */
void BasicSc2Bot::BattlecruiserAttack(const sc2::Units &squad, const sc2::Units &enemies) {
    // all enemies or squad dead
    if (enemies.empty() || squad.empty()) {
        return;
    }
    
    // filter out battlecrusiers
    sc2::Units battlecruisers{};

    for (const auto &unit : squad) {
        if (unit->unit_type == sc2::UNIT_TYPEID::TERRAN_BATTLECRUISER) {
            battlecruisers.push_back(unit);
        }
    }

    // do nothing if no battlecruisers selected
    if (battlecruisers.empty()) {
        return;
    }

    // TODO: get fusion cores, check if yamato cannon available
    const sc2::ObservationInterface *obs = Observation();
    // const sc2::Upgrades &upgrades = obs->GetUpgrades();


    // sc2::Units fusion_cores = 
    for (const auto &battlecruiser : battlecruisers) {
        if (!battlecruiser->is_alive) {
            // skip dead
            continue;
        }
        
        // if (battlecruiser->buffs)
        
        // use yamato cannon when enemy
        // townhall
        // > 200 health
        // near death (15 health)
    }


}