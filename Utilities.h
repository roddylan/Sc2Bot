// Utilities.h
#pragma once
#include "sc2api/sc2_api.h"
#include <sc2api/sc2_typeenums.h>

// returns string corresponding to the input race
std::string GetStringFromRace(const sc2::Race RaceIn);

// returns the value of the bit at the given x and y coordinates of an imagedata (bitmap)
int GetDataValueBit(sc2::ImageData data, int x, int y);

// prints map with no markers
void PrintMap(sc2::ImageData data);
// prints map with a singular marker
void PrintMap(sc2::ImageData data, sc2::Point2DI marker);
// prints map with multiple markers
void PrintMap(sc2::ImageData data, std::vector<sc2::Point2DI> markers);

// returns whether a type id corresponds to a unit
struct IsUnit {
    IsUnit(sc2::UNIT_TYPEID type);
    sc2::UNIT_TYPEID type_;
    bool operator()(const sc2::Unit& unit);
};

// segments the map into a chunk of the desired size (using bitmaps from the imagedata)
sc2::ImageData GetMapChunk(sc2::ImageData data, sc2::Point2DI start, sc2::Point2DI end);

// returns a list of 50 (default) pinch points on the map
std::vector<sc2::Point2DI> FindAllPinchPoints(sc2::ImageData data, int num_pinch_points=75, int num_chunks=1, int stride=192);

// structure check
bool IsStructure(const sc2::UNIT_TYPEID &unit_type);
bool IsStructure(const sc2::Unit &unit);

// functor for structure check
struct FIsStructure {
    bool operator()(const sc2::Unit &unit) const {
        return IsStructure(unit);
    }
};
// functor for structure check
struct NotStructure {
    bool operator()(const sc2::Unit &unit) const {
        return !IsStructure(unit);
    }
};


bool IsRock(const sc2::UNIT_TYPEID &unit_type);
bool IsRock(const sc2::Unit &unit);


// IsArmy filter 
struct IsArmy {
    IsArmy(const sc2::ObservationInterface* obs) : observation_(obs) {
    }

    bool operator()(const sc2::Unit& unit) {
        auto attributes = observation_->GetUnitTypeData().at(unit.unit_type).attributes;
        for (const auto& attribute : attributes) {
            if (attribute == sc2::Attribute::Structure) {
                return false;
            }
        }
        switch (unit.unit_type.ToType()) {
            case sc2::UNIT_TYPEID::ZERG_OVERLORD:
                return false;
            case sc2::UNIT_TYPEID::PROTOSS_PROBE:
                return false;
            case sc2::UNIT_TYPEID::ZERG_DRONE:
                return false;
            case sc2::UNIT_TYPEID::TERRAN_SCV:
                return false;
            case sc2::UNIT_TYPEID::ZERG_QUEEN:
                return false;
            case sc2::UNIT_TYPEID::ZERG_LARVA:
                return false;
            case sc2::UNIT_TYPEID::ZERG_EGG:
                return false;
            case sc2::UNIT_TYPEID::TERRAN_MULE:
                return false;
            case sc2::UNIT_TYPEID::TERRAN_NUKE:
                return false;
            default:
                return true;
        }
    }

    const sc2::ObservationInterface* observation_;
};