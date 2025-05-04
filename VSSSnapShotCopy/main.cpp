#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>
#include <filesystem>
#include "SnapshotRestorer.h"

int main() {
    // Get the directory of the executable
    std::filesystem::path exePath = std::filesystem::current_path();
    std::filesystem::path configPath = exePath / "config.json";

    // Open the config file
    std::ifstream configFile(configPath);
    if (!configFile.is_open()) {
        std::cerr << "Error: Could not open config.json at " << configPath << std::endl;
        return 1;
    }

    // Parse the JSON configuration
    nlohmann::json config;
    try {
        configFile >> config;
    }
    catch (const std::exception& e) {
        std::cerr << "Error: Failed to parse config.json - " << e.what() << std::endl;
        return 1;
    }

    // Initialize and restore
    SnapshotRestorer restorer(config, 4096);
    if (!restorer.Restore()) {
        std::cerr << "Error: Restore operation failed." << std::endl;
        return 1;
    }

    std::cout << "Restore operation completed successfully." << std::endl;
    return 0;
}
