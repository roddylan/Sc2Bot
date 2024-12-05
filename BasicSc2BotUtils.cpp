// BasicSc2BotUtils.cpp
// contains implementation for misc. utility functions (finders, is, in, count, etc.)

#include "BasicSc2Bot.h"
#include "Utilities.h"
#include "Betweenness.h"
#include "sc2api/sc2_api.h"
#include "sc2api/sc2_unit.h"
#include "sc2api/sc2_interfaces.h"
#include <limits>
#include <sc2api/sc2_common.h>
#include <sc2api/sc2_map_info.h>
#include <sc2api/sc2_typeenums.h>
#include <sc2api/sc2_unit_filters.h>
#include <sc2lib/sc2_search.h>
#include <iostream>
#include <cmath>
/*
 * Enemy Filter
 */
sc2::Filter isEnemy = [](const sc2::Unit& unit) {
    return unit.alliance != sc2::Unit::Alliance::Self; 
};
/*
 * @brief InBunker
 *
 * @param myUnits
 * @param marine
 * @return bool
 */
bool InBunker (sc2::Units myUnits, const sc2::Unit* marine) {
    for (auto& bunker : myUnits) {
        for (const auto& passenger : bunker->passengers) {
            // Check if already in bunker
            if (passenger.tag == marine->tag) {
                return true;
            }
        }
    }
    return false;
}
/*
 * @brief Loads marine into a bunker
 *
 * @param marine
 * @return bool
 */
bool BasicSc2Bot::LoadBunker(const sc2::Unit* marine) {
    sc2::Units myUnits = Observation()->GetUnits(sc2::Unit::Alliance::Self, IsUnit(sc2::UNIT_TYPEID::TERRAN_BUNKER));
    bool bunkerLoaded = false;
    // If already in bunker 
    if (InBunker(myUnits, marine)) return true;
    // Load bunkers
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
/*
 * @brief Counts how many siege tanks are near factory
 *
 * @param factory
 * @return int
 */
int BasicSc2Bot::CountNearbySeigeTanks(const sc2::Unit* factory) {
    sc2::Units seige_tanks = Observation()->GetUnits(sc2::Unit::Alliance::Self, IsUnit(sc2::UNIT_TYPEID::TERRAN_SIEGETANK));
    int count = 0;
    // Count siege tanks within 2.0f of factory
    for (const auto &seige_tank : seige_tanks) {
        float distance = sc2::Distance2D(seige_tank->pos, factory->pos);
        if (distance < 2.0f) {
            ++count;
        }
    }
    return count;
}

/*
 * Counts units of type unit_type
 */
size_t BasicSc2Bot::CountUnitType(sc2::UNIT_TYPEID unit_type) {
    return Observation()->GetUnits(sc2::Unit::Alliance::Self, IsUnit(unit_type)).size();
}

/*
 * SCV Filter
 */
sc2::Filter scvFilter = [](const sc2::Unit& unit) {
    return unit.unit_type == sc2::UNIT_TYPEID::TERRAN_SCV;
};

/*
 * @brief Finds nearest mineral patch to start
 *
 * @param start
 * @return const Unit*
 */
const sc2::Unit* BasicSc2Bot::FindNearestMineralPatch(const sc2::Point2D& start) {
    sc2::Units units = Observation()->GetUnits(sc2::Unit::Alliance::Neutral);
    float distance = std::numeric_limits<float>::max();
    const sc2::Unit* target = nullptr;
    // Get closest
    for (const auto& u : units) {
        if (u->unit_type == sc2::UNIT_TYPEID::NEUTRAL_MINERALFIELD || u->unit_type == sc2::UNIT_TYPEID::NATURALMINERALS || u->unit_type == sc2::UNIT_TYPEID::MINERALCRYSTAL || u->unit_type == sc2::UNIT_TYPEID::NEUTRAL_RICHMINERALFIELD  ) {
            float d = sc2::DistanceSquared2D(u->pos, start);
            if (d < distance) {
                distance = d;
                target = u;
            }
        }
    }
    return target;
}

/*
 * @brief Finds nearest vespene geyser to start
 *
 * @param start
 * @return const Unit*
 */
const sc2::Unit* BasicSc2Bot::FindNearestVespeneGeyser(const sc2::Point2D& start) {
    sc2::Units units = Observation()->GetUnits(sc2::Unit::Alliance::Neutral);
    float distance = std::numeric_limits<float>::max();
    const sc2::Unit* target = nullptr;
    for (const auto& u : units) {
        if (u->unit_type == sc2::UNIT_TYPEID::NEUTRAL_VESPENEGEYSER || u->unit_type == sc2::UNIT_TYPEID::NEUTRAL_SPACEPLATFORMGEYSER) {
            float d = sc2::DistanceSquared2D(u->pos, start);
            if (d < distance) {
                distance = d;
                target = u;
            }
        }
    }
    return target;
}

/*
 * @brief Finds nearest command center to start
 *
 * @param start
 * @param not_start_location
 * @return const Point2D
 */
const sc2::Point2D BasicSc2Bot::FindNearestCommandCenter(const sc2::Point2D& start, bool not_start_location) {

    sc2::Units bases = Observation()->GetUnits(sc2::Unit::Alliance::Self, sc2::IsTownHall());
    float distance = std::numeric_limits<float>::max();
    const sc2::Unit* target = nullptr;

    for (const auto& base : bases) {
        if (base->unit_type == sc2::UNIT_TYPEID::TERRAN_ORBITALCOMMAND 
            || base->unit_type == sc2::UNIT_TYPEID::TERRAN_COMMANDCENTER 
            || base->unit_type == sc2::UNIT_TYPEID::TERRAN_ORBITALCOMMANDFLYING) {
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

/*
 * @brief Finds nearest refinery to start
 *
 * @param start
 * @return Point2D
 */
const sc2::Point2D BasicSc2Bot::FindNearestRefinery(const sc2::Point2D& start) {

    sc2::Units refineries = Observation()->GetUnits(sc2::Unit::Self, sc2::IsUnit(sc2::UNIT_TYPEID::TERRAN_REFINERY));
    float distance = std::numeric_limits<float>::max();
    const sc2::Unit* target = nullptr;

    for (const auto& refinery : refineries) {

        float d = sc2::DistanceSquared2D(refinery->pos, start);
        if (d < distance) {
            distance = d;
            target = refinery;
        }
        
    }

    if (target != nullptr) {
        return target->pos;
    }
    else {
        return sc2::Point2D(0, 0);
    }
}

/*
 * @brief Returns the size of a given marine cluster
 *
 * @param marine
 * @param marines
 * @return int: size of cluster
 */
int BasicSc2Bot::MarineClusterSize(const sc2::Unit* marine, const sc2::Units& marines) {
    int num_nearby_marines = 0;
    const float distance_threshold_sq = 25.0f;
    // first find closest marine
    for (const auto& other_marine : marines) {
        if (marine == other_marine) continue;

        float dx = marine->pos.x - other_marine->pos.x;
        float dy = marine->pos.y - other_marine->pos.y;
        float dist_to_marine_sq = dx * dx + dy * dy;

        if (dist_to_marine_sq < distance_threshold_sq) {
            ++num_nearby_marines;
        }
    }

    return num_nearby_marines;
}

/*
 * @brief Finds a marine whose health is not at max (for medivacs)
 *
 * @return Unit*
 */
const sc2::Unit* BasicSc2Bot::FindInjuredMarine() {
    const sc2::Units marines = Observation()->GetUnits(sc2::Unit::Alliance::Self, IsUnit(sc2::UNIT_TYPEID::TERRAN_MARINE));
    
    for (const auto marine : marines) {
        
        if (marine->health < marine->health_max) {
            sc2::Units medivacs = SortMedivacsAccordingToDistance(marine->pos);
            if (!medivacs.empty()) {
                // medivacs[0] because first element is closest
                if (sc2::Distance2D(medivacs[0]->pos, marine->pos) < 5.0f) {
                    continue;
                }
                return marine;
            }
            
            
        }
    }
    return nullptr;
}
/*
 * @brief Sorts medivacs according to their distance to start
 *
 * @param start
 * @return Units
 */
const sc2::Units BasicSc2Bot::SortMedivacsAccordingToDistance(const sc2::Point2D start) {
    sc2::Units medivacs = Observation()->GetUnits(sc2::Unit::Alliance::Self, IsUnit(sc2::UNIT_TYPEID::TERRAN_MEDIVAC));
    std::sort(medivacs.begin(), medivacs.end(), [&start](const sc2::Unit* unit_a, const sc2::Unit* unit_b) {
        float dx_a = unit_a->pos.x - start.x;
        float dy_a = unit_a->pos.y - start.y;
        float dist_a_sq = dx_a * dx_a + dy_a * dy_a;

        float dx_b = unit_b->pos.x - start.x;
        float dy_b = unit_b->pos.y - start.y;
        float dist_b_sq = dx_b * dx_b + dy_b * dy_b;

        return dist_a_sq < dist_b_sq;
        });
    return medivacs;
}

/*
 * @brief Finds largest marine cluster to start
 *
 * @param start
 * @param unit
 * @return const Point2D(0, 0) if unseccessful
 * @return Point2D
 */
const sc2::Point2D BasicSc2Bot::FindLargestMarineCluster(const sc2::Point2D& start, const sc2::Unit& unit) {
    const sc2::Units marines = Observation()->GetUnits(sc2::Unit::Alliance::Self, IsUnit(sc2::UNIT_TYPEID::TERRAN_MARINE));
    sc2::Units medivacs = SortMedivacsAccordingToDistance(start);

    if (medivacs.empty()) {
        return sc2::Point2D(0, 0);
    }

    // Sort medivacs by distance to the starting point
    
    const sc2::Unit* closest_medivac = nullptr;
    // Get closest medivac that isnt itself (unit)
    if (medivacs.size() > 1) {
        closest_medivac = medivacs[0] == &unit ? medivacs[1] : medivacs[0];
    }
    else {
        return marines.size() > 0 ? marines[0]->pos : sc2::Point2D(0, 0);
    }
       
    
    // Marine to go to
    const sc2::Unit* target_marine = nullptr;
    int largest_cluster_size = 0;

    for (const auto& marine : marines) {
        int cluster_size = MarineClusterSize(marine, marines);

        if (cluster_size > largest_cluster_size && sc2::Distance2D(closest_medivac->pos, marine->pos) > 15.0f) {
            largest_cluster_size = cluster_size;
            target_marine = marine;
        }
    }

    return target_marine ? target_marine->pos : sc2::Point2D(0, 0);
}


/*
 * @brief Gets an SCV that is currently gathering, or return nullptr if there are none.
 * Useful to call when you need to assign an SCV to do a task but you don't want to
 * interrupt other important tasks.
 *
 * @param starting_point
 * @return const Unit*
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

/*
 * @brief Functor that implements < for a Point2D so that we can holds points in a std::set in FindPlaceablePositionNear
 */
struct Point2DLt {
    bool operator()(const sc2::Point2D& lhs, const sc2::Point2D& rhs) const {
        if (lhs.x == rhs.x) {
            return lhs.y < rhs.y;
        }
        return lhs.x < rhs.x;
    }
};
/*
 * @brief Finds a location to place a building at using a triangle with its base as a line between 2 nearest structures to starting_point
 *
 * @param starting_point
 * @param ability_to_place_building
 * @return Point2D(0, 0) if unseccessful
 * @return Point2D
 */
sc2::Point2D BasicSc2Bot::FindPlaceablePositionNear(const sc2::Point2D& starting_point, const sc2::ABILITY_ID& ability_to_place_building) {

    // Get all existing structures
    sc2::Units all_buildings = Observation()->GetUnits(sc2::Unit::Alliance::Self, sc2::IsUnits({ sc2::UNIT_TYPEID::TERRAN_COMMANDCENTER,
        sc2::UNIT_TYPEID::TERRAN_ORBITALCOMMAND, 
        sc2::UNIT_TYPEID::TERRAN_PLANETARYFORTRESS, 
        sc2::UNIT_TYPEID::TERRAN_REFINERY, 
        sc2::UNIT_TYPEID::TERRAN_FACTORY, 
        sc2::UNIT_TYPEID::TERRAN_BARRACKS, 
        sc2::UNIT_TYPEID::TERRAN_STARPORT, 
        sc2::UNIT_TYPEID::TERRAN_ENGINEERINGBAY, 
        sc2::UNIT_TYPEID::TERRAN_ARMORY } ));

    // Sort buildings by distance to the starting point
    std::sort(all_buildings.begin(), all_buildings.end(),
        [&starting_point](const sc2::Unit* a, const sc2::Unit* b) {
            float dist_a = sc2::DistanceSquared2D(starting_point, a->pos);
            float dist_b = sc2::DistanceSquared2D(starting_point, b->pos);
            return dist_a < dist_b; 
        });

    // Start by using nearest structures


    /*
        This portion of the code was implemented with the help of ChatGPT
        * via ChatGPT: "im trying to use triangles to nicely space out my structures from another, take 2 and make a triangle out of them using the line between the structures"
        * "i want you to make sure it finds a point it has to keep finding one until the placement condition is true"
    */
    for (int i = 0; i < all_buildings.size(); i++) {
        if ((i + 1) < all_buildings.size()) {
            const sc2::Unit* a = all_buildings[i];
            const sc2::Unit* b = all_buildings[i + 1];

            // Initial height of the triangle
            float height_of_triangle = 5.0f;
            // Used to increment height if placement fails
            const float height_increment = 2.0f;
            sc2::Point2D midpoint = sc2::Point2D((a->pos.x + b->pos.x) / 2, (a->pos.y + b->pos.y) / 2);
            // Drection vector from building a to b
            sc2::Point2D direction = sc2::Point2D(b->pos.x - a->pos.x, b->pos.y - a->pos.y);
            float magnitude = std::sqrt(direction.x * direction.x + direction.y * direction.y);
            sc2::Point2D normalized_direction = sc2::Point2D(direction.x / magnitude, direction.y / magnitude);

            // First perpendicular vector 
            sc2::Point2D perpendicular = sc2::Point2D(-normalized_direction.y, normalized_direction.x);
            // How many times we failed to seach search -> 4 = done
            int direction_changes = 0;

            sc2::Point2D new_point;
            const sc2::Unit* placing_unit = nullptr; 

            while (true) {
                new_point = midpoint + perpendicular * height_of_triangle;

                // Check placement with add on
                if (Query()->Placement(ability_to_place_building, new_point, placing_unit)) {
                    // Check for wiggle room for techlab/reactor
                    bool can_wiggle_left = Query()->Placement(ability_to_place_building, sc2::Point2D(new_point.x - 2, new_point.y), placing_unit);
                    bool can_wiggle_right = Query()->Placement(ability_to_place_building, sc2::Point2D(new_point.x + 2, new_point.y), placing_unit);

                    if (can_wiggle_left && can_wiggle_right) {
                        break;
                    }
                }

                // Increment the height and retry
                height_of_triangle += height_increment;
                // Change direction if height exceeds 20
                if (height_of_triangle > 20.0f) {
                    direction_changes++;
                    // Alternate between two perpendicular directions
                    perpendicular = (direction_changes % 2 == 0)
                        // Rotate other way
                        ? sc2::Point2D(normalized_direction.y, -normalized_direction.x) 
                        // First direction
                        : sc2::Point2D(-normalized_direction.y, normalized_direction.x); 

                    // Reset height and continue searching
                    height_of_triangle = 5.0f;

                    // Stop searching if we cannot find a point before threshold -> return error point
                    if (direction_changes > 4) {
                        new_point = sc2::Point2D(0, 0); 
                        return new_point;
                    }
                }
            }
            // If we found a placement point
            if (new_point != sc2::Point2D(0, 0)) {
                return new_point;
            }
        }
    }
}

/**
 * @brief Check if enemy near base
 * 
 * @param base 
 * @return true if enemy in range
 * @return false if not
 */
bool BasicSc2Bot::EnemyNearBase(const sc2::Unit *base) {
    const sc2::ObservationInterface *obs = Observation();
    sc2::Units enemies = obs->GetUnits(sc2::Unit::Alliance::Enemy);
    const float base_rad = base->radius + 30.0F;

    for (const auto &enemy : enemies) {
        if (sc2::Distance2D(enemy->pos, base->pos) < base_rad) {
            return true;
        }
    }
    return false;
}

/**
 * @brief Find nearest worker
 * 
 * @param pos position to search from
 * @param is_busy allow selecting busy workers
 * @param mineral allow selecting workers holding minerals (use if implement mineral walk)
 * @return const sc2::Unit* 
 */
const sc2::Unit* BasicSc2Bot::FindNearestWorker(const sc2::Point2D& pos, bool is_busy, bool mineral) {
    const sc2::ObservationInterface *obs = Observation();
    
    sc2::Units workers = obs->GetUnits(sc2::Unit::Alliance::Self, sc2::IsUnit(sc2::UNIT_TYPEID::TERRAN_SCV));
    if (workers.empty()) {
        return nullptr;
    }

    float min_dist = std::numeric_limits<float>::max();

    const sc2::Unit *res = nullptr;
    for (auto &scv : workers) {
        // if worker busy
        if (!scv->orders.empty() && !is_busy) {
            continue;
        }

        bool carrying_mineral = false;
        for (const auto &buff : scv->buffs) {
            // if scv carrying minerals or gas
            if (buff == sc2::BUFF_ID::CARRYHARVESTABLEVESPENEGEYSERGAS ||
                buff == sc2::BUFF_ID::CARRYHIGHYIELDMINERALFIELDMINERALS ||
                buff == sc2::BUFF_ID::CARRYMINERALFIELDMINERALS) {
                
                carrying_mineral = true;
                break;
            } 
        }

        if (carrying_mineral && !mineral) {
            continue;
        }

        float dist = sc2::Distance2D(scv->pos, pos);

        if (dist < min_dist) {
            min_dist = dist;
            res = scv;
        }
    }

    return res;
}


/**
 * @brief Count units that have spawned in + in production
 * 
 * @param obs 
 * @param unit_type 
 * @param prod_unit 
 * @param ability 
 * @return size_t 
 */
size_t BasicSc2Bot::CountUnitTotal(const sc2::ObservationInterface *obs,
                      sc2::UNIT_TYPEID unit_type, sc2::UNIT_TYPEID prod_unit,
                      sc2::ABILITY_ID ability) {
    // count existing
    size_t existing = obs->GetUnits(sc2::Unit::Alliance::Self, sc2::IsUnit(unit_type)).size();
    // size_t in_production = 0;

    // sc2::Units production_units = obs->GetUnits(sc2::Unit::Alliance::Self, sc2::IsUnit(prod_unit));
    size_t in_production = obs->GetUnits(sc2::Unit::Alliance::Self, [&prod_unit, &ability](const sc2::Unit &unit) {
        bool is_unit = (unit.unit_type == prod_unit);
        bool is_producing = false;

        for (const auto &order : unit.orders) {
            if (order.ability_id == ability) {
                is_producing = true;
                break;
            }
        }

        return is_unit && is_producing;
    }).size();

    return existing + in_production;
}


/**
 * @brief Count units that have spawned in + in production
 * 
 * @param obs 
 * @param unit_type 
 * @param prod_unit 
 * @param ability 
 * @return size_t 
 */
size_t BasicSc2Bot::CountUnitTotal(const sc2::ObservationInterface *obs,
                      const std::vector<sc2::UNIT_TYPEID> &unit_type,
                      const std::vector<sc2::UNIT_TYPEID> &prod_unit,
                      sc2::ABILITY_ID ability) {
    // count existing
    size_t existing = obs->GetUnits(sc2::Unit::Alliance::Self, sc2::IsUnits(unit_type)).size();
    size_t in_production = 0;

    sc2::Units production_units = obs->GetUnits(sc2::Unit::Alliance::Self, sc2::IsUnits(prod_unit));

    for (const auto &unit : production_units) {
        if (unit->orders.empty()) {
            continue;
        }
        
        for (const auto &order : unit->orders) {
            if (order.ability_id == ability) {
                ++in_production;
            }
        }
    }

    return existing + in_production;
}


/**
 * @brief Add partition of units to squad
 * 
 * @param split_sz number of units to take from units (partition size)
 * @param units units that can be moved into squad
 * @param squad squad vector
 */
void BasicSc2Bot::SquadSplit(const size_t &split_sz, sc2::Units &units, sc2::Units &squad) {
    for (size_t i = 0; i < split_sz; ++i) {
        if (units.empty()) {
            return;
        }

        squad.push_back(units.back());
        units.pop_back();
    }
}

/**
 * @brief Find random scout location with unit
 * 
 * @param unit unit to scout
 * @param target 
 * @return true 
 * @return false 
 */
bool BasicSc2Bot::ScoutRandom(const sc2::Unit *unit, sc2::Point2D &target) {
    const sc2::ObservationInterface *obs = Observation();
    sc2::GameInfo gin = obs->GetGameInfo();

    float playable_w = gin.playable_max.x - gin.playable_min.x;
    float playable_h = gin.playable_max.y - gin.playable_min.y;

    // if game info data invalid
    if (playable_w == 0 || playable_h == 0) {
        // default dims
        playable_w = 236;
        playable_h = 228;
    }

    target.x = playable_w * sc2::GetRandomFraction() + gin.playable_min.x;
    target.y = playable_h * sc2::GetRandomFraction() + gin.playable_min.y;

    // check if valid
    float dist = Query()->PathingDistance(unit, target);
    return dist > 0.1F;
}