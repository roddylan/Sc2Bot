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

// Indicates whether a scout has died
bool BasicSc2Bot::scout_died = false;

/**
 * @brief Initialize game
 * 
 */
void BasicSc2Bot::OnGameStart() {
    const sc2::ObservationInterface *obs = Observation();
    sc2::QueryInterface *query = Query();
    expansion_locations = sc2::search::CalculateExpansionLocations(obs, query);
    start_location = obs->GetStartLocation();
    base_location = start_location;
    scout = nullptr; // No scout initially
    unexplored_enemy_starting_locations = Observation()->GetGameInfo().enemy_start_locations;
    enemy_starting_location = nullptr;  // We use a scout to find this
    sent = false;
}

// Last death location of a unit
sc2::Point2D BasicSc2Bot::last_death_location = sc2::Point2D(0, 0);

/**
 * @brief Gets called on every step of the game
 * 
 */
void BasicSc2Bot::OnStep() {
    const sc2::ObservationInterface *obs = Observation();
    sc2::Units bases = obs->GetUnits(sc2::Unit::Self, sc2::IsTownHall());

    // Skip a few frames for speed; avoid duplicate commands
    int skip_frame = SKIP_FRAME;

    if (obs->GetGameLoop() % skip_frame) {
        return;
    }

    // **NOTE** order matters as the amount of minerals we have gets consumed, seige tanks are important to have at each expansion 
    TryBuildSupplyDepot();
    LaunchAttack(); 
    HandleBuild();
    BuildWorkers();
    RecheckUnitIdle();
    CheckScoutStatus();
    AttackIntruders();
    LaunchAttack();
    HandleAttack();

    sc2::Units tanks = obs->GetUnits(sc2::Unit::Alliance::Self, sc2::IsUnits({
        sc2::UNIT_TYPEID::TERRAN_SIEGETANK,
        sc2::UNIT_TYPEID::TERRAN_SIEGETANKSIEGED
    }));
    sc2::Units enemies = obs->GetUnits(sc2::Unit::Alliance::Enemy, [](const sc2::Unit &unit){
        return unit.display_type == sc2::Unit::DisplayType::Visible;
    });

    // Prevent tanks from getting stuck
    const size_t RANGE = 16;
    if (!enemies.empty() && !tanks.empty()) {
        for (const auto &tank : tanks) {
            float min_dist = std::numeric_limits<float>::max();
            for (const auto& enemy : enemies) {
                // Dont care about flying
                if (enemy->is_flying) {
                    continue;
                }
                float dist = sc2::Distance2D(enemy->pos, tank->pos);
                if (dist < min_dist) {
                    min_dist = dist;
                }
            }
            // If enemies are out of preferred range, unsiege
            if (min_dist > RANGE) {
                Actions()->UnitCommand(tank, sc2::ABILITY_ID::MORPH_UNSIEGE);
            }
        }
    }
    
    return;
}


/**
 * @brief Recall idle units
 * 
 */
void BasicSc2Bot::RecheckUnitIdle() {
    /*
     * The OnUnitIdle hook that's automatically called by the game is only called ONCE when the unit starts idling.
     * This is an issue for barracks because when OnUnitIdle is called for them but they don't have the resources to
     * train a unit, they won't take an action and OnUnitIdle is never called on them again so they never get a kick to
     * start training when resources are available.
     */
    const sc2::Units& idle_units = Observation()->GetUnits(sc2::Unit::Alliance::Self, [](const sc2::Unit& unit) {
        return unit.orders.empty();
    });
    for (const sc2::Unit* unit : idle_units) {
        OnUnitIdle(unit);
    }
}

/**
 * @brief Gives instructions to units on the event that they are created
 * 
 * @param unit 
 */
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
        Actions()->UnitCommand(unit, sc2::ABILITY_ID::MORPH_SUPPLYDEPOT_LOWER);
        break;
    }

    /*
        
        Send army units to largest marine cluster upon creation
        
    */
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
        sc2::Point2D largest_marine_cluster = FindLargestMarineCluster(unit->pos, *unit);
        if (largest_marine_cluster == sc2::Point2D(0, 0)) return;
        Actions()->UnitCommand(unit, sc2::ABILITY_ID::SMART, largest_marine_cluster);
        break;
    }
    case sc2::UNIT_TYPEID::TERRAN_MARINE: {
        sc2::Point2D largest_marine_cluster = FindLargestMarineCluster(unit->pos, *unit);
        if (largest_marine_cluster == sc2::Point2D(0, 0)) return;
        Actions()->UnitCommand(unit, sc2::ABILITY_ID::SMART, largest_marine_cluster);
        break;
    }
    default: 
        break;
    }
}

/**
 * @brief Handle logic when a unit is destroyed
 * 
 * @param unit 
 */
void BasicSc2Bot::OnUnitDestroyed(const sc2::Unit* unit) {
    const sc2::ObservationInterface* obs = Observation();
    if (obs->GetGameLoop() % SKIP_FRAME) {
        return;
    }
    static int unit_destroyed;

    ++unit_destroyed;

    if (unit_destroyed % 10) {
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

        if ((Observation()->GetGameLoop() < 13200 && Distance2D(unit->pos, start_location) > 50.0f)) {
            return;
        }
        // Remove units that are father than 70 away
        auto filter_units = [&](sc2::Units& units) {
            units.erase(std::remove_if(units.begin(), units.end(),
                [&](const sc2::Unit* target_unit) {
                    return sc2::Distance2D(target_unit->pos, unit->pos) > 70.0f;
                }),
                units.end());
            };
        // Add units to squad
        sc2::Units vikings = Observation()->GetUnits(sc2::Unit::Self, sc2::IsUnit(sc2::UNIT_TYPEID::TERRAN_VIKINGFIGHTER));
        filter_units(vikings);

        sc2::Units marines = Observation()->GetUnits(sc2::Unit::Self, sc2::IsUnit(sc2::UNIT_TYPEID::TERRAN_MARINE));
        filter_units(marines);

        sc2::Units marauders = Observation()->GetUnits(sc2::Unit::Self, sc2::IsUnit(sc2::UNIT_TYPEID::TERRAN_MARAUDER));
        filter_units(marauders);

        sc2::Units banshees = Observation()->GetUnits(sc2::Unit::Self, sc2::IsUnit(sc2::UNIT_TYPEID::TERRAN_BANSHEE));
        filter_units(banshees);
        // Command to attack
        Actions()->UnitCommand(marines, sc2::ABILITY_ID::ATTACK_ATTACK, unit->pos);
        Actions()->UnitCommand(marauders, sc2::ABILITY_ID::ATTACK_ATTACK, unit->pos);
        Actions()->UnitCommand(vikings, sc2::ABILITY_ID::ATTACK_ATTACK, unit->pos);
        // Cloak banshees
        for (const auto& banshee : banshees) {
            Actions()->UnitCommand(banshee, sc2::ABILITY_ID::BEHAVIOR_CLOAKON_BANSHEE);
            Actions()->UnitCommand(banshee, sc2::ABILITY_ID::ATTACK_ATTACK, unit->pos);
        }

        // Add units to squad and order to attack
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

        // Send 2 medivacs
        int sent_medivacs = 0;
        for (const auto& medivac : medivacs) {
            Actions()->UnitCommand(medivac, sc2::ABILITY_ID::SMART, unit->pos);
            ++sent_medivacs;
            if (sent_medivacs % 2 == 0) break;
        }

    }

    // If the unit that ws destoryed is the scout that means we found the base
    if (unit == this->scout) {
        scout_died = true;
        const sc2::GameInfo& info = Observation()->GetGameInfo();
        // Get nearest base to death location
        sc2::Point2D closest_base_position = info.enemy_start_locations[0];
        for (const sc2::Point2D& position : info.enemy_start_locations) {
            if (sc2::DistanceSquared2D(unit->pos, position) < sc2::DistanceSquared2D(unit->pos, closest_base_position)) {
                closest_base_position = position;
            }
        }
        this->enemy_starting_location = new sc2::Point2D(closest_base_position.x, closest_base_position.y);
        this->visited_start = false;
    }
}

/**
 * @brief Decides what to do when a unit enters idle state
 * 
 * @param unit 
 */
void BasicSc2Bot::OnUnitIdle(const sc2::Unit* unit) {
    const sc2::ObservationInterface* obs = Observation();
    if (obs->GetGameLoop() % SKIP_FRAME) {
        return;
    }

    sc2::Units barracks = Observation()->GetUnits(sc2::Unit::Self, sc2::IsUnits({ sc2::UNIT_TYPEID::TERRAN_BARRACKS, sc2::UNIT_TYPEID::TERRAN_BARRACKSFLYING }));
    sc2::Units bases = Observation()->GetUnits(sc2::Unit::Self, sc2::IsTownHall());

    switch (unit->unit_type.ToType()) {

    case sc2::UNIT_TYPEID::TERRAN_SCV: {
        // Assign scv to gather or scout if still valid
        AssignIdleWorkers(unit);
        if (TryScouting(*unit)) {
            break;
        }
        break;
    }
    /*
    
        Start upgrading
    
    */
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
    case sc2::UNIT_TYPEID::TERRAN_BARRACKS: {
        AssignBarrackAction(unit);
        break;
    }
    case sc2::UNIT_TYPEID::TERRAN_BARRACKSREACTOR: {
        AssignBarrackAction(unit);
    }
    case sc2::UNIT_TYPEID::TERRAN_FUSIONCORE: {
        AssignFusionCoreAction(unit);
        break;

    }
    case sc2::UNIT_TYPEID::TERRAN_BARRACKSTECHLAB: {
        AssignBarrackTechLabAction(*unit);
        break;
    }
    case sc2::UNIT_TYPEID::TERRAN_ENGINEERINGBAY: {
        AssignEngineeringBayAction(*unit);
        break;
    }
    case sc2::UNIT_TYPEID::TERRAN_FACTORY: {
        UpgradeFactoryTechLab(unit);
    }

    case sc2::UNIT_TYPEID::TERRAN_FACTORYTECHLAB: {
        AssignFactoryTechlabAction(*unit);
        AssignFactoryAction(unit);
    }
    /*
    
        Medivacs must either go to injured marine or to largest marine cluster

    */
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
    // Lower supply depots
    case sc2::UNIT_TYPEID::TERRAN_SUPPLYDEPOT: {
        Actions()->UnitCommand(unit, sc2::ABILITY_ID::MORPH_SUPPLYDEPOT_LOWER);
        break;
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