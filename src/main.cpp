/*
    Makes a basic collision detection simulation Using a spatial Hash grid

    keys:
    > space - pause
    > Esc   - close
    > 1     - grid size +
    > 2     - grid size -
*/

#include <iostream>
#include <SFML/Graphics.hpp>

#include <vector>
#include <ctime>
#include <string>
#include <sstream>
#include "simulation.h"

int main()
{
    Simulation simulation{};
	simulation.run();
	return 0;
}
