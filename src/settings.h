#pragma once


struct Settings
{
    const unsigned int particles = 10'000;
    const unsigned int vertexReserve = 100;
    const unsigned int circleSides = 30;
    const unsigned int deltaGridRate = 1;

    const float maxSpeed = 5.0f;
    const float entityRadius = 5.0f;
    const float zoomStrength = 1.0f;

    const float screenWidth = 1600.0f;
    const float screenHeight = 900.0f;

    unsigned int CellsX = 10;
    unsigned int CellsY = 10;

	sf::Color colorActive = { 255, 0, 0 };
	sf::Color colorInctive = { 255, 255, 255 };
};
