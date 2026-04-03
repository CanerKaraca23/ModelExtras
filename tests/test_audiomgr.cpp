#include "pch.h"
#include <iostream>
#include <cassert>
#include "utils/audiomgr.h"

// Mock globals
CIniReader gConfig;
CAudioEngine AudioEngine;

// Redefining members to avoid including the whole .cpp in a way that causes link errors,
// or we can just include the .cpp and mock everything it needs.
// Given the complexity of the .cpp, let's include it but ensure all its dependencies are mocked.
#include "../src/utils/audiomgr.cpp"

void test_ShouldPlaySound() {
    std::cout << "Running test_ShouldPlaySound..." << std::endl;

    // Test case 1: SoundEffects is true
    gConfig.boolValue = true;
    assert(AudioMgr::ShouldPlaySound() == true);
    std::cout << "  - Case true passed" << std::endl;

    // Test case 2: SoundEffects is false
    gConfig.boolValue = false;
    assert(AudioMgr::ShouldPlaySound() == false);
    std::cout << "  - Case false passed" << std::endl;

    std::cout << "test_ShouldPlaySound passed!" << std::endl;
}

int main() {
    test_ShouldPlaySound();
    return 0;
}
