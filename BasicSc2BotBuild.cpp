// BasicSc2BotBuild.cpp
// contains implementation for all build/upgrade related functions

#include "BasicSc2Bot.h"
#include "Betweenness.h"
#include "Utilities.h"
#include "sc2api/sc2_api.h"
#include "sc2api/sc2_interfaces.h"
#include "sc2api/sc2_unit.h"
#include <cmath>
#include <iostream>
#include <sc2api/sc2_common.h>
#include <sc2api/sc2_typeenums.h>
#include <sc2api/sc2_unit_filters.h>
#include <sc2lib/sc2_search.h>

/**
 * @brief Commands the passed-in factory to build tech-lab addon
 * @return true
 */
bool BasicSc2Bot::UpgradeFactoryTechLab(const sc2::Unit *factory) {

    Actions()->UnitCommand(factory, sc2::ABILITY_ID::BUILD_TECHLAB_FACTORY);

    return true;
}

/**
 * @brief Check if conditions to build a factory are met, and build factory if
 * they are
 * @return true
 */
bool BasicSc2Bot::TryBuildFactory() {
    const sc2::ObservationInterface *observation = Observation();

    // need at least one barrack before we can build a factory
    if (CountUnitType(sc2::UNIT_TYPEID::TERRAN_BARRACKS) < 1) {
        return false;
    }

    // check that we can afford it
    if (observation->GetVespene() < 100) {
        return false;
    }
    return TryBuildStructure(sc2::ABILITY_ID::BUILD_FACTORY);
}

/**
 * @brief Tries to build a siege tank if conditions are met
 * @return true
 */
bool BasicSc2Bot::TryBuildSiegeTank() {
    const sc2::ObservationInterface *observation = Observation();

    // must have techlab
    if (CountUnitType(sc2::UNIT_TYPEID::TERRAN_FACTORYTECHLAB) < 1) {
        return false;
    }

    // must have enough resources
    if (observation->GetVespene() < 125 || observation->GetMinerals() < 150) {
        return false;
    }

    sc2::Units techlabs =
        observation->GetUnits(sc2::Unit::Alliance::Self,
                              IsUnit(sc2::UNIT_TYPEID::TERRAN_FACTORYTECHLAB));

    sc2::Units factories = observation->GetUnits(
        sc2::Unit::Alliance::Self, IsUnit(sc2::UNIT_TYPEID::TERRAN_FACTORY));

    // build siege tanks if there aren't many around
    if (!techlabs.empty()) {
        for (auto factory : factories) {
            if (CountNearbySeigeTanks(factory) > N_TANKS &&
                factories.size() > 1) {
                continue;
            }
            TryBuildSiegeTank(factory);
        }
    }

    return true;
}

/**
 * @brief Build siege tank on given techlab factory
 *
 * @param factory: the factory to build the techlab
 * @return true
 */
bool BasicSc2Bot::TryBuildSiegeTank(const sc2::Unit *factory) {
    const sc2::ObservationInterface *observation = Observation();
    const sc2::Unit *add_on = observation->GetUnit(factory->add_on_tag);

    // must have techlab
    if (add_on == nullptr) {
        return false;
    }
    // must have techlab
    if (add_on->unit_type != sc2::UNIT_TYPEID::TERRAN_FACTORYTECHLAB) {
        return false;
    }

    // up to 2 in production
    if (factory->orders.size() > 1) {
        return false;
    }

    bool build = false;
    if (CountNearbySeigeTanks(factory) <= N_TANKS) {
        build = true;
        Actions()->UnitCommand(factory, sc2::ABILITY_ID::TRAIN_SIEGETANK);
    }

    return true;
}

/**
 * @brief Try to build a thor if conditions are met
 * @return true
 */
bool BasicSc2Bot::TryBuildThor() {
    const sc2::ObservationInterface *observation = Observation();

    if (CountUnitType(sc2::UNIT_TYPEID::TERRAN_FACTORYTECHLAB) < 1) {
        return false;
    }
    if (observation->GetVespene() < 200 || observation->GetMinerals() < 300) {
        return false;
    }
    sc2::Units units =
        observation->GetUnits(sc2::Unit::Alliance::Self,
                              IsUnit(sc2::UNIT_TYPEID::TERRAN_FACTORYTECHLAB));
    size_t marines_size = CountUnitType(sc2::UNIT_TYPEID::TERRAN_MARINE);
    // TODO: Handle logic for Thor creation as in creating a speicifed amount to
    // cover an area or something
    sc2::Units thors = observation->GetUnits(
        sc2::Unit::Alliance::Self, IsUnit(sc2::UNIT_TYPEID::TERRAN_THOR));
    if (thors.size() >= (marines_size / 5))
        return false;
    for (auto unit : units) {

        TryBuildThor(unit);
        // Actions()->UnitCommand(unit, sc2::ABILITY_ID::TRAIN_THOR);
    }
    return true;
}

/**
 * @brief Tries to build a Thor with a specific factory
 * @param factory: the factory to build the thor
 * @return true if build command was issued, false otherwise
 */
bool BasicSc2Bot::TryBuildThor(const sc2::Unit *factory) {
    const sc2::ObservationInterface *observation = Observation();
    const sc2::Unit *add_on = observation->GetUnit(factory->add_on_tag);

    // if cant build thor
    if (add_on == nullptr) {
        return false;
    }
    if (add_on->unit_type != sc2::UNIT_TYPEID::TERRAN_FACTORYTECHLAB) {
        return false;
    }

    // up to 2 in production
    if (factory->orders.size() > 1) {
        return false;
    }

    if (observation->GetVespene() < 200 || observation->GetMinerals() < 300) {
        return false;
    }

    // make
    size_t marines_size = CountUnitType(sc2::UNIT_TYPEID::TERRAN_MARINE);

    sc2::Units thors = observation->GetUnits(
        sc2::Unit::Alliance::Self, IsUnit(sc2::UNIT_TYPEID::TERRAN_THOR));
    if (thors.size() >= (marines_size / 5))
        return false;
    Actions()->UnitCommand(factory, sc2::ABILITY_ID::TRAIN_THOR);

    return true;
}

/**
 * @brief Tries to build a bunker
 * @return true if the build was successful, false otherwise
 */
bool BasicSc2Bot::TryBuildBunker() {

    const sc2::ObservationInterface *observation = Observation();

    // must have barracks so we have troops to put in bunker
    if (CountUnitType(sc2::UNIT_TYPEID::TERRAN_BARRACKS) < 1) {
        return false;
    }

    return TryBuildStructure(sc2::ABILITY_ID::BUILD_BUNKER);
}

/*
 * @brief Tries to build barracks if conditions are met
 * @return true if the build command was issued, false otherwise
 */
bool BasicSc2Bot::TryBuildBarracks() {
    const sc2::ObservationInterface *observation = Observation();
    int n_depot = CountUnitType(sc2::UNIT_TYPEID::TERRAN_SUPPLYDEPOT);
    int n_lower_depot =
        CountUnitType(sc2::UNIT_TYPEID::TERRAN_SUPPLYDEPOTLOWERED);

    // must have supply depots first
    if ((n_depot + n_lower_depot) < 1) {
        return false;
    }

    return TryBuildStructure(sc2::ABILITY_ID::BUILD_BARRACKS);
}

/**
 * @brief Tries building a supply depot if conditions are met
 * @return true if build command was issued, false otherwise
 */
bool BasicSc2Bot::TryBuildSupplyDepot() {
    const sc2::ObservationInterface *observation = Observation();
    // Lower + normal supply depots = total # of depots

    size_t n_supply_depots =
        observation
            ->GetUnits(sc2::Unit::Alliance::Self,
                       sc2::IsUnit(sc2::UNIT_TYPEID::TERRAN_SUPPLYDEPOT))
            .size();
    size_t n_lower_supply_depots =
        observation
            ->GetUnits(sc2::Unit::Alliance::Self,
                       sc2::IsUnit(sc2::UNIT_TYPEID::TERRAN_SUPPLYDEPOTLOWERED))
            .size();

    size_t n_bases =
        observation->GetUnits(sc2::Unit::Alliance::Self, sc2::IsTownHall())
            .size();

    // make a new supply depot if we are at 2/3 unit capacity
    uint32_t current_supply_use = observation->GetFoodUsed();
    uint32_t cur_max_supply = observation->GetFoodCap();

    if (current_supply_use == 200 ||
        3 * current_supply_use < 2 * cur_max_supply &&
            (n_lower_supply_depots + n_supply_depots) > 0) {
        // do not build if current_supply_use/max_suply < 2/3 or reached max
        // supply
        return false;
    }

    // make sure you can afford
    if (observation->GetMinerals() < 100) {
        return false;
    }

    return TryBuildStructure(sc2::ABILITY_ID::BUILD_SUPPLYDEPOT);
}

/**
 * @brief Tries to build a refinery if conditions are met
 * @return true
 */
bool BasicSc2Bot::TryBuildRefinery() {
    const sc2::ObservationInterface *observation = Observation();

    return BuildRefinery();
}

/**
 * @brief Tries to build a fusion core if conditions are met
 * @return true if build command was issued, false otherwise
 */
bool BasicSc2Bot::TryBuildFusionCore() {
    const sc2::ObservationInterface *observation = Observation();

    return TryBuildStructure(sc2::ABILITY_ID::BUILD_FUSIONCORE);
}

/**
 * @brief Builds a refinery
 * @return true
 */
bool BasicSc2Bot::BuildRefinery() {
    const sc2::ObservationInterface *observation = Observation();
    const sc2::Unit *unit_to_build = GetGatheringScv();

    // build refinery on vespene geyser
    const sc2::Unit *target;
    if (unit_to_build != nullptr) {
        const sc2::Unit *target = FindNearestVespeneGeyser(unit_to_build->pos);
        Actions()->UnitCommand(unit_to_build, sc2::ABILITY_ID::BUILD_REFINERY,
                               target);
    }

    return true;
}

/**
 * @brief Tries find a place to build a structure and issues command to build
 * the structure
 * @param ability_type_for_structure: The command to build the structure
 * @param unit_tyupe: THe type of unit to command to build the structure
 * @return true if the command was issued, false otherwise
 */
bool BasicSc2Bot::TryBuildStructure(sc2::ABILITY_ID ability_type_for_structure,
                                    sc2::UNIT_TYPEID unit_type) {
    const sc2::ObservationInterface *observation = Observation();
    sc2::Units bases =
        Observation()->GetUnits(sc2::Unit::Self, sc2::IsTownHall());

    // find unit to build the structure
    const sc2::Unit *unit_to_build = nullptr;
    sc2::Units units = observation->GetUnits(sc2::Unit::Alliance::Self);
    for (const auto &unit : units) {
        for (const auto &order : unit->orders) {
            if (order.ability_id == ability_type_for_structure) {
                // do not build this strucutre if we have a unit that's already
                // building this type
                return false;
            }
        }

        if (unit->unit_type == unit_type) {
            unit_to_build = unit;
        }
    }

    // nullptr check, no units available
    if (unit_to_build == nullptr) {
        return false;
    }

    // get random noise to move things around a bit
    float ry = sc2::GetRandomScalar() * 15.0f;
    float rx = sc2::GetRandomScalar() * 15.0f;

    // we want to build the structure near our base
    sc2::Point2D nearest_command_center =
        FindNearestCommandCenter(unit_to_build->pos, true);
    sc2::Point2D starting_point =
        sc2::Point2D(base_location.x + rx, base_location.y + ry);

    sc2::Point2D pos_to_place_at =
        FindPlaceablePositionNear(starting_point, ability_type_for_structure);
    if (pos_to_place_at == sc2::Point2D(0, 0))
        return false;

    // special cases for bunker & factory
    switch (unit_type) {
    case sc2::UNIT_TYPEID::TERRAN_BUNKER: {
        Actions()->ToggleAutocast(unit_to_build->tag,
                                  sc2::ABILITY_ID::BUNKERATTACK);
        break;
    }

    case sc2::UNIT_TYPEID::TERRAN_FACTORY: {
        Actions()->UnitCommand(unit_to_build, ability_type_for_structure,
                               pos_to_place_at);
        return true;
    }

    default: {
        break;
    }
    }

    Actions()->UnitCommand(unit_to_build, ability_type_for_structure,
                           pos_to_place_at);

    return true;
}

/**
 * @brief Tries to build a structure near an expanison
 * @param ability_type_for_structure: The command to build the structure
 * @param location: Ignored
 * @param expansion_starting_point: The position of a base expansion that we
 * want to build at
 * @return true
 */
bool BasicSc2Bot::TryBuildStructure(sc2::ABILITY_ID ability_type_for_structure,
                                    sc2::Point2D location,
                                    sc2::Point2D expansion_starting_point) {
    const sc2::ObservationInterface *obs = Observation();
    sc2::Units workers = obs->GetUnits(sc2::Unit::Alliance::Self,
                                       IsUnit(sc2::UNIT_TYPEID::TERRAN_SCV));

    // no workers
    if (workers.empty()) {
        return false;
    }

    // find a unit to build the structure
    const sc2::Unit *unit_to_build = nullptr;
    for (const auto &worker : workers) {
        for (const auto &order : worker->orders) {
            if (order.ability_id == ability_type_for_structure) {
                // don't build structure if someone else is already building it
                return false;
            }
        }

        unit_to_build = worker;
        break;
    }

    // nullptr check, no units available
    if (unit_to_build == nullptr) {
        return false;
    }

    // build at expansion starting point
    Actions()->UnitCommand(unit_to_build, ability_type_for_structure,
                           expansion_starting_point);

    return true;
}

/**
 * @brief Tries building a missile turret near the base if conditions are met
 * @return true if build command was issued, false otherwise
 */
bool BasicSc2Bot::TryBuildMissileTurret() {
    const sc2::ObservationInterface *observation = Observation();

    sc2::Units engineering_bays = observation->GetUnits(
        sc2::Unit::Alliance::Self,
        sc2::IsUnit(sc2::UNIT_TYPEID::TERRAN_ENGINEERINGBAY));
    // must have engineering bays
    if (engineering_bays.empty()) {
        return false;
    }
    // need to be able to afford it
    if (observation->GetMinerals() < 75) {
        return false;
    }
    size_t max_turrets_per_base = n_missile;
    size_t base_count =
        observation->GetUnits(sc2::Unit::Alliance::Self, sc2::IsTownHall())
            .size();
    size_t turret_count =
        observation
            ->GetUnits(sc2::Unit::Alliance::Self,
                       sc2::IsUnit(sc2::UNIT_TYPEID::TERRAN_MISSILETURRET))
            .size();
    // make sure we dont have too many turrets overall
    if (max_turrets_per_base * base_count < turret_count) {
        return false;
    }
    return TryBuildStructure(sc2::ABILITY_ID::BUILD_MISSILETURRET);
}

/**
 * @brief Tries building an armory if conditions are met
 * @return true if build command was issued, false otherwise
 */
bool BasicSc2Bot::TryBuildArmory() {
    return TryBuildStructure(sc2::ABILITY_ID::BUILD_ARMORY);
}

/**
 * @brief Decides what building to try and start building this frame
 */
void BasicSc2Bot::HandleBuild() {
    // mineral costs
    static const uint32_t SUPPLY_DEPOT_COST = 100;
    static const uint32_t REFINERY_COST = 75;
    static const uint32_t FACTORY_MINERAL_COST = 150;
    static const uint32_t BUNKER_COST = 100;
    static const uint32_t BARRACKS_COST = 150;
    static const uint32_t ORBITAL_COMMAND_COST = 150;
    static const uint32_t STARPORT_COST = 150;
    static const uint32_t ARMORY_MINERAL_COST = 150;

    // vespene gas costs
    static const uint32_t ARMORY_GAS_COST = 100;
    static const uint32_t FACTORY_GAS_COST = 100;
    static const uint32_t STARPORT_GAS_COST = 100;

    const sc2::ObservationInterface *obs = Observation();

    // track each type of unit
    sc2::Units bases = obs->GetUnits(sc2::Unit::Self, sc2::IsTownHall());
    sc2::Units barracks =
        obs->GetUnits(sc2::Unit::Self,
                      sc2::IsUnits({sc2::UNIT_TYPEID::TERRAN_BARRACKS,
                                    sc2::UNIT_TYPEID::TERRAN_BARRACKSFLYING}));
    sc2::Units factory =
        obs->GetUnits(sc2::Unit::Self,
                      sc2::IsUnits({sc2::UNIT_TYPEID::TERRAN_FACTORY,
                                    sc2::UNIT_TYPEID::TERRAN_FACTORYFLYING,
                                    sc2::UNIT_TYPEID::TERRAN_FACTORYREACTOR,
                                    sc2::UNIT_TYPEID::TERRAN_FACTORYTECHLAB}));
    sc2::Units starports =
        obs->GetUnits(sc2::Unit::Self,
                      sc2::IsUnits({sc2::UNIT_TYPEID::TERRAN_STARPORT,
                                    sc2::UNIT_TYPEID::TERRAN_STARPORTFLYING,
                                    sc2::UNIT_TYPEID::TERRAN_STARPORTREACTOR,
                                    sc2::UNIT_TYPEID::TERRAN_STARPORTTECHLAB}));
    sc2::Units engg_bays = obs->GetUnits(
        sc2::Unit::Self, sc2::IsUnit(sc2::UNIT_TYPEID::TERRAN_ENGINEERINGBAY));
    sc2::Units bunkers = obs->GetUnits(
        sc2::Unit::Self, sc2::IsUnit(sc2::UNIT_TYPEID::TERRAN_BUNKER));
    sc2::Units techlab_factory = obs->GetUnits(
        sc2::Unit::Self, sc2::IsUnit(sc2::UNIT_TYPEID::TERRAN_FACTORYTECHLAB));
    sc2::Units techlab_starports = obs->GetUnits(
        sc2::Unit::Self, sc2::IsUnit(sc2::UNIT_TYPEID::TERRAN_STARPORTTECHLAB));
    sc2::Units armorys = obs->GetUnits(
        sc2::Unit::Self, sc2::IsUnit(sc2::UNIT_TYPEID::TERRAN_ARMORY));
    sc2::Units fusion_cores = obs->GetUnits(
        sc2::Unit::Self, sc2::IsUnit(sc2::UNIT_TYPEID::TERRAN_FUSIONCORE));

    // goal amnts of buildings PER BASE
    const size_t n_barracks_target = 2;
    const size_t n_factory_target = 1;
    const size_t n_armory_target = 1;
    const size_t n_engg_target = 2;
    const size_t n_bunkers_target = 8;
    const size_t n_starports_target = 1;

    sc2::Units marines = obs->GetUnits(
        sc2::Unit::Self, sc2::IsUnit(sc2::UNIT_TYPEID::TERRAN_MARINE));
    sc2::Units tanks = obs->GetUnits(
        sc2::Unit::Self, sc2::IsUnit(sc2::UNIT_TYPEID::TERRAN_SIEGETANK));
    sc2::Units refineries = obs->GetUnits(
        sc2::Unit::Self, sc2::IsUnit(sc2::UNIT_TYPEID::TERRAN_REFINERY));
    // no more than 2 refineries per base
    if (refineries.size() < bases.size() * 2) {
        TryBuildRefinery();
    }

    // build barracks if under capacity
    if (barracks.size() < n_barracks_target * bases.size()) {
        TryBuildBarracks();
    }

    // build factory
    if (!barracks.empty() &&
        factory.size() < (n_factory_target * bases.size())) {
        if (obs->GetMinerals() > FACTORY_MINERAL_COST &&
            obs->GetVespene() > FACTORY_GAS_COST &&
            obs->GetMinerals() - FACTORY_MINERAL_COST >= 400) {
            // std::cout << "building factory\n\n";
            TryBuildFactory();
        }
    }
    // 400 = command center cost
    if (obs->GetMinerals() - 150 >= 400 &&
        tanks.size() * bases.size() < bases.size()) {
        TryBuildSiegeTank();
    }

    // Handle Orbital Command

    if (!barracks.empty()) {
        for (const auto &base : bases) {
            if (base->build_progress != 1) {
                continue;
            }
            sc2::Units orbital_commands = obs->GetUnits(
                sc2::Unit::Self,
                sc2::IsUnit(sc2::UNIT_TYPEID::TERRAN_ORBITALCOMMAND));

            if (obs->GetMinerals() - 150 >= 400) {
                if (orbital_commands.size() >= (bases.size() / 3)) {
                    Actions()->UnitCommand(
                        base, sc2::ABILITY_ID::MORPH_PLANETARYFORTRESS);
                } else {
                    Actions()->UnitCommand(
                        base, sc2::ABILITY_ID::MORPH_ORBITALCOMMAND);
                }
            }
        }
    }

    if (bases.size() < 2) {
        sc2::Units command_centers = Observation()->GetUnits(
            sc2::Unit::Alliance::Self,
            sc2::IsUnit(sc2::UNIT_TYPEID::TERRAN_COMMANDCENTER));
        bool cc_in_progress = false;
        for (auto &command_center : command_centers) {
            if (command_center->build_progress < 1.0f) {
                // The second Command Center is already being built
                cc_in_progress = true;
            }
            if (!cc_in_progress) {
                HandleExpansion(true);
            }
        }
    }
    // build a starport
    if (factory.size() > 0 &&
        starports.size() < n_starports_target * bases.size()) {
        if (obs->GetMinerals() >= STARPORT_COST &&
            obs->GetVespene() >= STARPORT_GAS_COST) {
            TryBuildStructure(sc2::ABILITY_ID::BUILD_STARPORT);
        }
    }
    // Dont do anything until we have enough marines to defend and enough bases
    // to start so we dont run out of resources
    if (marines.size() < 20 || tanks.size() < 3 || starports.size() < 2) {
        return;
    }
    /*

        **PHASE ONE DONE**

    */
    if (armorys.size() < n_armory_target &&
        obs->GetMinerals() >= ARMORY_MINERAL_COST &&
        obs->GetVespene() >= ARMORY_GAS_COST) {
        TryBuildArmory();
    }

    // try building fusion core
    if (starports.size() > 0) {
        for (const auto &starport : starports) {
            if (starport->add_on_tag != 0) {
                // Get the add-on unit using its tag
                const sc2::Unit *add_on = obs->GetUnit(starport->add_on_tag);
                if (add_on->unit_type.ToType() ==
                        sc2::UNIT_TYPEID::TERRAN_STARPORTTECHLAB &&
                    fusion_cores.size() < 1) {
                    TryBuildFusionCore();
                    return;
                }
            }
        }
    }

    if (obs->GetMinerals() >= 400 && bases.size() <= 1) {
        HandleExpansion(false);
    }

    // build engg bay for missile turret
    // Only need 2 engineering bays
    if (engg_bays.size() < n_engg_target) {
        if (obs->GetMinerals() > 150 && obs->GetVespene() > 100) {
            TryBuildStructure(sc2::ABILITY_ID::BUILD_ENGINEERINGBAY);
        }
    }

    TryBuildMissileTurret();
    if (obs->GetVespene() - 150 >= 200) {
        TryBuildThor();
    }
}
