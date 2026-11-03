
#include "Application.h"
#include <iostream>
#include <string>
#include <exception>

int main() {
    try {
        Application app;
        std::string error;

        const std::string folder = "database";

        // 1. LOAD: TXT -> RAM
        if (!app.service().load(folder, error)) {
            std::cerr << "Load failed: " << error << std::endl;
            return 1;
        }

        // 2. LOGIN
        if (!app.users().login("admin", "admin123")) {
            std::cerr << "Login failed" << std::endl;
            return 1;
        }

        // 3. CREATE
        ResourceId id = app.resources().nextResourceId();

        Vocabulary item(
            id,
            LevelId("L005"),
            "hon",
            "hon",
            "book",
            PartOfSpeech::NOUN,
            "This is a book."
        );

        if (!app.resources().addVocabulary(item, error)) {
            std::cerr << "Create failed: " << error << std::endl;
            return 1;
        }

        std::cout << "CREATE OK" << std::endl;

        // 4. READ
        const Resource* found = app.resources().find(id);

        if (!found) {
            std::cerr << "Read failed" << std::endl;
            return 1;
        }

        std::cout << "READ OK: "
                  << found->getId().getId()
                  << " - " << found->getTerm()
                  << std::endl;

        // 5. UPDATE
        Vocabulary updated(
            id,
            LevelId("L005"),
            "hon",
            "hon",
            "book updated",
            PartOfSpeech::NOUN,
            "This is my book."
        );

        if (!app.resources().updateVocabulary(updated, error)) {
            std::cerr << "Update failed: " << error << std::endl;
            return 1;
        }

        std::cout << "UPDATE OK" << std::endl;

        // 6. SAVE: RAM -> TXT
        if (!app.service().save(folder, error)) {
            std::cerr << "Save failed: " << error << std::endl;
            return 1;
        }

        std::cout << "SAVE OK" << std::endl;
        std::cout << "Saved ID: " << id.getId() << std::endl;

        return 0;

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}
