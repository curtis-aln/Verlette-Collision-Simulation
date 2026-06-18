#pragma once

#include <chrono>
#include <thread>

class frame_rater {
public:
    explicit frame_rater(double fps = 60.0) :
        time_between_frames{ 1.0 / fps },
        tp{ std::chrono::steady_clock::now() }
    {}

    // change the frame rate on the fly
    void set_fps(double fps) {
        time_between_frames = std::chrono::duration<double>{ 1.0 / fps };
    }

    void sleep() {
        // add to time point
        tp += std::chrono::duration_cast<std::chrono::steady_clock::duration>(time_between_frames);

        // and sleep until that time point
        std::this_thread::sleep_until(tp);
    }

private:
    // duration representing 1/fps seconds, recomputed when fps changes
    std::chrono::duration<double> time_between_frames;

    // the time point we'll add to in every loop
    std::chrono::time_point<std::chrono::steady_clock> tp;
};