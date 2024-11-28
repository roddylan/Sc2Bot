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
    
    const size_t COMMAND_CENTER_COST = 400;
    
    // THRESHOLD
    // if have some units, maintain enough money to expand
    if (bases.size() > 0 && obs->GetFoodArmy() > MIN_ARMY_FOOD && obs->GetMinerals() <= COMMAND_CENTER_COST) {
        return;
    }

    // UNIT COUNTS
    // TODO: finish

    size_t marine_count = CountUnitTotal(
        obs, {sc2::UNIT_TYPEID::TERRAN_MARINE}, 
        {sc2::UNIT_TYPEID::TERRAN_BARRACKS, sc2::UNIT_TYPEID::TERRAN_BARRACKSFLYING, 
        sc2::UNIT_TYPEID::TERRAN_BARRACKSTECHLAB, sc2::UNIT_TYPEID::TERRAN_BARRACKSREACTOR},
        sc2::ABILITY_ID::TRAIN_MARINE
    );

    size_t marauder_count = CountUnitTotal(
        obs, sc2::UNIT_TYPEID::TERRAN_MARAUDER, 
        sc2::UNIT_TYPEID::TERRAN_BARRACKSTECHLAB,
        sc2::ABILITY_ID::TRAIN_MARAUDER
    );
    
    size_t tank_count = CountUnitTotal(
        obs, {sc2::UNIT_TYPEID::TERRAN_SIEGETANK, sc2::UNIT_TYPEID::TERRAN_SIEGETANKSIEGED},
        {sc2::UNIT_TYPEID::TERRAN_FACTORYTECHLAB}, sc2::ABILITY_ID::TRAIN_SIEGETANK
    );
    
    size_t thor_count = CountUnitTotal(
        obs, {sc2::UNIT_TYPEID::TERRAN_THOR, sc2::UNIT_TYPEID::TERRAN_THORAP},
        {sc2::UNIT_TYPEID::TERRAN_FACTORYTECHLAB}, sc2::ABILITY_ID::TRAIN_THOR
    );
    
    
    size_t viking_count = CountUnitTotal(
        obs, {sc2::UNIT_TYPEID::TERRAN_VIKINGASSAULT, sc2::UNIT_TYPEID::TERRAN_VIKINGFIGHTER},
        {sc2::UNIT_TYPEID::TERRAN_STARPORT, sc2::UNIT_TYPEID::TERRAN_STARPORTFLYING, 
         sc2::UNIT_TYPEID::TERRAN_STARPORTREACTOR, sc2::UNIT_TYPEID::TERRAN_STARPORTTECHLAB},
        sc2::ABILITY_ID::TRAIN_VIKINGFIGHTER
    );

    size_t medivac_count = CountUnitTotal(
        obs, {sc2::UNIT_TYPEID::TERRAN_MEDIVAC},
        {sc2::UNIT_TYPEID::TERRAN_STARPORT, sc2::UNIT_TYPEID::TERRAN_STARPORTFLYING, 
         sc2::UNIT_TYPEID::TERRAN_STARPORTREACTOR, sc2::UNIT_TYPEID::TERRAN_STARPORTTECHLAB},
        sc2::ABILITY_ID::TRAIN_MEDIVAC
    );

    size_t liberator_count = CountUnitTotal(
        obs, {sc2::UNIT_TYPEID::TERRAN_LIBERATOR, sc2::UNIT_TYPEID::TERRAN_LIBERATORAG},
        {sc2::UNIT_TYPEID::TERRAN_STARPORT, sc2::UNIT_TYPEID::TERRAN_STARPORTFLYING, 
         sc2::UNIT_TYPEID::TERRAN_STARPORTREACTOR, sc2::UNIT_TYPEID::TERRAN_STARPORTTECHLAB},
        sc2::ABILITY_ID::TRAIN_LIBERATOR
    );

    size_t banshee_count = CountUnitTotal(
        obs, sc2::UNIT_TYPEID::TERRAN_BANSHEE,
        sc2::UNIT_TYPEID::TERRAN_STARPORTTECHLAB,
        sc2::ABILITY_ID::TRAIN_BANSHEE
    );

    size_t battlecruiser_count = CountUnitTotal(
        obs, sc2::UNIT_TYPEID::TERRAN_BATTLECRUISER,
        sc2::UNIT_TYPEID::TERRAN_STARPORTTECHLAB,
        sc2::ABILITY_ID::TRAIN_BATTLECRUISER
    );


    // TODO: finish
    // if (bases.size() == 1)

    // handle starport units
    if (starports.size() > 0) {
        for (const auto &starport : starports) {
            
        }
    }
    
}