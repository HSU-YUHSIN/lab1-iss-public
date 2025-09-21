#include "mem_map.h"
#include "abstract_mem.h"
#include "common.h"

#include <stdlib.h>
#include <assert.h>

int MemoryMap_ctor(MemoryMap *self) {
    assert(self != NULL);
    self->num_device     = 0;
    self->memory_map_arr = NULL;
    return 0;
}

void MemoryMap_dtor(MemoryMap *self) {
    assert(self != NULL);
    free(self->memory_map_arr);
}

int MemoryMap_add_device(MemoryMap *self, mmap_unit_t new_mem_map_unit) {
    assert(self != NULL);

    if (self->num_device == 0) {
        if (NULL == (self->memory_map_arr = malloc(sizeof(mmap_unit_t)))) {
            return -1;
        }
        self->num_device += 1;
        self->memory_map_arr[0] = new_mem_map_unit;
        return 0;
    }

    self->num_device += 1;
    mmap_unit_t *old_arr = self->memory_map_arr;
    if (NULL == (self->memory_map_arr = malloc(self->num_device * sizeof(mmap_unit_t)))) {
        return -1;
    }

    // copy old devices
    for (int i = 0; i < self->num_device - 1; i++) {
        self->memory_map_arr[i] = old_arr[i];
    }
    self->memory_map_arr[self->num_device - 1] = new_mem_map_unit;

    // release old array
    free(old_arr);
    return 0;
}

void MemoryMap_generic_load(MemoryMap *self, addr_t base_addr, unsigned length, byte_t *buffer) {
    assert(self != NULL);

    // search in self->memory_map_arr
    mmap_unit_t *mmap_unit_ptr = NULL;
    for (int i = 0; i < self->num_device; i++) {
        if ((base_addr >= self->memory_map_arr[i].addr_bound.first) &&
            (base_addr + length <= self->memory_map_arr[i].addr_bound.second)) {
            mmap_unit_ptr = &self->memory_map_arr[i];
        }
    }

    Assert(mmap_unit_ptr != NULL, "MMIO search failed! The requested address is: 0x%08x, length is: %d",
           base_addr, length);

    // call generic load function of the device
    AbstractMem_load(mmap_unit_ptr->device_ptr,
                     base_addr - mmap_unit_ptr->addr_bound.first, length, buffer);
}

void MemoryMap_generic_store(MemoryMap *self, addr_t base_addr, unsigned length, const byte_t *ref_data) {
    assert(self != NULL);

    // search in self->memory_map_arr
    mmap_unit_t *mmap_unit_ptr = NULL;
    for (int i = 0; i < self->num_device; i++) {
        if ((base_addr >= self->memory_map_arr[i].addr_bound.first) &&
            (base_addr + length <= self->memory_map_arr[i].addr_bound.second)) {
            mmap_unit_ptr = &self->memory_map_arr[i];
        }
    }

    Assert(mmap_unit_ptr != NULL, "MMIO search failed! The requested address is: 0x%08x, length is: %d",
           base_addr, length);

    // call generic store function of the device
    AbstractMem_store(mmap_unit_ptr->device_ptr,
                      base_addr - mmap_unit_ptr->addr_bound.first, length, ref_data);
}
