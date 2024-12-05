// BasicSc2Bot.cpp
// Implementation for main loop stuff (game start, step, idle, etc.)

#include "BasicSc2Bot.h"
#include "Utilities.h"
#include "Betweenness.h"
#include "sc2api/sc2_api.h"
#include "sc2api/sc2_unit.h"
#include "sc2api/sc2_interfaces.h"
#include <sc2api/sc2_common.h>
#include <sc2api/sc2_gametypes.h>
#include <sc2api/sc2_map_info.h>
#include <sc2api/sc2_score.h>
#include <sc2api/sc2_typeenums.h>
#include <sc2api/sc2_unit_filters.h>
#include <sc2lib/sc2_search.h>
#include <iostream>
#include <cmath>
#include <string>

bool BasicSc2Bot::scout_died = false;
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
static bool protoss_enemy = false;
void BasicSc2Bot::OnGameFullStart() {
	// this->pinchpoints = FindAllPinchPoints(Observation()->GetGameInfo().pathing_grid);
	// PrintMap(Observation()->GetGameInfo().pathing_grid, pinchpoints);
    
    const sc2::GameInfo game_info = Observation()->GetGameInfo();
    for (const auto& player : game_info.player_info) {
        if (player.race_actual == sc2::Race::Protoss) {
            protoss_enemy = true;
            std::cout << "protoss" << std::endl;
        }
        
    }
    



    return;
}
	


sc2::Point2D BasicSc2Bot::last_death_location = sc2::Point2D(0, 0);





void BasicSc2Bot::OnStep() {
    // HandleBuild(); // TODO: move rest of build inside
    const sc2::ObservationInterface *obs = Observation();
    sc2::Units bases = obs->GetUnits(sc2::Unit::Self, sc2::IsTownHall());
    // skip a few frames for speed; avoid duplicate commands
    int skip_frame = SKIP_FRAME;

    if (obs->GetGameLoop() % skip_frame) {
        return;
    }
    sc2::Units scvs = obs->GetUnits(sc2::Unit::Alliance::Self, sc2::IsUnit(
        sc2::UNIT_TYPEID::TERRAN_SCV
        ));
    sc2::Units marines = obs->GetUnits(sc2::Unit::Alliance::Self, sc2::IsUnit(
        sc2::UNIT_TYPEID::TERRAN_MARINE
    ));
    

    // SendSquad();
    // AssignWorkers();
    // **NOTE** order matters as the amount of minerals we have gets consumed, seige tanks are important to have at each expansion 
    TryBuildSupplyDepot();
    /*
    if (protoss_enemy) {
        SendSquadProtoss();
        ProtossBuild();
    }
    else {
        
    }
    */
    // SendSquad();
    LaunchAttack(); // TODO: fix implementation for final attack logic
    HandleBuild();
    
    

    BuildWorkers();
    RecheckUnitIdle();

    CheckScoutStatus();
    AttackIntruders();

    // Wall();

    // BuildArmy(); // TODO: use this
    

    LaunchAttack(); // TODO: fix implementation for final attack logic

    HandleAttack();

    // TODO: temporary, move
    sc2::Units tanks = obs->GetUnits(sc2::Unit::Alliance::Self, sc2::IsUnits({
        sc2::UNIT_TYPEID::TERRAN_SIEGETANK,
        sc2::UNIT_TYPEID::TERRAN_SIEGETANKSIEGED
    }));
    sc2::Units enemies = obs->GetUnits(sc2::Unit::Alliance::Enemy, [](const sc2::Unit &unit){
        return unit.display_type == sc2::Unit::DisplayType::Visible;
    });


    // prevent tanks from getting stuck
    const size_t RANGE = 16;
    if (!enemies.empty() && !tanks.empty()) {
        for (const auto &tank : tanks) {
            float min_dist = std::numeric_limits<float>::max();
            for (const auto& enemy : enemies) {
                // dont care about flying
                if (enemy->is_flying) {
                    continue;
                }
                float dist = sc2::Distance2D(enemy->pos, tank->pos);
                if (dist < min_dist) {
                    min_dist = dist;
                }
            }
            if (min_dist > RANGE) {
                Actions()->UnitCommand(tank, sc2::ABILITY_ID::MORPH_UNSIEGE);
            }
        }
    }
    
    // if (TryBuildSeigeTank()) {
    //     return;
    // }
    // if (TryBuildMissileTurret()) {
    //     return;
    // }
    /*
    if (obs->GetMinerals() - 100 >= 400 || current_supply_use < ((1 / 2) * obs->get)) {
        TryBuildSupplyDepot();
        return;
    } 
    */
    
    

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
    const sc2::ObservationInterface* obs = Observation();
    if (obs->GetGameLoop() % SKIP_FRAME) {
        return;
    }
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

    default: 
        break;
    }
    
}

void BasicSc2Bot::OnUnitDestroyed(const sc2::Unit* unit) {
    const sc2::ObservationInterface* obs = Observation();
    if (obs->GetGameLoop() % SKIP_FRAME) {
        return;
    }
    static int units_destroyed;

    ++units_destroyed;
    if (units_destroyed % 10) {
        HandleExpansion(true);
    }
     // save last death location for sending attack
    if (unit->unit_type == sc2::UNIT_TYPEID::TERRAN_VIKINGFIGHTER
        || unit->unit_type == sc2::UNIT_TYPEID::TERRAN_LIBERATOR
        || unit->unit_type == sc2::UNIT_TYPEID::TERRAN_BATTLECRUISER
        || unit->unit_type == sc2::UNIT_TYPEID::TERRAN_MARAUDER
        || unit->unit_type == sc2::UNIT_TYPEID::TERRAN_SIEGETANK
        || unit->unit_type == sc2::UNIT_TYPEID::TERRAN_MARINE
        || unit->unit_type == sc2::UNIT_TYPEID::TERRAN_SCV)
    {
        BasicSc2Bot::last_death_location.x = unit->pos.x;
        BasicSc2Bot::last_death_location.y = unit->pos.y;
        // std::cout << "last death location: " << BasicSc2Bot::last_death_location.x << BasicSc2Bot::last_death_location.y << std::endl;
    }
    if (unit->unit_type == sc2::UNIT_TYPEID::TERRAN_ORBITALCOMMAND
        || unit->unit_type == sc2::UNIT_TYPEID::TERRAN_COMMANDCENTER
        || unit->unit_type == sc2::UNIT_TYPEID::TERRAN_PLANETARYFORTRESS
        || unit->unit_type == sc2::UNIT_TYPEID::TERRAN_STARPORT
        || unit->unit_type == sc2::UNIT_TYPEID::TERRAN_FACTORY
        || unit->unit_type == sc2::UNIT_TYPEID::TERRAN_BARRACKS
        || unit->unit_type == sc2::UNIT_TYPEID::TERRAN_REFINERY
        || unit->unit_type == sc2::UNIT_TYPEID::TERRAN_MARINE
        || unit->unit_type == sc2::UNIT_TYPEID::TERRAN_SCV && unit != this->scout) {

        /*
        for (const auto& marine : marines) {
            Actions()->UnitCommand(marine, sc2::ABILITY_ID::SMART, unit->pos);
        }
        */
        if ((Observation()->GetGameLoop() < 13200 && Distance2D(unit->pos, start_location) > 50.0f)) {
            return;
        }
        if (unit->unit_type == sc2::UNIT_TYPEID::TERRAN_VIKINGFIGHTER
            || unit->unit_type == sc2::UNIT_TYPEID::TERRAN_LIBERATOR
            || unit->unit_type == sc2::UNIT_TYPEID::TERRAN_BATTLECRUISER
            || unit->unit_type == sc2::UNIT_TYPEID::TERRAN_MARAUDER
            || unit->unit_type == sc2::UNIT_TYPEID::TERRAN_SIEGETANK
            || unit->unit_type == sc2::UNIT_TYPEID::TERRAN_MARINE
            || unit->unit_type == sc2::UNIT_TYPEID::TERRAN_SCV)
        {
            /*
            if (sc2::Distance2D(unit->pos, start_location) > 70.0f) {
                return;
            }
            */
        }
        auto filter_units = [&](sc2::Units& units) {
            units.erase(std::remove_if(units.begin(), units.end(),
                [&](const sc2::Unit* target_unit) {
                    return sc2::Distance2D(target_unit->pos, unit->pos) > 70.0f;
                }),
                units.end());
            };

        sc2::Units vikings = Observation()->GetUnits(sc2::Unit::Self, sc2::IsUnit(sc2::UNIT_TYPEID::TERRAN_VIKINGFIGHTER));
        filter_units(vikings);

        sc2::Units marines = Observation()->GetUnits(sc2::Unit::Self, sc2::IsUnit(sc2::UNIT_TYPEID::TERRAN_MARINE));
        filter_units(marines);

        sc2::Units marauders = Observation()->GetUnits(sc2::Unit::Self, sc2::IsUnit(sc2::UNIT_TYPEID::TERRAN_MARAUDER));
        filter_units(marauders);

        sc2::Units banshees = Observation()->GetUnits(sc2::Unit::Self, sc2::IsUnit(sc2::UNIT_TYPEID::TERRAN_BANSHEE));
        filter_units(banshees);

        Actions()->UnitCommand(marines, sc2::ABILITY_ID::ATTACK_ATTACK, unit->pos);
        Actions()->UnitCommand(marauders, sc2::ABILITY_ID::ATTACK_ATTACK, unit->pos);
        Actions()->UnitCommand(vikings, sc2::ABILITY_ID::ATTACK_ATTACK, unit->pos);

        for (const auto& banshee : banshees) {
            Actions()->UnitCommand(banshee, sc2::ABILITY_ID::BEHAVIOR_CLOAKON_BANSHEE);
            Actions()->UnitCommand(banshee, sc2::ABILITY_ID::ATTACK_ATTACK, unit->pos);
        }

        sc2::Units thors = Observation()->GetUnits(sc2::Unit::Self, sc2::IsUnit(sc2::UNIT_TYPEID::TERRAN_THOR));
        filter_units(thors);
        Actions()->UnitCommand(thors, sc2::ABILITY_ID::ATTACK_ATTACK, unit->pos);

        sc2::Units liberators = Observation()->GetUnits(sc2::Unit::Self, sc2::IsUnit(sc2::UNIT_TYPEID::TERRAN_LIBERATOR));
        filter_units(liberators);
        Actions()->UnitCommand(liberators, sc2::ABILITY_ID::ATTACK_ATTACK, unit->pos);

        sc2::Units tanks = Observation()->GetUnits(sc2::Unit::Self, sc2::IsUnit(sc2::UNIT_TYPEID::TERRAN_SIEGETANK));
        filter_units(tanks);
        Actions()->UnitCommand(tanks, sc2::ABILITY_ID::ATTACK_ATTACK, unit->pos);

        sc2::Units battlecruisers = Observation()->GetUnits(sc2::Unit::Self, sc2::IsUnit(sc2::UNIT_TYPEID::TERRAN_BATTLECRUISER));
        filter_units(battlecruisers);
        Actions()->UnitCommand(battlecruisers, sc2::ABILITY_ID::ATTACK_ATTACK, unit->pos);

        sc2::Units medivacs = Observation()->GetUnits(sc2::Unit::Self, sc2::IsUnit(sc2::UNIT_TYPEID::TERRAN_MEDIVAC));
        filter_units(medivacs);

        int sent_medivacs = 0;
        for (const auto& medivac : medivacs) {
            Actions()->UnitCommand(medivac, sc2::ABILITY_ID::SMART, unit->pos);
            ++sent_medivacs;
            if (sent_medivacs % 2 == 0) break;
        }

    }

   
    if (unit == this->scout) {
        scout_died = true;
        // the scout was destroyed, so we found the base!
        const sc2::GameInfo& info = Observation()->GetGameInfo();
        sc2::Point2D closest_base_position = info.enemy_start_locations[0];
        for (const sc2::Point2D& position : info.enemy_start_locations) {
            if (sc2::DistanceSquared2D(unit->pos, position) < sc2::DistanceSquared2D(unit->pos, closest_base_position)) {
                closest_base_position = position;
            }
        }
        this->enemy_starting_location = new sc2::Point2D(closest_base_position.x, closest_base_position.y);
    }
}

void BasicSc2Bot::OnUnitIdle(const sc2::Unit* unit) {
    const sc2::ObservationInterface* obs = Observation();
    if (obs->GetGameLoop() % SKIP_FRAME) {
        return;
    }
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
         
    //case sc2::UNIT_TYPEID::TERRAN_MARINE: {
        // if the bunkers are full

       // if (!LoadBunker(unit)) {
         //   const sc2::GameInfo& game_info = Observation()->GetGameInfo();
            /*Actions()->UnitCommand(unit, sc2::ABILITY_ID::ATTACK_ATTACK
                , game_info.enemy_start_locations.front(), true);*/
            // std::cout << "sent";
      //  }

     //   break;
  // }

    case sc2::UNIT_TYPEID::TERRAN_FACTORY: {
        UpgradeFactoryTechLab(unit);
        // AssignFactoryAction(unit);
    }

    case sc2::UNIT_TYPEID::TERRAN_FACTORYTECHLAB: {
        AssignFactoryTechlabAction(*unit);
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
    /*
    const sc2::ObservationInterface* obs = Observation();
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
    */



}

/**
 * @brief Game end
 * 
 */
void BasicSc2Bot::OnGameEnd() {
    const sc2::ObservationInterface *obs = Observation();

    const sc2::GameInfo &gin = obs->GetGameInfo();

    const std::vector<sc2::PlayerResult> &results = obs->GetResults();

    const sc2::Score &score = obs->GetScore();

    // OUTPUT SCORE DETAILS
    
    std::cout << "------------------------- BOT SCORE -------------------------\n";

    std::string score_type = "";

    if (score.score_type == sc2::ScoreType::Curriculum) {
        score_type = "CURRICULUM";
    } else {
        score_type = "MELEE";
    }

    std::cout << "Score Type: " << score_type << std::endl;
    std::cout << "Score = " << score.score << std::endl;

    // std::cout << "\n\n-------------------------------------------------------------\n\n";
    
    // OUTPUT RESULT DETAILS
    
    std::cout << "\n------------------------ GAME RESULTS -----------------------\n";

    uint32_t pid = obs->GetPlayerID();
    
    for (const auto &result : results) {
        std::string player_result{};
        std::string player{};

        switch (result.result) {
        case sc2::GameResult::Loss: {
            player_result = "Loss";
            break;
        }
        case sc2::GameResult::Tie: {
            player_result = "Tie";
            break;
        }
        case sc2::GameResult::Win: {
            player_result = "Win";
            break;
        }
        case sc2::GameResult::Undecided: {
            player_result = "Undecided";
            break;
        }
        }

        if (result.player_id == pid) {
            std::cout << "Player [SELF] | " << player_result << std::endl;
        } else {
            std::cout << "Player [ENEMY] (" << result.player_id << ") | " << player_result << std::endl;
        }
        std::cout << "\n";

    }
    /*
    ------------------------- BOT SCORE -------------------------
    ------------------------ GAME RESULTS -----------------------
    -------------------------------------------------------------
    */


}