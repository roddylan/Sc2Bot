// BasicSc2BotAttack.cpp
// contains implementation for all offensive (attacking, harrassment, expansion)
// and attack-like defensive functions

#include "BasicSc2Bot.h"
#include "Betweenness.h"
#include "Utilities.h"
#include "sc2api/sc2_api.h"
#include "sc2api/sc2_interfaces.h"
#include "sc2api/sc2_unit.h"
#include <cmath>
#include <cstddef>
#include <iostream>
#include <limits>
#include <sc2api/sc2_common.h>
#include <sc2api/sc2_data.h>
#include <sc2api/sc2_map_info.h>
#include <sc2api/sc2_typeenums.h>
#include <sc2api/sc2_unit_filters.h>
#include <sc2lib/sc2_search.h>

/**
 * @brief Sends squad of units to enemy start location that is closest to the
 * last death location of a unit
 */
void BasicSc2Bot::SendSquad() {

    const uint64_t minute = 1344;
    // Dont continue if less than 13 mins has elapsed (build order not ready
    // yet) if (Observation()->GetGameLoop() < 13 * minute) {
    if (Observation()->GetFoodUsed() > ATTACK_FOOD) {
        return;
    }
    std::vector<sc2::Point2D> enemy_locations =
        Observation()->GetGameInfo().enemy_start_locations;
    // Get Army units
    sc2::Units marines =
        Observation()->GetUnits(sc2::Unit::Alliance::Self,
                                sc2::IsUnit(sc2::UNIT_TYPEID::TERRAN_MARINE));
    sc2::Units banshees =
        Observation()->GetUnits(sc2::Unit::Alliance::Self,
                                sc2::IsUnit(sc2::UNIT_TYPEID::TERRAN_BANSHEE));
    sc2::Units marauders =
        Observation()->GetUnits(sc2::Unit::Alliance::Self,
                                sc2::IsUnit(sc2::UNIT_TYPEID::TERRAN_MARAUDER));
    sc2::Units liberators = Observation()->GetUnits(
        sc2::Unit::Alliance::Self,
        sc2::IsUnit(sc2::UNIT_TYPEID::TERRAN_LIBERATOR));
    sc2::Units tanks = Observation()->GetUnits(
        sc2::Unit::Alliance::Self,
        sc2::IsUnit(sc2::UNIT_TYPEID::TERRAN_SIEGETANK));
    sc2::Units vikings = Observation()->GetUnits(
        sc2::Unit::Alliance::Self,
        sc2::IsUnit(sc2::UNIT_TYPEID::TERRAN_VIKINGFIGHTER));
    sc2::Units battlecruisers = Observation()->GetUnits(
        sc2::Unit::Alliance::Self,
        sc2::IsUnit(sc2::UNIT_TYPEID::TERRAN_BATTLECRUISER));
    sc2::Units thors = Observation()->GetUnits(
        sc2::Unit::Alliance::Self, sc2::IsUnit(sc2::UNIT_TYPEID::TERRAN_THOR));

    // Create a squad of units with empty orders
    std::vector<const sc2::Unit *> squad;
    auto filter_units = [](const sc2::Units &units,
                           std::vector<const sc2::Unit *> &squad) {
        for (const auto &unit : units) {
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
        // Get enemy start location closest to last death location to prevent
        // from sending to empty location
        float min_distance = std::numeric_limits<float>::max();
        for (const auto &start_location : enemy_locations) {
            float distance = sc2::Distance2D(start_location,
                                             BasicSc2Bot::last_death_location);
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
    if (current_time > (last_send_time + 15 * frames_per_second) &&
        squad.size() > 11) {
        filter_units(marines, squad);
        if (!scout_died) {
            location = closest_start_location;
        } else {
            location = *enemy_starting_location;
        }
        Actions()->UnitCommand(squad, sc2::ABILITY_ID::ATTACK_ATTACK, location);

        // Update last send time
        last_send_time = current_time;
        units_sent++;
    }

    // Stop sending more units if all enemy locations are covered
    if (units_sent >= enemy_locations.size()) {
        return;
    }
}

/**
 * @brief Check if enemies are near the base, and attack them if they are
 */
bool BasicSc2Bot::AttackIntruders() {
    /*
     * This method does too much work to be called every frame. Call it every
     * few hundred frames instead
     */
    static size_t last_frame_checked = 0;
    const sc2::ObservationInterface *observation = Observation();
    const uint32_t &current_frame = observation->GetGameLoop();
    if (current_frame - last_frame_checked < 400) {
        return false;
    }
    last_frame_checked = current_frame;
    const sc2::Units &enemy_units =
        observation->GetUnits(sc2::Unit::Alliance::Enemy);

    /*
     * Attack enemy units that are near the base
     */

    const sc2::Units &bases = observation->GetUnits(
        sc2::Unit::Alliance::Self,
        sc2::IsUnit(sc2::UNIT_TYPEID::TERRAN_COMMANDCENTER));

    for (const sc2::Unit *base : bases) {
        const sc2::Unit *enemy_near_base = nullptr;
        for (const sc2::Unit *enemy_unit : enemy_units) {
            if (sc2::DistanceSquared2D(base->pos, enemy_unit->pos) < 40 * 40) {
                enemy_near_base = enemy_unit;
                break;
            }
        }
        // we didn't find a nearby enemy to attack
        if (enemy_near_base == nullptr) {
            continue;
        }

        const sc2::Units &defending_units = observation->GetUnits(
            sc2::Unit::Alliance::Self,
            sc2::IsUnits({sc2::UNIT_TYPEID::TERRAN_MARINE,
                          sc2::UNIT_TYPEID::TERRAN_MARAUDER,
                          sc2::UNIT_TYPEID::TERRAN_SIEGETANK,
                          sc2::UNIT_TYPEID::TERRAN_SIEGETANKSIEGED,
                          sc2::UNIT_TYPEID::TERRAN_THOR,
                          sc2::UNIT_TYPEID::TERRAN_THORAP,
                          sc2::UNIT_TYPEID::TERRAN_VIKINGFIGHTER,
                          sc2::UNIT_TYPEID::TERRAN_VIKINGASSAULT}));

        // send all units to attack
        Actions()->UnitCommand(defending_units, sc2::ABILITY_ID::ATTACK_ATTACK,
                               enemy_near_base);

        // move the medivacs to the battle so that they heal the units
        if (!defending_units.empty()) {
            const sc2::Units &medivacs = observation->GetUnits(
                sc2::Unit::Alliance::Self,
                sc2::IsUnit(sc2::UNIT_TYPEID::TERRAN_MEDIVAC));
            for (const sc2::Unit *medivac : medivacs) {
                const sc2::Point2D &pos_to_move_to =
                    defending_units.front()->pos;
                Actions()->UnitCommand(medivac, sc2::ABILITY_ID::MOVE_MOVE,
                                       pos_to_move_to);
            }
        }

        break;
    }

    return true;
}

/**
 * @brief Handles expanding the base by building a new command center near a new
 * mineral patch
 * @param resources_depleted:
 * @return true if command to build expansion was sent, false otherwise
 */
bool BasicSc2Bot::HandleExpansion(bool resources_depleted) {
    const sc2::ObservationInterface *obs = Observation();
    // cant afford
    if (obs->GetMinerals() < 400) {
        return false;
    }
    sc2::Units bases =
        obs->GetUnits(sc2::Unit::Alliance::Self, sc2::IsTownHall());
    sc2::Units siege_tanks =
        obs->GetUnits(sc2::Unit::Alliance::Self,
                      sc2::IsUnit(sc2::UNIT_TYPEID::TERRAN_SIEGETANK));
    sc2::Units marines =
        obs->GetUnits(sc2::Unit::Alliance::Self,
                      sc2::IsUnit(sc2::UNIT_TYPEID::TERRAN_MARINE));

    size_t n_bases = bases.size();
    size_t n_siege_tanks = siege_tanks.size();
    size_t n_marines = marines.size();

    if (resources_depleted) {
        goto expand;
    }

    if (n_bases > 0 && (n_marines < this->n_marines * n_bases)) {
        // only expand when enough units to defend base + protect expansion
        return false;
    }

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

    for (const auto &exp : expansion_locations) {
        float cur_dist = sc2::Distance2D(sc2::Point2D(start_location),
                                         sc2::Point2D(exp.x, exp.y));
        if (cur_dist < min_dist) {
            sc2::Point2D nearest_command_center =
                FindNearestCommandCenter(sc2::Point2D(exp.x, exp.y));
            if (nearest_command_center == sc2::Point2D(0, 0)) {
                continue;
            }

            float dist_to_base = sc2::Distance2D(nearest_command_center,
                                                 sc2::Point2D(exp.x, exp.y));

            if (Query()->Placement(sc2::ABILITY_ID::BUILD_COMMANDCENTER, exp) &&
                dist_to_base > 1.0f) {
                min_dist = cur_dist;
                closest_expansion = exp;
            }
        }
    }

    if (closest_expansion != sc2::Point3D(0, 0, 0)) {
        sc2::Point2D expansion_location(closest_expansion.x,
                                        closest_expansion.y);
        sc2::Point2D p(0, 0);

        if (TryBuildStructure(sc2::ABILITY_ID::BUILD_COMMANDCENTER, p,
                              expansion_location)) {
            base_location = closest_expansion; // set base to closest expansion
        }
    }

    return true;
}

/**
 * @brief Handle attack for squad with tanks
 * @param squad: the squad of our troops
 */
void BasicSc2Bot::TankAttack(const sc2::Units &squad) {
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
    const float THRESHOLD = (TANK_RANGE + TANK_SIEGE_RANGE) / 2 +
                            1; // threshold distance to choose b/w modes

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

        // closest enemy detected within siege range -> move back and go siege
        // mode
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
 * @param squad: Our collection of troops
 * @param enemies: Collection of enemy troops to target
 */
void BasicSc2Bot::TankAttack(const sc2::Units &squad,
                             const sc2::Units &enemies) {

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
    const float THRESHOLD = (TANK_RANGE + TANK_SIEGE_RANGE) / 2 +
                            1; // threshold distance to choose b/w modes

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

        // closest enemy detected within siege range -> move back and go siege
        // mode
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
void BasicSc2Bot::AttackWithUnit(const sc2::Unit *unit,
                                 const sc2::Units &enemies,
                                 const bool &atk_pos) {
    if (enemies.empty()) {
        return;
    }

    // attack enemy
    if (atk_pos) {
        Actions()->UnitCommand(unit, sc2::ABILITY_ID::ATTACK,
                               enemies.front()->pos);
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

    sc2::Units enemies = obs->GetUnits(sc2::Unit::Alliance::Enemy);

    if (enemies.empty() && obs->GetFoodUsed() < ATTACK_FOOD) {
        sc2::Units units =
            obs->GetUnits(sc2::Unit::Alliance::Self, IsArmy(obs));
        for (const auto &unit : units) {
            if (unit->unit_type == sc2::UNIT_TYPEID::TERRAN_SIEGETANKSIEGED) {
                act->UnitCommand(unit, sc2::ABILITY_ID::MORPH_UNSIEGE);
            }
            Retreat(unit);
        }
        // leave if retreating
        return;
    }

    if (obs->GetFoodArmy() < (175 - obs->GetFoodWorkers() - N_ARMY_THRESHOLD)) {
        return;
    }

    const sc2::Units raid_squad =
        obs->GetUnits(sc2::Unit::Alliance::Self, IsArmy(obs));

    // do nothing if raid squad dne
    if (raid_squad.empty()) {
        return;
    }

    for (const auto &unit : raid_squad) {
        if (enemies.empty() &&
            unit->unit_type == sc2::UNIT_TYPEID::TERRAN_VIKINGFIGHTER) {
            if (!unit->orders.empty()) {
                continue;
            }
            sc2::Point2D location{};
            // bool check = ScoutRandom(raid_squad.front(), location);
            bool check = ScoutRandom(unit, location);

            if (check) {
                act->UnitCommand(unit, sc2::ABILITY_ID::SMART, location);
            }
        } else if (!enemies.empty()) {
            if (!unit->orders.empty()) {
                sc2::UnitOrder order = unit->orders.front();
                if (order.ability_id == sc2::ABILITY_ID::ATTACK ||
                    order.ability_id == sc2::ABILITY_ID::ATTACK_ATTACK) {
                    continue;
                }
            }
            if (enemies.front()->is_alive) {
                act->UnitCommand(unit, sc2::ABILITY_ID::ATTACK,
                                 enemies.front()->pos);
            }
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
const sc2::Unit *BasicSc2Bot::ChooseAttackTarget(const sc2::Unit *unit,
                                                 const sc2::Units &enemies) {
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
        float ratio =
            (health + 1) / (distance + 1); // + 1 to prevent 0 division

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
void BasicSc2Bot::VikingAttack(const sc2::Units &squad,
                               const sc2::Units &enemies) {
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
            AttackWithUnit(viking, {target_air}, false);
            // dont check ground if already found an air target
            continue;
        }
    }
}

/**
 * @brief Handle attack
 *
 * @param unit: attacking unit
 */
void BasicSc2Bot::AttackWithUnit(const sc2::Unit *unit) {
    const sc2::ObservationInterface *obs = Observation();

    const sc2::Units enemies = obs->GetUnits(sc2::Unit::Alliance::Enemy);

    if (enemies.empty()) {
        return;
    }

    if (unit->orders.empty() ||
        unit->orders.front().ability_id != sc2::ABILITY_ID::ATTACK) {
        if (unit->unit_type == sc2::UNIT_TYPEID::TERRAN_SIEGETANK ||
            unit->unit_type == sc2::UNIT_TYPEID::TERRAN_SIEGETANKSIEGED) {
            TankAttack({unit}, enemies);
        } else if (unit->unit_type == sc2::UNIT_TYPEID::TERRAN_VIKINGASSAULT ||
                   unit->unit_type == sc2::UNIT_TYPEID::TERRAN_VIKINGFIGHTER) {
            VikingAttack({unit}, enemies);
        } else {
            Actions()->UnitCommand(unit, sc2::ABILITY_ID::ATTACK,
                                   enemies.front()->pos);
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
 * @brief Handle attacking in general case for unit (attack whenver enemy in
 * range)
 *
 * @param unit
 */
void BasicSc2Bot::HandleAttack(const sc2::Unit *unit,
                               const sc2::ObservationInterface *obs) {
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
    range += RANGE_BUFFER;

    sc2::Units enemies = obs->GetUnits(sc2::Unit::Alliance::Enemy);

    // rocks
    sc2::Units neutral{};

    // no enemies, find rocks
    if (enemies.empty()) {
        // get destructible rocks
        neutral = obs->GetUnits(sc2::Unit::Alliance::Neutral,
                                [](const sc2::Unit &unit) {
                                    // check for rocks
                                    if (IsRock(unit.unit_type)) {
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

            enemies_in_range.push_back(enemy);
        }

        // set units to attack
        attacking = enemy_units_in_range.empty() ? enemy_structures_in_range
                                                 : enemy_units_in_range;

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

    // default attack
    AttackWithUnit(unit, enemies_in_range);
    return;
}

/**
 * @brief Handle attack for squad with battlecruisers
 *
 * @param squad with battlecruisers
 * @param enemies attackable enemies
 */
void BasicSc2Bot::BattlecruiserAttack(const sc2::Units &squad,
                                      const sc2::Units &enemies) {
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

    const sc2::ObservationInterface *obs = Observation();

    for (const auto &battlecruiser : battlecruisers) {
        if (!battlecruiser->is_alive) {
            // skip dead
            continue;
        }

        // use yamato cannon when enemy
        // townhall
        // > 200 health
        // near death (15 health)
    }
}