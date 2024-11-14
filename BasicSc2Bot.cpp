// BasicSc2Bot.cpp
// Implementation for main loop stuff (game start, step, idle, etc.)

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


void BasicSc2Bot::OnGameStart() {
    const sc2::ObservationInterface *obs = Observation();
    sc2::QueryInterface *query = Query();
    expansion_locations = sc2::search::CalculateExpansionLocations(obs, query);
    start_location = obs->GetStartLocation();
    scout = nullptr; // no scout initially
    unexplored_enemy_starting_locations = Observation()->GetGameInfo().enemy_start_locations;
    enemy_starting_location = nullptr;  // we use a scout to find this
}

void BasicSc2Bot::OnGameFullStart() {
    const sc2::ObservationInterface *obs = Observation();
    sc2::GameInfo gin = obs->GetGameInfo();
    sc2::Point3D start_3d = obs->GetUnits(sc2::Unit::Alliance::Self, IsUnit(sc2::UNIT_TYPEID::TERRAN_COMMANDCENTER))[0]->pos;
    sc2::Point2DI start = sc2::Point2DI(round(start_3d.x), round(start_3d.y));

	//sc2::Point2DI pinchpoint = FindPinchPointAroundPoint(obs.pathing_grid, start);
	//PrintMap(obs.pathing_grid, pinchpoint);
	return;
}

void BasicSc2Bot::OnStep() {
    // HandleBuild(); // TODO: move rest of build inside

    // **NOTE** order matters as the amount of minerals we have gets consumed, seige tanks are improtant to have at each expansion 
    HandleBuild();
    TryBuildSeigeTank();
    TryBuildMissileTurret();
    
    BuildWorkers();
    
    TryBuildBarracks();
    TryBuildRefinery();
    TryBuildBunker();
    TryBuildFactory();
    TryBuildSeigeTank();
    CheckScoutStatus();
    TryBuildSupplyDepot();

    /*
    if (TryBuildSupplyDepot()) {
        return;
    }
    if (TryBuildRefinery()) {
        return;
    }
    // TryBuildBarracks();
    // TryBuildBunker();
    // TryBuildFactory();
    */
    
    AttackIntruders();
    return;
}

void BasicSc2Bot::OnUnitCreated(const sc2::Unit* unit) {
    switch (unit->unit_type.ToType()) {
    case sc2::UNIT_TYPEID::TERRAN_SCV: {
        TryScouting(*unit);
        break;
    }
    case sc2::UNIT_TYPEID::TERRAN_SUPPLYDEPOT: {
        /*
         * For now, lower all supply depots by default
         * In the future, maybe we can take advantage of raising/lowering them to control movement
         */
        std::cout << "supply depot created!" << std::endl;
        Actions()->UnitCommand(unit, sc2::ABILITY_ID::MORPH_SUPPLYDEPOT_LOWER);
        break;
    }
    }
}

void BasicSc2Bot::OnUnitDestroyed(const sc2::Unit* unit) {
    if (unit == this->scout) {
        // the scout was destroyed, so we found the base!
        const sc2::GameInfo& info = Observation()->GetGameInfo();
        sc2::Point2D closest_base_position = info.enemy_start_locations[0];
        for (const sc2::Point2D& position : info.enemy_start_locations) {
            if (sc2::DistanceSquared2D(unit->pos, position) < sc2::DistanceSquared2D(unit->pos, closest_base_position)) {
                closest_base_position = position;
            }
        }
        this->enemy_starting_location = &closest_base_position;
    }
}

void BasicSc2Bot::OnUnitIdle(const sc2::Unit* unit) {
    // TODO: refactor
    sc2::Units barracks = Observation()->GetUnits(sc2::Unit::Self, sc2::IsUnits({ sc2::UNIT_TYPEID::TERRAN_BARRACKS, sc2::UNIT_TYPEID::TERRAN_BARRACKSFLYING }));
    sc2::Units bases = Observation()->GetUnits(sc2::Unit::Self, sc2::IsTownHall());

    switch (unit->unit_type.ToType()) {
        // case sc2::UNIT_TYPEID::TERRAN_COMMANDCENTER: {
        //     if (Observation()->GetFoodWorkers() > (n_workers * bases.size()) && !barracks.empty()){
        //         // std::cout << n_workers * bases.size() << std::endl;
        //         // TODO: refactor and move
        //     } else {
        //         sc2::Agent::Actions()->UnitCommand(unit, sc2::ABILITY_ID::TRAIN_SCV);
        //     }
        //     break;
        // }
        // case sc2::UNIT_TYPEID::TERRAN_ORBITALCOMMAND: {
        //     // std::cout << "ORBITAL COMMAND\n";
        //     sc2::Agent::Actions()->UnitCommand(unit, sc2::ABILITY_ID::TRAIN_SCV);
        // }
    case sc2::UNIT_TYPEID::TERRAN_SCV: {
        if (TryScouting(*unit)) {
            break;
        }
        AssignWorkers(unit);
        break;
    }
    case sc2::UNIT_TYPEID::TERRAN_STARPORT: {
        AssignStarportAction(*unit);
        break;
    }
    case sc2::UNIT_TYPEID::TERRAN_ORBITALCOMMAND: {
        // std::cout << "ORBITAL COMMAND\n";
        sc2::Agent::Actions()->UnitCommand(unit, sc2::ABILITY_ID::TRAIN_SCV);
    }
    case sc2::UNIT_TYPEID::TERRAN_MULE: {
        AssignWorkers(unit);
        break;
    }
    case sc2::UNIT_TYPEID::TERRAN_SUPPLYDEPOT: {
       // std::cout << "SUPPLY DEPOT IDLE" << std::endl;
        Actions()->UnitCommand(unit, sc2::ABILITY_ID::MORPH_SUPPLYDEPOT_LOWER);
        break;
    }
    case sc2::UNIT_TYPEID::TERRAN_BARRACKS: {
        AssignBarrackAction(*unit);
        break;
    }
    case sc2::UNIT_TYPEID::TERRAN_BARRACKSTECHLAB: {
        AssignBarrackTechLabAction(*unit);
    }
    case sc2::UNIT_TYPEID::TERRAN_MARINE: {
        // if the bunkers are full

        if (!LoadBunker(unit)) {
            const sc2::GameInfo& game_info = Observation()->GetGameInfo();
            Actions()->UnitCommand(unit, sc2::ABILITY_ID::ATTACK_ATTACK
                , game_info.enemy_start_locations.front(), true);
            // std::cout << "sent";
        }

        break;
    }
    case sc2::UNIT_TYPEID::TERRAN_FACTORY: {
        UpgradeFactoryTechLab(unit);
    }
    default: {
        break;
    }
    }
}

/*
 * Picks unit for the barrack to train and instructs it to train it
 */
void BasicSc2Bot::AssignBarrackAction(const sc2::Unit& barrack) {
    /*
    * What should a barrack do?
    * - if we have very few marines, train marines
    * - otherwise, if it doesnt have an addon, build an addon
    * - if it does have an addon, train marines if it has a reactor; train marauders if it has a tech lab
    */
    const sc2::ObservationInterface* observation = Observation();
    const sc2::Units marines = observation->GetUnits(sc2::Unit::Alliance::Self, sc2::IsUnit(sc2::UNIT_TYPEID::TERRAN_MARINE));
    const uint32_t& mineral_count = observation->GetMinerals();
    const uint32_t& gas_count = observation->GetVespene();
    size_t marine_count = marines.size();
    if (marine_count < 8) {
        Actions()->UnitCommand(&barrack, sc2::ABILITY_ID::TRAIN_MARINE);
        return;
    }

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
    const bool &has_tech_lab = barrack_addon->unit_type == sc2::UNIT_TYPEID::TERRAN_BARRACKSTECHLAB;

    // if it has a tech lab, train marauders constantly
    if (has_tech_lab) {
        Actions()->UnitCommand(&barrack, sc2::ABILITY_ID::TRAIN_MARAUDER);
        return;
    }

    // if you have a reactor, you can build things twice as fast so you should spam train marines
    Actions()->UnitCommand(&barrack, sc2::ABILITY_ID::TRAIN_MARINE);
}

/*
* Make sure the barrack tech lab is researching things
*/
void BasicSc2Bot::AssignBarrackTechLabAction(const sc2::Unit& tech_lab) {
    const sc2::ObservationInterface *observation = Observation();
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
    const sc2::Unit *starport_addon = observation->GetUnit(starport.add_on_tag);
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