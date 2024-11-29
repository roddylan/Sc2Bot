sc2::Point2D BasicSc2Bot::FindPlaceablePositionNear(const sc2::Point2D& starting_point, const sc2::ABILITY_ID& ability_to_place_building) {
    /*
    * We want to find a position that is near starting_point that works to fit the building
    * Idea: given a starting position, spiral outwards until you find a place that is suitable
    * - we want buildings to have 2 spaces to the left & right to make sure that there's room for addom buildings
    */

    sc2::QueryInterface* query = Query();
    /*
    * When leaving the loop, pos_to_place should be set
    */
    sc2::Point2D pos_to_place_at;
    float x_min = 0, x_max = 0;
    float y_min = 0, y_max = 0;
    float x_offset = 0, y_offset = 0;
    // static std::map<std::pair<int32_t,int32_t>, LastSuccessfulSearchValues, FlooredStartingPointLt> result_cache;
    std::pair<int32_t, int32_t> floored_starting_point = {std::floor(starting_point.x), std::floor(starting_point.y) };
    // if (result_cache.find(floored_starting_point) != result_cache.end()) {
    //     const LastSuccessfulSearchValues& values = result_cache[floored_starting_point];
    //     x_min = values.x_min;
    //     x_max = values.x_max;
    //     y_min = values.y_min;
    //     x_offset = values.x_offset;
    //     y_offset = values.y_offset;
    // }
    enum Direction { UP = 0, RIGHT = 1, DOWN = 2, LEFT = 3 };
    Direction current_direction = UP;  // 0 for up, 1 for right, 2 for down, 3 for left
    /*
    * If you are hitting the max/min offset, switch directions & increase the bounds for the
    * next time you spiral around.
    */
    const auto check_bounds_and_flip_direction = [&y_offset, &x_offset, &y_max, &y_min, &x_max, &x_min, &current_direction]() -> void {
        if (current_direction == UP && y_offset >= y_max) {
            current_direction = (Direction)((current_direction + 1) % 4);
            y_max += 2;
        }
        else if (current_direction == RIGHT && x_offset >= x_max) {
            current_direction = (Direction)((current_direction + 1) % 4);
            x_max += 2;
        }
        else if (current_direction == DOWN && y_offset <= y_min) {
            current_direction = (Direction)((current_direction + 1) % 4);
            y_min += -2;
        }
        else if (current_direction == LEFT && x_offset <= x_min) {
            current_direction = (Direction)((current_direction + 1) % 4);
            x_min += -2;
        }
        };
    /*
    * Adjust x_offset and y_offset to move in the current direction
    */
    const auto step_offset = [&x_offset, &y_offset, &current_direction]() -> void {
        switch (current_direction) {
        case UP:
            y_offset += 2;
            break;
        case RIGHT:
            x_offset += 2;
            break;
        case DOWN:
            y_offset += -2;
        case LEFT:
            x_offset += -2;
            break;
        }
        };
    /*
    * Cache points that are not valid placements & when we last checked that they are invalid placements.
    * - only re-check that a spot is still invalid if we havent checked within 1000 loops
    * - should cutdown on the amount of queries we need to make while also being able to find placement positions again if
    *   the previous building was destroyed
    */
    static std::map<sc2::Point2D, size_t, Point2DLt> point_cache;
    
    size_t loop_count = 0;
    bool done_searching = false;
    while (!done_searching) {
        ++loop_count;
        const sc2::Point2D current_position = starting_point + sc2::Point2D(x_offset, y_offset);
        const bool& point_is_in_cache = point_cache.find(current_position) != point_cache.end();
        if (point_is_in_cache) {
            // we have checked this point before, and it was not placeable. so its probably not placeable now?
            const size_t loops_since_this_point_was_last_checked = point_cache[current_position];
            if (loops_since_this_point_was_last_checked < 1'000) {
                // if it hasnt been that long since we learned that this spot is occupied, don't bother checking again
                // (but still increment the age)
                point_cache[current_position] += 1;
                check_bounds_and_flip_direction();
                step_offset();
                continue;
            }
            else {
                // otherwise, it has been a while since we last checked if this point was occupied or not, so we should check again
                point_cache[current_position] = 0;
            }
        }
        const bool can_wiggle_left = query->Placement(ability_to_place_building, sc2::Point2D(current_position.x - 3, current_position.y));
        if (!can_wiggle_left) {
            // switch direction
            
            check_bounds_and_flip_direction();
            step_offset();
            if (point_cache.find(current_position) == point_cache.end()) {
                point_cache[current_position] = 1;
            }
            else {
                point_cache[current_position] += 1;
            }
            continue;
        }
        const bool can_place_here = query->Placement(ability_to_place_building, current_position);
        if (!can_place_here) {
            check_bounds_and_flip_direction();
            step_offset();
            if (point_cache.find(current_position) == point_cache.end()) {
                point_cache[current_position] = 1;
            }
            else {
                point_cache[current_position] += 1;
            }
            continue;
        }
        const bool can_wiggle_right = query->Placement(ability_to_place_building, sc2::Point2D(current_position.x + 3, current_position.y));
        if (!can_wiggle_right) {
            // switch direction
            check_bounds_and_flip_direction();
            step_offset();
            if (point_cache.find(current_position) == point_cache.end()) {
                point_cache[current_position] = 1;
            }
            else {
                point_cache[current_position] += 1;
            }
            continue;
        }

        bool is_expansion_location = false;
        for (const sc2::Point3D& expansion_location : expansion_locations) {
            float distance_squared = sc2::DistanceSquared2D(expansion_location, current_position);
            if (distance_squared <= 10.0f) {
                is_expansion_location = true;
                check_bounds_and_flip_direction();
                step_offset();
                if (point_cache.find(current_position) == point_cache.end()) {
                    point_cache[current_position] = 1;
                }
                else {
                    point_cache[current_position] += 1;
                }
                break;
            }
        }
        if (!is_expansion_location) {
            pos_to_place_at = current_position;
            done_searching = true;
            return pos_to_place_at;
        }
        if (loop_count > 250) {
            std::cout << "loop count=" << loop_count << std::endl;
        }
    }

    // result_cache[floored_starting_point] = { x_min, x_max, y_min, y_max, x_offset, y_offset };

    return pos_to_place_at;
}