#include "BasicSc2Bot.h"
#include "sc2api/sc2_api.h"
#include "sc2api/sc2_unit.h"
#include "sc2api/sc2_interfaces.h"
#include <sc2api/sc2_common.h>
#include <sc2api/sc2_typeenums.h>
#include <sc2api/sc2_unit_filters.h>
#include <sc2lib/sc2_search.h>

/*
 * Picks unit for the barrack to train and instructs it to train it
 */
void BasicSc2Bot::AssignBarrackAction(const sc2::Unit& barrack) {
    /*
    * What should a barrack do?
    * - if it doesnt have an addon, build an addon
    * - if it does have an addon, train marines if it has a reactor; train marauders if it has a tech lab
    */
    const sc2::ObservationInterface* observation = Observation();
    const sc2::Units marines = observation->GetUnits(sc2::Unit::Alliance::Self, sc2::IsUnit(sc2::UNIT_TYPEID::TERRAN_MARINE));
    const uint32_t& mineral_count = observation->GetMinerals();
    const uint32_t& gas_count = observation->GetVespene();
   

    /*
    * The addon for the barrack, either a tech lab or a reactor or none
    * - if there is no addon, we should make one
    */
    const sc2::Unit* barrack_addon = observation->GetUnit(barrack.add_on_tag);
    if (barrack_addon == nullptr) {
        // get ALL the barrack tech labs (not just for this one)
        // - only have 1 tech lab
        const sc2::Units& barrack_tech_labs = observation->GetUnits(sc2::Unit::Alliance::Self, sc2::IsUnit(sc2::UNIT_TYPEID::TERRAN_BARRACKSTECHLAB));
        /*
        * If we don't have a tech lab yet, make one
        * - tech labs can research buffs for marines & marauders
        *   - marines can get +10 hp and stimpacks
        *     - stimpacks let you sacrifice 10hp for bonus combat stats, pairs well with medivacs
        * Costs 50 mineral, 25 gas
        */
        if (barrack_tech_labs.size() < 1 && mineral_count >= 50 && gas_count >= 25) {
            Actions()->UnitCommand(&barrack, sc2::ABILITY_ID::BUILD_TECHLAB_BARRACKS);
            return;
        }

        // have a tech lab already, so build a reactor (costs 50 mineral, 50 gas)
        if (mineral_count >= 50 && gas_count >= 50) {
            Actions()->UnitCommand(&barrack, sc2::ABILITY_ID::BUILD_REACTOR_BARRACKS);
            return;
        }
        return;
    }

    // now we know that this barrack has an addon, but is it a reactor or a tech lab?
    const bool& has_tech_lab = barrack_addon->unit_type == sc2::UNIT_TYPEID::TERRAN_BARRACKSTECHLAB;

    // if it has a tech lab, train marauders constantly
    // marauders only cost 25 gas, use >= 100 so that there's a bit of a buffer for other things
    if (has_tech_lab && mineral_count >= 100 && gas_count >= 100) {
        Actions()->UnitCommand(&barrack, sc2::ABILITY_ID::TRAIN_MARAUDER);
        return;
    }

    // if you have a reactor, you can build things twice as fast so you should spam train marines
    if (mineral_count >= 50) {
        Actions()->UnitCommand(&barrack, sc2::ABILITY_ID::TRAIN_MARINE);
    }
    // train a second one, if you can afford it (reactors build at double speed)
    if (mineral_count >= 100) {
        Actions()->UnitCommand(&barrack, sc2::ABILITY_ID::TRAIN_MARINE);
    }
}

/*
* Assigns an action to the engineering bay
*/
void BasicSc2Bot::AssignEngineeringBayAction(const sc2::Unit& engineering_bay) {
    const std::vector<sc2::UpgradeID>& upgrades = Observation()->GetUpgrades();
    const uint32_t minerals = Observation()->GetMinerals();
    const uint32_t gas = Observation()->GetVespene();
    std::cout << "in enginerring bay upgrade method" << std::endl;
    const bool has_infantry_weapons_3 = std::find(upgrades.begin(), upgrades.end(), sc2::UPGRADE_ID::TERRANINFANTRYWEAPONSLEVEL3) != upgrades.end();
    if (minerals >= 100 && gas >= 100 && !has_infantry_weapons_3) {
        std::cout << "upgrading enginerring bay" << std::endl;
        Actions()->UnitCommand(&engineering_bay, sc2::ABILITY_ID::RESEARCH_TERRANINFANTRYWEAPONSLEVEL3);
        return;
    }

    const bool has_infantry_armor_3 = std::find(upgrades.begin(), upgrades.end(), sc2::UPGRADE_ID::TERRANINFANTRYARMORSLEVEL3) != upgrades.end();
    if (minerals >= 100 && gas >= 100 && !has_infantry_armor_3) {
        std::cout << "upgrading enginerring bay" << std::endl;
        Actions()->UnitCommand(&engineering_bay, sc2::ABILITY_ID::RESEARCH_TERRANINFANTRYARMORLEVEL3);
        return;
    }

    const bool has_infantry_weapons_2 = std::find(upgrades.begin(), upgrades.end(), sc2::UPGRADE_ID::TERRANINFANTRYWEAPONSLEVEL2) != upgrades.end();
    if (minerals >= 100 && gas >= 100 && !has_infantry_weapons_2) {
        std::cout << "upgrading enginerring bay" << std::endl;
        Actions()->UnitCommand(&engineering_bay, sc2::ABILITY_ID::RESEARCH_TERRANINFANTRYWEAPONSLEVEL2);
        return;
    }

    const bool has_infantry_armor_2 = std::find(upgrades.begin(), upgrades.end(), sc2::UPGRADE_ID::TERRANINFANTRYARMORSLEVEL2) != upgrades.end();
    if (minerals >= 100 && gas >= 100 && !has_infantry_armor_2) {
        std::cout << "upgrading enginerring bay" << std::endl;
        Actions()->UnitCommand(&engineering_bay, sc2::ABILITY_ID::RESEARCH_TERRANINFANTRYARMORLEVEL2);
        return;
    }

    const bool has_infantry_weapons_1 = std::find(upgrades.begin(), upgrades.end(), sc2::UPGRADE_ID::TERRANINFANTRYWEAPONSLEVEL1) != upgrades.end();
    if (minerals >= 100 && gas >= 100 && !has_infantry_weapons_1) {
        std::cout << "upgrading enginerring bay" << std::endl;
        Actions()->UnitCommand(&engineering_bay, sc2::ABILITY_ID::RESEARCH_TERRANINFANTRYWEAPONSLEVEL1);
        return;
    }

    const bool has_infantry_armor_1 = std::find(upgrades.begin(), upgrades.end(), sc2::UPGRADE_ID::TERRANINFANTRYARMORSLEVEL1) != upgrades.end();
    if (minerals >= 100 && gas >= 100 && !has_infantry_armor_1) {
        std::cout << "upgrading enginerring bay" << std::endl;
        Actions()->UnitCommand(&engineering_bay, sc2::ABILITY_ID::RESEARCH_TERRANINFANTRYARMORLEVEL1);
        return;
    }
}


/*
* Make sure the barrack tech lab is researching things
*/
void BasicSc2Bot::AssignBarrackTechLabAction(const sc2::Unit& tech_lab) {
    const sc2::ObservationInterface* observation = Observation();
    const std::vector<sc2::UpgradeID>& upgrades = observation->GetUpgrades();
    const std::vector<sc2::UpgradeData>& upgrade_data = observation->GetUpgradeData();
    const uint32_t& mineral_count = observation->GetMinerals();
    const uint32_t& gas_count = observation->GetVespene();

    /*
    * Upgrades in order of best->worst are combat shield, stimpack, concussive shells
    * - combat shield: marines gain 10hp
    * - stimpack: marines can sacrifice 10hp for +50% hp & firing rate for 11 seconds
    * - concussive shells: marauder's attack slow
    */
    /*
    * Combat shield costs 100 mineral, 100 gas
    */
    const bool has_combat_shield = std::find(upgrades.begin(), upgrades.end(), sc2::UPGRADE_ID::COMBATSHIELD) != upgrades.end();
    if (mineral_count >= 100 && gas_count >= 100 && !has_combat_shield) {
        Actions()->UnitCommand(&tech_lab, sc2::ABILITY_ID::RESEARCH_COMBATSHIELD);
        return;
    }
    /*
    * Stimpack costs 100 mineral, 100 gas
    */
    const bool has_stimpack = std::find(upgrades.begin(), upgrades.end(), sc2::UPGRADE_ID::MARINESTIMPACK) != upgrades.end();
    if (mineral_count >= 100 && gas_count >= 100 && !has_stimpack) {
        Actions()->UnitCommand(&tech_lab, sc2::ABILITY_ID::RESEARCH_STIMPACK);
        return;
    }

    /*
    * Concussive shells costs 50 mineral, 50 gas
    */
    const bool has_concussive_shells = std::find(upgrades.begin(), upgrades.end(), sc2::UPGRADE_ID::COMBATSHIELD) != upgrades.end();
    if (mineral_count >= 50 && gas_count >= 50 && !has_concussive_shells) {
        Actions()->UnitCommand(&tech_lab, sc2::ABILITY_ID::RESEARCH_CONCUSSIVESHELLS);
        return;
    }
    return;
}


/*
* Gives the Starport an action
* - builds a reactor if it does not have it
* - otherwise, train a medivac (air unit that heals other units)
*/
void BasicSc2Bot::AssignStarportAction(const sc2::Unit& starport) {
    const sc2::ObservationInterface* observation = Observation();
    const uint32_t& minerals = observation->GetMinerals();
    const uint32_t& gas = observation->GetVespene();

    // currently the strategy is to spam medivacs, I'm not sure about the other air units & how good they are
    // if you don't have an addon, build a reactor
    const sc2::Unit* starport_addon = observation->GetUnit(starport.add_on_tag);
    if (starport_addon == nullptr && minerals >= 50 && gas >= 50) {
        Actions()->UnitCommand(&starport, sc2::ABILITY_ID::BUILD_REACTOR_STARPORT);
        return;
    }

    // build a medivac!
    if (minerals >= 100 && gas >= 75) {
        Actions()->UnitCommand(&starport, sc2::ABILITY_ID::TRAIN_MEDIVAC);
        return;
    }

    return;
}