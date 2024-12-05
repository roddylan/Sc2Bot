#include "BasicSc2Bot.h"
#include "sc2api/sc2_api.h"
#include "sc2api/sc2_interfaces.h"
#include "sc2api/sc2_unit.h"
#include <sc2api/sc2_common.h>
#include <sc2api/sc2_typeenums.h>
#include <sc2api/sc2_unit_filters.h>
#include <sc2lib/sc2_search.h>

/**
 * @brief Assign actions to barrack (build unit, upgrade)
 *
 * @param barrack
 */
void BasicSc2Bot::AssignBarrackAction(const sc2::Unit *barrack) {
    // Dont do anything if busy
    if (barrack->build_progress < 1 || barrack->orders.size() > 0) {
        return;
    }
    /*
     * What should a barrack do?
     * - if it doesnt have an addon, build an addon
     * - if it does have an addon, train marines if it has a reactor; train
     * marauders if it has a tech lab
     */
    const sc2::ObservationInterface *observation = Observation();
    const sc2::Units marines =
        observation->GetUnits(sc2::Unit::Alliance::Self,
                              sc2::IsUnit(sc2::UNIT_TYPEID::TERRAN_MARINE));
    const sc2::Units bases =
        observation->GetUnits(sc2::Unit::Alliance::Self, sc2::IsTownHall());
    const uint32_t &mineral_count = observation->GetMinerals();
    const uint32_t &gas_count = observation->GetVespene();

    const size_t marauder_count = CountUnitTotal(
        observation, sc2::UNIT_TYPEID::TERRAN_MARAUDER,
        sc2::UNIT_TYPEID::TERRAN_BARRACKS, sc2::ABILITY_ID::TRAIN_MARAUDER);

    const size_t marine_count = CountUnitTotal(
        observation, sc2::UNIT_TYPEID::TERRAN_MARINE,
        sc2::UNIT_TYPEID::TERRAN_BARRACKS, sc2::ABILITY_ID::TRAIN_MARAUDER);

    /*
     * The addon for the barrack, either a tech lab or a reactor or none
     * - if there is no addon, we should make one
     */
    const sc2::Unit *barrack_addon = observation->GetUnit(barrack->add_on_tag);
    if (barrack_addon == nullptr) {
        // get ALL the barrack tech labs (not just for this one)
        // - only have 1 tech lab
        const sc2::Units &barrack_tech_labs = observation->GetUnits(
            sc2::Unit::Alliance::Self,
            sc2::IsUnit(sc2::UNIT_TYPEID::TERRAN_BARRACKSTECHLAB));
        /*
         * If we don't have a tech lab yet, make one
         * - tech labs can research buffs for marines & marauders
         *   - marines can get +10 hp and stimpacks
         *     - stimpacks let you sacrifice 10hp for bonus combat stats, pairs
         * well with medivacs Costs 50 mineral, 25 gas
         */
        if (barrack_tech_labs.size() < 1 && mineral_count >= 50 &&
            gas_count >= 25) {
            Actions()->UnitCommand(barrack,
                                   sc2::ABILITY_ID::BUILD_TECHLAB_BARRACKS);
            return;
        }

        // Have a tech lab already, so build a reactor (costs 50 mineral, 50
        // gas)
        if (mineral_count >= 50 && gas_count >= 50) {
            Actions()->UnitCommand(barrack,
                                   sc2::ABILITY_ID::BUILD_REACTOR_BARRACKS);
            return;
        }
        return;
    }

    // Now we know that this barrack has an addon, but is it a reactor or a tech
    // lab?
    const bool &has_tech_lab =
        barrack_addon->unit_type == sc2::UNIT_TYPEID::TERRAN_BARRACKSTECHLAB;

    // If it has a tech lab, train marauders constantly
    // Marauders only cost 25 gas, use >= 100 so that there's a bit of a buffer
    // for other things
    const size_t goal_marauder = marine_count / 3 + 1;
    // If (has_tech_lab && mineral_count >= 100 && gas_count >= 100 &&
    // Observation()->GetFoodUsed() < 100) {
    if (has_tech_lab && mineral_count >= 100 && gas_count >= 100 &&
        marauder_count < goal_marauder) {
        // If (has_tech_lab && mineral_count >= 100 && gas_count >= 100 &&
        // marauder_count < N_MARAUDERS * bases.size()) {
        Actions()->UnitCommand(barrack, sc2::ABILITY_ID::TRAIN_MARAUDER);
        return;
    }

    size_t marine_goal = std::min<size_t>(N_MARINE * bases.size(), MAX_MARINE);
    // If you have a reactor, you can build things twice as fast so you should
    // spam train marines
    if (mineral_count >= 50 && marines.size() < marine_goal) {
        Actions()->UnitCommand(barrack, sc2::ABILITY_ID::TRAIN_MARINE);
    }

    // Train a second one, if you can afford it (reactors build at double speed)
    if (mineral_count >= 100 && marines.size() < marine_goal) {
        Actions()->UnitCommand(barrack, sc2::ABILITY_ID::TRAIN_MARINE);
    }
}

/**
 * @brief Assign action to engg bay
 *
 * @param engineering_bay
 */
void BasicSc2Bot::AssignEngineeringBayAction(const sc2::Unit &engineering_bay) {
    const std::vector<sc2::UpgradeID> &upgrades = Observation()->GetUpgrades();
    const uint32_t minerals = Observation()->GetMinerals();
    const uint32_t gas = Observation()->GetVespene();

    const bool has_infantry_weapons_1 =
        std::find(upgrades.begin(), upgrades.end(),
                  sc2::UPGRADE_ID::TERRANINFANTRYWEAPONSLEVEL1) !=
        upgrades.end();
    const bool has_infantry_weapons_2 =
        std::find(upgrades.begin(), upgrades.end(),
                  sc2::UPGRADE_ID::TERRANINFANTRYWEAPONSLEVEL2) !=
        upgrades.end();
    const bool has_infantry_weapons_3 =
        std::find(upgrades.begin(), upgrades.end(),
                  sc2::UPGRADE_ID::TERRANINFANTRYWEAPONSLEVEL3) !=
        upgrades.end();
    if (minerals >= 100 && gas >= 100 &&
        (!has_infantry_weapons_1 || !has_infantry_weapons_2 ||
         !has_infantry_weapons_3)) {

        Actions()->UnitCommand(&engineering_bay,
                               sc2::ABILITY_ID::RESEARCH_TERRANINFANTRYWEAPONS);
        return;
    }

    const bool has_infantry_armor_1 =
        std::find(upgrades.begin(), upgrades.end(),
                  sc2::UPGRADE_ID::TERRANINFANTRYARMORSLEVEL1) !=
        upgrades.end();
    const bool has_infantry_armor_2 =
        std::find(upgrades.begin(), upgrades.end(),
                  sc2::UPGRADE_ID::TERRANINFANTRYARMORSLEVEL2) !=
        upgrades.end();
    const bool has_infantry_armor_3 =
        std::find(upgrades.begin(), upgrades.end(),
                  sc2::UPGRADE_ID::TERRANINFANTRYARMORSLEVEL3) !=
        upgrades.end();
    if (minerals >= 100 && gas >= 100 &&
        (!has_infantry_armor_1 || !has_infantry_armor_2 ||
         !has_infantry_armor_3)) {

        Actions()->UnitCommand(&engineering_bay,
                               sc2::ABILITY_ID::RESEARCH_TERRANINFANTRYARMOR);
        return;
    }
}

/**
 * @brief Assigns an action to the armory
 *
 * @param armory
 */
void BasicSc2Bot::AssignArmoryAction(const sc2::Unit *armory) {
    // Get current vehicle weapons upgrades
    const std::vector<sc2::UpgradeID> &upgrades = Observation()->GetUpgrades();
    const bool has_vehicle_weapons_1 =
        std::find(upgrades.begin(), upgrades.end(),
                  sc2::UPGRADE_ID::TERRANVEHICLEWEAPONSLEVEL1) !=
        upgrades.end();
    const bool has_vehicle_weapons_2 =
        std::find(upgrades.begin(), upgrades.end(),
                  sc2::UPGRADE_ID::TERRANVEHICLEWEAPONSLEVEL2) !=
        upgrades.end();
    const bool has_vehicle_weapons_3 =
        std::find(upgrades.begin(), upgrades.end(),
                  sc2::UPGRADE_ID::TERRANVEHICLEWEAPONSLEVEL3) !=
        upgrades.end();
    if ((!has_vehicle_weapons_1 || !has_vehicle_weapons_2 ||
         !has_vehicle_weapons_3)) {

        Actions()->UnitCommand(armory,
                               sc2::ABILITY_ID::RESEARCH_TERRANVEHICLEWEAPONS);
    }
    // Get current ship weapons upgrades
    const bool has_ship_weapons_1 =
        std::find(upgrades.begin(), upgrades.end(),
                  sc2::UPGRADE_ID::TERRANSHIPWEAPONSLEVEL1) != upgrades.end();
    const bool has_ship_weapons_2 =
        std::find(upgrades.begin(), upgrades.end(),
                  sc2::UPGRADE_ID::TERRANSHIPWEAPONSLEVEL2) != upgrades.end();
    const bool has_ship_weapons_3 =
        std::find(upgrades.begin(), upgrades.end(),
                  sc2::UPGRADE_ID::TERRANSHIPWEAPONSLEVEL3) != upgrades.end();
    if ((!has_ship_weapons_1 || !has_ship_weapons_2 || !has_ship_weapons_3)) {

        Actions()->UnitCommand(armory,
                               sc2::ABILITY_ID::RESEARCH_TERRANSHIPWEAPONS);
    }
    // Get current ship armor upgrades
    const bool has_ship_armor_1 =
        std::find(upgrades.begin(), upgrades.end(),
                  sc2::UPGRADE_ID::TERRANSHIPARMORSLEVEL1) != upgrades.end();
    const bool has_ship_armor_2 =
        std::find(upgrades.begin(), upgrades.end(),
                  sc2::UPGRADE_ID::TERRANSHIPARMORSLEVEL2) != upgrades.end();
    const bool has_ship_armor_3 =
        std::find(upgrades.begin(), upgrades.end(),
                  sc2::UPGRADE_ID::TERRANSHIPARMORSLEVEL3) != upgrades.end();
    if ((!has_ship_armor_1 || !has_ship_armor_2 || !has_ship_armor_3)) {

        Actions()->UnitCommand(
            armory, sc2::ABILITY_ID::RESEARCH_TERRANVEHICLEANDSHIPPLATING);
    }
}

/**
 * @brief Assign action to starport techlab
 *
 * @param tech_lab
 */
void BasicSc2Bot::AssignStarportTechLabAction(const sc2::Unit *tech_lab) {
    const sc2::ObservationInterface *observation = Observation();
    const std::vector<sc2::UpgradeID> &upgrades = observation->GetUpgrades();
    const std::vector<sc2::UpgradeData> &upgrade_data =
        observation->GetUpgradeData();

    /*
     * Upgrades in order of best->worst are combat shield, stimpack, concussive
     * shells
     * - combat shield: marines gain 10hp
     * - stimpack: marines can sacrifice 10hp for +50% hp & firing rate for 11
     * seconds
     * - concussive shells: marauder's attack slow
     */
    /*
     * Combat shield costs 100 mineral, 100 gas
     */
    const bool has_banshee_cloak =
        std::find(upgrades.begin(), upgrades.end(),
                  sc2::UPGRADE_ID::BANSHEECLOAK) != upgrades.end();
    if (!has_banshee_cloak) {
        Actions()->UnitCommand(tech_lab,
                               sc2::ABILITY_ID::RESEARCH_BANSHEECLOAKINGFIELD);
        return;
    }
}

/**
 * @brief Assign action to barrack techlab
 *
 * @param tech_lab
 */
void BasicSc2Bot::AssignBarrackTechLabAction(const sc2::Unit &tech_lab) {
    const sc2::ObservationInterface *observation = Observation();
    const std::vector<sc2::UpgradeID> &upgrades = observation->GetUpgrades();
    const std::vector<sc2::UpgradeData> &upgrade_data =
        observation->GetUpgradeData();
    const uint32_t &mineral_count = observation->GetMinerals();
    const uint32_t &gas_count = observation->GetVespene();

    /*
     * Upgrades in order of best->worst are combat shield, stimpack, concussive
     * shells
     * - combat shield: marines gain 10hp
     * - stimpack: marines can sacrifice 10hp for +50% hp & firing rate for 11
     * seconds
     * - concussive shells: marauder's attack slow
     */
    /*
     * Combat shield costs 100 mineral, 100 gas
     */
    const bool has_combat_shield =
        std::find(upgrades.begin(), upgrades.end(),
                  sc2::UPGRADE_ID::COMBATSHIELD) != upgrades.end();
    if (mineral_count >= 100 && gas_count >= 100 && !has_combat_shield) {
        Actions()->UnitCommand(&tech_lab,
                               sc2::ABILITY_ID::RESEARCH_COMBATSHIELD);
        return;
    }
    /*
     * Stimpack costs 100 mineral, 100 gas
     */
    const bool has_stimpack =
        std::find(upgrades.begin(), upgrades.end(),
                  sc2::UPGRADE_ID::MARINESTIMPACK) != upgrades.end();
    if (mineral_count >= 100 && gas_count >= 100 && !has_stimpack) {
        Actions()->UnitCommand(&tech_lab, sc2::ABILITY_ID::RESEARCH_STIMPACK);
        return;
    }

    /*
     * Concussive shells costs 50 mineral, 50 gas
     */
    const bool has_concussive_shells =
        std::find(upgrades.begin(), upgrades.end(),
                  sc2::UPGRADE_ID::COMBATSHIELD) != upgrades.end();
    if (mineral_count >= 50 && gas_count >= 50 && !has_concussive_shells) {
        Actions()->UnitCommand(&tech_lab,
                               sc2::ABILITY_ID::RESEARCH_CONCUSSIVESHELLS);
        return;
    }

    return;
}

/**
 * @brief Assign action to factory techlab
 *
 * @param tech_lab
 */
void BasicSc2Bot::AssignFactoryTechlabAction(const sc2::Unit &tech_lab) {
    if (Observation()->GetMinerals() - 100 > 400)
        Actions()->UnitCommand(&tech_lab,
                               sc2::ABILITY_ID::RESEARCH_SMARTSERVOS);
    return;
}

/**
 * @brief Assign action to fusion core
 *
 * @param fusion_core
 */
void BasicSc2Bot::AssignFusionCoreAction(const sc2::Unit *fusion_core) {
    const sc2::ObservationInterface *observation = Observation();
    const uint32_t &minerals = observation->GetMinerals();
    const uint32_t &gas = observation->GetVespene();
    const sc2::Units &medivacs =
        observation->GetUnits(sc2::Unit::Alliance::Self,
                              sc2::IsUnit(sc2::UNIT_TYPEID::TERRAN_MEDIVAC));
    const sc2::Units &fusion_cores =
        observation->GetUnits(sc2::Unit::Alliance::Self,
                              sc2::IsUnit(sc2::UNIT_TYPEID::TERRAN_FUSIONCORE));
    const sc2::Units &bases =
        observation->GetUnits(sc2::Unit::Alliance::Self, sc2::IsTownHall());
    const sc2::Units starport_techlabs = observation->GetUnits(
        sc2::Unit::Alliance::Self,
        sc2::IsUnit(sc2::UNIT_TYPEID::TERRAN_STARPORTTECHLAB));

    Actions()->UnitCommand(fusion_core,
                           sc2::ABILITY_ID::RESEARCH_MEDIVACENERGYUPGRADE);

    return;
}

/**
 * @brief Assign action to starport
 *
 * @param starport
 */
void BasicSc2Bot::AssignStarportAction(const sc2::Unit *starport) {
    // do nothing if starport isnt built or is idle
    if (starport->build_progress < 1 || starport->orders.size() > 0) {
        return;
    }

    const sc2::ObservationInterface *observation = Observation();
    const uint32_t &minerals = observation->GetMinerals();
    const uint32_t &gas = observation->GetVespene();

    // builds a reactor if it does not have it, else build air units
    // if you don't have an addon, build a reactor
    const sc2::Unit *starport_addon =
        observation->GetUnit(starport->add_on_tag);
    const sc2::Units starport_techlabs = observation->GetUnits(
        sc2::Unit::Alliance::Self,
        sc2::IsUnit(sc2::UNIT_TYPEID::TERRAN_STARPORTTECHLAB));
    const sc2::Units starport_reactors = observation->GetUnits(
        sc2::Unit::Alliance::Self,
        sc2::IsUnit(sc2::UNIT_TYPEID::TERRAN_STARPORTREACTOR));
    if (starport_addon == nullptr && minerals >= 50 && gas >= 50) {
        if (starport_reactors.size() == 0) {
            Actions()->UnitCommand(starport,
                                   sc2::ABILITY_ID::BUILD_REACTOR_STARPORT);
        }
        if (starport_techlabs.size() > 0) {
            Actions()->UnitCommand(starport,
                                   sc2::ABILITY_ID::BUILD_REACTOR_STARPORT);
        }

        return;
    }
    const sc2::Units &medivacs =
        observation->GetUnits(sc2::Unit::Alliance::Self,
                              sc2::IsUnit(sc2::UNIT_TYPEID::TERRAN_MEDIVAC));
    const sc2::Units &fusion_cores =
        observation->GetUnits(sc2::Unit::Alliance::Self,
                              sc2::IsUnit(sc2::UNIT_TYPEID::TERRAN_FUSIONCORE));
    const sc2::Units &bases =
        observation->GetUnits(sc2::Unit::Alliance::Self, sc2::IsTownHall());
    const sc2::Units liberators =
        observation->GetUnits(sc2::Unit::Alliance::Self,
                              sc2::IsUnit(sc2::UNIT_TYPEID::TERRAN_LIBERATOR));

    // Build a medivac!
    if (minerals >= 100 && gas >= 75 &&
        (medivacs.size() < 2 || Observation()->GetFoodUsed() < 100)) {
        Actions()->UnitCommand(starport, sc2::ABILITY_ID::TRAIN_MEDIVAC);

        return;
    }
    const sc2::Units banshees =
        observation->GetUnits(sc2::Unit::Alliance::Self,
                              sc2::IsUnit(sc2::UNIT_TYPEID::TERRAN_BANSHEE));
    if (starport->add_on_tag != 0) {
        // Get the add-on unit using its tag
        const sc2::Unit *add_on = observation->GetUnit(starport->add_on_tag);
        if (add_on->unit_type.ToType() ==
            sc2::UNIT_TYPEID::TERRAN_STARPORTTECHLAB) {
            if (banshees.size() < 2) {
                Actions()->UnitCommand(starport,
                                       sc2::ABILITY_ID::TRAIN_BANSHEE);
            }
        }
    }

    const sc2::Units vikings = observation->GetUnits(
        sc2::Unit::Alliance::Self,
        sc2::IsUnit(sc2::UNIT_TYPEID::TERRAN_VIKINGFIGHTER));
    const sc2::Units battlecruisers = observation->GetUnits(
        sc2::Unit::Alliance::Self,
        sc2::IsUnit(sc2::UNIT_TYPEID::TERRAN_BATTLECRUISER));

    if (vikings.size() < 2 && battlecruisers.size() == 0) {
        Actions()->UnitCommand(starport, sc2::ABILITY_ID::TRAIN_VIKINGFIGHTER);

    }

    else if (liberators.size() < 2 && battlecruisers.size() == 0) {
        Actions()->UnitCommand(starport, sc2::ABILITY_ID::TRAIN_LIBERATOR);
    }

    Actions()->UnitCommand(starport, sc2::ABILITY_ID::TRAIN_BATTLECRUISER);
    return;
}

/**
 * @brief Assign actions to factory
 *
 * @param factory
 */
void BasicSc2Bot::AssignFactoryAction(const sc2::Unit *factory) {
    // Do nothing if building or idle
    if (factory->build_progress < 1 || factory->orders.size() > 0) {
        return;
    }
    TryBuildThor(factory);
    TryBuildSiegeTank(factory);
}