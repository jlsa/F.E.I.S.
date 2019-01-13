//
// Created by Syméon on 13/01/2019.
//

#include <imgui.h>
#include <list>
#include <set>
#include <c++/8.2.1/fstream>
#include "Toolbox.h"


bool Toolbox::isShortcutPressed(std::initializer_list<sf::Keyboard::Key> anyOf, std::initializer_list<sf::Keyboard::Key> allOf) {
    for (auto key : allOf) {
        if (not sf::Keyboard::isKeyPressed(key)) {
            return false;
        }
    }
    for (auto key : anyOf) {
        if (sf::Keyboard::isKeyPressed(key)) {
            return true;
        }
    }
    return false;
}

void Toolbox::pushNewRecentFile(std::filesystem::path path) {
    std::ifstream readFile(std::filesystem::path("settings/recent files.txt"));
    std::list<std::string> recent;
    std::set<std::string> recent_set;
    for(std::string line; getline( readFile, line );) {
        if (recent_set.find(line) == recent_set.end()) {
            recent.push_back(line);
            recent_set.insert(line);
        }
    }
    readFile.close();

    recent.remove(std::filesystem::canonical(path).string());

    while (recent.size() >= 10) {
        recent.pop_back();
    }

    recent.push_front(std::filesystem::canonical(path).string());

    std::ofstream writeFile("settings/recent files.txt", std::ofstream::out | std::ofstream::trunc);
    for (const auto& line : recent) {
        writeFile << line << std::endl;
    }
    writeFile.close();
}

std::vector<std::string> Toolbox::getRecentFiles() {
    std::ifstream readFile(std::filesystem::path("settings/recent files.txt"));
    std::vector<std::string> recent;
    for(std::string line; getline( readFile, line ); ){
        recent.push_back(line);
    }
    readFile.close();
    return recent;
}