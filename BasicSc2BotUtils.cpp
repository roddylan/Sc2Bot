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

const sc2::Point2D BasicSc2Bot::FindNearestExpansionLocation(const sc2::Point2D& start) {


    float distance = std::numeric_limits<float>::max();
    sc2::Point3D target(0, 0, 0);

    for (sc2::Point3D &base : expansion_locations) {
        float d = sc2::DistanceSquared2D(sc2::Point2D(base.x, base.y), start);
        if (d < distance) {
            distance = d;
            target = base;
        }
       
    }
    if (target != sc2::Point3D(0, 0, 0)) return sc2::Point2D(target.x, target.y);

    return sc2::Point2D(0, 0);

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