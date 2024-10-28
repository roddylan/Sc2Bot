// Utilities.h
#pragma once
#include "sc2api/sc2_api.h"

std::string GetStringFromRace(const sc2::Race RaceIn);
int GetDataValueBit(sc2::ImageData data, int x, int y);
void PrintMap(sc2::ImageData data, sc2::Point2DI marker);
void PrintMap(sc2::ImageData data);
struct IsUnit {
    IsUnit(sc2::UNIT_TYPEID type);
    sc2::UNIT_TYPEID type_;
    bool operator()(const sc2::Unit& unit);
};
sc2::ImageData GetMapChunk(sc2::ImageData data, sc2::Point2DI start, sc2::Point2DI end);
sc2::Point2DI FindPinchPointAroundPoint(sc2::ImageData data, sc2::Point2DI point, int num_chunks = 3, int stride = 16);