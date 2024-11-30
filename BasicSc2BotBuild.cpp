// BasicSc2BotBuild.cpp
// contains implementation for all build/upgrade related functions

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

bool BasicSc2Bot::UpgradeFactoryTechLab(const sc2::Unit* factory) {
        
    Actions()->UnitCommand(factory, sc2::ABILITY_ID::BUILD_TECHLAB_FACTORY);
    
    return true;

}

bool BasicSc2Bot::TryBuildFactory() {
    const sc2::ObservationInterface* observation = Observation();
    
    if (CountUnitType(sc2::UNIT_TYPEID::TERRAN_BARRACKS) < 1) {
        return false;
    }
    /*
    if (CountUnitType(sc2::UNIT_TYPEID::TERRAN_FACTORY) > 0) {
        return false;
    }
    */
    if (observation->GetVespene() < 100) {
        return false;
    }
    return TryBuildStructure(sc2::ABILITY_ID::BUILD_FACTORY);
}

bool BasicSc2Bot::TryBuildSiegeTank() {
    const sc2::ObservationInterface* observation = Observation();

    if (CountUnitType(sc2::UNIT_TYPEID::TERRAN_FACTORYTECHLAB) < 1) {
        return false;
    }

    if (observation->GetVespene() < 125 || observation->GetMinerals() < 150) {
        return false;
    }

    sc2::Units techlabs = observation->GetUnits(
        sc2::Unit::Alliance::Self,
        IsUnit(sc2::UNIT_TYPEID::TERRAN_FACTORYTECHLAB)
    );

    sc2::Units factories = observation->GetUnits(
        sc2::Unit::Alliance::Self,
        IsUnit(sc2::UNIT_TYPEID::TERRAN_FACTORY)
    );

    bool build = false;

    if (!techlabs.empty()) {
        for (auto factory : factories) {
            if (CountNearbySeigeTanks(factory) > n_tanks && factories.size() > 1) {
                continue;
            }

            build = true;
          //  std::cout << "Building siege tank\n";
            Actions()->UnitCommand(factory, sc2::ABILITY_ID::TRAIN_SIEGETANK);
        }
    }

    // TODO: Remove debug output here
    if (build) {
        sc2::Units tank = observation->GetUnits(
            sc2::Unit::Alliance::Self,
            sc2::IsUnits({
                sc2::UNIT_TYPEID::TERRAN_SIEGETANK,
                sc2::UNIT_TYPEID::TERRAN_SIEGETANKSIEGED
                })
        );

       // std::cout << "n_siegetanks = " << tank.size() << std::endl;
    }

    return true;
}


/**
 * @brief Build siege tank on given techlab factory
 * 
 * @param factory 
 * @return true 
 * @return false 
 */
bool BasicSc2Bot::TryBuildSiegeTank(const sc2::Unit* factory) {
    const sc2::ObservationInterface* observation = Observation();
    
    // if cant build tank
    if (factory->unit_type != sc2::UNIT_TYPEID::TERRAN_FACTORYTECHLAB) {
        return false;
    }

    // sc2::Units units = observation->GetUnits(sc2::Unit::Alliance::Self, IsUnit(sc2::UNIT_TYPEID::TERRAN_FACTORYTECHLAB));
    bool build = false;
    if (CountNearbySeigeTanks(factory) <= n_tanks) {
        build = true;
        Actions()->UnitCommand(factory, sc2::ABILITY_ID::TRAIN_SIEGETANK);
    }
    // TODO: get rid of couts here 
    if (build){
        sc2::Units tank = observation->GetUnits(
            sc2::Unit::Alliance::Self, 
            sc2::IsUnits({sc2::UNIT_TYPEID::TERRAN_SIEGETANK, sc2::UNIT_TYPEID::TERRAN_SIEGETANKSIEGED}));
       // std::cout << "n_siegetanks=" << tank.size() << std::endl;
    }
    return true;
}

bool BasicSc2Bot::TryBuildThor() {
    const sc2::ObservationInterface* observation = Observation();

    if (CountUnitType(sc2::UNIT_TYPEID::TERRAN_FACTORYTECHLAB) < 1) {
        return false;
    }
    if (observation->GetVespene() < 200 || observation->GetMinerals() < 300) {
        return false;
    }
    sc2::Units units = observation->GetUnits(sc2::Unit::Alliance::Self, IsUnit(sc2::UNIT_TYPEID::TERRAN_FACTORYTECHLAB));
    size_t marines_size = CountUnitType(sc2::UNIT_TYPEID::TERRAN_MARINE);
    // TODO: Handle logic for Thor creation as in creating a speicifed amount to cover an area or something
    sc2::Units thors = observation->GetUnits(sc2::Unit::Alliance::Self, IsUnit(sc2::UNIT_TYPEID::TERRAN_THOR));
    if (thors.size() >= (marines_size / 5)) return false;
    for (auto unit : units) {
        
        Actions()->UnitCommand(unit, sc2::ABILITY_ID::TRAIN_THOR);
    }
    return true;
}

bool BasicSc2Bot::TryBuildThor(const sc2::Unit* factory) {
    const sc2::ObservationInterface* observation = Observation();
    const sc2::Unit *add_on = observation->GetUnit(factory->add_on_tag);

    // if cant build thor
    if (add_on == nullptr) {
        return false;
    }
    if (add_on->unit_type != sc2::UNIT_TYPEID::TERRAN_FACTORYTECHLAB) {
        return false;
    }

    if (observation->GetVespene() < 200 || observation->GetMinerals() < 300) {
        return false;
    }
    size_t marines_size = CountUnitType(sc2::UNIT_TYPEID::TERRAN_MARINE);
    // TODO: Handle logic for Thor creation as in creating a speicifed amount to cover an area or something
    sc2::Units thors = observation->GetUnits(sc2::Unit::Alliance::Self, IsUnit(sc2::UNIT_TYPEID::TERRAN_THOR));
    if (thors.size() >= (marines_size / 5)) return false;
    Actions()->UnitCommand(factory, sc2::ABILITY_ID::TRAIN_THOR);
    
    return true;
}

bool BasicSc2Bot::TryBuildBunker() {

    const sc2::ObservationInterface* observation = Observation();


    if (CountUnitType(sc2::UNIT_TYPEID::TERRAN_BARRACKS) < 1) {
        return false;
    }
    /*
    if (CountUnitType(sc2::UNIT_TYPEID::TERRAN_BUNKER) > 2) {
        
        return false;
    }
    */
    return TryBuildStructure(sc2::ABILITY_ID::BUILD_BUNKER);
}

bool BasicSc2Bot::TryBuildBarracks() {
    const sc2::ObservationInterface* observation = Observation();
    int n_depot = CountUnitType(sc2::UNIT_TYPEID::TERRAN_SUPPLYDEPOT);
    int n_lower_depot = CountUnitType(sc2::UNIT_TYPEID::TERRAN_SUPPLYDEPOTLOWERED);

    if ((n_depot + n_lower_depot) < 1) {
       // std::cout << "supply dept < 1" << std::endl;
        return false;
    }

    // if (CountUnitType(sc2::UNIT_TYPEID::TERRAN_BARRACKS) > 0) {
    //     return false;
    // }
    
    return TryBuildStructure(sc2::ABILITY_ID::BUILD_BARRACKS);
}

bool BasicSc2Bot::TryBuildSupplyDepot() {
    const sc2::ObservationInterface* observation = Observation();
    // Lower + normal supply depots = total # of depots


    size_t n_supply_depots = observation->GetUnits(sc2::Unit::Alliance::Self, sc2::IsUnit(sc2::UNIT_TYPEID::TERRAN_SUPPLYDEPOT)).size();
    size_t n_lower_supply_depots = observation->GetUnits(sc2::Unit::Alliance::Self, sc2::IsUnit(sc2::UNIT_TYPEID::TERRAN_SUPPLYDEPOTLOWERED)).size();
    // std::cout << "n suppply depots " << n_supply_depots << std::endl;
    size_t n_bases = observation->GetUnits(sc2::Unit::Alliance::Self, sc2::IsTownHall()).size();
    // std::cout << "n basess " << n_bases << std::endl;
    // make a new supply depot if we are at 2/3 unit capacity
    uint32_t current_supply_use = observation->GetFoodUsed();
    uint32_t cur_max_supply = observation->GetFoodCap();

    if (current_supply_use == 200 || 3 * current_supply_use < 2 * cur_max_supply && (n_supply_depots + n_lower_supply_depots) > 0) {
        // do not build if current_supply_use/max_suply < 2/3 or reached max supply
        return false;
    }
    // commenting this out because our logic for making supply depots should just be based on our food used
    /*
    // do not build if theres more than 4 per base
    if ((n_supply_depots + n_lower_supply_depots) >= 4 * n_bases) {
        return false;
    }
    */
    if (observation->GetMinerals() < 100) {
        return false;
    }

    // bool check;
    // check = TryBuildStructure(sc2::ABILITY_ID::BUILD_SUPPLYDEPOT);
    // std::cout << "built supply depot: " << check << std::endl;
    // return check;
    return TryBuildStructure(sc2::ABILITY_ID::BUILD_SUPPLYDEPOT);
}


bool BasicSc2Bot::TryBuildRefinery() {
    const sc2::ObservationInterface* observation = Observation();

    return BuildRefinery();
}


bool BasicSc2Bot::TryBuildFusionCore() {
    const sc2::ObservationInterface* observation = Observation();

    return TryBuildStructure(sc2::ABILITY_ID::BUILD_FUSIONCORE);
}



bool BasicSc2Bot::BuildRefinery() {
    const sc2::ObservationInterface* observation = Observation();
    const sc2::Unit* unit_to_build = GetGatheringScv();

    const sc2::Unit* target;
    if (unit_to_build != nullptr) {
        const sc2::Unit* target = FindNearestVespeneGeyser(unit_to_build->pos);
        Actions()->UnitCommand(unit_to_build, sc2::ABILITY_ID::BUILD_REFINERY,
            target);
    }

    return true;
}

bool BasicSc2Bot::TryBuildStructure(sc2::ABILITY_ID ability_type_for_structure, sc2::UNIT_TYPEID unit_type) {
    const sc2::ObservationInterface* observation = Observation();

    const sc2::Unit* unit_to_build = nullptr;
    sc2::Units units = observation->GetUnits(sc2::Unit::Alliance::Self);
    sc2::Units bases = Observation()->GetUnits(sc2::Unit::Self, sc2::IsTownHall());
    for (const auto& unit : units) {
        for (const auto& order : unit->orders) {
            if (order.ability_id == ability_type_for_structure) {
                return false;
            }
        }

            if (unit->unit_type == unit_type) {
                unit_to_build = unit;
            }

        }
    }

    // TODO: bring back build logic

    float ry = sc2::GetRandomScalar() * 15.0f;
    float rx = sc2::GetRandomScalar() * 15.0f;
    // float ry = sc2::GetRandomScalar() * 5.0f;
    // float rx = sc2::GetRandomScalar() * 5.0f;
    // float ry = sc2::GetRandomScalar() * 10.0f;
    // float rx = sc2::GetRandomScalar() * 10.0f;
    sc2::Point2D nearest_command_center = FindNearestCommandCenter(unit_to_build->pos, true);
    sc2::Point2D starting_point = sc2::Point2D(base_location.x + rx, base_location.y + ry);
    
    sc2::Point2D pos_to_place_at = FindPlaceablePositionNear(starting_point, ability_type_for_structure);
    if (pos_to_place_at == sc2::Point2D(0, 0)) return false;
    // sc2::Point2D pos_to_place_at = starting_point;
    

    // sc2::Point2D start_location = bases.size() > 1 && nearest_command_center != sc2::Point2D(0, 0) ? nearest_command_center : sc2::Point2D(this->start_location.x, this->start_location.y);
    switch (unit_type) {
    case sc2::UNIT_TYPEID::TERRAN_BUNKER: {
        Actions()->ToggleAutocast(unit_to_build->tag, sc2::ABILITY_ID::BUNKERATTACK);
        break;
    }
    // TODO: fix placement so far enough away enough from obstructions so tech lab can be built on it
    case sc2::UNIT_TYPEID::TERRAN_FACTORY: {
        Actions()->UnitCommand(unit_to_build, ability_type_for_structure, pos_to_place_at);
        return true;
    }
    case sc2::UNIT_TYPEID::TERRAN_MISSILETURRET: {
        // place missile turret around base
        // within 14 of other missile turret
            // query check; selecting missile turret to build next to:
                // grab nearest turret to command center
                // <= 2 turrets within 14 of selected turret -> build 13 squares away
                // 
    }
    default: {
        break;
    }
    }

    Actions()->UnitCommand(unit_to_build, ability_type_for_structure, pos_to_place_at);
    

    return true;
}

bool BasicSc2Bot::TryBuildStructure(sc2::ABILITY_ID ability_type_for_structure, sc2::Point2D location, sc2::Point2D expansion_starting_point) {
    const sc2::ObservationInterface *obs = Observation();
    sc2::Units workers = obs->GetUnits(sc2::Unit::Alliance::Self, IsUnit(sc2::UNIT_TYPEID::TERRAN_SCV));

    // no workers
    if (workers.empty()) {
        return false;
    }

    const sc2::Unit* unit_to_build = nullptr;
    for (const auto& worker : workers) {
        for (const auto& order : worker->orders) {
            if (order.ability_id == ability_type_for_structure) {
                return false;
            }
        }
        
            unit_to_build = worker;
            break;
        }
    }
    // todo: this as possible fix?
    float rx = sc2::GetRandomScalar();
    float ry = sc2::GetRandomScalar();
  
    sc2::Point2D point = expansion_starting_point;
   
    // sc2::Point2D pos_to_place_at = FindPlaceablePositionNear(point, ability_type_for_structure);

    // sc2::Point3D expansion_point(point.x, point.y, 0);

    Actions()->UnitCommand(unit_to_build, ability_type_for_structure, point);

    // check if scv can get there
    // todo: fix
    /*
    if (Query()->PathingDistance(unit_to_build, location) < 0.1F) {
        return false;
    }

    if (Query()->Placement(ability_type_for_structure, location)) {
        Actions()->UnitCommand(unit_to_build, ability_type_for_structure, location);
        return true;
    }
    */
    return true;
}

bool BasicSc2Bot::TryBuildMissileTurret() {
    // TODO: make it so it only builds missiles around each expansion and not favour the first one
    const sc2::ObservationInterface* observation = Observation();

    sc2::Units engineering_bays = observation->GetUnits(sc2::Unit::Alliance::Self, sc2::IsUnit(sc2::UNIT_TYPEID::TERRAN_ENGINEERINGBAY));

    if (engineering_bays.empty()) {
        return false;  
    }
    if (observation->GetMinerals() < 75) {
        return false;
    }
    size_t max_turrets_per_base = n_missile;
    size_t base_count = observation->GetUnits(sc2::Unit::Alliance::Self, sc2::IsTownHall()).size();
    size_t turret_count = observation->GetUnits(sc2::Unit::Alliance::Self, sc2::IsUnit(sc2::UNIT_TYPEID::TERRAN_MISSILETURRET)).size();
    if (max_turrets_per_base * base_count < turret_count) {
        return false;
    }
    return TryBuildStructure(sc2::ABILITY_ID::BUILD_MISSILETURRET);

}
bool BasicSc2Bot::TryBuildArmory() {
    return TryBuildStructure(sc2::ABILITY_ID::BUILD_ARMORY);
}

/**
 * @brief Handle unit upgrades
 * 
 */
void BasicSc2Bot::HandleUpgrades() {
    const sc2::ObservationInterface *obs = Observation();

    auto cur_upgrades = obs->GetUpgrades();

    // get no. of bases
    size_t base_count = obs->GetUnits(sc2::Unit::Alliance::Self, sc2::IsTownHall()).size();
}

/**
 * @brief Handle building logic
 * 
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
    // TODO: separate barracks, factory, starports from techlab
    sc2::Units barracks = obs->GetUnits(sc2::Unit::Self, sc2::IsUnits({sc2::UNIT_TYPEID::TERRAN_BARRACKS, sc2::UNIT_TYPEID::TERRAN_BARRACKSFLYING}));
    sc2::Units factory = obs->GetUnits(sc2::Unit::Self, sc2::IsUnits({sc2::UNIT_TYPEID::TERRAN_FACTORY, sc2::UNIT_TYPEID::TERRAN_FACTORYFLYING, sc2::UNIT_TYPEID::TERRAN_FACTORYREACTOR, sc2::UNIT_TYPEID::TERRAN_FACTORYTECHLAB}));
    sc2::Units starports = obs->GetUnits(sc2::Unit::Self, sc2::IsUnits({sc2::UNIT_TYPEID::TERRAN_STARPORT, sc2::UNIT_TYPEID::TERRAN_STARPORTFLYING, sc2::UNIT_TYPEID::TERRAN_STARPORTREACTOR, sc2::UNIT_TYPEID::TERRAN_STARPORTTECHLAB}));
    sc2::Units engg_bays = obs->GetUnits(sc2::Unit::Self, sc2::IsUnit(sc2::UNIT_TYPEID::TERRAN_ENGINEERINGBAY));
    sc2::Units bunkers = obs->GetUnits(sc2::Unit::Self, sc2::IsUnit(sc2::UNIT_TYPEID::TERRAN_BUNKER));
    sc2::Units techlab_factory = obs->GetUnits(sc2::Unit::Self, sc2::IsUnit(sc2::UNIT_TYPEID::TERRAN_FACTORYTECHLAB));
    sc2::Units techlab_starports = obs->GetUnits(sc2::Unit::Self, sc2::IsUnit(sc2::UNIT_TYPEID::TERRAN_STARPORTTECHLAB));
    sc2::Units armorys = obs->GetUnits(sc2::Unit::Self, sc2::IsUnit( sc2::UNIT_TYPEID::TERRAN_ARMORY));
    sc2::Units fusion_cores = obs->GetUnits(sc2::Unit::Self, sc2::IsUnit(sc2::UNIT_TYPEID::TERRAN_FUSIONCORE));

    // TODO: handle other building


    // current counts
    uint32_t n_minerals = obs->GetMinerals();
    uint32_t n_gas = obs->GetMinerals();

    // goal amnts
    // TODO: change target amounts
    const size_t n_barracks_target = 2;
    const size_t n_factory_target = 1;
    const size_t n_armory_target = 1;
    const size_t n_engg_target = 2;
    const size_t n_bunkers_target = 8;
    const size_t n_starports_target = 1;
   // const std::vector<sc2::UpgradeID>& upgrades = Observation()->GetUpgrades();
  //  const bool has_infantry_weapons_1 = std::find(upgrades.begin(), upgrades.end(), sc2::UPGRADE_ID::TERRANINFANTRYWEAPONSLEVEL1) != upgrades.end();
    
    sc2::Units marines = obs->GetUnits(sc2::Unit::Self, sc2::IsUnit(sc2::UNIT_TYPEID::TERRAN_MARINE));
    sc2::Units tanks = obs->GetUnits(sc2::Unit::Self, sc2::IsUnit(sc2::UNIT_TYPEID::TERRAN_SIEGETANK));
    sc2::Units refineries = obs->GetUnits(sc2::Unit::Self, sc2::IsUnit(sc2::UNIT_TYPEID::TERRAN_REFINERY));
    if (refineries.size() < bases.size() * 2) {
        TryBuildRefinery();
    }
    
    if (barracks.size() < 2 * bases.size()) {
        TryBuildBarracks();
    }

    // build factory
    if (!barracks.empty() && factory.size() < (n_factory_target * bases.size())) {
        if (n_minerals > FACTORY_MINERAL_COST && n_gas > FACTORY_GAS_COST && n_minerals - FACTORY_MINERAL_COST >= 400) {
            //std::cout << "building factory\n\n";
            TryBuildFactory();
        }
    }
    // 400 = command center cost
    if (obs->GetMinerals() - 150 >= 400 && tanks.size() * bases.size() < bases.size()) {
        TryBuildSiegeTank();
    }
    
    // Handle Orbital Command

    if (!barracks.empty()) {
        for (const auto& base : bases) {
            if (base->build_progress != 1) {
                continue;
            }
            sc2::Units orbital_commands = obs->GetUnits(sc2::Unit::Self, sc2::IsUnit(sc2::UNIT_TYPEID::TERRAN_ORBITALCOMMAND));

            //std::cout << "inseting pos: " << base->pos.x << " " << base->pos.y << " " << base->pos.z << std::endl;
            if (obs->GetMinerals() - 150 >= 400) {
                if (orbital_commands.size() >= (bases.size() / 3)) {
                    Actions()->UnitCommand(base, sc2::ABILITY_ID::MORPH_PLANETARYFORTRESS);
                }
                else {
                    Actions()->UnitCommand(base, sc2::ABILITY_ID::MORPH_ORBITALCOMMAND);
                }

                //std::cout << "\nORBITAL COMMAND\n\n";
            }
        }
    }
    if (bases.size() < 2) {
        sc2::Units command_centers = Observation()->GetUnits(
            sc2::Unit::Alliance::Self,
            sc2::IsUnit(sc2::UNIT_TYPEID::TERRAN_COMMANDCENTER)
        );
        bool cc_in_progress = false;
        for (auto& command_center : command_centers) {
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
    if (factory.size() > 0 && starports.size() < n_starports_target * bases.size()) {
        if (n_minerals >= STARPORT_COST && n_gas >= STARPORT_GAS_COST) {
            TryBuildStructure(sc2::ABILITY_ID::BUILD_STARPORT);
        }
    }
    // Dont do anything until we have enough marines to defend and enough bases to start so we dont run out of resources
    if (marines.size() < 20 || tanks.size() < 3 || starports.size() < 2) {
       // HandleExpansion(true);
        return;
    }
    /*
    
        **PHASE ONE DONE**
        
    */
    if (armorys.size() < n_armory_target && n_minerals >= ARMORY_MINERAL_COST && n_gas >= ARMORY_GAS_COST) {
        TryBuildArmory();
    }
    if (starports.size() > 0) {
        for (const auto &starport : starports) {
            if (starport->add_on_tag != NULL) {
                // Get the add-on unit using its tag
                const sc2::Unit* add_on = obs->GetUnit(starport->add_on_tag);
                if (add_on->unit_type.ToType() == sc2::UNIT_TYPEID::TERRAN_STARPORTTECHLAB && fusion_cores.size() < 1) {
                    TryBuildFusionCore();
                    return;
                }
            }
        }
    }
    /*
    for (const auto& starport : starports) {
        if (starport) {
            const sc2::Unit* add_on = obs->GetUnit(starport->add_on_tag);
            if (add_on->unit_type.ToType() == sc2::UNIT_TYPEID::TERRAN_STARPORTTECHLAB && fusion_cores.size() < 1) {
                TryBuildFusionCore();
            }
        }
        
    }
    */
    
    /*
    if (barracks.size() < 2 * bases.size()) {
        TryBuildBarracks();
    }
    */
    //if (!has_infantry_weapons_1) return;
    if (n_minerals >= 400 && bases.size() <= 1) {
        HandleExpansion(false);
    }
    /*
    if (barracks.size() >= bases.size()) {
        HandleExpansion(true);
    }
    */
    // build barracks
    /*
    if (barracks.size() < n_barracks_target * bases.size()) {
        for (const auto &base : bases) {
            
            if (n_minerals > BARRACKS_COST) {
                TryBuildBarracks();
            }
            // if (base->assigned_harvesters >= n_workers_init) {
            //     // std::cout << "building barracks\n\n";
            //     TryBuildBarracks();
            // }
        }
    }
    */
    /*
    if (bunkers.size() < n_bunkers_target * bases.size() && n_minerals >= BUNKER_COST) {
        TryBuildBunker();
        
        sc2::Units scvs = obs->GetUnits(sc2::Unit::Self, sc2::IsUnit(sc2::UNIT_TYPEID::TERRAN_SCV));
        bool bunker_built = false;
        for (auto& scv : scvs) {
            bool is_building_scv = false;
            for (const auto& order : scv->orders) {
                if (order.ability_id == sc2::ABILITY_ID::TRAIN_SCV) {
                    is_building_scv = true;
                    break;
                }
            }

            if (scv->orders.empty() && !is_building_scv) {
                
                bunker_built = true;
                break;
            }
        }

        if (bunker_built) {
            // SCV already assigned to build bunker
        }
        
    }
    */
    // build engg bay for missile turret
    // TODO: improve count
    // Only need 2 engineering bays
    if (engg_bays.size() < n_engg_target) {
        if (n_minerals > 150 && n_gas > 100) {
            //std::cout << "building engg bay\n\n";
            TryBuildStructure(sc2::ABILITY_ID::BUILD_ENGINEERINGBAY);
        }
    }

    
    

  //  TryBuildSiegeTank();

    TryBuildMissileTurret();
    if (obs->GetVespene() - 150 >= 200) {
        TryBuildThor();
    }
    
    // build refinery
    

   
}


bool BasicSc2Bot::TryBuildAddOn(sc2::ABILITY_ID ability_type_for_structure, sc2::Tag base_structure) {
    // TODO: finish
    return false;
}