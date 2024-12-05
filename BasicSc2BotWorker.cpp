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

/*
 * @brief Directs commands for command centers
 */
void BasicSc2Bot::BuildWorkers() {
    const sc2::ObservationInterface *obs = Observation();
    sc2::Units bases = obs->GetUnits(sc2::Unit::Alliance::Self, sc2::IsTownHall());
    
    // build MULEs (unless being attacked (enemies in range))
    for (const auto &base : bases) {
        if (base->unit_type == sc2::UNIT_TYPEID::TERRAN_ORBITALCOMMAND || base->unit_type == sc2::UNIT_TYPEID::TERRAN_ORBITALCOMMANDFLYING) {

            // if we havent reached goal amnt or larger energy req met
            if ((CountUnitType(sc2::UNIT_TYPEID::TERRAN_MULE) < bases.size() * n_mules && base->energy > 50) || base->energy > 75) {
                // find mineral target
                const sc2::Unit *mineral_target = FindNearestMineralPatch(base->pos);
                // if we find a nearby mineral patch
                if (mineral_target) {
                    Actions()->UnitCommand(base, sc2::ABILITY_ID::EFFECT_CALLDOWNMULE, mineral_target);
                }
            }
        }
    }

    // Dont build too many workers
    if (obs->GetFoodWorkers() > N_TOTAL_WORKERS) {
        return;
    }

    for (const auto &base : bases) {
        // Build SCV
        if (base->assigned_harvesters < base->ideal_harvesters && base->build_progress == 1 && base->orders.empty()) {
            if (obs->GetMinerals() >= 50 && base->orders.empty() && obs->GetFoodUsed() < 120) {
                sc2::Agent::Actions()->UnitCommand(base, sc2::ABILITY_ID::TRAIN_SCV);
            }
        }
    }
}

/*
 * @brief Usually called from OnUnitIdle, Assigns work to SCV's
 *
 * @param unit
 */
void BasicSc2Bot::AssignIdleWorkers(const sc2::Unit *unit) {
    const sc2::ObservationInterface *obs = Observation();
    
    const sc2::Units refineries = obs->GetUnits(sc2::Unit::Alliance::Self, sc2::IsUnit(sc2::UNIT_TYPEID::TERRAN_REFINERY));
    const sc2::Units bases = obs->GetUnits(sc2::Unit::Alliance::Self, sc2::IsTownHall());
    const sc2::Unit* mineral_target;

    // No command centers
    if (bases.empty()) {
        return;
    }

    // Nothing to do for mules
    if (unit->unit_type == sc2::UNIT_TYPEID::TERRAN_MULE) {
        return;
    }

    for (const auto &refinery : refineries) {
        // If refinery under construction or all gas harvested
        if (refinery->build_progress != 1 || refinery->ideal_harvesters == 0) {
            continue;
        }

        if (refinery->assigned_harvesters < refinery->ideal_harvesters) {
            Actions()->UnitCommand(unit, sc2::ABILITY_ID::HARVEST_GATHER, refinery);
            return;
        }
    }

    for (const auto &base : bases) {
        // If base under construction or all minerals mined
        if (base->build_progress != 1 || base->ideal_harvesters == 0) {
            continue;
        }
        // Not enough harvesters at refinery
        if (base->assigned_harvesters < base->ideal_harvesters) {
            mineral_target = FindNearestMineralPatch(base->pos);
            Actions()->UnitCommand(unit, sc2::ABILITY_ID::HARVEST_GATHER, mineral_target);
            return;
        }
    }
    
    // Worker not idle
    if (!unit->orders.empty()) {
        return;
    }
    
    // Assign to last
    const sc2::Unit *base = bases.back();
    mineral_target = FindNearestMineralPatch(base->pos);
    Actions()->UnitCommand(unit, sc2::ABILITY_ID::HARVEST_GATHER, mineral_target);
}


/**
 * @brief Assign workers during onstep
 * 
 */
void BasicSc2Bot::AssignWorkers() {
    const sc2::ObservationInterface *obs = Observation();

    sc2::Units bases = obs->GetUnits(
        sc2::Unit::Alliance::Self,
        sc2::IsTownHall()
    );

    const sc2::Units refineries = obs->GetUnits(
        sc2::Unit::Alliance::Self, 
        sc2::IsUnit(sc2::UNIT_TYPEID::TERRAN_REFINERY)
    );

    const sc2::Unit* mineral_target;

    sc2::Units workers = obs->GetUnits(
        sc2::Unit::Alliance::Self,
        sc2::IsUnit(sc2::UNIT_TYPEID::TERRAN_SCV)
    );
    

    // No bases
    if (bases.empty()) {
        return;
    }

    for (const auto &base : bases) {
        // If base under construction or all minerals mined
        if (base->build_progress != 1 || base->ideal_harvesters == 0) {
            continue;
        }
        
        // Base has enough harvesters
        if (base->assigned_harvesters > base->ideal_harvesters) {
            for (const auto &scv : workers) {
                if (scv == scout) {
                    continue;
                }
                // SCV busy
                if (scv->orders.size() > 0) {
                    sc2::UnitOrder first_order = scv->orders.front();

                    // If unit already harvesting at current base
                    if (
                        first_order.ability_id == sc2::ABILITY_ID::HARVEST_GATHER && 
                        first_order.target_unit_tag == base->tag
                    ) {
                        // Reassign worker via idle worker assign
                        AssignIdleWorkers(scv);
                        return;
                    }
                }
            }
        }

    }

    for (const auto &refinery : refineries) {
        // If refinery under construction or all gas harvested -> skip
        if (refinery->build_progress != 1 || refinery->ideal_harvesters == 0) {
            continue;
        }

        // If refinery already has enough scvs
        if (refinery->assigned_harvesters > refinery->ideal_harvesters) {
            for (const auto &scv : workers) {
                if (scv == scout) {
                    continue;
                }
                if (!scv->orders.empty()) {
                    // SCV busy
                    sc2::UnitOrder first_order = scv->orders.front();

                    // If unit already harvesting at current base
                    if {
                        first_order.ability_id == sc2::ABILITY_ID::HARVEST_GATHER &&
                        first_order.target_unit_tag == refinery->tag
                    }
                    
                    // Reassign worker via idle worker assign
                    AssignIdleWorkers(scv);
                    return;
                    
                }
            }
        } 
        else {
            for (const auto &scv : workers) {
                if (!scv->orders.empty()) {
                    // SCV busy
                    sc2::UnitOrder first_order = scv->orders.front();

                    // Get target building
                    const sc2::Unit *target = obs->GetUnit(first_order.target_unit_tag);

                    // If no target (not mining, etc.) -> skip worker
                    if (target == nullptr) {
                        continue;
                    }

                    // If not at refinery
                    if (target->unit_type != sc2::UNIT_TYPEID::TERRAN_REFINERY) {
                        AssignIdleWorkers(scv);
                        return;
                    }
                }
            }
        }
    }

}