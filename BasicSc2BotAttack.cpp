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
#include <sc2api/sc2_typeenums.h>
#include <sc2api/sc2_unit_filters.h>
#include <sc2lib/sc2_search.h>
#include <iostream>
#include <cmath>

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

        const sc2::Units& defending_units = observation->GetUnits(sc2::Unit::Alliance::Self, sc2::IsUnits({ sc2::UNIT_TYPEID::TERRAN_MARINE, sc2::UNIT_TYPEID::TERRAN_MARAUDER }));
        for (const sc2::Unit* defending_unit : defending_units) {
            Actions()->UnitCommand(defending_unit, sc2::ABILITY_ID::ATTACK_ATTACK, enemy_near_base);
        }

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
        AttackWithUnit(tank, enemies_in_range);
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
        AttackWithUnit(tank, enemies);
    }
}

/**
 * @brief Attacking enemies with unit
 * 
 * @param unit 
 * @param enemies in range
 */
void BasicSc2Bot::AttackWithUnit(const sc2::Unit *unit, const sc2::Units &enemies) {
    if (enemies.empty()) {
        return;
    }

    // attack enemy
    Actions()->UnitCommand(unit, sc2::ABILITY_ID::ATTACK, enemies.front()->pos);
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
    if (obs->GetFoodArmy() < (200 - obs->GetFoodWorkers() - N_ARMY_THRESHOLD)) {
        return;
    }
    
    // TODO: decide if keep some at base or send all to attack

    // basic ground troops (marine, marauder)
    sc2::Units marines = obs->GetUnits(sc2::Unit::Alliance::Self, sc2::IsUnit(sc2::UNIT_TYPEID::TERRAN_MARINE));
    sc2::Units marauders = obs->GetUnits(sc2::Unit::Alliance::Self, sc2::IsUnit(sc2::UNIT_TYPEID::TERRAN_MARAUDER));
    
    // basic air troops

    // mech ground troops
    sc2::Units siege_tanks = obs->GetUnits(sc2::Unit::Alliance::Self, sc2::IsUnits({
        sc2::UNIT_TYPEID::TERRAN_SIEGETANK, sc2::UNIT_TYPEID::TERRAN_SIEGETANKSIEGED
    }));
    sc2::Units thor = obs->GetUnits(sc2::Unit::Alliance::Self, sc2::IsUnits({
        sc2::UNIT_TYPEID::TERRAN_THOR, sc2::UNIT_TYPEID::TERRAN_THORAP
    }));

    // mech air troops
    sc2::Units vikings = obs->GetUnits(sc2::Unit::Alliance::Self, sc2::IsUnits({
        sc2::UNIT_TYPEID::TERRAN_VIKINGASSAULT, sc2::UNIT_TYPEID::TERRAN_VIKINGFIGHTER
    }));

    
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
            act->UnitCommand(viking, sc2::ABILITY_ID::ATTACK, target_air);
            // AttackWithUnit(viking, {target_air});
            // dont check ground if already found an air target
            continue;
        }

        // TODO: retreat viking when no targetable (flying) enemies
    }
}