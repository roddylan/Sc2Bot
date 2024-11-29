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
                   // std::cout << "n_mules =" << CountUnitType(sc2::UNIT_TYPEID::TERRAN_MULE) << std::endl;
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
            if (obs->GetMinerals() >= 50 && base->orders.empty() && obs->GetFoodUsed() < 120) {
                sc2::Agent::Actions()->UnitCommand(base, sc2::ABILITY_ID::TRAIN_SCV);
            }
        }
    }
}

void BasicSc2Bot::AssignIdleWorkers(const sc2::Unit *unit) {
    const sc2::ObservationInterface *obs = Observation();
    
    const sc2::Units refineries = obs->GetUnits(sc2::Unit::Alliance::Self, sc2::IsUnit(sc2::UNIT_TYPEID::TERRAN_REFINERY));
    const sc2::Units bases = obs->GetUnits(sc2::Unit::Alliance::Self, sc2::IsTownHall());
    const sc2::Unit* mineral_target;
   // std::cout << "bases size: " << bases.size() << std::endl;

    if (bases.empty()) {
        return;
    }

    // nothing to do for mules
    if (unit->unit_type == sc2::UNIT_TYPEID::TERRAN_MULE) {
        return;
    }

    for (const auto &refinery : refineries) {
        // if refinery under construction or all gas harvested
        if (refinery->build_progress != 1 || refinery->ideal_harvesters == 0) {
            continue;
        }

        if (refinery->assigned_harvesters < refinery->ideal_harvesters) {
            // std::cout << "refinery assignment\n";
            // sc2::Point2D point = FindNearestRefinery(unit->pos);
            
            Actions()->UnitCommand(unit, sc2::ABILITY_ID::HARVEST_GATHER, refinery);
            // std::cout << refinery->assigned_harvesters << " : " << refinery->ideal_harvesters << std::endl;
            return;
        }
        // std::cout << refinery->assigned_harvesters << " : " << refinery->ideal_harvesters << std::endl;

    }

    for (const auto &base : bases) {
        // if base under construction or all minerals mined
        if (base->build_progress != 1 || base->ideal_harvesters == 0) {
            continue;
        }
        
        if (base->assigned_harvesters < base->ideal_harvesters) {
            mineral_target = FindNearestMineralPatch(base->pos);
            // Actions()->UnitCommand(unit, sc2::ABILITY_ID::SMART, mineral_target);
            Actions()->UnitCommand(unit, sc2::ABILITY_ID::HARVEST_GATHER, mineral_target);
            return;
        }
    }
    
    // worker not idle
    if (!unit->orders.empty()) {
        return;
    }
    
    // assign to last
    // const sc2::Unit *base = sc2::GetRandomEntry(bases);
    const sc2::Unit *base = bases.back();
    mineral_target = FindNearestMineralPatch(base->pos);
    Actions()->UnitCommand(unit, sc2::ABILITY_ID::HARVEST_GATHER, mineral_target);

}

// void BasicSc2Bot::AssignScvToRefineries() {
//     static size_t frame_worker_last_moved_to_refinery = 0;
//     const sc2::ObservationInterface* observation = Observation();
//     const uint32_t &current_frame = observation->GetGameLoop();
//     if (frame_worker_last_moved_to_refinery + 100'000 < current_frame) {
//         return;
//     }
//     const sc2::Units& refineries = Observation()->GetUnits(sc2::Unit::Alliance::Self, sc2::IsUnit(sc2::UNIT_TYPEID::TERRAN_REFINERY));
//     for (const sc2::Unit* refinery : refineries) {
//         if (refinery->assigned_harvesters < 3) {
//             const sc2::Unit *gathering_scv = this->GetGatheringScv();
//             if (gathering_scv != nullptr) {
//                 Actions()->UnitCommand(gathering_scv, sc2::ABILITY_ID::HARVEST_GATHER, refinery);
//                 frame_worker_last_moved_to_refinery = current_frame;

//             }
//         }
//     }
// }

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
    
    // std::cout << "bases size: " << bases.size() << std::endl;

    // no bases
    if (bases.empty()) {
        return;
    }

    for (const auto &base : bases) {
        // if base under construction or all minerals mined
        if (base->build_progress != 1 || base->ideal_harvesters == 0) {
            continue;
        }
        
        // base has enough harvesters
        if (base->assigned_harvesters > base->ideal_harvesters) {
            for (const auto &scv : workers) {
                if (scv == scout) {
                    continue;
                }
                // scv busy
                if (scv->orders.size() > 0) {
                    sc2::UnitOrder first_order = scv->orders.front();

                    // if unit already harvesting at current base
                    if (
                        first_order.ability_id == sc2::ABILITY_ID::HARVEST_GATHER && 
                        first_order.target_unit_tag == base->tag
                    ) {
                        // reassign worker via idle worker assign
                        AssignIdleWorkers(scv);
                        return;
                    }
                }
            }
        }

    }

    for (const auto &refinery : refineries) {
        // if refinery under construction or all gas harvested -> skip
        if (refinery->build_progress != 1 || refinery->ideal_harvesters == 0) {
            continue;
        }

        // if refinery already has enough scvs
        if (refinery->assigned_harvesters > refinery->ideal_harvesters) {
            for (const auto &scv : workers) {
                if (scv == scout) {
                    continue;
                }
                if (!scv->orders.empty()) {
                    // scv busy
                    sc2::UnitOrder first_order = scv->orders.front();

                    // if unit already harvesting at current base
                    if (
                        first_order.ability_id == sc2::ABILITY_ID::HARVEST_GATHER && 
                        first_order.target_unit_tag == refinery->tag
                    ) {
                        // reassign worker via idle worker assign
                        AssignIdleWorkers(scv);
                        return;
                    }
                }
            }
        } else {
            for (const auto &scv : workers) {
                if (!scv->orders.empty()) {
                    // scv busy
                    sc2::UnitOrder first_order = scv->orders.front();

                    // get target building
                    const sc2::Unit *target = obs->GetUnit(first_order.target_unit_tag);

                    // if no target (not mining, etc.) -> skip worker
                    if (target == nullptr) {
                        continue;
                    }

                    // if not at refinery
                    if (target->unit_type != sc2::UNIT_TYPEID::TERRAN_REFINERY) {
                        AssignIdleWorkers(scv);
                        return;
                    }
                }
            }
        }
    }



}