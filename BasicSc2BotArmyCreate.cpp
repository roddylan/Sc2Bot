// BasicSc2BotArmyCreate.cpp
// contains implementation for army creation

#include "BasicSc2Bot.h"
#include "sc2api/sc2_api.h"
#include "sc2api/sc2_unit.h"
#include "sc2api/sc2_interfaces.h"
#include <sc2api/sc2_common.h>
#include <sc2api/sc2_typeenums.h>
#include <sc2api/sc2_unit_filters.h>
#include <sc2lib/sc2_search.h>


/**
 * @brief Build army
 * 
 */
void BasicSc2Bot::BuildArmy() {
    const sc2::ObservationInterface *obs = Observation();

    // BASES
    sc2::Units bases = obs->GetUnits(sc2::Unit::Self, sc2::IsTownHall());

    // BARRACKS
    sc2::Units barracks = obs->GetUnits(sc2::Unit::Self, sc2::IsUnits({
        sc2::UNIT_TYPEID::TERRAN_BARRACKS, sc2::UNIT_TYPEID::TERRAN_BARRACKSFLYING
    }));
    // techlab and reactor barracks
    sc2::Units techlab_barracks = obs->GetUnits(sc2::Unit::Self, sc2::IsUnit(sc2::UNIT_TYPEID::TERRAN_BARRACKSTECHLAB));
    sc2::Units reactor_barracks = obs->GetUnits(sc2::Unit::Self, sc2::IsUnit(sc2::UNIT_TYPEID::TERRAN_BARRACKSREACTOR));
    
    // FACTORIES
    sc2::Units factories = obs->GetUnits(sc2::Unit::Self, sc2::IsUnits({
        sc2::UNIT_TYPEID::TERRAN_FACTORY, 
        sc2::UNIT_TYPEID::TERRAN_FACTORYFLYING
    }));
    // techlab and reactor barracks
    sc2::Units techlab_factories = obs->GetUnits(sc2::Unit::Self, sc2::IsUnit(sc2::UNIT_TYPEID::TERRAN_FACTORYTECHLAB));
    sc2::Units reactor_factories = obs->GetUnits(sc2::Unit::Self, sc2::IsUnit(sc2::UNIT_TYPEID::TERRAN_FACTORYREACTOR));

    // STARPORTS
    sc2::Units starports = obs->GetUnits(sc2::Unit::Self, sc2::IsUnits({
        sc2::UNIT_TYPEID::TERRAN_STARPORT, 
        sc2::UNIT_TYPEID::TERRAN_STARPORTFLYING
    }));
    // techlab and reactor starports
    sc2::Units techlab_starports = obs->GetUnits(sc2::Unit::Self, sc2::IsUnit(sc2::UNIT_TYPEID::TERRAN_STARPORTTECHLAB));
    sc2::Units reactor_starports = obs->GetUnits(sc2::Unit::Self, sc2::IsUnit(sc2::UNIT_TYPEID::TERRAN_STARPORTREACTOR));

    sc2::Units engg_bays = obs->GetUnits(sc2::Unit::Self, sc2::IsUnit(sc2::UNIT_TYPEID::TERRAN_ENGINEERINGBAY));
    
    sc2::Units bunkers = obs->GetUnits(sc2::Unit::Self, sc2::IsUnit(sc2::UNIT_TYPEID::TERRAN_BUNKER));
    

    // UNIT COUNTS
    // TODO: finish
    // TODO: use CountUnitTotal

    size_t marine_count = obs->GetUnits(sc2::Unit::Self, sc2::IsUnit(sc2::UNIT_TYPEID::TERRAN_MARINE)).size();
    size_t marauder_count = obs->GetUnits(sc2::Unit::Self, sc2::IsUnit(sc2::UNIT_TYPEID::TERRAN_MARAUDER)).size();
    
    size_t tank_count = obs->GetUnits(sc2::Unit::Self, sc2::IsUnits({
        sc2::UNIT_TYPEID::TERRAN_SIEGETANK, sc2::UNIT_TYPEID::TERRAN_SIEGETANKSIEGED
    })).size();
    
    size_t thor_count = obs->GetUnits(sc2::Unit::Self, sc2::IsUnits({
        sc2::UNIT_TYPEID::TERRAN_THOR, sc2::UNIT_TYPEID::TERRAN_THORAP
    })).size();
    
    size_t viking_count = obs->GetUnits(sc2::Unit::Self, sc2::IsUnits({
        sc2::UNIT_TYPEID::TERRAN_VIKINGASSAULT, sc2::UNIT_TYPEID::TERRAN_VIKINGFIGHTER
    })).size();

    size_t medivac_count = obs->GetUnits(sc2::Unit::Self, sc2::IsUnit(sc2::UNIT_TYPEID::TERRAN_MEDIVAC)).size();

    size_t liberator_count = obs->GetUnits(sc2::Unit::Self, sc2::IsUnits({
        sc2::UNIT_TYPEID::TERRAN_LIBERATOR, sc2::UNIT_TYPEID::TERRAN_LIBERATORAG
    })).size();

    size_t banshee_count = obs->GetUnits(sc2::Unit::Self, sc2::IsUnit(sc2::UNIT_TYPEID::TERRAN_BANSHEE)).size();

    size_t battlecruiser_count = obs->GetUnits(sc2::Unit::Self, sc2::IsUnit(sc2::UNIT_TYPEID::TERRAN_BATTLECRUISER)).size();


    // TODO: finish
    // if (bases.size() == 1)
    
}