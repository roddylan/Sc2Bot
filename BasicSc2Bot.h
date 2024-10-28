#ifndef BASIC_SC2_BOT_H_
#define BASIC_SC2_BOT_H_

#include "sc2api/sc2_api.h"
#include "sc2api/sc2_args.h"
#include "sc2lib/sc2_lib.h"
#include "sc2utils/sc2_manage_process.h"
#include "sc2utils/sc2_arg_parser.h"
 
class BasicSc2Bot : public sc2::Agent {
public:
	virtual void OnGameStart();
	virtual void OnStep();
	virtual void OnUnitIdle(const sc2::Unit* unit) final;
	virtual bool TryBuildSupplyDepot();
	virtual bool TryBuildRefinery();
	virtual bool TryBuildSeigeTank();
	virtual bool BuildRefinery();
	virtual bool TryBuildFactory();
	virtual bool TryBuildBunker();
	virtual bool TryBuildBarracks();
	virtual const sc2::Unit* FindNearestMineralPatch(const sc2::Point2D& start);
	virtual bool TryBuildStructure(sc2::ABILITY_ID ability_type_for_structure, sc2::UNIT_TYPEID unit_type = sc2::UNIT_TYPEID::TERRAN_SCV);
	virtual size_t CountUnitType(sc2::UNIT_TYPEID unit_type);
	virtual const sc2::Unit* FindNearestVespeneGeyser(const sc2::Point2D& start);
	virtual void HandleUpgrades();
	virtual void HandleBuild(); // logic for building instead of just trying on each step

	virtual void OnUnitDestroyed(const sc2::Unit* unit);
private:
	const size_t n_tanks = 8;
	const size_t n_bases = 3;
	const size_t n_medivacs = 2;
	const size_t n_workers = 22; // workers per base goal amnt
	const size_t n_missile = 3; // no. missile turrets per base
	const sc2::Unit *scout;
	std::vector<sc2::Point2D> unexplored_enemy_starting_locations;
	sc2::Point2D *enemy_starting_location;
	bool TryBuildSupplyDepot();
	bool TryBuildRefinery();
	bool TryBuildStructure(sc2::ABILITY_ID ability_type_for_structure, sc2::UNIT_TYPEID unit_type = sc2::UNIT_TYPEID::TERRAN_SCV);
	bool TryScouting(const sc2::Unit&);
	void CheckScoutStatus();
};

#endif