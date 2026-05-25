#pragma once


inline int randint(const unsigned int start, const unsigned int end)
{
	return rand() % (end - start) + start;
}

inline float randfloat(const float start, const float end)
{
	return (static_cast<float>(rand()) / static_cast<float>(RAND_MAX) * (end - start)) + start;
}

template<typename T>
inline void removeBySwappingAndPopping(std::vector<T>& vec, size_t index) {
    // Check if the index is valid
    if (index >= vec.size()) {
        return;
    }

    // Swap the object at the specified index with the last object in the vector
    std::swap(vec[index], vec.back());

    // Remove the last object (which is now a duplicate of the object that was previously at the specified index)
    vec.pop_back();
}
