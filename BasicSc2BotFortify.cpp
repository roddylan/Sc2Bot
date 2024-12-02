// BasicSc2BotFortify.cpp
// implementation for pure defensive functions (walling chokepoints, etc.)

#include "BasicSc2Bot.h"
#include "Utilities.h"
#include "sc2api/sc2_api.h"
#include "sc2api/sc2_unit.h"
#include "sc2api/sc2_interfaces.h"
#include <limits>
#include <sc2api/sc2_common.h>
#include <sc2api/sc2_typeenums.h>
#include <sc2api/sc2_unit_filters.h>
#include <sc2lib/sc2_search.h>


/**
 * @brief Defend area with turret
 * 
 * @param turret 
 */
void BasicSc2Bot::TurretDefend(const sc2::Unit *turret) {
    const sc2::ObservationInterface *obs = Observation();
    sc2::ActionInterface *acts = Actions();
    
    sc2::Units enemies = obs->GetUnits(sc2::Unit::Alliance::Enemy, [](const sc2::Unit &unit) {
        // enemy visible
        bool is_vis = (unit.display_type == sc2::Unit::DisplayType::Visible);

        return is_vis && unit.is_flying;
    });

    if (enemies.empty()) {
        return;
    }

    const float MISSILE_TURRET_ATTACK_RANGE = turret->detect_range;

    float max_danger = 0;
    const sc2::Unit *attack = nullptr;

    for (const auto &enemy : enemies) {
        // dont attack dead
        if (!enemy->is_alive) {
            continue;
        }

        float enemy_health = enemy->health + enemy->shield;
        float distance = sc2::Distance2D(enemy->pos, turret->pos);

        // out of range
        if (distance > MISSILE_TURRET_ATTACK_RANGE) {
            continue;
        }

        // danger ratio
        float ratio = (enemy_health + 1) / (distance + 1); // + 1 to prevent 0 division

        if (max_danger < ratio) {
            max_danger = ratio;
            attack = enemy;
        }
    }

    if (attack != nullptr) {
       // std::cout << "TURRET ATTACK\n";
        // able to attack
        acts->UnitCommand(turret, sc2::ABILITY_ID::ATTACK_ATTACK, attack);
    }


}


/**
 * @brief Retreat unit to location or nearest base
 * 
 * @param unit 
 * @param location 
 */
void BasicSc2Bot::Retreat(const sc2::Unit *unit, sc2::Point2D location) {
    if (location == sc2::Point2D{0, 0}) {
        location = FindNearestCommandCenter(unit->pos);
    }

    if (location == sc2::Point2D{0, 0}) {
        return;
    }

    float dist = sc2::Distance2D(unit->pos, location);

    if (dist < 10) {
        // do nothing if there and chilling
        if (unit->orders.empty()) {
            return;
        }
        // stop moving when close
        Actions()->UnitCommand(unit, sc2::ABILITY_ID::STOP);
    }

    Actions()->UnitCommand(unit, sc2::ABILITY_ID::SMART, location);
    return;

}

/**
 * @brief Repair base
 * 
 */
void BasicSc2Bot::RepairBase() {
    const sc2::ObservationInterface *obs = Observation();
    sc2::Units bases = obs->GetUnits(sc2::Unit::Alliance::Self, sc2::IsTownHall());;
    sc2::Units scvs = obs->GetUnits(sc2::Unit::Alliance::Self, sc2::IsUnit(sc2::UNIT_TYPEID::TERRAN_SCV));

    

}


/**
 * @brief Make supply depots walls (rise up) when enemy near
 * 
 */

void BasicSc2Bot::Wall() {
    // interfaces
    const sc2::ObservationInterface *obs = Observation();
    sc2::ActionInterface *act = Actions();

    const sc2::Units supply_depots = obs->GetUnits(
        sc2::Unit::Alliance::Self, sc2::IsUnits({
            sc2::UNIT_TYPEID::TERRAN_SUPPLYDEPOT,
            sc2::UNIT_TYPEID::TERRAN_SUPPLYDEPOTLOWERED
        })
    );

    // enemy troops
    const sc2::Units enemies = obs->GetUnits(sc2::Unit::Alliance::Enemy, IsArmy(obs));

    for (const auto &depot : supply_depots) {
        bool wall = false;
        for (const auto &enemy : enemies) {
            // dont care about walling flying or dead enemies
            if (enemy->is_flying || !enemy->is_alive) {
                continue;
            }
            float dist = sc2::Distance2D(enemy->pos, depot->pos);
            wall = (dist <= WALL_RANGE);
            // if need to wall and depot is lowered -> raise up wall
            if (depot->unit_type == sc2::UNIT_TYPEID::TERRAN_SUPPLYDEPOTLOWERED && wall) {
                act->UnitCommand(depot, sc2::ABILITY_ID::MORPH_SUPPLYDEPOT_RAISE);
            }
            // go to next depot if walling
            if (wall) {
                break;
            }
        }
        // go to next depot if walled up
        if (wall) {
            continue;
        } else if (depot->unit_type == sc2::UNIT_TYPEID::TERRAN_SUPPLYDEPOT) {
            // dont need to wall but depot is raised -> lower it
            act->UnitCommand(depot, sc2::ABILITY_ID::MORPH_SUPPLYDEPOT_LOWER);
        }
    }

}