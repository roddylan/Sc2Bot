#ifndef BASIC_SC2_BOT_H_
#define BASIC_SC2_BOT_H_

#include "sc2api/sc2_api.h"
#include "sc2api/sc2_args.h"
#include "sc2lib/sc2_lib.h"
#include "sc2utils/sc2_manage_process.h"
#include "sc2utils/sc2_arg_parser.h"
#include <sc2api/sc2_common.h>
#include <sc2api/sc2_interfaces.h>
#include <sc2api/sc2_typeenums.h>
#include <sc2api/sc2_unit.h>
#include <vector>
#include <mutex>

class BasicSc2Bot : public sc2::Agent {
public:

	void OnGameFullStart();
	void OnGameStart();
	void OnStep();

	//! Called when the unit in the current observation has lower health or shields than in the previous observation.
    //!< \param unit The damaged unit.
    //!< \param health The change in health (damage is positive)
    //!< \param shields The change in shields (damage is positive)
    void OnUnitDamaged(const sc2::Unit*, float /*health*/, float /*shields*/);
	void OnUnitIdle(const sc2::Unit* unit) final;
	void OnUnitCreated(const sc2::Unit* unit);

	bool AttackIntruders();
	bool LoadBunker(const sc2::Unit* marine);
	bool UpgradeFactoryTechLab(const sc2::Unit* factory);
	bool TryBuildSupplyDepot();
	bool TryBuildRefinery();
	bool TryBuildSiegeTank();
	bool TryBuildSiegeTank(const sc2::Unit* factory);
	bool BuildRefinery();
	bool TryBuildFactory();
	bool TryBuildBunker();
	bool TryBuildBarracks();
	bool MobAttackNearbyBaseOrEnemy();
	const sc2::Unit* FindNearestMineralPatch(const sc2::Point2D& start);
	
	bool TryBuildStructure(
		sc2::ABILITY_ID ability_type_for_structure, 
		sc2::UNIT_TYPEID unit_type = sc2::UNIT_TYPEID::TERRAN_SCV
	);
	bool TryBuildStructure(
		sc2::ABILITY_ID ability_type_for_structure, 
		sc2::Point2D location, 
		sc2::Point2D expansion_starting_point = sc2::Point2D(0, 0)
	); // generalized; for expansions

	size_t CountUnitType(sc2::UNIT_TYPEID unit_type);
	const sc2::Unit* FindNearestVespeneGeyser(const sc2::Point2D& start);
	void HandleUpgrades();
	void HandleBuild(); // logic for building instead of just trying on each step
	void AssignIdleWorkers(const sc2::Unit *);
	void AssignWorkers();
	void BuildWorkers();
	bool TryBuildThor();
	void AssignFusionCoreAction(const sc2::Unit& fusion_core);
	bool TryBuildFusionCore();
	void AssignStarportTechLabAction(const sc2::Unit& tech_lab);
	const sc2::Point2D FindNearestRefinery(const sc2::Point2D& start);
	bool UpgradeStarportTechlab(const sc2::Unit& starport);
	void AssignArmoryAction(const sc2::Unit& armory);
	bool TryBuildThor(const sc2::Unit* factory);
	const sc2::Unit* FindInjuredMarine();
	const sc2::Point2D FindLargestMarineCluster(const sc2::Point2D& start, const sc2::Unit& unit);
	const sc2::Units SortMedivacsAccordingToDistance(const sc2::Point2D start);
	int MarineClusterSize(const sc2::Unit* marine, const sc2::Units& marines, sc2::Units& cluster );
	bool HandleExpansion(bool resources_depleted);
	int CountNearbySeigeTanks(const sc2::Unit* factory);
	const sc2::Point2D FindNearestCommandCenter(const sc2::Point2D& start, bool not_start_location = false);
	bool TryBuildMissileTurret();
	bool TryBuildAddOn(sc2::ABILITY_ID ability_type_for_structure, sc2::Tag base_structure);
	bool TryBuildArmory();
	void OnUnitDestroyed(const sc2::Unit* unit);
	void TankAttack(const sc2::Units &squad);
	void TankAttack(const sc2::Units &squad, const sc2::Units &enemies); 
	void VikingAttack(const sc2::Units &squad, const sc2::Units &enemies); // handle attack for a viking
	void AttackWithUnit(const sc2::Unit *unit, const sc2::Units &enemies);
	void AttackWithUnit(const sc2::Unit *unit);

	
	void CheckRefineries();
	// void AssignWorkers(const sc2::Unit *);
	// virtual int MarineClusterSize(const sc2::Unit* marine, const sc2::Units& marines);

	
	
	void LaunchAttack();
	void BuildArmy(); // handle army build
	// void TurretDefend(const sc2::Units &turrets); // missile turret defend (multiple turret)
	void TurretDefend(const sc2::Unit *turret); // missile turret defend (one turret)
	const sc2::Unit* FindNearestWorker(const sc2::Point2D& pos, bool is_busy = false, bool mineral = false);
private:
	sc2::Units marine_cluster;
	const size_t n_tanks = 3;
	const size_t n_bases = 3;
	const size_t n_medivacs = 2;
	// TODO: increase to 22
	// const size_t n_workers_init = 13; // workers per base building split point (build rest of stuff)
	// const size_t n_workers = 20; // workers per base goal amnt
	// const size_t n_missile = 3; // no. missile turrets per base
	const size_t n_mules = 2; // goal no. mules per base
	const size_t N_MARINES = 16; // per base
	const size_t n_marauders = 5; // per base
	
	const size_t N_ARMY_THRESHOLD = 30; // 200 - workers - threshold -> attack; allow bot to keep making units while attacking
	const size_t N_TOTAL_WORKERS = 70; // max no. of workers
	const size_t min_minerals_for_units = 300; // do not dip below this minearl count when training units

	const size_t MIN_MARINE = 20; // per base
	const size_t MIN_VIKINGS = 2; // total
	const size_t GOAL_VIKINGS = 10; // total
	// structure goals
	// per base
	const size_t N_BARRACKS = 2;
	const size_t N_FACTORY = 2;
	const size_t N_STARPORT = 2;
	const size_t N_TURRET = 6;
	const size_t N_BUNKERS = 8;
	const size_t N_MISSILE = 5;
	
	// total
	const size_t N_ENGG_TOTAL = 1;
	const size_t N_ARMORY_TOTAL = 1;
	const size_t N_FUSION_CORE_TOTAL = 1;

	// TODO: fill
	// track upgrades
	// ...

	// TODO: adjust
	const float N_REPAIR_RATIO = 1.5;
	std::vector<sc2::Point3D> expansion_locations;
	std::vector<sc2::Point2DI> pinchpoints;

	sc2::Point3D start_location;
	sc2::Point3D base_location;
	const sc2::Unit *scout;
	std::vector<sc2::Point2D> unexplored_enemy_starting_locations;
	sc2::Point2D *enemy_starting_location;
	bool TryScouting(const sc2::Unit&);
	bool TryScoutingForAttack(const sc2::Unit* unit_to_scout, bool refill_enemy_locations);
	void CheckScoutStatus();
	const sc2::Unit *GetGatheringScv();
	void AssignBarrackAction(const sc2::Unit& barrack);
	void AssignBarrackTechLabAction(const sc2::Unit& barrack_tech_lab);
	void AssignStarportAction(const sc2::Unit& starport);
	void AssignEngineeringBayAction(const sc2::Unit& engineering_bay);
	void AssignFactoryAction(const sc2::Unit *factory);
	void RecheckUnitIdle();
	// void AssignScvToRefineries();
	sc2::Point2D FindPlaceablePositionNear(const sc2::Point2D& starting_point, const sc2::ABILITY_ID& ability_to_place_building);
	bool EnemyNearBase(const sc2::Unit *base);
	
	const sc2::Unit* ChooseAttackTarget(const sc2::Unit *unit, const sc2::Units &enemies);
	bool UseAbility(const sc2::Unit *unit, sc2::ABILITY_ID ability);

	size_t CountUnitTotal(
		const sc2::ObservationInterface *obs, 
		sc2::UNIT_TYPEID unit_type, 
		sc2::UNIT_TYPEID prod_unit, 
		sc2::ABILITY_ID ability
	);

	size_t CountUnitTotal(
		const sc2::ObservationInterface *obs, 
		const std::vector<sc2::UNIT_TYPEID> &unit_type, 
		const std::vector<sc2::UNIT_TYPEID> &prod_unit, 
		sc2::ABILITY_ID ability
	);

	void SquadSplit(const size_t &split_sz, sc2::Units &units, sc2::Units &squad);
	void HandleAttack();
	bool IsStructure(const sc2::Unit &unit) const;
};



#endif