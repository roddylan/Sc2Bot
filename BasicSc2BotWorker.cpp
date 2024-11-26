// BasicSc2BotWorker.cpp
// contains implementation for all (main) worker related functions

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

void BasicSc2Bot::BuildWorkers() {
    const sc2::ObservationInterface *obs = Observation();
    // const sc2::QueryInterface *query = Query();
    sc2::Units bases = obs->GetUnits(sc2::Unit::Alliance::Self, sc2::IsTownHall());
    
    // build MULEs (unless being attacked (enemies in range))
    for (const auto &base : bases) {
        if (base->unit_type == sc2::UNIT_TYPEID::TERRAN_ORBITALCOMMAND || base->unit_type == sc2::UNIT_TYPEID::TERRAN_ORBITALCOMMANDFLYING) {
            // if enemies in range
            // if () {
                
            // }
            
            // if we havent reached goal amnt or larger energy req met
            if ((CountUnitType(sc2::UNIT_TYPEID::TERRAN_MULE) < bases.size() * n_mules && base->energy > 50) || base->energy > 75) {
                // find mineral target
                const sc2::Unit *mineral_target = FindNearestMineralPatch(base->pos);
                // if we find a nearby mineral patch
                if (mineral_target) {
                    Actions()->UnitCommand(base, sc2::ABILITY_ID::EFFECT_CALLDOWNMULE, mineral_target);
                    std::cout << "n_mules =" << CountUnitType(sc2::UNIT_TYPEID::TERRAN_MULE) << std::endl;
                }
            }
            // else if (base->energy > 75) {
            //     // still create mules, but larger energy req
            //     if (FindNearestMineralPatch(base->pos)) {
            //         Actions()->UnitCommand(base, sc2::ABILITY_ID::EFFECT_CALLDOWNMULE);
            //     }
            // }
        }
    }

    // dont build too many workers
    if (obs->GetFoodWorkers() > N_TOTAL_WORKERS) {
        return;
    }
    for (const auto &base : bases) {
        // build SCV
        // TODO: maybe just use ideal_harvesters (max)
        if (base->assigned_harvesters < base->ideal_harvesters && base->build_progress == 1 && base->orders.empty()) {
        // if (obs->GetFoodWorkers() < n_workers) {
            if (obs->GetMinerals() >= 50 && base->orders.empty()) {
                sc2::Agent::Actions()->UnitCommand(base, sc2::ABILITY_ID::TRAIN_SCV);
            }
        }
    }
}

void BasicSc2Bot::AssignWorkers(const sc2::Unit *unit) {
    const sc2::ObservationInterface *obs = Observation();
    const sc2::Units refineries = obs->GetUnits(sc2::Unit::Alliance::Self, sc2::IsUnit(sc2::UNIT_TYPEID::TERRAN_REFINERY));
    const sc2::Units bases = obs->GetUnits(sc2::Unit::Alliance::Self, sc2::IsTownHall());
    const sc2::Unit* mineral_target;
    std::cout << "bases size: " << bases.size() << std::endl;

    if (unit->unit_type == sc2::UNIT_TYPEID::TERRAN_SCV) {
        for (const auto &refinery : refineries) {
            if (refinery->assigned_harvesters < refinery->ideal_harvesters) {
                 std::cout << "refinery assignment\n";
                 sc2::Point2D point = FindNearestRefinery(unit->pos);
                Actions()->UnitCommand(unit, sc2::ABILITY_ID::HARVEST_GATHER, refinery);
            }
            std::cout << refinery->assigned_harvesters << " : " << refinery->ideal_harvesters << std::endl;
        }

        for (const auto &base : bases) {
            // if building
            if (base->build_progress != 1) {
                continue;
            }
            // TODO: maybe just use ideal_harvesters (max)
            if (base->assigned_harvesters < base->ideal_harvesters) {
                mineral_target = FindNearestMineralPatch(unit->pos);
                Actions()->UnitCommand(unit, sc2::ABILITY_ID::SMART, mineral_target);
                return;
            }
        }
    }

    // if (unit->unit_type == sc2::UNIT_TYPEID::TERRAN_MULE) {
    //     mineral_target = FindNearestMineralPatch(unit->pos);
    //     Actions()->UnitCommand(unit, sc2::ABILITY_ID::SMART, mineral_target);
    // }
}

void BasicSc2Bot::AssignScvToRefineries() {
    static size_t frame_worker_last_moved_to_refinery = 0;
    const sc2::ObservationInterface* observation = Observation();
    const uint32_t &current_frame = observation->GetGameLoop();
    if (frame_worker_last_moved_to_refinery + 100'000 < current_frame) {
        return;
    }
    const sc2::Units& refineries = Observation()->GetUnits(sc2::Unit::Alliance::Self, sc2::IsUnit(sc2::UNIT_TYPEID::TERRAN_REFINERY));
    for (const sc2::Unit* refinery : refineries) {
        if (refinery->assigned_harvesters < 3) {
            const sc2::Unit *gathering_scv = this->GetGatheringScv();
            if (gathering_scv != nullptr) {
                Actions()->UnitCommand(gathering_scv, sc2::ABILITY_ID::HARVEST_GATHER, refinery);
                frame_worker_last_moved_to_refinery = current_frame;

            }
        }
    }
}