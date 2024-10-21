#include "BasicSc2Bot.h"
#include "sc2api/sc2_api.h"
#include "sc2api/sc2_unit.h"
#include "sc2api/sc2_interfaces.h"
void BasicSc2Bot::OnGameStart() { 
    std::cout << "Hello, World!" << std::endl;
    return; 
}

void BasicSc2Bot::OnStep() { 
    //std::cout << Observation()->GetGameLoop() << std::endl;
    TryBuildSupplyDepot();
    //Actions()->SendChat("Hello, World!", sc2::ChatChannel::All);
    
    

    return; 
}
sc2::Filter scvFilter = [](const sc2::Unit& unit) {
    return unit.unit_type == sc2::UNIT_TYPEID::TERRAN_SCV;
    };
void BasicSc2Bot::OnUnitIdle(const sc2::Unit* unit){



    switch (unit->unit_type.ToType()){
    case sc2::UNIT_TYPEID::TERRAN_COMMANDCENTER: {
        int numSCV = Observation()->GetUnits(sc2::Unit::Alliance::Self, scvFilter).size();
        if (numSCV < 20) {
            Actions()->UnitCommand(unit, sc2::ABILITY_ID::TRAIN_SCV);
            break;
        }
        break;
        }
        default: {
            break;
        }
    }

}
bool BasicSc2Bot::TryBuildSupplyDepot()
{
    const sc2::ObservationInterface* observation = Observation();

    // If we are not supply capped, don't build a supply depot.
    if (observation->GetFoodUsed() <= observation->GetFoodCap() - 2) {
        return false;
    }

    // Try and build a depot. Find a random SCV and give it the order.
    return TryBuildStructure(sc2::ABILITY_ID::BUILD_SUPPLYDEPOT);

}
bool BasicSc2Bot::TryBuildStructure(sc2::ABILITY_ID ability_type_for_structure, sc2::UNIT_TYPEID unit_type)
{
    const sc2::ObservationInterface* observation = Observation();

    // If a unit already is building a structure of this type, do nothing.
    // Also get an SCV to build the structure.
    const sc2::Unit* unit_to_build = nullptr;
    sc2::Units units = observation->GetUnits(sc2::Unit::Alliance::Self);
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

    float rx = sc2::GetRandomScalar();
    float ry = sc2::GetRandomScalar();

    Actions()->UnitCommand(unit_to_build,
        ability_type_for_structure,
        sc2::Point2D(unit_to_build->pos.x + rx * 15.0f, unit_to_build->pos.y + ry * 15.0f));

    return true;
}



