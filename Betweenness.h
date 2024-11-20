#pragma once
#include "sc2api/sc2_api.h"

// returns the point and betweenness of the point with the highest betweenness value
std::tuple<sc2::Point2DI, double> calculateBetweenness(sc2::ImageData data);
// returns a list of points and their betweenness values
std::vector<std::pair<sc2::Point2DI, double>> getBetweennessList(sc2::ImageData data);