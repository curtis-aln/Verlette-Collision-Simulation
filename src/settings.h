#pragma once


struct Settings
{
    const unsigned int particles = 1'000;
    const unsigned int vertexReserve = 100;
    const unsigned int circleSides = 30;
    const unsigned int deltaGridRate = 1;

    const float maxSpeed = 5.0f;
    const float entityRadius = 5.0f;
    const float zoomStrength = 1.0f;

    const float screenWidth = 1600.0f;
    const float screenHeight = 900.0f;


    float scale_factor = 1.0f;

	const float world_width = screenWidth * scale_factor;
	const float world_height = screenHeight * scale_factor;

    unsigned int CellsX = 30;
    unsigned int CellsY = 30;

	sf::Color colorActive = { 255, 0, 0 };
	sf::Color colorInctive = { 255, 255, 255 };
};
