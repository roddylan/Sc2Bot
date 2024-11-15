// BasicSc2BotUtils.cpp
// contains implementation for misc. utility functions (finders, is, in, count, etc.)

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
                pos_to_place_at = current_pos;
            }
        }
        // increase the search space
        x_lo -= x_step;
        x_hi += x_step;
        y_lo -= y_step;
        x_hi += y_step;

        if (loop_count++ > 10) {
            std::cout << "LOTS OF LOOPS OOPS " << loop_count << std::endl;
            float rand_x = sc2::GetRandomScalar() * 5.0f;
            float rand_y = sc2::GetRandomScalar() * 5.0f;
            return this->FindPlaceablePositionNear(starting_point + sc2::Point2D(rand_x, rand_y), ability_to_place_building);
        }
    }
    return pos_to_place_at;
}