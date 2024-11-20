// BasicSc2BotFortify.cpp
// implementation for pure defensive functions (walling chokepoints, etc.)

#include "BasicSc2Bot.h"
#include "sc2api/sc2_api.h"
#include "sc2api/sc2_unit.h"
#include "sc2api/sc2_interfaces.h"
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
    
    sc2::Units enemies = obs->GetUnits(sc2::Unit::Alliance::Enemy, [](const sc2::Unit &unit) {
        // enemy visible
        bool is_vis = (unit.display_type == sc2::Unit::DisplayType::Visible);

        // enemy in air
        // bool is_air = ();
        // TODO: fix check
        return is_vis;
    });

    auto bufs = turret->buffs;
}