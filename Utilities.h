// Utilities.h
#pragma once
#include "sc2api/sc2_api.h"

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