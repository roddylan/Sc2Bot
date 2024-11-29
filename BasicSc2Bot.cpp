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
    base_location = start_location;
    scout = nullptr; // no scout initially
    unexplored_enemy_starting_locations = Observation()->GetGameInfo().enemy_start_locations;
    enemy_starting_location = nullptr;  // we use a scout to find this
}

void BasicSc2Bot::OnGameFullStart() {
	this->pinchpoints = FindAllPinchPoints(Observation()->GetGameInfo().pathing_grid);
	PrintMap(Observation()->GetGameInfo().pathing_grid, pinchpoints);
	return;
}


// This is never called
void BasicSc2Bot::CheckRefineries() {
    if (Observation()->GetVespene() / Observation()->GetMinerals() >= 0.6) {
        return;
    }

    sc2::Units refineries = Observation()->GetUnits(sc2::Unit::Alliance::Self, sc2::IsUnit(sc2::UNIT_TYPEID::TERRAN_REFINERY));
    if (refineries.empty()) {
        return;
    }

    for (const auto& refinery : refineries) {
        if (!refinery || !refinery->is_alive) {
            continue; 
        }

        if (refinery->assigned_harvesters < refinery->ideal_harvesters) {
            int scvs_needed = refinery->ideal_harvesters - refinery->assigned_harvesters;

            sc2::Units scvs = Observation()->GetUnits(sc2::Unit::Alliance::Self, sc2::IsUnit(sc2::UNIT_TYPEID::TERRAN_SCV));
            if (scvs.empty()) {
                return;
            }

            for (const auto& scv : scvs) {
                if (!scv || !scv->is_alive) {
                    continue; 
                }

                bool is_harvesting = false;
                for (const auto& order : scv->orders) {
                    if (order.ability_id == sc2::ABILITY_ID::HARVEST_GATHER) {
                        is_harvesting = true;
                        break;
                    }
                }
                if (scvs_needed == 0) {
                    break;
                }

                if (scv->orders.empty() || (is_harvesting && Observation()->GetVespene() / Observation()->GetMinerals() < 0.6)) {
                    Actions()->UnitCommand(scv, sc2::ABILITY_ID::HARVEST_GATHER_SCV, refinery);
                    --scvs_needed;
                }
            }
        }
    }
}

void BasicSc2Bot::OnStep() {
    // HandleBuild(); // TODO: move rest of build inside
    const sc2::ObservationInterface *obs = Observation();
    sc2::Units bases = obs->GetUnits(sc2::Unit::Self, sc2::IsTownHall());
    // skip a few frames for speed; avoid duplicate commands
    int skip_frame = 5;

    if (obs->GetGameLoop() % skip_frame) {
        return;
    }
    sc2::Units scvs = obs->GetUnits(sc2::Unit::Alliance::Self, sc2::IsUnit(
        sc2::UNIT_TYPEID::TERRAN_SCV
        ));
    sc2::Units marines = obs->GetUnits(sc2::Unit::Alliance::Self, sc2::IsUnit(
        sc2::UNIT_TYPEID::TERRAN_MARINE
    ));
    if (marines.size() > 10) {
        if (scvs[0]->orders.empty()) {
            TryScoutingForAttack(scvs[0], false);
        }
        
    }
    AssignWorkers();
    // **NOTE** order matters as the amount of minerals we have gets consumed, seige tanks are important to have at each expansion 
    TryBuildSupplyDepot();
    HandleBuild();
    
    BuildWorkers();
    RecheckUnitIdle();

    CheckScoutStatus();
    AttackIntruders();

    // BuildArmy();
    

    // LaunchAttack(); // TODO: fix implementation for final attack logic
    
    HandleAttack();

    // TODO: temporary, move
    sc2::Units tanks = obs->GetUnits(sc2::Unit::Alliance::Self, sc2::IsUnits({
        sc2::UNIT_TYPEID::TERRAN_SIEGETANK,
        sc2::UNIT_TYPEID::TERRAN_SIEGETANKSIEGED
    }));
    sc2::Units enemies = obs->GetUnits(sc2::Unit::Alliance::Enemy, [](const sc2::Unit &unit){
        return unit.display_type == sc2::Unit::DisplayType::Visible;
    });

    if (!enemies.empty() && !tanks.empty()) {
        TankAttack(tanks, enemies);
    }
    
    // if (TryBuildSeigeTank()) {
    //     return;
    // }
    // if (TryBuildMissileTurret()) {
    //     return;
    // }
    if (obs->GetMinerals() - 100 >= 400 || obs->GetFoodUsed() - obs->GetFoodCap() < 30) {
        TryBuildSupplyDepot();
        return;
    }    
    
    

    return;
}

/*
* The OnUnitIdle hook that's automatically called by the game is only called ONCE when the unit starts idling.
* This is an issue for barracks because when OnUnitIdle is called for them but they don't have the resources to
* train a unit, they won't take an action and OnUnitIdle is never called on them again so they never get a kick to
* start training when resources are available.
*/
void BasicSc2Bot::RecheckUnitIdle() {
    const sc2::Units& idle_units = Observation()->GetUnits(sc2::Unit::Alliance::Self, [](const sc2::Unit& unit) {
        return unit.orders.empty();
    });
    for (const sc2::Unit* unit : idle_units) {
        OnUnitIdle(unit);
    }
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
        //std::cout << "supply depot created!" << std::endl;
        Actions()->UnitCommand(unit, sc2::ABILITY_ID::MORPH_SUPPLYDEPOT_LOWER);
        break;
    }
    case sc2::UNIT_TYPEID::TERRAN_MEDIVAC: {
        const sc2::Unit* injured_marine = FindInjuredMarine();
        if (injured_marine) {
            Actions()->UnitCommand(unit, sc2::ABILITY_ID::SMART, injured_marine);
            return;
        }
        sc2::Point2D largest_marine_cluster = FindLargestMarineCluster(unit->pos, *unit);
        if (largest_marine_cluster == sc2::Point2D(0, 0)) return;
        Actions()->UnitCommand(unit, sc2::ABILITY_ID::SMART, largest_marine_cluster);
        break;

    }
    case sc2::UNIT_TYPEID::TERRAN_BANSHEE: {
        sc2::Point2D largest_marine_cluster = FindLargestMarineCluster(unit->pos, *unit);
        if (largest_marine_cluster == sc2::Point2D(0, 0)) return;
        Actions()->UnitCommand(unit, sc2::ABILITY_ID::SMART, largest_marine_cluster);
        break;

    }
    case sc2::UNIT_TYPEID::TERRAN_LIBERATOR: {
        const sc2::Unit* injured_marine = FindInjuredMarine();
        if (injured_marine) {
            Actions()->UnitCommand(unit, sc2::ABILITY_ID::SMART, injured_marine);
            return;
        }
        sc2::Point2D largest_marine_cluster = FindLargestMarineCluster(unit->pos, *unit);
        if (largest_marine_cluster == sc2::Point2D(0, 0)) return;
        Actions()->UnitCommand(unit, sc2::ABILITY_ID::SMART, largest_marine_cluster);
        break;

    }
    case sc2::UNIT_TYPEID::TERRAN_THOR: {
        // TODO: thor should go to choke point when created?
        sc2::Point2D largest_marine_cluster = FindLargestMarineCluster(unit->pos, *unit);
        if (largest_marine_cluster == sc2::Point2D(0, 0)) return;
        Actions()->UnitCommand(unit, sc2::ABILITY_ID::SMART, largest_marine_cluster);
        break;
    }
    case sc2::UNIT_TYPEID::TERRAN_MARINE: {
        // TODO: marine should go to choke point when created
        sc2::Point2D largest_marine_cluster = FindLargestMarineCluster(unit->pos, *unit);
        if (largest_marine_cluster == sc2::Point2D(0, 0)) return;
        Actions()->UnitCommand(unit, sc2::ABILITY_ID::SMART, largest_marine_cluster);
        break;
    }
    }
    
}

void BasicSc2Bot::OnUnitDestroyed(const sc2::Unit* unit) {
    static int mineral_fields_destoryed;

    ++mineral_fields_destoryed;
    std::cout << "mineral_destoryed count " << mineral_fields_destoryed << std::endl;
    if (mineral_fields_destoryed % 10) {
       HandleExpansion(true);
    }
    std::cout << "Minerals destroyed" << std::endl;
    
    // send marines to attack intruders
   
    if (unit->unit_type == sc2::UNIT_TYPEID::TERRAN_ORBITALCOMMAND 
        || unit->unit_type == sc2::UNIT_TYPEID::TERRAN_COMMANDCENTER 
        || unit->unit_type == sc2::UNIT_TYPEID::TERRAN_FACTORY 
        || unit->unit_type == sc2::UNIT_TYPEID::TERRAN_BARRACKS
        || unit->unit_type == sc2::UNIT_TYPEID::TERRAN_REFINERY
        || unit->unit_type == sc2::UNIT_TYPEID::TERRAN_MARINE
        || unit->unit_type == sc2::UNIT_TYPEID::TERRAN_SCV) {

        /*
        for (const auto& marine : marines) {
            Actions()->UnitCommand(marine, sc2::ABILITY_ID::SMART, unit->pos);
        }
        */
        sc2::Units vikings = Observation()->GetUnits(sc2::Unit::Self, sc2::IsUnit(sc2::UNIT_TYPEID::TERRAN_VIKINGFIGHTER));
        sc2::Units marines = Observation()->GetUnits(sc2::Unit::Self, sc2::IsUnit(sc2::UNIT_TYPEID::TERRAN_MARINE));
        sc2::Units marauders = Observation()->GetUnits(sc2::Unit::Self, sc2::IsUnit(sc2::UNIT_TYPEID::TERRAN_MARAUDER));
        sc2::Units banshees = Observation()->GetUnits(sc2::Unit::Self, sc2::IsUnit(sc2::UNIT_TYPEID::TERRAN_BANSHEE));
        if (marines.size() + marauders.size()  + banshees.size() + vikings.size() > 15) {
            Actions()->UnitCommand(marines, sc2::ABILITY_ID::ATTACK_ATTACK, unit->pos);
            sc2::Units thors = Observation()->GetUnits(sc2::Unit::Self, sc2::IsUnit(sc2::UNIT_TYPEID::TERRAN_THOR));
            Actions()->UnitCommand(thors, sc2::ABILITY_ID::ATTACK_ATTACK, unit->pos);
            Actions()->UnitCommand(vikings, sc2::ABILITY_ID::ATTACK_ATTACK, unit->pos);
            Actions()->UnitCommand(marauders, sc2::ABILITY_ID::ATTACK_ATTACK, unit->pos);
            sc2::Units liberators = Observation()->GetUnits(sc2::Unit::Self, sc2::IsUnit(sc2::UNIT_TYPEID::TERRAN_LIBERATOR));
            Actions()->UnitCommand(liberators, sc2::ABILITY_ID::ATTACK_ATTACK, unit->pos);
            Actions()->UnitCommand(banshees, sc2::ABILITY_ID::BEHAVIOR_CLOAKON_BANSHEE);
            Actions()->UnitCommand(banshees, sc2::ABILITY_ID::ATTACK_ATTACK, unit->pos);
            sc2::Units tanks = Observation()->GetUnits(sc2::Unit::Self, sc2::IsUnit(sc2::UNIT_TYPEID::TERRAN_SIEGETANK));
            Actions()->UnitCommand(tanks, sc2::ABILITY_ID::ATTACK_ATTACK, unit->pos);
                    
        }

        sc2::Units battlecruisers = Observation()->GetUnits(sc2::Unit::Self, sc2::IsUnit(sc2::UNIT_TYPEID::TERRAN_BATTLECRUISER));
        Actions()->UnitCommand(battlecruisers, sc2::ABILITY_ID::ATTACK_ATTACK, unit->pos);
        sc2::Units medivacs = Observation()->GetUnits(sc2::Unit::Self, sc2::IsUnit(sc2::UNIT_TYPEID::TERRAN_MEDIVAC));
        int sent_medivacs = 0;
        for (const auto& medivac : medivacs) {
            Actions()->UnitCommand(medivac, sc2::ABILITY_ID::SMART, unit->pos);
            ++sent_medivacs;
            if (sent_medivacs % 2) break;
        }

    }
    
   
   
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
    case sc2::UNIT_TYPEID::TERRAN_SCV: {
        AssignIdleWorkers(unit);
        if (TryScouting(*unit)) {
            break;
        }
        break;
    }
    case sc2::UNIT_TYPEID::TERRAN_STARPORT: {
        AssignStarportAction(unit);
        break;
    }
    case sc2::UNIT_TYPEID::TERRAN_STARPORTTECHLAB: {
        AssignStarportTechLabAction(unit);
        break;
    }
    case sc2::UNIT_TYPEID::TERRAN_ARMORY: {
        AssignArmoryAction(unit);
        break;
    }
    case sc2::UNIT_TYPEID::TERRAN_MEDIVAC: {
        int skip_frame = 10;

        if (Observation()->GetGameLoop() % skip_frame) {
            return;
        }
        const sc2::Unit* injured_marine = FindInjuredMarine();
        if (injured_marine) {
            Actions()->UnitCommand(unit, sc2::ABILITY_ID::SMART, injured_marine);
            return;
        }
        sc2::Point2D largest_marine_cluster = FindLargestMarineCluster(unit->pos, *unit);
        if (largest_marine_cluster == sc2::Point2D(0, 0)) return;
        Actions()->UnitCommand(unit, sc2::ABILITY_ID::SMART, largest_marine_cluster);
        break;

    }
    case sc2::UNIT_TYPEID::TERRAN_ORBITALCOMMAND: {
        // std::cout << "ORBITAL COMMAND\n";
        //sc2::Agent::Actions()->UnitCommand(unit, sc2::ABILITY_ID::TRAIN_SCV);
        break;
    }
    // case sc2::UNIT_TYPEID::TERRAN_MULE: {
    //     AssignWorkers(unit);
    //     break;
    // }
    case sc2::UNIT_TYPEID::TERRAN_SUPPLYDEPOT: {
       // std::cout << "SUPPLY DEPOT IDLE" << std::endl;
        Actions()->UnitCommand(unit, sc2::ABILITY_ID::MORPH_SUPPLYDEPOT_LOWER);
        break;
    }
    case sc2::UNIT_TYPEID::TERRAN_BARRACKS: {
        AssignBarrackAction(unit);
        break;
    }
     
    case sc2::UNIT_TYPEID::TERRAN_FUSIONCORE: {
        AssignFusionCoreAction(unit);
        break;
       
    }
                                  
    case sc2::UNIT_TYPEID::TERRAN_BARRACKSREACTOR: {
        AssignBarrackAction(unit);
    }
    case sc2::UNIT_TYPEID::TERRAN_BARRACKSTECHLAB: {
        AssignBarrackTechLabAction(*unit);
        break;
    }
    case sc2::UNIT_TYPEID::TERRAN_ENGINEERINGBAY: {
        AssignEngineeringBayAction(*unit);
        break;
    }
    case sc2::UNIT_TYPEID::TERRAN_MARINE: {
        // if the bunkers are full

        if (!LoadBunker(unit)) {
            const sc2::GameInfo& game_info = Observation()->GetGameInfo();
            /*Actions()->UnitCommand(unit, sc2::ABILITY_ID::ATTACK_ATTACK
                , game_info.enemy_start_locations.front(), true);*/
            // std::cout << "sent";
        }

        break;
    }
    case sc2::UNIT_TYPEID::TERRAN_FACTORY: {
        // UpgradeFactoryTechLab(unit);
        AssignFactoryAction(unit);
    }

    case sc2::UNIT_TYPEID::TERRAN_FACTORYTECHLAB: {
        AssignFactoryAction(unit); // TODO: techlab should only be upgrading (not the actual factory)
    }
    case sc2::UNIT_TYPEID::TERRAN_MISSILETURRET: {
        TurretDefend(unit);
    }
    default: {
        break;
    }
    }
}

/**
 * @brief Called when the unit in the current observation has lower health or shields than in the previous observation.
 * 
 * @param unit The damaged unit.
 * @param health The change in health (damage is positive)
 * @param shields The change in shields (damage is positive)
 */
void BasicSc2Bot::OnUnitDamaged(const sc2::Unit *unit, float health, float shields) {
    const sc2::ObservationInterface *obs = Observation();
    sc2::ActionInterface *acts = Actions();

    // filter for buildings
    // TODO: add rest of buildings
    if (!(unit->unit_type == sc2::UNIT_TYPEID::TERRAN_SUPPLYDEPOT ||
          unit->unit_type == sc2::UNIT_TYPEID::TERRAN_SUPPLYDEPOTLOWERED ||
          unit->unit_type == sc2::UNIT_TYPEID::TERRAN_ARMORY ||
          unit->unit_type == sc2::UNIT_TYPEID::TERRAN_BARRACKS ||
          unit->unit_type == sc2::UNIT_TYPEID::TERRAN_BARRACKSFLYING ||
          unit->unit_type == sc2::UNIT_TYPEID::TERRAN_BARRACKSREACTOR ||
          unit->unit_type == sc2::UNIT_TYPEID::TERRAN_BARRACKSTECHLAB ||
          unit->unit_type == sc2::UNIT_TYPEID::TERRAN_COMMANDCENTER ||
          unit->unit_type == sc2::UNIT_TYPEID::TERRAN_ORBITALCOMMAND ||
          unit->unit_type == sc2::UNIT_TYPEID::TERRAN_FACTORY ||
          unit->unit_type == sc2::UNIT_TYPEID::TERRAN_FACTORYFLYING ||
          unit->unit_type == sc2::UNIT_TYPEID::TERRAN_FACTORYREACTOR ||
          unit->unit_type == sc2::UNIT_TYPEID::TERRAN_FACTORYTECHLAB ||
          unit->unit_type == sc2::UNIT_TYPEID::TERRAN_STARPORT ||
          unit->unit_type == sc2::UNIT_TYPEID::TERRAN_STARPORTFLYING ||
          unit->unit_type == sc2::UNIT_TYPEID::TERRAN_STARPORTREACTOR ||
          unit->unit_type == sc2::UNIT_TYPEID::TERRAN_STARPORTTECHLAB
    )) {
        return;
    }


    const sc2::Units allies = obs->GetUnits(sc2::Unit::Alliance::Self, sc2::IsUnits({
        sc2::UNIT_TYPEID::TERRAN_MARINE,
        sc2::UNIT_TYPEID::TERRAN_MARAUDER,
        sc2::UNIT_TYPEID::TERRAN_SIEGETANK,
        sc2::UNIT_TYPEID::TERRAN_SIEGETANKSIEGED,
        sc2::UNIT_TYPEID::TERRAN_THOR,
        sc2::UNIT_TYPEID::TERRAN_CYCLONE,
        sc2::UNIT_TYPEID::TERRAN_CYCLONE,
    }));

    // orbital commands
    const sc2::Units orbitals = obs->GetUnits(sc2::Unit::Alliance::Self, sc2::IsUnits({
        sc2::UNIT_TYPEID::TERRAN_ORBITALCOMMAND,
        sc2::UNIT_TYPEID::TERRAN_ORBITALCOMMANDFLYING
    }));

    // supply depot -> try to do supplies calldown to heal depot
    // TODO: add condition
    if (unit->unit_type == sc2::UNIT_TYPEID::TERRAN_SUPPLYDEPOT ||
        unit->unit_type == sc2::UNIT_TYPEID::TERRAN_SUPPLYDEPOTLOWERED) {
        for (const auto &orbital : orbitals) {
            // TODO: orbital command probably never has enough energy, handle this
            if (orbital->energy >= 50) {
                acts->UnitCommand(orbital, sc2::ABILITY_ID::EFFECT_SUPPLYDROP, unit, false);
            }
        }
    }
    
    // const sc2::Point2D base_pos = FindNearestCommandCenter(unit->pos);
    
    // radius around structure
    const float rad = 20;

    const sc2::Units enemies = obs->GetUnits(sc2::Unit::Alliance::Enemy);

    // count friendlies and enemies
    size_t n_friendly{};
    size_t n_enemy{};
    
    for (const auto &ally : allies){
        if (sc2::Distance2D(ally->pos, unit->pos) < rad) {
            ++n_friendly;
        }
    }

    for (const auto &enemy : enemies){
        if (sc2::Distance2D(enemy->pos, unit->pos) < rad) {
            ++n_enemy;
        }
    }

    // no point in repairing, will probably die
    if (n_friendly < N_REPAIR_RATIO * n_enemy) {
        return;
    }

    // get nearby unit to repair
    const sc2::Unit *scv = FindNearestWorker(unit->pos, true);
    if (scv == nullptr) {
        scv = FindNearestWorker(unit->pos, true, true);
    }



}