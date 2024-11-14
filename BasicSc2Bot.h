#ifndef BASIC_SC2_BOT_H_
#define BASIC_SC2_BOT_H_

#include "sc2api/sc2_api.h"
#include "sc2api/sc2_args.h"
#include "sc2lib/sc2_lib.h"
#include "sc2utils/sc2_manage_process.h"
#include "sc2utils/sc2_arg_parser.h"
#include <sc2api/sc2_common.h>
#include <sc2api/sc2_typeenums.h>
#include <sc2api/sc2_unit.h>

class BasicSc2Bot : public sc2::Agent {
public:

	virtual void OnGameFullStart();
	virtual void OnGameStart();
	virtual void OnStep();
	virtual bool AttackIntruders();
	virtual bool LoadBunker(const sc2::Unit* marine);
	virtual void OnUnitIdle(const sc2::Unit* unit) final;
	virtual void OnUnitCreated(const sc2::Unit* unit);
	virtual bool UpgradeFactoryTechLab(const sc2::Unit* factory);
	virtual bool TryBuildSupplyDepot();
	virtual bool TryBuildRefinery();
	virtual bool TryBuildSeigeTank();
	virtual bool BuildRefinery();
	virtual bool TryBuildFactory();
	virtual bool TryBuildBunker();
	virtual bool TryBuildBarracks();
	virtual const sc2::Unit* FindNearestMineralPatch(const sc2::Point2D& start);
	virtual bool TryBuildStructure(sc2::ABILITY_ID ability_type_for_structure, sc2::UNIT_TYPEID unit_type = sc2::UNIT_TYPEID::TERRAN_SCV);
	virtual bool TryBuildStructure(sc2::ABILITY_ID ability_type_for_structure, sc2::Point2D location, sc2::Point2D expansion_starting_point = sc2::Point2D(0, 0)); // generalized; for expansions
	virtual size_t CountUnitType(sc2::UNIT_TYPEID unit_type);
	virtual const sc2::Unit* FindNearestVespeneGeyser(const sc2::Point2D& start);
	virtual void HandleUpgrades();
	virtual void HandleBuild(); // logic for building instead of just trying on each step
	virtual void AssignWorkers(const sc2::Unit *);
	virtual void BuildWorkers();
	virtual bool HandleExpansion();
	virtual int CountNearbySeigeTanks(const sc2::Unit* factory);
	virtual const sc2::Point2D FindNearestCommandCenter(const sc2::Point2D& start, bool not_start_location = false);
	virtual bool TryBuildMissileTurret();

	virtual void OnUnitDestroyed(const sc2::Unit* unit);
private:
	const size_t n_tanks = 8;
	const size_t n_bases = 3;
	const size_t n_medivacs = 2;
	// TODO: increase to 22
	const size_t n_workers_init = 15; // workers per base building split point (build rest of stuff)
	const size_t n_workers = 20; // workers per base goal amnt
	const size_t n_missile = 3; // no. missile turrets per base
	const size_t n_mules = 3; // goal no. mules per base
	const size_t n_marines = 6;
	const size_t n_bunkers = 6;
	std::vector<sc2::Point3D> expansion_locations;

	sc2::Point3D start_location;
	const sc2::Unit *scout;
	std::vector<sc2::Point2D> unexplored_enemy_starting_locations;
	sc2::Point2D *enemy_starting_location;
	bool TryScouting(const sc2::Unit&);
	void CheckScoutStatus();
	const sc2::Unit *GetGatheringScv();
	void AssignBarrackAction(const sc2::Unit& barrack);
	void AssignBarrackTechLabAction(const sc2::Unit& barrack_tech_lab);
	void AssignStarportAction(const sc2::Unit& starport);
};



#endif