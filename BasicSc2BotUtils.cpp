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
#include <sc2api/sc2_typeenums.h>
#include <sc2api/sc2_unit_filters.h>
#include <sc2lib/sc2_search.h>
#include <iostream>
#include <cmath>

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

size_t BasicSc2Bot::CountUnitType(sc2::UNIT_TYPEID unit_type) {
    return Observation()->GetUnits(sc2::Unit::Alliance::Self, IsUnit(unit_type)).size();
}

sc2::Filter scvFilter = [](const sc2::Unit& unit) {
    return unit.unit_type == sc2::UNIT_TYPEID::TERRAN_SCV;
};

const sc2::Unit* BasicSc2Bot::FindNearestMineralPatch(const sc2::Point2D& start) {
    sc2::Units units = Observation()->GetUnits(sc2::Unit::Alliance::Neutral);
    float distance = std::numeric_limits<float>::max();
    const sc2::Unit* target = nullptr;
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

const sc2::Point2D BasicSc2Bot::FindNearestCommandCenter(const sc2::Point2D& start, bool not_start_location) {

    sc2::Units bases = Observation()->GetUnits(sc2::Unit::Self, sc2::IsTownHall());
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



// checks that marine is nearby atleast size other marines
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
const sc2::Unit* BasicSc2Bot::FindInjuredMarine() {
    const sc2::Units marines = Observation()->GetUnits(sc2::Unit::Alliance::Self, IsUnit(sc2::UNIT_TYPEID::TERRAN_MARINE));
    
    for (const auto marine : marines) {
        
        if (marine->health < marine->health_max) {
            sc2::Units medivacs = SortMedivacsAccordingToDistance(marine->pos);
            // medivacs[0] because first element is closest
            if (sc2::Distance2D(medivacs[0]->pos, marine->pos) < 5.0f) {
                continue;
            }
            return marine;
        }
    }
    return nullptr;
}
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

/*
* Functor that implements < for a Point2D so that we can holds points in a std::set in FindPlaceablePositionNear
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
* Finds a location to place a building at that is near a given starting position
*/
sc2::Point2D BasicSc2Bot::FindPlaceablePositionNear(const sc2::Point2D& starting_point, const sc2::ABILITY_ID& ability_to_place_building) {
    /*
    * We want to find a position that is near starting_point that works to fit the building
    * Idea: given a starting position, spiral outwards until you find a place that is suitable
    * - we want buildings to have 2 spaces to the left & right to make sure that there's room for addom buildings
    */

    sc2::QueryInterface* query = Query();
    /*
    * When leaving the loop, pos_to_place should be set
    */
    sc2::Point2D pos_to_place_at;
    float x_min = 0, x_max = 0;
    float y_min = 0, y_max = 0;
    float x_offset = 0, y_offset = 0;
    enum Direction {UP=0,RIGHT=1,DOWN=2,LEFT=3};
    Direction current_direction = UP;  // 0 for up, 1 for right, 2 for down, 3 for left
    /*
    * If you are hitting the max/min offset, switch directions & increase the bounds for the
    * next time you spiral around.
    */
    const auto check_bounds_and_flip_direction = [&y_offset, &x_offset, &y_max, &y_min, &x_max, &x_min, &current_direction]() -> void {
        if (current_direction == UP && y_offset >= y_max) {
            current_direction = (Direction)((current_direction + 1) % 4);
            y_max++;
        }
        else if (current_direction == RIGHT && x_offset >= x_max) {
            current_direction = (Direction)((current_direction + 1) % 4);
            x_max++;
        }
        else if (current_direction == DOWN && y_offset <= y_min) {
            current_direction = (Direction)((current_direction + 1) % 4);
            y_min--;
        }
        else if (current_direction == LEFT && x_offset <= x_min) {
            current_direction = (Direction)((current_direction + 1) % 4);
            x_min--;
        }
        };
    /*
    * Adjust x_offset and y_offset to move in the current direction
    */
    const auto step_offset = [&x_offset, &y_offset](int direction) -> void {
        switch (direction) {
        case UP:
            y_offset++;
            break;
        case RIGHT:
            x_offset++;
            break;
        case DOWN:
            y_offset--;
        case LEFT:
            x_offset--;
            break;
        }
        };
    while (true) {
        const sc2::Point2D current_position = starting_point + sc2::Point2D(x_offset, y_offset);
        const bool can_wiggle_left = query->Placement(ability_to_place_building, sc2::Point2D(current_position.x - 2, current_position.y));
        if (!can_wiggle_left) {
            // switch direction
            check_bounds_and_flip_direction();
            step_offset(current_direction);
            continue;
        }
        const bool can_place_here = query->Placement(ability_to_place_building, current_position);
        if (!can_place_here) {
            check_bounds_and_flip_direction();
            step_offset(current_direction);
            continue;
        }
        const bool can_wiggle_right = query->Placement(ability_to_place_building, sc2::Point2D(current_position.x + 2, current_position.y));
        if (!can_wiggle_right) {
            // switch direction
            check_bounds_and_flip_direction();
            step_offset(current_direction);
            continue;
        }

        pos_to_place_at = current_position;
        break;
    }

    return pos_to_place_at;
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