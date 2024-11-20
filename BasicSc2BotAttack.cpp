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
    if (resources_depleted) {
        // goto expand;
    }
    // TODO: change siege tank req
    // if (n_bases > 0 && n_siege_tanks < (n_bases * 1 + 1) && n_marines >= this->n_marines * n_bases) {
    // if (n_bases > 0 && (n_siege_tanks < (n_bases * this->n_tanks) || n_marines < this->n_marines * n_bases)) {
    if (n_bases > 0 && (n_siege_tanks < (n_bases * 1 + 1) || n_marines < this->n_marines * n_bases)) {
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
            // std::cout << "distance to base: " << dist_to_base << std::endl;

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
            std::cout << "EXPANSION TIME BABY\n\n";
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