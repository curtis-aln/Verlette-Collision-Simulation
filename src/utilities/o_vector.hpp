#pragma once

#include <vector>

/*
* o_vector is my highly optimized datastructure for managing the adding and removal of objects
* it works with several containers:
- std::vector<bool> active_{}; - Tracks what indexes are active or inactive
- std::vector<uint32_t> array{}; - Contains all the indexes // todo - does this need to exist

// Usage:
- to add objects to this datastructure use emplace() it allocates new memory and stores an object in its internal container
- to add and remove existing objects use the add() and remove() functions
- To iterate, for (Obj* object : o_vector)

*/


 // add this as a public parent struct to the Obj class/structure
struct o_vec_object
{
    uint32_t o_vec_index = 0;
    bool active = true;

    o_vec_object() = default;
};


template <class Obj>
class o_vector
{
public:
    // this tracks the amount of objects there are active
    int active_objs = 0;
    int resize_buffering = 300; // if the array is full, we add this amount of empty slots to the end of the array 

private:
    // this vector tracks which objects are active and which are not, it is used to iterate over the array and skip inactive objects
    // every object emplaced into this datastructure gets its own index in this vector
    std::vector<bool> active_{};

    // this array contains all the items and is never directly modified
    std::vector<uint32_t> all_object_indexes_{};

    // this tracks the actual initilised and implemented Objects being used
    uint32_t array_size = 0;

    // this vectorPtr stores all the actual objects on the heap, they are never modified or removed from. only added to
    std::vector<Obj> raw_object_store_{};

    // These are all of the indexes where active_[id_] is false, this is used to find the next available slot in the array when adding a new object
    std::vector<uint32_t> free_list{};
    
    // The current size of the free_list container, this lets us avoid true removal from the vector
    int free_count = 0;


private:
    // This iterator allows the datastructure to be used in a for loop as so
    // It iterates through all *active* objects in the datastructure, any inactive objects will be skipped
    // for (Obj* object : o_vector)
    class Iterator
    {
    public:
        using iterator_category = std::forward_iterator_tag;
        using value_type = Obj*;
        using difference_type = std::ptrdiff_t;
        using pointer = Obj*;
        using reference = Obj&;

        explicit Iterator(o_vector* vec = nullptr, const unsigned index = 0) 
            : vectorPtr(vec), currentIndex(index) {}

        Iterator() : vectorPtr(nullptr) {}

        Iterator& operator++()
        {
            ++currentIndex;
            while (currentIndex < vectorPtr->array_size && !vectorPtr->active_[currentIndex])
                ++currentIndex;
            return *this;
        }

        pointer operator*()   const { return &vectorPtr->raw_object_store_[currentIndex]; }
        pointer operator->()  const { return &vectorPtr->raw_object_store_[currentIndex]; }

        bool operator==(const Iterator& other) const { return currentIndex == other.currentIndex; }
        bool operator!=(const Iterator& other) const { return !(*this == other); }

    private:
        o_vector* vectorPtr;
        unsigned currentIndex = 0;
    };


    unsigned getFirstAvalableIteration() const
    {
        unsigned currentIndex = 0;
        while (currentIndex < array_size && !active_[currentIndex])
            ++currentIndex;
        return currentIndex;
    }


public:
    explicit o_vector(const std::size_t N)
    {
        raw_object_store_.reserve(N);
        all_object_indexes_.reserve(N);
		active_.reserve(N);
        free_list.reserve(N);
    }

    [[nodiscard]] Iterator begin() const { return Iterator(const_cast<o_vector*>(this), getFirstAvalableIteration()); }
    [[nodiscard]] Iterator end()   const { return Iterator(const_cast<o_vector*>(this), array_size); }
    [[nodiscard]] unsigned size()  const { return active_objs; }
    [[nodiscard]] unsigned raw_objects_size()  const { return raw_object_store_.size(); }
	[[nodiscard]] bool can_add() const { return free_count > 0; }


    // used to initilise items inside of raw_object_store_.
	// returns the item in the array
    Obj* emplace(bool is_active = true)
    {
        // Add the actual object to our object store
        raw_object_store_.emplace_back();
		int id_ = raw_object_store_.size() - 1;

		// add the object's index to the list of all object indexes
        all_object_indexes_.push_back(id_);
        ++array_size;

		// allocating memory in both of these containers
        active_.push_back(is_active);
		free_list.push_back(-1); // value does not matter as free_count will overwrite it when incremented

        if (is_active)
        {
            ++active_objs;
        }
		else // Not active, add to free list
        {
            free_list[free_count++] = id_;
        }
        
        // setting the object's id_
		Obj* object = &raw_object_store_.back();
		object->id_ = array_size - 1;

        return object;
    }

    void smart_resize()
    {
        // Grow when almost full
        if (free_count < resize_buffering / 2)
        {
            const uint32_t old_size = array_size;

            // todo reserve doesn't prevent reallocation on emplace_back if called multiple times
            raw_object_store_.reserve(raw_object_store_.size() + resize_buffering);
            for (int i = 0; i < resize_buffering; ++i)
            {
                raw_object_store_.emplace_back();
                all_object_indexes_.push_back(raw_object_store_.size() - 1);
                free_list.push_back(old_size + i);
                ++free_count;
            }
            array_size += resize_buffering;
        }

        // Shrink when too much slack — only trim inactive tail slots
        else if (free_count > resize_buffering * 2)
        {
            int trimmed = 0;
            while (trimmed < resize_buffering && array_size > 0 && !active_[array_size - 1])
            {
                all_object_indexes_.pop_back();
                --array_size;
                // remove the corresponding entry from free_list
                for (int i = 0; i < free_count; ++i)
                {
                    if (free_list[i] == array_size)
                    {
                        free_list[i] = free_list[--free_count];
                        break;
                    }
                }
                ++trimmed;
            }
        }
    }


    Obj* at(const unsigned i) { return &raw_object_store_[i]; }
    const Obj* at(const unsigned i) const { return &raw_object_store_[i]; }


    Obj* add()
    {
        if (free_count == 0)
            return nullptr;

        const unsigned idx = free_list[--free_count];
        active_[idx] = true;
        ++active_objs;
        return &raw_object_store_[idx];
    }


    void remove(Obj* obj) 
    { 
        remove(obj->id_); 
    }

    void remove(const unsigned vector_index)
    {
        if (active_[vector_index])
        {
            active_[vector_index] = false;
            --active_objs;
            free_list[free_count++] = vector_index;
        }
    }

    int      free_slots()   const { return free_count; }
};

// Current: 191 lines, gonna try and write 30 lines of commenting