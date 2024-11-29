#include "Utilities.h"
#include <iostream>
#include <cmath>
#include "Betweenness.h"
#include <unordered_map>
#include <functional>


std::string GetStringFromRace(const sc2::Race RaceIn)
{
    if (RaceIn == sc2::Race::Terran)
    {
        return "terran";
    }
    else if (RaceIn == sc2::Race::Protoss)
    {
        return "protoss";
    }
    else if (RaceIn == sc2::Race::Zerg)
    {
        return "zerg";
    }
    else if (RaceIn == sc2::Race::Random)
    {
        return "random";
    }
    return "random";
}

int GetDataValueBit(sc2::ImageData data, int x, int y)
{
    int pixelID = x + y * data.width;
    int byteLocation = pixelID / 8;
    int bitLocation = pixelID % 8;
    int bit = data.data[byteLocation] & (1 << (7 - bitLocation));
    return bit == 0 ? 0 : 1;
}

// prints map with a singular marker
void PrintMap(sc2::ImageData data, sc2::Point2DI marker) {
    for (int y = data.height - 1; y > -1; --y) { // print starting at top of y level
        for (int x = 0; x < data.width; ++x) {
            if (marker.x == x && marker.y == y) {
                std::cout << "X";
            }
            else {
                std::cout << (GetDataValueBit(data, x, y) == 1 ? " " : "0");
            }
        }
        std::cout << std::endl;
    }
}

// prints map without any markers
void PrintMap(sc2::ImageData data) {
    for (int y = data.height - 1; y > -1; --y) { // print starting at top of y level
        for (int x = 0; x < data.width; ++x) {
           std::cout << (GetDataValueBit(data, x, y) == 1 ? " " : "0");
        }
        std::cout << std::endl;
    }
}

// prints map with multiple markers
void PrintMap(sc2::ImageData data, std::vector<sc2::Point2DI> markers) {
    for (int y = data.height - 1; y > -1; --y) { // print starting at top of y level
		for (int x = 0; x < data.width; ++x) {
			bool found = false;
			for (sc2::Point2DI marker : markers) {
				if (marker.x == x && marker.y == y) {
					std::cout << "X";
					found = true;
                    break;
				}
                
			}
            if (!found) {
                std::cout << (GetDataValueBit(data, x, y) == 1 ? " " : "0");
            }
        }
        std::cout << std::endl;
    }
}

IsUnit::IsUnit(sc2::UNIT_TYPEID type) : type_(type) {}
bool IsUnit::operator()(const sc2::Unit& unit) { return unit.unit_type == type_; }


// segments the map into a chunk of the desired size (using bitmaps from the imagedata)
sc2::ImageData GetMapChunk(sc2::ImageData data, sc2::Point2DI lower_left, sc2::Point2DI top_right) {
    // Validate the input bounds
    if (lower_left.x < 0) {
        lower_left.x = 0;
    }
    if (lower_left.y < 0) {
        lower_left.y = 0;
    }
	if (top_right.x > data.width) {
		top_right.x = data.width;
	}
    if (top_right.y > data.height) {
        top_right.y = data.height;
    }
    // Calculate the new chunk's width and height
    int chunk_width = top_right.x - lower_left.x;
    int chunk_height = top_right.y - lower_left.y;

    // Initialize a new ImageData object for the chunk
    sc2::ImageData chunk;
    chunk.width = chunk_width;
    chunk.height = chunk_height;
    int num_bytes = (chunk_width * chunk_height + 7) / 8;
    chunk.data.resize(num_bytes);  // Resize to hold the new chunk's bit data

    // Iterate through each pixel in the chunk region
    for (int y = lower_left.y; y < top_right.y; ++y) {
        for (int x = lower_left.x; x < top_right.x; ++x) {
            // Calculate the corresponding position in the chunk
            int chunk_x = x - lower_left.x;
            int chunk_y = y - lower_left.y;

            // Get the original bit value using GetDataValueBit
            int bit_value = GetDataValueBit(data, x, y);

            // Calculate the pixel ID and the corresponding byte and bit locations in the chunk
            int pixelID = chunk_x + chunk_y * chunk_width;
            int byteLocation = pixelID / 8;
            int bitLocation = pixelID % 8;

            // Update the corresponding byte with the bit value
            if (bit_value == 1) {
                chunk.data[byteLocation] |= (1 << (7 - bitLocation));
            }
            else {
                chunk.data[byteLocation] &= ~(1 << (7 - bitLocation));
            }
        }
    }
    return chunk;
}

// hash function for std::tuple<int, int>
namespace std {
    template <>
    struct hash<std::tuple<int, int>> {
        size_t operator()(const std::tuple<int, int>& t) const {
            // Combine the hashes of the two integers using XOR and bit shifts
            size_t h1 = hash<int>()(std::get<0>(t));
            size_t h2 = hash<int>()(std::get<1>(t));
            return h1 ^ (h2 << 1);  // Combine the two hashes
        }
    };
}

// returns a list of 50 pinch points on the map
std::vector<sc2::Point2DI> FindAllPinchPoints(sc2::ImageData data, int num_pinch_points, int num_chunks, int stride) {
	// creates a square of num_chunks^2, with each chunk of size stride
    // does half strides resulting in (num_chunks * 2) - 1 chunk computations per dimension
	// (num_chunks * 2 - 1)^2 total chunks are computed (just like doing convolutions)
	// returns an array of num_pinch_points pinch points

    int min_dist_between_points = 10;  // Minimum distance between pinch points
    std::unordered_map<std::tuple<int, int>, double> point_betweenness_map;

    // Iterate over chunks
    for (int chunk_y = 0; chunk_y < num_chunks; ++chunk_y) {
        for (int chunk_x = 0; chunk_x < num_chunks; ++chunk_x) {
            for (int divider = 0; divider < 2; ++divider) {
                int grid_x = (chunk_x * stride) + (divider * stride);
                int grid_y = (chunk_y * stride) + (divider * stride);

				if (grid_x + stride > data.width || grid_y + stride > data.height) {
					continue; // Skip if the chunk is out of bounds
				}

                // Get chunk data
                sc2::ImageData chunk_data = GetMapChunk(data, sc2::Point2DI(grid_x, grid_y), sc2::Point2DI(grid_x + stride, grid_y + stride));
                std::vector<std::pair<sc2::Point2DI, double>> betweennessList = getBetweennessList(chunk_data);

                // Accumulate betweenness scores
                for (const auto& in_list : betweennessList) {
                    int new_x = in_list.first.x + grid_x;
                    int new_y = in_list.first.y + grid_y;
                    auto point_key = std::make_tuple(new_x, new_y);
                    point_betweenness_map[point_key] += in_list.second; // accumulate betweenness
                }
            }
        }
    }

    // Convert map to vector for sorting
    std::vector<std::pair<sc2::Point2DI, double>> all_points;
    all_points.reserve(point_betweenness_map.size());  // reserve memory
    for (const auto& entry : point_betweenness_map) {
        all_points.emplace_back(sc2::Point2DI(std::get<0>(entry.first), std::get<1>(entry.first)), entry.second);
    }

    // Sort points by betweenness (descending)
    std::partial_sort(all_points.begin(), all_points.begin() + std::min(num_pinch_points*500, (int)all_points.size()), all_points.end(),
        [](const std::pair<sc2::Point2DI, double>& a, const std::pair<sc2::Point2DI, double>& b) {
            return a.second > b.second;
        });

    // Select top points based on distance criteria
    std::vector<sc2::Point2DI> best_points;
    best_points.reserve(num_pinch_points);  // reserve memory for best points
    for (const auto& all_point : all_points) {
        bool rejected = false;
		// makes sure that points are at least min_dist_between_points apart in both x and y
		// to prevent clumps of points all together
        for (const sc2::Point2DI& best_point : best_points) {
            if (std::abs(best_point.x - all_point.first.x) < min_dist_between_points && std::abs(best_point.y - all_point.first.y) < min_dist_between_points) {
                rejected = true;
                break;
            }
        }
        if (!rejected) {
            best_points.push_back(all_point.first);
            if (best_points.size() >= num_pinch_points) {
                break;
            }
        }
    }
    return best_points;
}


/**
 * @brief Check if unit_type is structure
 * 
 * @param unit_type
 * @return true if unit_type is structure
 * @return false if unit_type is not structure
 */
bool IsStructure(const sc2::UNIT_TYPEID &unit_type) {
    return (
        // TERRAN UNITS
        unit_type == sc2::UNIT_TYPEID::TERRAN_COMMANDCENTER ||
        unit_type == sc2::UNIT_TYPEID::TERRAN_COMMANDCENTERFLYING ||
        unit_type == sc2::UNIT_TYPEID::TERRAN_ORBITALCOMMAND ||
        unit_type == sc2::UNIT_TYPEID::TERRAN_ORBITALCOMMANDFLYING ||
        unit_type == sc2::UNIT_TYPEID::TERRAN_PLANETARYFORTRESS ||
        unit_type == sc2::UNIT_TYPEID::TERRAN_SUPPLYDEPOT ||
        unit_type == sc2::UNIT_TYPEID::TERRAN_SUPPLYDEPOTLOWERED ||
        unit_type == sc2::UNIT_TYPEID::TERRAN_REFINERY ||
        unit_type == sc2::UNIT_TYPEID::TERRAN_REFINERYRICH ||
        unit_type == sc2::UNIT_TYPEID::TERRAN_BARRACKS ||
        unit_type == sc2::UNIT_TYPEID::TERRAN_BARRACKSFLYING ||
        unit_type == sc2::UNIT_TYPEID::TERRAN_BARRACKSREACTOR ||
        unit_type == sc2::UNIT_TYPEID::TERRAN_BARRACKSTECHLAB ||
        unit_type == sc2::UNIT_TYPEID::TERRAN_ENGINEERINGBAY ||
        unit_type == sc2::UNIT_TYPEID::TERRAN_FACTORY ||
        unit_type == sc2::UNIT_TYPEID::TERRAN_FACTORYFLYING ||
        unit_type == sc2::UNIT_TYPEID::TERRAN_FACTORYREACTOR ||
        unit_type == sc2::UNIT_TYPEID::TERRAN_FACTORYTECHLAB ||
        unit_type == sc2::UNIT_TYPEID::TERRAN_BUNKER ||
        unit_type == sc2::UNIT_TYPEID::TERRAN_MISSILETURRET ||
        unit_type == sc2::UNIT_TYPEID::TERRAN_SENSORTOWER ||
        unit_type == sc2::UNIT_TYPEID::TERRAN_ARMORY ||
        unit_type == sc2::UNIT_TYPEID::TERRAN_STARPORT ||
        unit_type == sc2::UNIT_TYPEID::TERRAN_STARPORTFLYING ||
        unit_type == sc2::UNIT_TYPEID::TERRAN_STARPORTREACTOR ||
        unit_type == sc2::UNIT_TYPEID::TERRAN_STARPORTTECHLAB ||
        unit_type == sc2::UNIT_TYPEID::TERRAN_FUSIONCORE ||
        // PROTOSS UNITS
        unit_type == sc2::UNIT_TYPEID::PROTOSS_NEXUS ||
        unit_type == sc2::UNIT_TYPEID::PROTOSS_ASSIMILATOR ||
        unit_type == sc2::UNIT_TYPEID::PROTOSS_ASSIMILATORRICH ||
        unit_type == sc2::UNIT_TYPEID::PROTOSS_PYLON ||
        unit_type == sc2::UNIT_TYPEID::PROTOSS_PYLONOVERCHARGED ||
        unit_type == sc2::UNIT_TYPEID::PROTOSS_GATEWAY ||
        unit_type == sc2::UNIT_TYPEID::PROTOSS_FORGE ||
        unit_type == sc2::UNIT_TYPEID::PROTOSS_CYBERNETICSCORE ||
        unit_type == sc2::UNIT_TYPEID::PROTOSS_PHOTONCANNON ||
        unit_type == sc2::UNIT_TYPEID::PROTOSS_SHIELDBATTERY ||
        unit_type == sc2::UNIT_TYPEID::PROTOSS_TWILIGHTCOUNCIL ||
        unit_type == sc2::UNIT_TYPEID::PROTOSS_STARGATE ||
        unit_type == sc2::UNIT_TYPEID::PROTOSS_ROBOTICSFACILITY ||
        unit_type == sc2::UNIT_TYPEID::PROTOSS_TEMPLARARCHIVE ||
        unit_type == sc2::UNIT_TYPEID::PROTOSS_DARKSHRINE ||
        unit_type == sc2::UNIT_TYPEID::PROTOSS_FLEETBEACON ||
        unit_type == sc2::UNIT_TYPEID::PROTOSS_ROBOTICSBAY ||
        // ZERG UNITS
        unit_type == sc2::UNIT_TYPEID::ZERG_HATCHERY ||
        unit_type == sc2::UNIT_TYPEID::ZERG_EXTRACTOR ||
        unit_type == sc2::UNIT_TYPEID::ZERG_EXTRACTORRICH ||
        unit_type == sc2::UNIT_TYPEID::ZERG_SPAWNINGPOOL ||
        unit_type == sc2::UNIT_TYPEID::ZERG_EVOLUTIONCHAMBER ||
        unit_type == sc2::UNIT_TYPEID::ZERG_LAIR ||
        unit_type == sc2::UNIT_TYPEID::ZERG_ROACHWARREN ||
        unit_type == sc2::UNIT_TYPEID::ZERG_BANELINGNEST ||
        unit_type == sc2::UNIT_TYPEID::ZERG_SPINECRAWLER ||
        unit_type == sc2::UNIT_TYPEID::ZERG_SPINECRAWLERUPROOTED ||
        unit_type == sc2::UNIT_TYPEID::ZERG_SPORECRAWLER ||
        unit_type == sc2::UNIT_TYPEID::ZERG_SPORECRAWLERUPROOTED ||
        unit_type == sc2::UNIT_TYPEID::ZERG_HYDRALISKDEN ||
        unit_type == sc2::UNIT_TYPEID::ZERG_INFESTATIONPIT ||
        unit_type == sc2::UNIT_TYPEID::ZERG_SPIRE ||
        unit_type == sc2::UNIT_TYPEID::ZERG_NYDUSNETWORK ||
        unit_type == sc2::UNIT_TYPEID::ZERG_LURKERDENMP ||
        unit_type == sc2::UNIT_TYPEID::ZERG_HIVE ||
        unit_type == sc2::UNIT_TYPEID::ZERG_NYDUSCANAL ||
        unit_type == sc2::UNIT_TYPEID::ZERG_ULTRALISKCAVERN ||
        unit_type == sc2::UNIT_TYPEID::ZERG_GREATERSPIRE
    );
}


/**
 * @brief Check if unit is structure
 * 
 * @param unit
 * @return true if unit is structure
 * @return false if unit is not structure
 */
bool IsStructure(const sc2::Unit &unit) {
    const sc2::UNIT_TYPEID unit_type = unit.unit_type;

    return IsStructure(unit_type);
}