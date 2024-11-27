// BasicSc2BotFortify.cpp
// implementation for pure defensive functions (walling chokepoints, etc.)

#include "BasicSc2Bot.h"
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