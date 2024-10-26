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
	virtual void OnUnitDestroyed(const sc2::Unit* unit);
private:
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