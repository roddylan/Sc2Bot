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
    else if (bases.size() > 0) {
        return bases[0]->pos;
    }
    return sc2::Point2D(0, 0);
}

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


// Counts how many marines are nearby
int BasicSc2Bot::MarineClusterSize(const sc2::Unit* marine, const sc2::Units& marines, sc2::Units &cluster) {
    cluster.clear();
    int num_nearby_marines = 0;
    const float distance_threshold_sq = 25.0f;
    // first find closest marine
    for (const auto& other_marine : marines) {
        if (marine == other_marine) continue;

        float dx = marine->pos.x - other_marine->pos.x;
        float dy = marine->pos.y - other_marine->pos.y;
        float dist_to_marine_sq = dx * dx + dy * dy;

        if (dist_to_marine_sq < distance_threshold_sq) {
            // Add to cluster so we can send commands to all units in cluster at once
            cluster.push_back(other_marine);
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
        sc2::Units cluster{};
        int cluster_size = MarineClusterSize(marine, marines, cluster);

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
        if (unit.orders.empty()) {
            return true;
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
struct FlooredStartingPointLt {
    bool operator()(const std::pair<int32_t,int32_t>& lhs, const std::pair<int32_t,int32_t>& rhs) const {
        if (lhs.first == rhs.first) {
            return lhs.second < rhs.second;
        }
        return lhs.first < rhs.first;
    }
};

/*
* Finds a location to place a building at that is near a given starting position
*/
sc2::Point2D BasicSc2Bot::FindPlaceablePositionNear(const sc2::Point2D& starting_point, const sc2::ABILITY_ID& ability_to_place_building) {
    /*
    * We want to find a position that is near starting_point that works to fit the building
    * Idea: search in a square around the starting point for the first available position. If no where
    * is found, increase the size of the square and search again.
    * - so you still build relatively close to the starting point and increase the search space gradually
    * - though this does seem to be a bit slow sometimes
    */

    /*
    * Starting lower & upper bounds for x
    */
    int x_lo = -5, x_hi = 5;
    /*
    * Starting lower & upper bounds for y
    */
    int y_lo = -5, y_hi = 5;

    /*
    * How much to change the lower & upper bounds if you don't find a suitable spot in the current search square
    */
    int x_step = 5;
    int y_step = 5;
    bool found_pos_to_place_at = false;

    /*
    * When leaving the loop, pos_to_place should be set
    */
    sc2::Point2D pos_to_place_at;
    /*
    * Use a cache to not repeatedly query positions we know don't work
    */
    std::set<sc2::Point2D, Point2DLt> searched_points = {};

    size_t loop_count = 0;
    while (!found_pos_to_place_at) {
        for (int x = x_lo; x <= x_hi; x += 3) {
            for (int y = y_lo; y <= y_hi; y += 3) {
                const sc2::Point2D current_pos = starting_point + sc2::Point2D(x, y);
                sc2::QueryInterface* query = Query();
                if (searched_points.find(current_pos) != searched_points.end()) {
                    continue;
                }

                const bool can_place_here = query->Placement(ability_to_place_building, current_pos);
                if (!can_place_here) {
                    searched_points.insert(current_pos);
                    continue;
                }

                /*
                * We need wiggle room left & right so that the buildings aren't packed and they have room to build addons
                */
                const bool can_wiggle_left = query->Placement(ability_to_place_building, sc2::Point2D(current_pos.x - 2, current_pos.y));
                if (!can_wiggle_left) {
                    searched_points.insert(current_pos);
                    continue;
                }
                const bool can_wiggle_right = query->Placement(ability_to_place_building, sc2::Point2D(current_pos.x + 2, current_pos.y));
                if (!can_wiggle_right) {
                    searched_points.insert(current_pos);
                    continue;
                }
                // we found a valid position to place at, don't iterate again
                found_pos_to_place_at = true;
                // TODO: Find a way to speed this up
                // ensures we dont build at expansion location
                bool is_expansion_location = false;
                for (const auto& expansion_location : expansion_locations) {
                    float distance_squared = sc2::DistanceSquared2D(expansion_location, current_pos);
                    if (distance_squared <= 100.f) {
                        is_expansion_location = true;
                        break;
                    }
                }

                if (!is_expansion_location) {
                    pos_to_place_at = current_pos;
                }

            }
        }
        // increase the search space
        x_lo -= x_step;
        x_hi += x_step;
        y_lo -= y_step;
        x_hi += y_step;

        if (loop_count++ > 5) { // todo: change back to 10 (?)
            std::cout << "LOTS OF LOOPS OOPS " << loop_count << std::endl;
            return sc2::Point2D(0, 0);
            /*
            float rand_x = sc2::GetRandomScalar() * 5.0f;
            float rand_y = sc2::GetRandomScalar() * 5.0f;
            return this->FindPlaceablePositionNear(starting_point + sc2::Point2D(rand_x, rand_y), ability_to_place_building);
            */
        }

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
 * @brief Add units to squad
 * 
 * @param split_sz number of units to take from units
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
 * @brief Remove enemy base from set
 * 
 * @param base base unit
 */
void BasicSc2Bot::RemoveEnemyBase(const sc2::Unit *base) {
    // not in set
    if (enemy_bases.find(base) == enemy_bases.end()) {
        return;
    }
    
    this->enemy_bases.erase(base);
}


/**
 * @brief Remove enemy base from set
 * 
 * @param base_tag tag for base
 */
void BasicSc2Bot::RemoveEnemyBase(const sc2::Tag &base_tag) {
    const sc2::Unit *base = Observation()->GetUnit(base_tag);
    if (base == nullptr) {
        return;
    }

    RemoveEnemyBase(base);
}