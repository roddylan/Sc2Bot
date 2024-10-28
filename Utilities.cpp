#include "Utilities.h"
#include <iostream>
#include <cmath>
#include "Betweenness.h"


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

void PrintMap(sc2::ImageData data) {
    for (int y = data.height - 1; y > -1; --y) { // print starting at top of y level
        for (int x = 0; x < data.width; ++x) {
           std::cout << (GetDataValueBit(data, x, y) == 1 ? " " : "0");
        }
        std::cout << std::endl;
    }
}

IsUnit::IsUnit(sc2::UNIT_TYPEID type) : type_(type) {}
bool IsUnit::operator()(const sc2::Unit& unit) { return unit.unit_type == type_; }


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

sc2::Point2DI FindPinchPointAroundPoint(sc2::ImageData data, sc2::Point2DI point, int num_chunks, int stride) {
	// num_chunks: number of chunks to divide the map section into
	// stride: size of each chunk
	// has an overlap of stride/2
	// explores a total of (num_chunks-1)/2 chunks in each direction from the provided point
    double best_betweenness = 0;
    sc2::Point2DI center = sc2::Point2DI(192 / 2, 192 / 2);
    point = sc2::Point2DI(point.x + round((center.x - point.x) / 5) - stride * ((num_chunks - 1) / 2), point.y + round((center.y - point.y) / 5) - stride * ((num_chunks - 1) / 2));
    sc2::Point2DI best_point = sc2::Point2DI(0, 0);
    for (int chunk_y = 0; chunk_y < num_chunks; ++chunk_y) {
        for (int chunk_x = 0; chunk_x < num_chunks; ++chunk_x) {
            int grid_x = (chunk_x * stride / 2) + point.x;
            int grid_y = (chunk_y * stride / 2) + point.y;
            sc2::ImageData chunk_data = GetMapChunk(data, sc2::Point2DI(grid_x, grid_y), sc2::Point2DI(grid_x + stride, grid_y + stride));
            std::tuple<sc2::Point2DI, double> tup = calculateBetweenness(chunk_data);
            if (std::get<1>(tup) > best_betweenness) {
                best_betweenness = std::get<1>(tup);
                best_point = sc2::Point2D((std::get<0>(tup).x + grid_x), (std::get<0>(tup).y + grid_y));
            }
        }
    }
	return best_point;
}

